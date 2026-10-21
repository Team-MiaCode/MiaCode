#include "batch_cleaner/BatchCleanerController.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QSettings>
#include <QVariantMap>
#include <QThread>

#include "core/chart/document/SimaiDocument.h"

namespace miacode::batch_cleaner {
namespace {

using Syntax = miacode::chart_transform::ChartNormalizationSyntax;

const int kSectionChoices[] = {4, 2, 0};

QString statusText(const QString& status)
{
    static const QHash<QString, QString> texts = {
        {QStringLiteral("pending"), QStringLiteral("待预检")},
        {QStringLiteral("changed"), QStringLiteral("将修改")},
        {QStringLiteral("unchanged"), QStringLiteral("无变化")},
        {QStringLiteral("warning"), QStringLiteral("有警告")},
        {QStringLiteral("failed"), QStringLiteral("失败")},
        {QStringLiteral("written"), QStringLiteral("已写入")},
    };
    return texts.value(status, status);
}

QString chartKey(const QString& path)
{
    return QDir::cleanPath(path).toLower();
}

QString metadataText(const QString& text)
{
    QVector<SimaiRawField> fields = SimaiDocument::parseRawFields(text);
    fields.erase(std::remove_if(fields.begin(), fields.end(),
                                [](const SimaiRawField& field) {
                                    return field.key.startsWith(QStringLiteral("inote_"));
                                }),
                 fields.end());
    return SimaiDocument::serializeRawFields(fields);
}

QString difficultyChart(const QString& text, int id)
{
    const SimaiDocument document = SimaiDocument::fromText(text);
    const SimaiDifficultyData* difficulty = document.difficulty(id);
    return difficulty != nullptr ? difficulty->chart : QString();
}

QVariantList diffRows(const QStringList& lines, const QVector<bool>& marked)
{
    QVariantList rows;
    rows.reserve(lines.size());
    for (qsizetype i = 0; i < lines.size(); ++i) {
        rows.append(QVariantMap{{QStringLiteral("text"), lines.at(i)},
                                {QStringLiteral("marked"), marked.value(i)}});
    }
    return rows;
}

QString describeReport(const JobResult& result)
{
    QStringList parts;
    if (result.report.removedComments > 0) {
        parts.append(QStringLiteral("−%1 注释").arg(result.report.removedComments));
    }
    if (result.report.removedTimeSignatures > 0) {
        parts.append(QStringLiteral("−%1 拍号").arg(result.report.removedTimeSignatures));
    }
    int normalized = 0;
    for (const DifficultyReport& difficulty : result.report.difficulties) {
        normalized += difficulty.normalized ? 1 : 0;
    }
    if (normalized > 0) {
        parts.append(QStringLiteral("整理 %1 个难度").arg(normalized));
    }
    if (result.report.designerChanges > 0) {
        parts.append(QStringLiteral("谱师 %1 处").arg(result.report.designerChanges));
    }
    if (parts.isEmpty()) {
        parts.append(QStringLiteral("无变化"));
    }
    return parts.join(QStringLiteral(" · "));
}

struct PreviewData {
    QStringList sections;
    int section = 0;
    QVariantList original;
    QVariantList cleaned;
};

// Runs off the UI thread so a slow chart never blocks the window.
PreviewData computePreview(const ChartJob& job, const CleanupOptions& options, const QString& wantedSection,
                           int wantedIndex)
{
    PreviewData data;
    const JobResult result = processJob(job, options, false);
    if (result.status == JobResult::Status::Failed) {
        return data;
    }

    QVector<int> ids = SimaiDocument::fromText(result.originalText).difficultyIds();
    for (int id : SimaiDocument::fromText(result.cleanedText).difficultyIds()) {
        if (!ids.contains(id)) {
            ids.append(id);
        }
    }
    std::sort(ids.begin(), ids.end());
    data.sections.append(QStringLiteral("信息"));
    for (int id : ids) {
        data.sections.append(SimaiDocument::difficultyName(id));
    }
    const int restored = data.sections.indexOf(wantedSection);
    data.section = restored >= 0 ? restored : qBound(0, wantedIndex, int(data.sections.size() - 1));

    QString left;
    QString right;
    if (data.section == 0) {
        left = metadataText(result.originalText);
        right = metadataText(result.cleanedText);
    } else {
        const int id = ids.at(data.section - 1);
        left = difficultyChart(result.originalText, id);
        right = difficultyChart(result.cleanedText, id);
    }
    const QStringList leftLines = left.split(QLatin1Char('\n'));
    const QStringList rightLines = right.split(QLatin1Char('\n'));
    const LineDiff diff = diffLines(leftLines, rightLines);
    data.original = diffRows(leftLines, diff.removed);
    data.cleaned = diffRows(rightLines, diff.added);
    return data;
}

}  // namespace

int BatchJobListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : entries_.size();
}

