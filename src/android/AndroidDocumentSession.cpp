#include "AndroidDocumentSession.h"
#include "common/ChartAssetPaths.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <QSaveFile>
#include <QStringDecoder>
#include <QUuid>

namespace miacode::android {
Q_LOGGING_CATEGORY(mobileFileLog, "miacode.android.files", QtWarningMsg)
namespace {
constexpr qint64 maxChartBytes = 16 * 1024 * 1024;
}

AndroidDocumentSession::AndroidDocumentSession(QString storageRoot, QObject* parent)
    : QObject(parent), workspace_(this), storageRoot_(std::move(storageRoot))
{
    QDir().mkpath(storageRoot_);
    recoveryAvailable_ = QFileInfo::exists(storageRoot_ + "/session.json");
    recoveryTimer_.setSingleShot(true);
    recoveryTimer_.setInterval(350);
    connect(&recoveryTimer_, &QTimer::timeout, this, &AndroidDocumentSession::flushRecovery);
    connect(&workspace_, &ChartWorkspace::changed, this, [this] {
        recoveryTimer_.start();
        const auto generation = documentGeneration();
        if (generation != publishedGeneration_) {
            publishedGeneration_ = generation;
            emit documentReplaced();
        }
        emit chartTextChanged();
        emit documentStateChanged();
        emit metadataChanged();
        emit changed();
    });
    // Do not overwrite the previous process's recovery before the user loads it.
    if (!recoveryAvailable_) newProject();
}

QString AndroidDocumentSession::chartText() const
{
    const auto* difficulty = workspace_.document().difficulty(activeDifficulty());
    return difficulty ? difficulty->chart : QString();
}

#ifndef Q_OS_ANDROID
bool AndroidDocumentSession::loadHostFixture(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > maxChartBytes) return false;
    QStringDecoder decoder(QStringDecoder::Utf8);
    const QByteArray bytes = file.readAll();
    const QString source = decoder(bytes);
    if (decoder.hasError()) return false;
    const QDir inputRoot(QFileInfo(path).absolutePath());
    const QString fixtureRoot = storageRoot_ + "/host-fixtures/" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (!QDir().mkpath(fixtureRoot)) return false;
    QDirIterator files(inputRoot.absolutePath(), QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    qint64 copiedBytes = 0;
    while (files.hasNext()) {
        const auto input = files.next();
        copiedBytes += QFileInfo(input).size();
        if (copiedBytes > 1024LL * 1024 * 1024) return false;
        const auto destination = QDir(fixtureRoot).filePath(inputRoot.relativeFilePath(input));
        if (!QDir().mkpath(QFileInfo(destination).absolutePath()) || !QFile::copy(input, destination)) return false;
    }
    const auto result = workspace_.openSource(source, QDir(fixtureRoot).filePath("maidata.txt"));
    if (!result.accepted) return false;
    savedSource_ = workspace_.snapshot().sourceText;
    recoveryAvailable_ = false;
    return true;
}
#endif

QVariantList AndroidDocumentSession::difficulties() const
{
    QVariantList result;
    for (const int id : workspace_.document().difficultyIds()) {
        const auto* difficulty = workspace_.document().difficulty(id);
        const QString name = SimaiDocument::difficultyName(id);
        result.append(QVariantMap{{"id", id}, {"name", name},
            {"label", difficulty->level.trimmed().isEmpty()
                ? name : QStringLiteral("%1 %2").arg(name, difficulty->level)}, {"level", difficulty->level},
            {"designer", difficulty->designer}, {"dirty", workspace_.snapshot().dirtyDifficultyIds.contains(id)}});
    }
    return result;
}

QString AndroidDocumentSession::currentDifficultyLabel() const
{
    const QString name = SimaiDocument::difficultyName(activeDifficulty());
    const QString level = currentDifficultyLevel().trimmed();
    return level.isEmpty() ? name : QStringLiteral("%1 %2").arg(name, level);
}

QString AndroidDocumentSession::previewAssetPath(const QString& kind) const
{
    if (kind == "video" && videoDisabled_) return {};
    // Folder assets are a library, not a selection: card.jpg must not displace bg.jpg.
    for (auto it = assets_.crbegin(); it != assets_.crend(); ++it) {
        const auto asset = it->toMap();
        const QString path = asset.value("path").toString();
        if (asset.value("kind").toString() == kind && asset.value("selectedForPreview").toBool()
            && QFileInfo(path).isFile()) return path;
    }
    if (kind == "audio") return chart_assets::resolveTrackPath(currentFilePath());
    if (kind == "image") return chart_assets::resolveBackgroundMediaPath(currentFilePath(), false);
    if (kind == "video") {
        const QString path = chart_assets::resolveChartVideoPath(currentFilePath(), workspace_.document().videoPath);
        return chart_assets::isVideoBackgroundPath(path) ? path : QString();
    }
    return {};
}

void AndroidDocumentSession::newProject()
{
    if (busy()) return;
    sourceUri_.clear();
    assets_.clear();
    editorFont_.clear();
    videoDisabled_ = false;
    const QString workingPath = storageRoot_ + "/projects/"
        + QUuid::createUuid().toString(QUuid::WithoutBraces) + "/maidata.txt";
    workspace_.openSource("&title=\n&artist=\n&first=0\n&lv_5=\n&inote_5=(120){4}1,2,3,4,E\n", workingPath);
    emit mediaAssetsChanged();
    savedSource_ = workspace_.snapshot().sourceText;
    // A new chart still needs an external destination before it is saved.
    status_ = tr("新建工程；请选择保存位置。");
    recoveryAvailable_ = false;
    flushRecovery();
}

bool AndroidDocumentSession::beginIo(const QString& kind, const QString& uri, const QString& payload)
{
    if (busy()) return false;
    pendingKind_ = kind;
    status_ = tr("正在处理文件…");
    emit changed();
    emit ioRequested(kind, uri, payload);
    return true;
}

void AndroidDocumentSession::openProject() { beginIo("open"); }
void AndroidDocumentSession::openProjectFolder() { beginIo("openFolder"); }

void AndroidDocumentSession::save(bool saveAs)
{
    if (busy() || !workspace_.snapshot().hasDocument) return;
    // Ensure even a revoked URI / partial provider write leaves a local copy.
    if (!flushRecovery()) return;
    pendingSaveSource_ = workspace_.snapshot().sourceText;
    beginIo(saveAs || sourceUri_.isEmpty() ? "saveAs" : "save", sourceUri_,
            QString::fromLatin1(pendingSaveSource_.toUtf8().toBase64()));
}

void AndroidDocumentSession::setChartText(const QString& text)
{
    workspace_.replaceActiveDifficultyChart(text);
}
void AndroidDocumentSession::setTitle(const QString& text)
{
    workspace_.updateDocumentField(ChartWorkspaceDocumentField::Title, text);
}
void AndroidDocumentSession::selectDifficulty(int id) { workspace_.selectDifficulty(id); }
void AndroidDocumentSession::addDifficulty(int id) { workspace_.addDifficulty(id); }

void AndroidDocumentSession::importAsset(const QString& kind)
{
    if (kind != "audio" && kind != "image" && kind != "video" && kind != "font") return;
    beginIo("asset", {}, kind);
}

void AndroidDocumentSession::share()
{
    if (dirty() || sourceUri_.isEmpty()) {
        status_ = tr("请先保存工程，再分享已保存的谱面文件。");
        emit changed();
        return;
    }
    beginIo("share", sourceUri_);
}

void AndroidDocumentSession::runMediaProbe()
{
    beginIo("probe", {}, backgroundExportAllowed_ ? "background" : "foreground");
}

void AndroidDocumentSession::exportProbeResults() { beginIo("probeReport"); }

void AndroidDocumentSession::setBackgroundExportAllowed(bool allowed)
{
    if (backgroundExportAllowed_ == allowed) return;
    backgroundExportAllowed_ = allowed;
    flushRecovery();
}

void AndroidDocumentSession::completeIo(const QJsonObject& result)
{
    const QString kind = result.value("kind").toString();
    qCDebug(mobileFileLog) << "File result received:" << kind << result.value("ok").toBool();
    if (kind != pendingKind_ || pendingKind_.isEmpty()) return;
    pendingKind_.clear();
    if (!result.value("ok").toBool()) {
        status_ = result.value("cancelled").toBool() ? tr("已取消。")
            : tr("操作失败：%1；编辑内容仍保留在本机。").arg(result.value("error").toString());
        pendingSaveSource_.clear();
        emit changed();
        return;
    }
    if (kind == "open" || kind == "openFolder") {
        const QByteArray bytes = QByteArray::fromBase64(result.value("data").toString().toLatin1());
        QStringDecoder decoder(QStringDecoder::Utf8);
        const QString source = decoder(bytes);
        if (bytes.size() > maxChartBytes || decoder.hasError()) {
            status_ = tr("谱面必须是有效的 UTF-8 文本，且不超过 16 MiB。");
            emit changed();
            return;
        }
        QString workingPath = result.value("path").toString();
        if (workingPath.isEmpty()) workingPath = storageRoot_ + "/projects/"
            + QUuid::createUuid().toString(QUuid::WithoutBraces) + "/maidata.txt";
        qCDebug(mobileFileLog) << "Opening workspace source";
        const auto accepted = workspace_.openSource(source, workingPath);
        qCDebug(mobileFileLog) << "Workspace source returned:" << accepted.accepted;
        if (!accepted.accepted) {
            status_ = tr("谱面字段格式无效，当前工程保留。");
            emit changed();
            return;
        }
        sourceUri_ = result.value("uri").toString();
        savedSource_ = workspace_.snapshot().sourceText;
        assets_ = result.value("assets").toArray().toVariantList();
        videoDisabled_ = false;
        editorFont_.clear();
        loadFonts();
        qCDebug(mobileFileLog) << "Publishing imported media";
        emit mediaAssetsChanged();
        qCDebug(mobileFileLog) << "Imported media published";
        status_ = tr("已打开谱面；工程素材已载入，可继续导入音频、图片和视频。");
    } else if (kind == "save" || kind == "saveAs") {
        sourceUri_ = result.value("uri").toString();
        savedSource_ = pendingSaveSource_;
        // A result must not clear edits made after the asynchronous write began.
        workspace_.rebindSavePoint(savedSource_);
        status_ = dirty() ? tr("已保存提交时的内容；后续编辑尚未保存。") : tr("已保存。");
        pendingSaveSource_.clear();
    } else if (kind == "asset") {
        auto asset = result.value("asset").toObject();
        asset.insert("selectedForPreview", true);
        assets_.append(asset.toVariantMap());
        if (asset.value("kind").toString() == "video") videoDisabled_ = false;
        loadFonts();
        emit mediaAssetsChanged();
        emit metadataChanged();
        status_ = tr("素材已复制到本机，可离线使用。");
    } else if (kind == "probe") {
        const QJsonObject report = QJsonDocument::fromJson(
            result.value("report").toString().toUtf8()).object();
        status_ = tr("媒体探针完成：%1×%2，%3 fps，%4 帧，用时 %5 秒。MP4、WAV 和 PNG 已生成，可导出结果。").arg(
            report.value("width").toInt()).arg(report.value("height").toInt())
            .arg(report.value("fps").toInt()).arg(report.value("frames").toInt())
            .arg(report.value("elapsedMs").toDouble() / 1000.0, 0, 'f', 2);
    } else if (kind == "probeReport") {
        status_ = tr("已导出探针结果。");
    } else {
        status_ = tr("已打开系统分享面板。");
    }
    recoveryAvailable_ = false;
    flushRecovery();
}

bool AndroidDocumentSession::writeState()
{
    const auto snapshot = workspace_.snapshot();
    if (!snapshot.hasDocument) return false;
    const QJsonObject state{{"schema", 1}, {"sourceUri", sourceUri_},
        {"workingPath", snapshot.filePath},
        {"source", snapshot.sourceText}, {"savedSource", savedSource_},
        {"difficulty", snapshot.activeDifficultyId},
        {"assets", QJsonArray::fromVariantList(assets_)},
        {"backgroundExportAllowed", backgroundExportAllowed_}, {"videoDisabled", videoDisabled_}};
    if (!QDir().mkpath(QFileInfo(snapshot.filePath).absolutePath())) {
        status_ = tr("无法创建本机工程目录；编辑仍在内存中。");
        return false;
    }
    QSaveFile chart(snapshot.filePath);
    chart.setDirectWriteFallback(false);
    const QByteArray source = snapshot.sourceText.toUtf8();
    if (!chart.open(QIODevice::WriteOnly) || chart.write(source) != source.size() || !chart.commit()) {
        status_ = tr("无法更新本机谱面副本：%1。编辑仍在内存中。").arg(chart.errorString());
        return false;
    }
    QSaveFile file(storageRoot_ + "/session.json");
    file.setDirectWriteFallback(false);
    const QByteArray bytes = QJsonDocument(state).toJson(QJsonDocument::Compact);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        status_ = tr("无法写入恢复副本：%1。请释放空间后保存，编辑仍在内存中。").arg(file.errorString());
        return false;
    }
    return true;
}

