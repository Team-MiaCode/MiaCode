#include "app/services/ChartWorkspaceFileService.h"
#include "core/chart/ChartAssetPaths.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>

namespace {

bool expect(bool condition, const QString& message, QTextStream& out)
{
    if (!condition) out << "FAIL: " << message << Qt::endl;
    return condition;
}

bool writeBytes(const QString& path, const QByteArray& bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}

bool verifyOpenSaveAndSaveAs(QTextStream& out)
{
    QTemporaryDir directory;
    const QString openedPath = directory.filePath(QStringLiteral("opened.txt"));
    const QString savedPath = directory.filePath(QStringLiteral("saved.txt"));
    const QString maidataPath = directory.filePath(QStringLiteral("maidata.txt"));
    const QByteArray bomSource("\xEF\xBB\xBF&lv_5=12\n&inote_5=(120){4}1,\n");
    if (!expect(directory.isValid() && writeBytes(openedPath, bomSource)
                    && writeBytes(maidataPath, bomSource),
                QStringLiteral("temporary document is prepared"), out)) return false;

    miacode::ChartWorkspace workspace;
    miacode::ChartWorkspaceFileService files(workspace);
    const auto opened = files.open(openedPath);
    bool ok = expect(opened.accepted && !workspace.snapshot().dirty
                         && workspace.snapshot().filePath == openedPath,
                     QStringLiteral("open decodes a BOM document and commits one clean workspace"), out);

    workspace.replaceActiveDifficultyChart(QStringLiteral("(120){4}2,"));
    const auto saved = files.save(5);
    QFile savedOpened(openedPath);
    savedOpened.open(QIODevice::ReadOnly);
    ok &= expect(saved.accepted && !workspace.snapshot().dirty
                     && savedOpened.readAll().contains("(120){4}2,"),
                 QStringLiteral("save atomically writes the workspace and establishes its save point"), out);

    const auto savedAs = files.saveAs(savedPath, 0);
    ok &= expect(savedAs.accepted && workspace.snapshot().filePath == savedPath,
                 QStringLiteral("save as changes file identity only after a successful write"), out);

    const auto reopenedFromDirectory = files.open(directory.path());
    ok &= expect(reopenedFromDirectory.accepted
                     && workspace.snapshot().filePath == maidataPath,
                 QStringLiteral("opening a chart directory resolves its maidata.txt child"), out);

    // Saving one difficulty leaves the others on disk as they were. A single
    // question about "the document" cannot express that, which is why the
    // unsaved-changes flow walks the changed difficulties one at a time.
    const QString twoDifficulties =
        QStringLiteral("&lv_5=12\n&inote_5=(120){4}1,\n&lv_6=13\n&inote_6=(120){4}3,\n");
    const QString multiPath = directory.filePath(QStringLiteral("multi.txt"));
    ok &= expect(writeBytes(multiPath, twoDifficulties.toUtf8()),
                 QStringLiteral("a two-difficulty document is prepared"), out);
    ok &= expect(files.open(multiPath).accepted,
                 QStringLiteral("the two-difficulty document opens clean"), out);

    workspace.selectDifficulty(5);
    workspace.replaceActiveDifficultyChart(QStringLiteral("(120){4}5,"));
    workspace.selectDifficulty(6);
    workspace.replaceActiveDifficultyChart(QStringLiteral("(120){4}6,"));
    ok &= expect(workspace.snapshot().dirtyDifficultyIds.size() == 2,
                 QStringLiteral("both difficulties are reported changed"), out);

    ok &= expect(files.save(5).accepted, QStringLiteral("saving one difficulty succeeds"), out);
    QFile multiFile(multiPath);
    multiFile.open(QIODevice::ReadOnly);
    const QByteArray afterSectionSave = multiFile.readAll();
    ok &= expect(afterSectionSave.contains("(120){4}5,")
                     && afterSectionSave.contains("(120){4}3,")
                     && !afterSectionSave.contains("(120){4}6,"),
                 QStringLiteral("only the saved difficulty reaches the file"), out);
    ok &= expect(workspace.snapshot().dirty
                     && workspace.snapshot().dirtyDifficultyIds == QVector<int>{6},
                 QStringLiteral("the other difficulty stays unsaved and still says so"), out);

    multiFile.close();
    ok &= expect(files.save(0).accepted && !workspace.snapshot().dirty,
                 QStringLiteral("a whole-document save succeeds"), out);
    multiFile.close();
    multiFile.open(QIODevice::ReadOnly);
    const QByteArray afterWholeSave = multiFile.readAll();
    ok &= expect(afterWholeSave.contains("(120){4}5,")
                     && afterWholeSave.contains("(120){4}6,"),
                 QStringLiteral("a whole-document save writes every live difficulty"), out);
    return ok;
}

bool verifyOpenAcceptsEmptyInoteFile(QTextStream& out)
{
    QTemporaryDir directory;
    const QString path = directory.filePath(QStringLiteral("maidata.txt"));
    const QByteArray source =
        "&title=empty-slots\n&lv_2=\n&inote_2=\n&lv_5=12\n&inote_5=(120){4}1,\n";
    if (!expect(directory.isValid() && writeBytes(path, source),
                QStringLiteral("temporary empty-slot document is prepared"), out)) {
        return false;
    }

    miacode::ChartWorkspace workspace;
    miacode::ChartWorkspaceFileService files(workspace);
    const auto opened = files.open(path);
    return expect(opened.accepted && opened.error.isEmpty()
                      && !opened.issues.isEmpty()
                      && workspace.snapshot().hasDocument
                      && workspace.snapshot().activeDifficultyId == 5
                      && workspace.document().difficulty(2) != nullptr,
                  QStringLiteral("opening a file with empty inote slots loads the document"), out);
}

bool verifyFailedOpenRetainsWorkspace(QTextStream& out)
{
    miacode::ChartWorkspace workspace;
    workspace.openSource(QStringLiteral("&lv_5=12\n&inote_5=(120){4}1,\n"));
    miacode::ChartWorkspaceFileService files(workspace);
    const auto before = workspace.snapshot();
    const auto result = files.open(QStringLiteral("Z:/does-not-exist/maidata.txt"));
    const auto after = workspace.snapshot();
    return expect(!result.accepted && result.error == QLatin1String("open_failed")
                      && before.revision == after.revision && before.sourceText == after.sourceText,
                  QStringLiteral("failed open leaves the current workspace untouched"), out);
}

QByteArray readBytes(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

bool verifyProjectSaveAs(QTextStream& out, miacode::ChartDocumentOrigin origin)
{
    QTemporaryDir directory;
    const QDir source(directory.filePath(QStringLiteral("source")));
    const QDir target(directory.filePath(QStringLiteral("target")));
    const QString original = source.filePath(QStringLiteral("maidata.txt"));
    const QString saved = target.filePath(QStringLiteral("copy.simai"));
    if (!QDir().mkpath(source.filePath(QStringLiteral(".miacode")))
        || !QDir().mkpath(source.filePath(QStringLiteral("videos")))
        || !QDir().mkpath(target.filePath(QStringLiteral(".miacode")))) return false;
    const QByteArray chart = "&title=Online\n&video=videos/custom.mp4\n&lv_5=12\n&inote_5=(120){4}1,E\n";
    const QStringList resources{QStringLiteral("track.flac"), QStringLiteral("bg.jpg"),
        QStringLiteral("pv.mp4"), QStringLiteral("videos/custom.mp4"),
        QStringLiteral(".miacode/preferences.json"), QStringLiteral(".miacode/miacode_settings.json")};
    if (!writeBytes(original, chart)) return false;
    for (const QString& resource : resources) {
        if (!writeBytes(source.filePath(resource), resource.toUtf8())) return false;
    }
    if (!writeBytes(target.filePath(QStringLiteral("track.mp3")), "old track")
        || !writeBytes(target.filePath(QStringLiteral(".miacode/preferences.json")), "old settings")) return false;
    miacode::ChartWorkspace workspace;
    miacode::ChartWorkspaceFileService files(workspace);
    if (!files.open(original, origin).accepted) return false;
    const auto generation = workspace.snapshot().documentOpenGeneration;
    workspace.replaceActiveDifficultyChart(QStringLiteral("(120){4}2,E"));
    if (origin == miacode::ChartDocumentOrigin::NetPreview
        && !expect(!files.save(0).accepted && !files.saveAs(original, 0).accepted,
                   QStringLiteral("preview saving establishes a separate local project"), out)) return false;
    bool ok = expect(files.saveAs(saved, 0).accepted && !workspace.isNetPreview()
        && workspace.snapshot().documentOpenGeneration == generation
        && readBytes(saved).contains("(120){4}2,E"),
        QStringLiteral("save as preserves the document and its generation"), out);
    for (const QString& resource : resources) {
        ok &= expect(readBytes(target.filePath(resource)) == readBytes(source.filePath(resource)),
                     QStringLiteral("save as carries %1").arg(resource), out);
    }
    ok &= expect(miacode::chart_assets::resolveTrackPath(saved) == target.filePath(QStringLiteral("track.flac"))
        && miacode::chart_assets::resolveChartVideoPath(saved, workspace.document().videoPath)
            == target.filePath(QStringLiteral("videos/custom.mp4")),
        QStringLiteral("saved media resolves in the target project"), out);
    QDir(source.path()).removeRecursively();
    ok &= expect(files.open(saved).accepted
        && miacode::chart_assets::resolveTrackPath(saved) == target.filePath(QStringLiteral("track.flac"))
        && miacode::chart_assets::resolveChartVideoPath(saved, workspace.document().videoPath)
            == target.filePath(QStringLiteral("videos/custom.mp4")),
        QStringLiteral("saved project reopens with media after its source is removed"), out);
    return ok;
}

bool verifyExternalVideoSaveAs(QTextStream& out)
{
    QTemporaryDir directory;
    const QDir source(directory.filePath(QStringLiteral("source")));
    const QString external = directory.filePath(QStringLiteral("external.mp4"));
    const QString original = source.filePath(QStringLiteral("maidata.txt"));
    const QString saved = directory.filePath(QStringLiteral("target/maidata.txt"));
    QDir().mkpath(source.path());
    if (!writeBytes(external, "external video") || !writeBytes(original,
        (QStringLiteral("&video=%1\n&lv_5=12\n&inote_5=(120){4}1,E\n").arg(external)).toUtf8())) return false;
    miacode::ChartWorkspace workspace;
    miacode::ChartWorkspaceFileService files(workspace);
    if (!files.open(original).accepted) return false;
    const auto revision = workspace.snapshot().revision;
    bool ok = expect(files.saveAs(saved, 5).accepted && !workspace.snapshot().dirty
        && workspace.snapshot().revision == revision + 1
        && workspace.document().videoPath == QLatin1String("media/external.mp4"),
        QStringLiteral("section save relocates external video in one workspace transaction"), out);
    QFile::remove(external);
    ok &= expect(files.open(saved).accepted
        && readBytes(miacode::chart_assets::resolveChartVideoPath(saved, workspace.document().videoPath)) == "external video",
        QStringLiteral("external video becomes an independent local resource"), out);
    return ok;
}

bool verifyFailedProjectSaveAs(QTextStream& out)
{
    QTemporaryDir directory;
    const QDir source(directory.filePath(QStringLiteral("source")));
    const QDir target(directory.filePath(QStringLiteral("target")));
    QDir().mkpath(source.filePath(QStringLiteral(".miacode")));
    // A directory at a resource's destination forces a failure after the track replacement.
    QDir().mkpath(target.filePath(QStringLiteral("bg.jpg")));
    const QString original = source.filePath(QStringLiteral("maidata.txt"));
    const QString saved = target.filePath(QStringLiteral("maidata.txt"));
    if (!writeBytes(original, "&lv_5=12\n&inote_5=(120){4}1,E\n")
        || !writeBytes(source.filePath(QStringLiteral("track.mp3")), "new track")
        || !writeBytes(source.filePath(QStringLiteral("bg.jpg")), "new background")
        || !writeBytes(target.filePath(QStringLiteral("track.mp3")), "old track")
        || !writeBytes(saved, "old chart")) return false;
    miacode::ChartWorkspace workspace;
    miacode::ChartWorkspaceFileService files(workspace);
    if (!files.open(original, miacode::ChartDocumentOrigin::NetPreview).accepted) return false;
    workspace.replaceActiveDifficultyChart(QStringLiteral("(120){4}2,E"));
    const auto before = workspace.snapshot();
    const auto result = files.saveAs(saved, 0);
    return expect(!result.accepted && workspace.isNetPreview()
        && workspace.snapshot().filePath == before.filePath
        && workspace.snapshot().revision == before.revision
        && workspace.snapshot().dirty == before.dirty
        && readBytes(target.filePath(QStringLiteral("track.mp3"))) == "old track"
        && readBytes(saved) == "old chart"
        && readBytes(source.filePath(QStringLiteral("track.mp3"))) == "new track",
        QStringLiteral("resource failure restores the target and preserves the source workspace"), out);
}

bool verifySavedFieldsAndDifficultyStructure(QTextStream& out)
{
    QTemporaryDir directory;
    const QString path = directory.filePath(QStringLiteral("maidata.txt"));
    if (!directory.isValid() || !writeBytes(path,
        "&title=old\n&artist=old\n&first=0\n&des_3=old name\n"
        "&lv_5=12\n&inote_5=(120){4}1,E\n&lv_6=13\n&inote_6=(120){4}2,E\n"
        "&lv_7=\n&des_7=\n&inote_7=\n")) return false;
    miacode::ChartWorkspace workspace;
    miacode::ChartWorkspaceFileService files(workspace);
    if (!files.open(path).accepted) return false;
    workspace.updateDocumentField(miacode::ChartWorkspaceDocumentField::Title, "new");
    workspace.updateDocumentField(miacode::ChartWorkspaceDocumentField::Artist, "new artist");
    workspace.updateDocumentField(miacode::ChartWorkspaceDocumentField::First, "0.125");
    workspace.updateDifficultyField(5, miacode::ChartWorkspaceDifficultyField::Level, "14");
    workspace.setDesignerForSlot(5, "new slot");
    workspace.setDesignerForSlot(7, "Alice");
    workspace.replaceExtraFields("&wholebpm=180\n&custom_3=kept\n&another=new", "6");
    workspace.replaceDifficultyChart(5, "(120){4}3,E\n");
    workspace.replaceDifficultyChart(6, "(120){4}4,E");
    bool ok = expect(files.save(miacode::ChartWorkspace::MetadataSection).accepted,
                     QStringLiteral("metadata save succeeds with multiple field groups"), out);
    const auto metadata = SimaiDocument::fromText(QString::fromUtf8(readBytes(path)));
    ok &= expect(metadata.title == "new" && metadata.artist == "new artist" && metadata.first == "0.125"
        && metadata.difficulty(5)->level == "14" && metadata.designerForSlot(5) == "new slot"
        && metadata.difficulty(7) != nullptr && metadata.designerForSlot(7) == "Alice"
        && metadata.difficulty(3) == nullptr && readBytes(path).contains("&custom_3=kept")
        && readBytes(path).contains("&another=new") && readBytes(path).contains("&clock_count=6")
        && readBytes(path).contains("&wholebpm=180") && !workspace.metadataDirty()
        && metadata.difficulty(5)->chart == "(120){4}1,E"
        && metadata.difficulty(6)->chart == "(120){4}2,E",
        QStringLiteral("metadata preserves named empty slots and unknown fields without saving chart drafts"), out);
    ok &= expect(files.save(5).accepted && workspace.snapshot().dirtyDifficultyIds == QVector<int>{6}
        && QString::fromUtf8(readBytes(path)).replace("\r\n", "\n").contains("&inote_5=(120){4}3,E\n\n")
        && files.save(0).accepted && !workspace.snapshot().dirty
        && QString::fromUtf8(readBytes(path)).replace("\r\n", "\n") == workspace.snapshot().sourceText,
        QStringLiteral("section and whole saves establish the exact written save point"), out);
    ok &= expect(files.open(path).accepted && workspace.document().difficulty(7) != nullptr
        && workspace.document().difficulty(3) == nullptr,
        QStringLiteral("reopening preserves saved difficulty structure"), out);
    workspace.addDifficulty(3);
    workspace.setDesignerForSlot(3, "new name");
    workspace.updateDifficultyField(3, miacode::ChartWorkspaceDifficultyField::Level, "10");
    ok &= expect(files.save(miacode::ChartWorkspace::MetadataSection).accepted
        && workspace.document().standaloneDesignerIds().isEmpty()
        && readBytes(path).contains("&des_3=new name"),
        QStringLiteral("an added difficulty exclusively owns its adopted designer"), out);
    workspace.removeDifficulty(3);
    ok &= expect(files.save(miacode::ChartWorkspace::MetadataSection).accepted
        && !readBytes(path).contains("&des_3=") && !readBytes(path).contains("&inote_3=")
        && workspace.document().designerForSlot(3).isEmpty(),
        QStringLiteral("deleting an adopted difficulty never writes its old designer"), out);
    return ok;
}

}  // namespace

int main()
{
    QTextStream out(stderr);
    if (!verifyOpenSaveAndSaveAs(out)
        || !verifySavedFieldsAndDifficultyStructure(out)
        || !verifyOpenAcceptsEmptyInoteFile(out)
        || !verifyProjectSaveAs(out, miacode::ChartDocumentOrigin::Local)
        || !verifyProjectSaveAs(out, miacode::ChartDocumentOrigin::NetPreview)
        || !verifyExternalVideoSaveAs(out)
        || !verifyFailedProjectSaveAs(out)
        || !verifyFailedOpenRetainsWorkspace(out)) return 1;
    QTextStream result(stdout);
    result << "Chart workspace file-service checks passed." << Qt::endl;
    return 0;
}