QVariant BatchJobListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= entries_.size()) {
        return {};
    }
    const Entry& entry = entries_.at(index.row());
    switch (role) {
    case TitleRole:
        return entry.title.isEmpty() ? QFileInfo(entry.job.sourceDir).fileName() : entry.title;
    case PathRole:
        return QDir::toNativeSeparators(entry.job.sourceDir);
    case CheckedRole:
        return entry.checked;
    case StatusRole:
        return entry.status;
    case StatusTextRole:
        return statusText(entry.status);
    case DifficultyCountRole:
        return entry.difficultyCount;
    case CommentCountRole:
        return entry.commentCount;
    case DetailRole:
        return entry.detail;
    default:
        return {};
    }
}

QHash<int, QByteArray> BatchJobListModel::roleNames() const
{
    return {
        {TitleRole, "title"},
        {PathRole, "path"},
        {CheckedRole, "checked"},
        {StatusRole, "status"},
        {StatusTextRole, "statusText"},
        {DifficultyCountRole, "difficultyCount"},
        {CommentCountRole, "commentCount"},
        {DetailRole, "detail"},
    };
}

void BatchJobListModel::setEntries(QVector<Entry> entries)
{
    beginResetModel();
    entries_ = std::move(entries);
    endResetModel();
}

void BatchJobListModel::removeRows(const QList<int>& rows)
{
    QList<int> ordered = rows;
    std::sort(ordered.begin(), ordered.end(), std::greater<int>());
    for (int row : ordered) {
        if (row < 0 || row >= entries_.size()) {
            continue;
        }
        beginRemoveRows(QModelIndex(), row, row);
        entries_.removeAt(row);
        endRemoveRows();
    }
}

void BatchJobListModel::touch(int row)
{
    if (row >= 0 && row < entries_.size()) {
        emit dataChanged(index(row), index(row));
    }
}

void BatchJobListModel::touchAll()
{
    if (!entries_.isEmpty()) {
        emit dataChanged(index(0), index(entries_.size() - 1));
    }
}

BatchCleanerController::BatchCleanerController(QObject* parent)
    : QObject(parent)
{
    QSettings settings;
    options_.normalize = settings.value(QStringLiteral("normalize"), true).toBool();
    options_.normalization.reduceTo384Grid = settings.value(QStringLiteral("alignTo384"), false).toBool();
    options_.normalization.sectionMeasureCount = settings.value(QStringLiteral("sectionMeasureCount"), 4).toInt();
    options_.normalization.splitEveryFourMeasures = options_.normalization.sectionMeasureCount == 4;
    options_.normalization.syntax = settings.value(QStringLiteral("syntax")).toString()
            == QStringLiteral("compact_single_line")
        ? Syntax::CompactSingleLine
        : Syntax::SegmentPreserving;
    options_.stripComments = settings.value(QStringLiteral("stripComments"), true).toBool();
    options_.stripTimeSignatures = settings.value(QStringLiteral("stripTimeSignatures"), false).toBool();
    options_.unifyDesigner = settings.value(QStringLiteral("unifyDesigner"), true).toBool();
    options_.designer = settings.value(QStringLiteral("designer")).toString();
    connect(&model_, &QAbstractItemModel::dataChanged, this, &BatchCleanerController::jobsChanged);
    connect(&model_, &QAbstractItemModel::modelReset, this, &BatchCleanerController::jobsChanged);
    connect(&model_, &QAbstractItemModel::rowsRemoved, this, &BatchCleanerController::jobsChanged);
}

