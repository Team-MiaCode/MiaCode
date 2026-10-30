#include "app/services/ChartWorkspaceFileService.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QTemporaryDir>
#include "core/chart/ChartAssetPaths.h"
#include "common/AtomicFileCopy.h"
#include "common/DebugLog.h"
#include <QStringConverter>
#include <memory>
#include <vector>

namespace miacode {
namespace {

class ProjectSaveTransaction
{
public:
    explicit ProjectSaveTransaction(const QDir& target)
        : target_(target)
        , staging_(target.filePath(QStringLiteral(".miacode-saveas-XXXXXX")))
    {
    }

    ~ProjectSaveTransaction()
    {
        if (committed_) return;
        for (auto it = files_.crbegin(); it != files_.crend(); ++it) {
            if (!it->published) continue;
            const bool restored = it->backup.isEmpty()
                ? QFile::remove(it->target)
                : file_io::copyFileAtomically(it->backup, it->target);
            if (!restored) {
                staging_.setAutoRemove(false);
                debug_log::appendLine(debug_log::Channel::Runtime,
                    QStringLiteral("document/save_as_restore_failed"),
                    QStringLiteral("target=%1 backup=%2").arg(it->target, it->backup),
                    true, debug_log::Level::Error);
            }
        }
    }

    bool add(const QString& source, const QString& relativeName)
    {
        if (!staging_.isValid()) return false;
        const QString targetPath = target_.filePath(relativeName);
        for (const auto& file : files_) {
            if (file.target == targetPath) return file.source == source;
        }
        Entry file;
        file.source = source;
        file.target = targetPath;
        if (!source.isEmpty()) {
            if (!QDir().mkpath(QFileInfo(targetPath).absolutePath())) return false;
            QFile input(source);
            if (!input.open(QIODevice::ReadOnly)) return false;
            file.replacement = std::make_unique<QSaveFile>(targetPath);
            if (!file.replacement->open(QIODevice::WriteOnly)
                || !file_io::copyFileContents(input, *file.replacement)) return false;
        }
        files_.push_back(std::move(file));
        return true;
    }

    bool publish()
    {
        for (auto& file : files_) {
            if (!QDir().mkpath(QFileInfo(file.target).absolutePath())) return false;
            if (QFileInfo::exists(file.target)) {
                if (!QFileInfo(file.target).isFile()) return false;
                file.backup = staging_.filePath(QStringLiteral("backup-%1").arg(&file - files_.data()));
                if (!QFile::copy(file.target, file.backup)) return false;
            }
            if (file.replacement) {
                if (!file.replacement->commit()) return false;
            } else if (QFileInfo::exists(file.target) && !QFile::remove(file.target)) {
                return false;
            }
            file.published = true;
        }
        return true;
    }

