#include "app/services/ChartWorkspaceFileService.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QTemporaryDir>
#include "core/chart/ChartAssetPaths.h"
#include <QStringConverter>
#include <memory>

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
            QFile::remove(it->target);
            if (!it->backup.isEmpty() && !QFile::rename(it->backup, it->target))
                staging_.setAutoRemove(false);
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
            file.staged = staging_.filePath(QString::number(files_.size()));
            if (!QFile::copy(source, file.staged)) return false;
        }
        files_.append(file);
        return true;
    }

    bool publish()
    {
        for (auto& file : files_) {
            if (!QDir().mkpath(QFileInfo(file.target).absolutePath())) return false;
            if (QFileInfo::exists(file.target)) {
                if (!QFileInfo(file.target).isFile()) return false;
                file.backup = staging_.filePath(QStringLiteral("backup-%1").arg(&file - files_.data()));
                if (!QFile::rename(file.target, file.backup)) return false;
            }
            file.published = true;
            if (!file.staged.isEmpty() && !QFile::rename(file.staged, file.target)) return false;
        }
        return true;
    }

    void commit() { committed_ = true; }

private:
    struct Entry {
        QString source;
        QString target;
        QString staged;
        QString backup;
        bool published = false;
    };
    QDir target_;
    QTemporaryDir staging_;
    QVector<Entry> files_;
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
    const QDir settings(source.filePath(QStringLiteral(".miacode")));
    for (const auto& file : settings.entryInfoList(QDir::Files | QDir::Hidden)) {
        if (!transaction.add(file.absoluteFilePath(), QStringLiteral(".miacode/") + file.fileName()))
            return false;
    }
    const QDir targetSettings(target.filePath(QStringLiteral(".miacode")));
    for (const auto& file : targetSettings.entryInfoList(QDir::Files | QDir::Hidden)) {
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
    QFile file(normalizedPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {false, workspace_->snapshot().revision, QStringLiteral("open_failed"), {}};
    }
    bool usedSystemEncoding = false;
    const QString text = decodeDocumentText(file.readAll(), &usedSystemEncoding);
    const ChartWorkspaceResult result = workspace_->openSource(text, normalizedPath, 0, origin);
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

QString ChartWorkspaceFileService::decodeDocumentText(
    const QByteArray& bytes, bool* usedSystemEncoding)
{
    if (usedSystemEncoding != nullptr) *usedSystemEncoding = false;
    if (bytes.startsWith("\xEF\xBB\xBF")) return QString::fromUtf8(bytes.mid(3));

    QStringDecoder utf8Decoder(QStringConverter::Utf8);
    const QString text = utf8Decoder.decode(bytes);
    if (!utf8Decoder.hasError()) return text;

    if (usedSystemEncoding != nullptr) *usedSystemEncoding = true;
    QStringDecoder systemDecoder(QStringConverter::System);
    return systemDecoder.decode(bytes);
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
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return {false, before.revision, QStringLiteral("open_failed"), {}};
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
        file.cancelWriting();
        return {false, before.revision, QStringLiteral("write_failed"), {}};
    }
    if (!file.commit()) return {false, before.revision, QStringLiteral("commit_failed"), {}};

    if (transaction) transaction->commit();
    workspace_->markSectionSaved(savedDocument, normalizedPath, currentVideoPath);
    return {true, workspace_->snapshot().revision, QString(), {}};
}

}  // namespace miacode