BatchCleanerController::~BatchCleanerController()
{
    if (cancelFlag_) {
        cancelFlag_->store(true);
    }
    if (worker_ != nullptr) {
        worker_->wait();
        delete worker_;
    }
    for (QThread* thread : std::as_const(previewThreads_)) {
        thread->wait();
        delete thread;
    }
}

int BatchCleanerController::checkedCount() const
{
    int count = 0;
    for (const auto& entry : model_.entries()) {
        count += entry.checked ? 1 : 0;
    }
    return count;
}

void BatchCleanerController::setOutputRoot(const QString& path)
{
    if (busy_) {
        return;
    }
    // An emptied field returns to the default next to the first input.
    const bool edited = !path.trimmed().isEmpty();
    const QString cleaned = edited ? QDir::cleanPath(QDir::fromNativeSeparators(path.trimmed()))
                                   : (inputs_.isEmpty() ? QString() : defaultOutputRoot(inputs_));
    outputRootEdited_ = edited;
    if (cleaned == outputRoot_) {
        emit outputRootChanged();
        return;
    }
    outputRoot_ = cleaned;
    emit outputRootChanged();
    rebuildJobs();
}

void BatchCleanerController::setNormalize(bool value)
{
    if (options_.normalize != value) {
        options_.normalize = value;
        optionsEdited();
    }
}

void BatchCleanerController::setReduceTo384Grid(bool value)
{
    if (options_.normalization.reduceTo384Grid != value) {
        options_.normalization.reduceTo384Grid = value;
        optionsEdited();
    }
}

int BatchCleanerController::sectionIndex() const
{
    for (int i = 0; i < 3; ++i) {
        if (kSectionChoices[i] == options_.normalization.sectionMeasureCount) {
            return i;
        }
    }
    return 0;
}

void BatchCleanerController::setSectionIndex(int index)
{
    const int count = kSectionChoices[qBound(0, index, 2)];
    if (options_.normalization.sectionMeasureCount != count) {
        options_.normalization.sectionMeasureCount = count;
        options_.normalization.splitEveryFourMeasures = count == 4;
        optionsEdited();
    }
}

int BatchCleanerController::syntaxIndex() const
{
    return options_.normalization.syntax == Syntax::CompactSingleLine ? 1 : 0;
}

void BatchCleanerController::setSyntaxIndex(int index)
{
    const Syntax syntax = index == 1 ? Syntax::CompactSingleLine : Syntax::SegmentPreserving;
    if (options_.normalization.syntax != syntax) {
        options_.normalization.syntax = syntax;
        optionsEdited();
    }
}

void BatchCleanerController::setStripComments(bool value)
{
    if (options_.stripComments != value) {
        options_.stripComments = value;
        optionsEdited();
    }
}

void BatchCleanerController::setStripTimeSignatures(bool value)
{
    if (options_.stripTimeSignatures != value) {
        options_.stripTimeSignatures = value;
        optionsEdited();
    }
}

void BatchCleanerController::setUnifyDesigner(bool value)
{
    if (options_.unifyDesigner != value) {
        options_.unifyDesigner = value;
        optionsEdited();
    }
}

void BatchCleanerController::setDesigner(const QString& value)
{
    if (options_.designer != value) {
        options_.designer = value;
        optionsEdited();
    }
}

void BatchCleanerController::optionsEdited()
{
    saveSettings();
    emit optionsChanged();
    invalidateResults();
    refreshPreview();
}

void BatchCleanerController::invalidateResults()
{
    for (int row = 0; row < model_.rowCount(); ++row) {
        auto& entry = model_.entryAt(row);
        entry.status = QStringLiteral("pending");
        entry.detail.clear();
    }
    model_.touchAll();
}

