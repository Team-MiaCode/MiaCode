#include "CoverBatchExport.h"
#include "CoverCompositionState.h"
#include "CoverLayoutModel.h"
#include "SceneFrameRenderer.h"
#include "preview/runtime/PreviewQuickExportSession.h"
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QPointer>
#include <QSet>
#include <QTimer>
#include <algorithm>

Q_LOGGING_CATEGORY(coverBatchLog, "miacode.cover.batch", QtWarningMsg)

namespace miacode::cover_export {
CoverBatchExport::CoverBatchExport(ExportEngine& engine, ui::CoverExportSession& session, QObject* parent)
    : QObject(parent), engine_(engine), session_(session)
{
    connect(&session_, &ui::CoverExportSession::presetsChanged, this, &CoverBatchExport::optionsChanged);
}
CoverBatchExport::~CoverBatchExport()
{
    if (running_ && cancelPublication_) cancelPublication_();
    if (running_ && end_) end_();
}
void CoverBatchExport::setExecutionHooks(Begin begin, std::function<void()> end,
                                        std::function<void(int)> progress)
{
    begin_ = std::move(begin); end_ = std::move(end); progress_ = std::move(progress);
}
void CoverBatchExport::setFilePublisher(ui::CoverExportSession::FilePublisher publisher,
                                       std::function<void()> cancelPublication)
{
    publisher_ = std::move(publisher); cancelPublication_ = std::move(cancelPublication);
}
QVariantList CoverBatchExport::presets() const
{
    QVariantList rows{QVariantMap{{"kind", "current"}, {"name", ""}, {"label", qtTrId("cover.batch_current")}}};
    for (const auto& value : session_.builtinPresets()) {
        const auto row = value.toMap();
        rows.append(QVariantMap{{"kind", "builtin"}, {"name", row.value("id")}, {"label", row.value("label")}});
    }
    for (const auto& value : session_.presets()) {
        const auto name = value.toMap().value("name").toString();
        rows.append(QVariantMap{{"kind", "user"}, {"name", name}, {"label", name}});
    }
    return rows;
}
QVariantList CoverBatchExport::results() const
{
    QVariantList rows;
    for (const auto& job : jobs_) rows.append(QVariantMap{
        {"label", job.label}, {"status", job.status}, {"path", job.path}, {"error", job.error}});
    return rows;
}
bool CoverBatchExport::start(const QVariantList& difficulties, const QVariantList& presets)
{
    qCDebug(coverBatchLog) << "start" << qGuiApp->applicationState()
                         << "suspend event blocking" << qgetenv("QT_BLOCK_EVENT_LOOPS_WHEN_SUSPENDED");
    if (running_ || session_.busy()) return false;
    error_.clear();
    QList<int> ids;
    const auto available = engine_.difficultyIds();
    for (const auto& value : difficulties) {
        bool valid = false;
        const int id = value.toInt(&valid);
        if (!valid || !available.contains(id)) {
            error_ = qtTrId("cover.no_difficulty_selected"); emit changed(); return false;
        }
        if (!ids.contains(id)) ids.append(id);
    }
    QVariantList selected;
    const auto options = this->presets();
    QSet<QString> seen;
    for (const auto& value : presets) {
        const auto row = value.toMap();
        const auto kind = row.value("kind").toString();
        const auto name = row.value("name").toString();
        const QString key = kind + QChar(0) + name;
        if (seen.contains(key)) continue;
        const auto option = std::find_if(options.cbegin(), options.cend(), [&](const QVariant& option) {
            const auto map = option.toMap();
            return map.value("kind").toString() == kind && map.value("name").toString() == name;
        });
        if (option == options.cend()) {
            error_ = qtTrId("cover.batch_invalid_preset"); emit changed(); return false;
        }
        seen.insert(key); selected.append(*option);
    }
    if (ids.isEmpty() || selected.isEmpty()) {
        error_ = qtTrId("cover.batch_selection_required"); emit changed(); return false;
    }
    if (session_.outputDirectory().trimmed().isEmpty()) {
        error_ = qtTrId("cover.batch_output_required"); emit changed(); return false;
    }
    QList<Job> pending;
    for (int id : ids) {
        const auto task = engine_.buildSeedTask(id);
        for (const auto& option : selected) {
            const auto row = option.toMap();
            Job job;
            job.task = task;
            job.composition = session_.batchComposition(row.value("kind").toString(), row.value("name").toString());
            job.inputs = session_.batchInputs(task, job.composition);
            job.label = task.intro.difficulty + QStringLiteral(" / ") + row.value("label").toString();
            job.baseName = task.intro.title + '-' + task.intro.difficulty + '-' + row.value("label").toString();
            pending.append(std::move(job));
        }
    }
    if (begin_ && !begin_(&error_)) { emit changed(); return false; }
    jobs_ = std::move(pending); completed_ = 0; canceled_ = false; running_ = true; ++generation_;
    session_.setBatchBusy(true);
    emit changed();
    // Keep capture contexts bounded to two renderers and reuse them for the
    // queue; each job replaces its chart state and QML composition.
    if (!compositeRenderer_) compositeRenderer_ = std::make_unique<PreviewQuickExportSession>();
    compositeRenderer_->setFrameSize(QSize(session_.outputWidth(), session_.outputHeight()));
    if (!compositeRenderer_->initialize(QSurfaceFormat(), nullptr, &error_)) {
        canceled_ = true; finish(); return false;
    }
    for (const auto& job : jobs_) {
        CoverLayoutModel layout;
        layout.fromJson(job.composition.value(QStringLiteral("layout")).toObject());
        if (layout.visibleChartFrameLayers().isEmpty() || job.task.noteMarkers.isEmpty()) continue;
        if (!frameRenderer_) frameRenderer_ = std::make_unique<SceneFrameRenderer>();
        const int side = std::clamp(std::max(session_.outputWidth(), session_.outputHeight()), 512, 4096);
        if (!frameRenderer_->bootstrap(job.task, &error_)
            || !frameRenderer_->prepareCaptureWindow(side, 0.0, &error_)) {
            canceled_ = true; finish(); return false;
        }
        break;
    }
    if (canceled_) { finish(); return true; }
    QTimer::singleShot(0, this, &CoverBatchExport::advance);
    qCDebug(coverBatchLog) << "queue prepared" << jobs_.size();
    return true;
}
void CoverBatchExport::cancel()
{
    if (!running_ || canceled_) return;
    canceled_ = true;
    if (cancelPublication_) cancelPublication_();
    emit changed();
    // The current renderer owns its stack until it returns. Its next checkpoint
    // handles cancellation; publication callbacks settle the publishing stage.
}
void CoverBatchExport::advance()
{
    qCDebug(coverBatchLog) << "advance" << completed_ << qGuiApp->applicationState();
    if (!running_) return;
    if (canceled_ || completed_ >= jobs_.size()) { finish(); return; }
    auto& job = jobs_[completed_];
    job.status = QStringLiteral("rendering"); emit changed();
    CoverCompositionState state;
    QString error;
    if (job.composition.isEmpty() || !CoverCompositionState::fromJson(job.composition, &state, &error)
        || state.size.width() <= 0 || state.size.height() <= 0) {
        completeCurrent(false, {}, error.isEmpty() ? qtTrId("cover.batch_invalid_preset") : error); return;
    }
    CoverLayoutModel layout;
    layout.fromJson(state.layout);
    const auto missing = [&](const QString& path) { return !path.isEmpty() && !QFileInfo(path).isFile(); };
    if ((job.inputs.backgroundMode == CoverBackgroundMode::Custom
         && (job.inputs.backgroundPath.isEmpty() || missing(job.inputs.backgroundPath)))
        || missing(state.card.value("fontDisplay").toString()) || missing(state.card.value("fontBody").toString())) {
        completeCurrent(false, {}, qtTrId("cover.batch_missing_asset")); return;
    }
    for (const auto* layer : layout.layers()) {
        if (layer->visible() && ((layer->kind() == QStringLiteral("image")
             && (layer->imagePath().isEmpty() || missing(layer->imagePath()))) || missing(layer->fontPath()))) {
            completeCurrent(false, {}, qtTrId("cover.batch_missing_asset")); return;
        }
    }
    const auto frames = layout.visibleChartFrameLayers();
    if (!frames.isEmpty()) {
        if (job.task.noteMarkers.isEmpty() || !frameRenderer_ || !frameRenderer_->bootstrap(job.task, &error)) {
            completeCurrent(false, {}, error.isEmpty() ? qtTrId("cover.batch_empty_chart") : error); return;
        }
        job.inputs.chartFrameDiskDiameter = frameRenderer_->playfieldDiskDiameterFraction();
        const int side = std::clamp(std::max(state.size.width(), state.size.height()), 512, 4096);
        for (auto* layer : frames) {
            if (canceled_) { finish(); return; }
            const double second = std::clamp(layer->frameSeconds(), 0.0, frameRenderer_->contentDurationSeconds());
            auto image = frameRenderer_->renderAt(second, side, &error);
            qCDebug(coverBatchLog) << "frame rendered" << second << !image.isNull();
            if (canceled_) { finish(); return; }
            if (image.isNull()) { completeCurrent(false, {}, error); return; }
            layout.setLayerImage(layer->key(), image);
        }
    }
    if (canceled_) { finish(); return; }
    const auto result = exportCoverComposite(&layout, job.inputs, state.size, state.outputDirectory,
        job.baseName, compositeRenderer_.get());
    qCDebug(coverBatchLog) << "composite returned" << result.success;
    if (canceled_) { finish(); return; }
    if (!result.success) { completeCurrent(false, result.outputPath, result.errorMessage); return; }
    if (!publisher_) { completeCurrent(true, result.outputPath, {}); return; }
    job.status = QStringLiteral("publishing"); emit changed();
    const quint64 token = generation_;
    const QPointer<CoverBatchExport> guard(this);
    publisher_(result.outputPath, [guard, token](bool ok, QString path, QString error) {
        if (!guard || !guard->running_ || guard->generation_ != token) return;
        guard->completeCurrent(ok, path, error);
    });
}
void CoverBatchExport::completeCurrent(bool success, const QString& path, const QString& error)
{
    qCDebug(coverBatchLog) << "publication settled" << completed_ << success << canceled_;
    auto& job = jobs_[completed_];
    job.status = canceled_ ? QStringLiteral("canceled") : success ? QStringLiteral("success") : QStringLiteral("failed");
    // A path becomes user-visible only after publication succeeds. Failed or
    // canceled jobs keep their private render cache available for diagnostics.
    if (success && !path.isEmpty()) job.path = path;
    job.error = error; ++completed_;
    if (progress_) progress_(completed_ * 100 / total());
    emit changed();
    QTimer::singleShot(0, this, &CoverBatchExport::advance);
}
void CoverBatchExport::finish()
{
    for (int i = completed_; i < jobs_.size(); ++i) jobs_[i].status = QStringLiteral("canceled");
    running_ = false;
    session_.setBatchBusy(false);
    if (end_) end_();
    emit changed(); emit finished();
}
}