bool AndroidDocumentSession::flushRecovery()
{
    recoveryTimer_.stop();
    const bool ok = writeState();
    emit changed();
    return ok;
}

bool AndroidDocumentSession::readState(QJsonObject* state)
{
    QFile file(storageRoot_ + "/session.json");
    // JSON escaping and two document versions need headroom beyond the import cap.
    if (!file.open(QIODevice::ReadOnly) || file.size() > 8 * maxChartBytes) return false;
    QJsonParseError error;
    const auto doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) return false;
    *state = doc.object();
    return state->value("schema").toInt() == 1
        && state->value("source").isString() && state->value("savedSource").isString();
}

bool AndroidDocumentSession::recover()
{
    if (busy()) return false;
    QJsonObject state;
    if (!readState(&state)) {
        status_ = tr("恢复副本无法读取；原副本保留，请勿清除应用数据。");
        emit changed();
        return false;
    }
    const QString workingPath = state.value("workingPath").toString(storageRoot_ + "/projects/recovered/maidata.txt");
    if (!workspace_.openSource(state.value("source").toString(), workingPath,
            state.value("difficulty").toInt()).accepted) {
        status_ = tr("恢复副本无法读取；原副本保留，请勿清除应用数据。");
        emit changed();
        return false;
    }
    sourceUri_ = state.value("sourceUri").toString();
    savedSource_ = state.value("savedSource").toString();
    workspace_.rebindSavePoint(savedSource_);
    assets_ = state.value("assets").toArray().toVariantList();
    videoDisabled_ = state.value("videoDisabled").toBool();
    backgroundExportAllowed_ = state.value("backgroundExportAllowed").toBool();
    recoveryAvailable_ = false;
    loadFonts();
    emit mediaAssetsChanged();
    emit metadataChanged();
    status_ = dirty() ? tr("已恢复未保存的编辑。") : tr("已恢复上次工程。");
    emit changed();
    return true;
}

void AndroidDocumentSession::loadFonts()
{
    for (const auto& entry : assets_) {
        const auto asset = entry.toMap();
        if (asset.value("kind").toString() != "font") continue;
        const QString path = asset.value("path").toString();
        const int id = fontIds_.contains(path) ? fontIds_.value(path)
            : QFontDatabase::addApplicationFont(path);
        if (id >= 0) fontIds_.insert(path, id);
        const auto families = QFontDatabase::applicationFontFamilies(id);
        if (!families.isEmpty()) editorFont_ = families.first();
    }
}
}