void BatchCleanerController::saveSettings() const
{
    QSettings settings;
    settings.setValue(QStringLiteral("normalize"), options_.normalize);
    settings.setValue(QStringLiteral("alignTo384"), options_.normalization.reduceTo384Grid);
    settings.setValue(QStringLiteral("sectionMeasureCount"), options_.normalization.sectionMeasureCount);
    settings.setValue(QStringLiteral("syntax"),
                      options_.normalization.syntax == Syntax::CompactSingleLine
                          ? QStringLiteral("compact_single_line")
                          : QStringLiteral("segment_preserving"));
    settings.setValue(QStringLiteral("stripComments"), options_.stripComments);
    settings.setValue(QStringLiteral("stripTimeSignatures"), options_.stripTimeSignatures);
    settings.setValue(QStringLiteral("unifyDesigner"), options_.unifyDesigner);
    settings.setValue(QStringLiteral("designer"), options_.designer);
}

void BatchCleanerController::addUrls(const QList<QUrl>& urls)
{
    QStringList paths;
    for (const QUrl& url : urls) {
        if (url.isLocalFile()) {
            paths.append(url.toLocalFile());
        }
    }
    addPaths(paths);
}

void BatchCleanerController::addPaths(const QStringList& paths)
{
    if (busy_) {
        return;
    }
    bool added = false;
    for (const QString& path : paths) {
        const QString cleaned = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
        if (path.trimmed().isEmpty() || inputs_.contains(cleaned)) {
            continue;
        }
        inputs_.append(cleaned);
        added = true;
    }
    if (!added) {
        return;
    }
    removedCharts_.clear();
    if (!outputRootEdited_) {
        outputRoot_ = defaultOutputRoot(inputs_);
        emit outputRootChanged();
    }
    rebuildJobs();
}

void BatchCleanerController::rebuildJobs()
{
    if (busy_) {
        return;
    }
    QHash<QString, BatchJobListModel::Entry> previous;
    for (const auto& entry : model_.entries()) {
        previous.insert(chartKey(entry.job.chartPath), entry);
    }
    const QString selectedChart = selectedRow_ >= 0 && selectedRow_ < model_.rowCount()
        ? chartKey(model_.entries().at(selectedRow_).job.chartPath)
        : QString();

    const JobCollection collection = collectJobs(inputs_, outputRoot_);
    collectErrors_ = collection.errors;
    QVector<BatchJobListModel::Entry> entries;
    int nextSelected = -1;
    for (const ChartJob& job : collection.jobs) {
        const QString key = chartKey(job.chartPath);
        if (removedCharts_.contains(key)) {
            continue;
        }
        BatchJobListModel::Entry entry = previous.value(key);
        entry.job = job;
        if (!previous.contains(key)) {
            entry.checked = true;
        } else if (entry.status == QStringLiteral("written")) {
            entry.status = QStringLiteral("pending");
        }
        if (key == selectedChart) {
            nextSelected = entries.size();
        }
        entries.append(entry);
    }
    model_.setEntries(std::move(entries));
    messages_ = collectErrors_;
    summary_.clear();
    emit summaryChanged();
    selectedRow_ = nextSelected >= 0 ? nextSelected : (model_.rowCount() > 0 ? 0 : -1);
    refreshPreview();
}

void BatchCleanerController::setChecked(int row, bool checked)
{
    if (busy_ || row < 0 || row >= model_.rowCount() || model_.entryAt(row).checked == checked) {
        return;
    }
    model_.entryAt(row).checked = checked;
    model_.touch(row);
}

void BatchCleanerController::setAllChecked(bool checked)
{
    if (busy_) {
        return;
    }
    for (int row = 0; row < model_.rowCount(); ++row) {
        model_.entryAt(row).checked = checked;
    }
    model_.touchAll();
}

void BatchCleanerController::removeRow(int row)
{
    if (busy_ || row < 0 || row >= model_.rowCount()) {
        return;
    }
    removedCharts_.insert(chartKey(model_.entries().at(row).job.chartPath));
    model_.removeRows({row});
    if (selectedRow_ >= model_.rowCount()) {
        selectedRow_ = model_.rowCount() - 1;
    }
    refreshPreview();
}