    void commit() { committed_ = true; }

private:
    struct Entry {
        QString source;
        QString target;
        std::unique_ptr<QSaveFile> replacement;
        QString backup;
        bool published = false;
    };
    QDir target_;
    QTemporaryDir staging_;
    std::vector<Entry> files_;
    bool committed_ = false;
};

bool stageProjectFiles(ProjectSaveTransaction& transaction, const QDir& source,
                       const QDir& target)
{
    const QStringList candidates = chart_assets::trackCandidateFileNames()
        + chart_assets::backgroundMediaCandidateFileNames();
    for (const QString& name : candidates) {
        const QString path = source.filePath(name);
        if (QFileInfo::exists(path)) {
            if (!transaction.add(path, name)) return false;
        } else if (QFileInfo::exists(target.filePath(name))) {
            // Remove stale target candidates so resolution selects the saved project's media.
            if (!transaction.add({}, name)) return false;
        }
    }
    const QDir targetSettings(target.filePath(QStringLiteral(".miacode")));
    const auto targetSettingsFiles = targetSettings.entryInfoList(QDir::Files | QDir::Hidden);
    const QDir settings(source.filePath(QStringLiteral(".miacode")));
    for (const auto& file : settings.entryInfoList(QDir::Files | QDir::Hidden)) {
        if (!transaction.add(file.absoluteFilePath(), QStringLiteral(".miacode/") + file.fileName()))
            return false;
    }
    for (const auto& file : targetSettingsFiles) {
        if (!QFileInfo::exists(settings.filePath(file.fileName()))
            && !transaction.add({}, QStringLiteral(".miacode/") + file.fileName())) return false;
    }
    return true;
}

bool stageVideoFile(ProjectSaveTransaction& transaction, const QDir& source, QString& videoPath)
{
    const QString reference = videoPath.trimmed();
    if (reference.isEmpty()) return true;
    const QString path = QFileInfo(reference).isAbsolute()
        ? reference : source.absoluteFilePath(reference);
    const QFileInfo media(path);
    if (!media.isFile()) return true;
    QString relative = source.relativeFilePath(media.absoluteFilePath());
    if (relative == QLatin1String("..") || relative.startsWith(QLatin1String("../"))
        || QDir::isAbsolutePath(relative)) {
        relative = QStringLiteral("media/") + media.fileName();
    }
    if (!transaction.add(media.absoluteFilePath(), relative)) return false;
    videoPath = relative;
    return true;
}

} // namespace

ChartWorkspaceFileService::ChartWorkspaceFileService(ChartWorkspace& workspace)
    : workspace_(&workspace)
{
}

ChartWorkspaceFileResult ChartWorkspaceFileService::open(const QString& path, ChartDocumentOrigin origin) const
{
    if (workspace_ == nullptr) return {false, 0, QStringLiteral("workspace_unavailable"), {}};
    QString normalizedPath = path.isEmpty() ? QString() : QDir::cleanPath(path);
    if (normalizedPath.isEmpty()) {
        return {false, workspace_->snapshot().revision, QStringLiteral("path_empty"), {}};
    }
    const QFileInfo inputInfo(normalizedPath);
    if (inputInfo.isDir()) {
        normalizedPath = QDir(inputInfo.absoluteFilePath()).filePath(QStringLiteral("maidata.txt"));
    }
    // A native file dialog's name filter can be bypassed by pasting a path.
    // Reject unsupported types before reading or parsing their contents.
    if (!chart_assets::supportedChartFileExtensions().contains(QFileInfo(normalizedPath).suffix().toLower())) {
        return {false, workspace_->snapshot().revision, QStringLiteral("unsupported_file_type"), {}};
    }
    QFile file(normalizedPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {false, workspace_->snapshot().revision, QStringLiteral("open_failed"), {}};
    }
    bool usedSystemEncoding = false;
    const auto text = decodeDocumentText(file.readAll(), &usedSystemEncoding);
    if (!text || file.error() != QFileDevice::NoError) {
        return {false, workspace_->snapshot().revision, QStringLiteral("decode_failed"), {}};
    }
    const ChartWorkspaceResult result = workspace_->openSource(*text, normalizedPath, 0, origin);
    return {result.accepted, result.revision,
            result.accepted ? QString() : QStringLiteral("open_failed"), result.issues,
            usedSystemEncoding};
}

ChartWorkspaceFileResult ChartWorkspaceFileService::createEmptyDocument(const QString& path) const
{
    return createEmptyDocument(path, SimaiDocument::createEmpty());
}

ChartWorkspaceFileResult ChartWorkspaceFileService::createEmptyDocument(
    const QString& path, const SimaiDocument& document) const
{
    const QString normalizedPath = path.isEmpty() ? QString() : QDir::cleanPath(path);
    if (normalizedPath.isEmpty()) return {false, 0, QStringLiteral("path_empty"), {}};
    const QDir parent = QFileInfo(normalizedPath).absoluteDir();
    if (!parent.exists() && !QDir().mkpath(parent.absolutePath())) {
        return {false, 0, QStringLiteral("mkpath_failed"), {}};
    }
    const QByteArray payload =
        QStringEncoder(QStringConverter::Utf8).encode(document.toText());
    QSaveFile file(normalizedPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return {false, 0, QStringLiteral("open_failed"), {}};
    }
    if (file.write(payload) != payload.size()) {
        file.cancelWriting();
        return {false, 0, QStringLiteral("write_failed"), {}};
    }
    if (!file.commit()) return {false, 0, QStringLiteral("commit_failed"), {}};
    return {true, 0, QString(), {}};
}

ChartWorkspaceFileResult ChartWorkspaceFileService::save(int difficultyId) const
{
    if (workspace_ == nullptr) return {false, 0, QStringLiteral("workspace_unavailable"), {}};
    if (workspace_->isNetPreview()) return {false, workspace_->snapshot().revision, QStringLiteral("save_as_required"), {}};
    return writeToPath(workspace_->snapshot().filePath, difficultyId);
}

ChartWorkspaceFileResult ChartWorkspaceFileService::saveAs(const QString& path, int difficultyId) const
{
    return writeToPath(path, difficultyId);
}

std::optional<QString> ChartWorkspaceFileService::decodeDocumentText(
    const QByteArray& bytes, bool* usedSystemEncoding)
{
    if (usedSystemEncoding != nullptr) *usedSystemEncoding = false;
    QStringDecoder utf8Decoder(QStringConverter::Utf8, QStringConverter::Flag::Stateless);
    const QString text = utf8Decoder.decode(bytes);
    if (!utf8Decoder.hasError()) return text;
    if (bytes.startsWith("\xEF\xBB\xBF")) return std::nullopt;

    QStringDecoder systemDecoder(QStringConverter::System, QStringConverter::Flag::Stateless);
    const QString systemText = systemDecoder.decode(bytes);
    if (systemDecoder.hasError()) return std::nullopt;
    if (usedSystemEncoding != nullptr) *usedSystemEncoding = true;
    return systemText;
}

ChartWorkspaceFileResult ChartWorkspaceFileService::writeToPath(
    const QString& path, int difficultyId) const
{
    if (workspace_ == nullptr) return {false, 0, QStringLiteral("workspace_unavailable"), {}};
    const QString normalizedPath = path.isEmpty() ? QString() : QDir::cleanPath(path);
    const ChartWorkspaceSnapshot before = workspace_->snapshot();
    if (!before.hasDocument) return {false, before.revision, QStringLiteral("document_unavailable"), {}};
    if (difficultyId != 0 && difficultyId != ChartWorkspace::MetadataSection
        && workspace_->document().difficulty(difficultyId) == nullptr)
        return {false, before.revision, QStringLiteral("difficulty_unavailable"), {}};
    if (normalizedPath.isEmpty()) return {false, before.revision, QStringLiteral("path_empty"), {}};
    if (workspace_->isNetPreview()) {
        const auto targetIdentity = QFileInfo(normalizedPath).canonicalFilePath();
        const auto sourceIdentity = QFileInfo(before.filePath).canonicalFilePath();
        if (normalizedPath == before.filePath || (!targetIdentity.isEmpty() && targetIdentity == sourceIdentity))
            return {false, before.revision, QStringLiteral("save_as_required"), {}};
    }

    SimaiDocument savedDocument = workspace_->documentForSectionSave(difficultyId);
    QString currentVideoPath = workspace_->document().videoPath;
    const QDir sourceDirectory = QFileInfo(before.filePath).absoluteDir();
    const QDir targetDirectory = QFileInfo(normalizedPath).absoluteDir();
    const bool copyProject = !before.filePath.isEmpty()
        && sourceDirectory.canonicalPath() != targetDirectory.canonicalPath();
    if (!QDir().mkpath(targetDirectory.absolutePath()))
        return {false, before.revision, QStringLiteral("mkpath_failed"), {}};
    QSaveFile file(normalizedPath);
    // The failure keeps QSaveFile's own reason so the notice can say why.
    const auto fileFailure = [&](const QString& code) {
        ChartWorkspaceFileResult result{false, before.revision, code, {}};
        result.failedPath = normalizedPath;
        result.fileError = file.error();
        result.systemError = file.errorString();
        return result;
    };
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return fileFailure(QStringLiteral("open_failed"));
    }
    std::unique_ptr<ProjectSaveTransaction> transaction;
    if (copyProject) {
        transaction = std::make_unique<ProjectSaveTransaction>(targetDirectory);
        if (!stageProjectFiles(*transaction, sourceDirectory, targetDirectory)
            || !stageVideoFile(*transaction, sourceDirectory, savedDocument.videoPath)
            || !stageVideoFile(*transaction, sourceDirectory, currentVideoPath)
            || !transaction->publish()) {
            return {false, before.revision, QStringLiteral("resource_copy_failed"), {}};
        }
    }
    const QByteArray payload = QStringEncoder(QStringConverter::Utf8).encode(savedDocument.toText());
    if (file.write(payload) != payload.size()) {
        ChartWorkspaceFileResult result = fileFailure(QStringLiteral("write_failed"));
        file.cancelWriting();
        return result;
    }
    if (!file.commit()) return fileFailure(QStringLiteral("commit_failed"));

    if (transaction) transaction->commit();
    workspace_->markSectionSaved(savedDocument, normalizedPath, currentVideoPath);
    return {true, workspace_->snapshot().revision, QString(), {}};
}

}  // namespace miacode