void BatchCleanerController::removeChecked()
{
    if (busy_) {
        return;
    }
    QList<int> rows;
    for (int row = 0; row < model_.rowCount(); ++row) {
        if (model_.entries().at(row).checked) {
            rows.append(row);
            removedCharts_.insert(chartKey(model_.entries().at(row).job.chartPath));
        }
    }
    model_.removeRows(rows);
    selectedRow_ = model_.rowCount() > 0 ? 0 : -1;
    refreshPreview();
}

void BatchCleanerController::clear()
{
    if (busy_) {
        return;
    }
    inputs_.clear();
    removedCharts_.clear();
    collectErrors_.clear();
    model_.setEntries({});
    if (!outputRootEdited_) {
        outputRoot_.clear();
        emit outputRootChanged();
    }
    messages_.clear();
    summary_.clear();
    emit summaryChanged();
    selectedRow_ = -1;
    refreshPreview();
}

void BatchCleanerController::preflight()
{
    start(false);
}

void BatchCleanerController::run()
{
    start(true);
}

void BatchCleanerController::start(bool write)
{
    if (busy_) {
        return;
    }
    QVector<QPair<int, ChartJob>> work;
    for (int row = 0; row < model_.rowCount(); ++row) {
        const auto& entry = model_.entries().at(row);
        if (entry.checked) {
            work.append({row, entry.job});
        }
    }
    if (work.isEmpty()) {
        summary_ = QStringLiteral("没有勾选的谱面");
        emit summaryChanged();
        return;
    }
    if (write && options_.unifyDesigner && options_.designer.trimmed().isEmpty()) {
        summary_ = QStringLiteral("请填写谱师名义，或关闭「统一谱师」");
        emit summaryChanged();
        return;
    }

    const quint64 generation = ++generation_;
    cancelFlag_ = std::make_shared<std::atomic_bool>(false);
    busy_ = true;
    writing_ = write;
    progress_ = 0;
    progressTotal_ = work.size();
    messages_ = collectErrors_;
    summary_ = write ? QStringLiteral("正在写入…") : QStringLiteral("正在预检…");
    emit busyChanged();
    emit progressChanged();
    emit summaryChanged();

    const CleanupOptions options = options_;
    const auto cancelFlag = cancelFlag_;
    if (worker_ != nullptr) {
        worker_->wait();
        delete worker_;
    }
    worker_ = QThread::create([this, work, options, write, generation, cancelFlag]() {
        bool cancelled = false;
        for (const auto& [row, job] : work) {
            if (cancelFlag->load()) {
                cancelled = true;
                break;
            }
            const JobResult result = processJob(job, options, write);
            QMetaObject::invokeMethod(this, [this, generation, row, result, write]() {
                applyResult(generation, row, result, write);
            }, Qt::QueuedConnection);
        }
        QMetaObject::invokeMethod(this, [this, generation, write, cancelled]() {
            finish(generation, write, cancelled);
        }, Qt::QueuedConnection);
    });
    worker_->start();
}

void BatchCleanerController::applyResult(quint64 generation, int row, const JobResult& result, bool write)
{
    if (generation != generation_ || row < 0 || row >= model_.rowCount()) {
        return;
    }
    auto& entry = model_.entryAt(row);
    if (!result.title.isEmpty()) {
        entry.title = result.title;
    }
    if (result.originalText.size() > 0 || result.status != JobResult::Status::Failed) {
        entry.difficultyCount = result.difficultyCount;
        entry.commentCount = result.sourceCommentCount;
    }
    const QString name = entry.title.isEmpty() ? QFileInfo(entry.job.sourceDir).fileName() : entry.title;
    if (result.status == JobResult::Status::Failed) {
        entry.status = QStringLiteral("failed");
        entry.detail = result.error;
        messages_.append(QStringLiteral("%1：%2").arg(name, result.error));
    } else {
        if (write) {
            entry.status = QStringLiteral("written");
        } else if (!result.report.warnings.isEmpty()) {
            entry.status = QStringLiteral("warning");
        } else {
            entry.status = result.status == JobResult::Status::Changed ? QStringLiteral("changed")
                                                                       : QStringLiteral("unchanged");
        }
        entry.detail = describeReport(result);
        for (const QString& warning : result.report.warnings) {
            messages_.append(QStringLiteral("%1：%2").arg(name, warning));
        }
    }
    model_.touch(row);
    ++progress_;
    emit progressChanged();
}

void BatchCleanerController::finish(quint64 generation, bool write, bool cancelled)
{
    if (generation != generation_) {
        return;
    }
    busy_ = false;
    writing_ = false;
    cancelFlag_.reset();
    int changed = 0;
    int failed = 0;
    int warned = 0;
    int written = 0;
    for (const auto& entry : model_.entries()) {
        if (!entry.checked) {
            continue;
        }
        changed += entry.status == QStringLiteral("changed") ? 1 : 0;
        failed += entry.status == QStringLiteral("failed") ? 1 : 0;
        warned += entry.status == QStringLiteral("warning") ? 1 : 0;
        written += entry.status == QStringLiteral("written") ? 1 : 0;
    }
    if (write) {
        summary_ = QStringLiteral("已写入 %1 首，失败 %2 首").arg(written).arg(failed);
    } else {
        summary_ = QStringLiteral("预检完成：将修改 %1，有警告 %2，失败 %3").arg(changed).arg(warned).arg(failed);
    }
    if (cancelled) {
        summary_ = QStringLiteral("已取消 · ") + summary_;
    }
    emit busyChanged();
    emit summaryChanged();
    refreshPreview();
}

void BatchCleanerController::cancel()
{
    if (cancelFlag_) {
        cancelFlag_->store(true);
    }
}

void BatchCleanerController::openOutputRoot() const
{
    if (!outputRoot_.isEmpty() && QFileInfo(outputRoot_).isDir()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(outputRoot_));
    }
}

QString BatchCleanerController::pathFromUrl(const QUrl& url) const
{
    return url.isLocalFile() ? QDir::cleanPath(url.toLocalFile()) : url.toString();
}

QUrl BatchCleanerController::urlFromPath(const QString& path) const
{
    return path.isEmpty() ? QUrl() : QUrl::fromLocalFile(path);
}

void BatchCleanerController::setSelectedRow(int row)
{
    const int bounded = row >= 0 && row < model_.rowCount() ? row : -1;
    if (bounded == selectedRow_) {
        return;
    }
    selectedRow_ = bounded;
    previewSection_ = 0;
    refreshPreview();
}

void BatchCleanerController::setPreviewSection(int section)
{
    if (section == previewSection_ || section < 0 || section >= previewSections_.size()) {
        return;
    }
    previewSection_ = section;
    refreshPreview();
}

void BatchCleanerController::refreshPreview()
{
    const quint64 generation = ++previewGeneration_;
    if (selectedRow_ < 0 || selectedRow_ >= model_.rowCount()) {
        previewSections_.clear();
        previewSection_ = 0;
        previewOriginal_.clear();
        previewCleaned_.clear();
        emit previewChanged();
        return;
    }
    const ChartJob job = model_.entries().at(selectedRow_).job;
    const CleanupOptions options = options_;
    const QString wantedSection = previewSections_.value(previewSection_);
    const int wantedIndex = previewSection_;
    QThread* thread = QThread::create([this, generation, job, options, wantedSection, wantedIndex]() {
        const PreviewData data = computePreview(job, options, wantedSection, wantedIndex);
        QMetaObject::invokeMethod(this, [this, generation, data]() {
            if (generation != previewGeneration_) {
                return;
            }
            previewSections_ = data.sections;
            previewSection_ = data.section;
            previewOriginal_ = data.original;
            previewCleaned_ = data.cleaned;
            emit previewChanged();
        }, Qt::QueuedConnection);
    });
    previewThreads_.append(thread);
    connect(thread, &QThread::finished, this, [this, thread]() {
        previewThreads_.removeOne(thread);
        thread->deleteLater();
    });
    thread->start();
}

}  // namespace miacode::batch_cleaner
