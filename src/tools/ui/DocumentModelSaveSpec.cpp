#include "app/ui/document/DocumentModel.h"

#include <QCoreApplication>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTextStream>

namespace {
class FakeBridge final : public miacode::DocumentBridge
{
public:
    QString sourceText() const override { return {}; }
    QString filePath() const override { return {}; }
    int activeDifficultyId() const override { return 5; }
    bool applyCommittedDocument(const QString&, const QString&, int, bool, quint64, CommitKind, bool) override
    { return true; }
    QVariantList recentDocumentEntries() override { return {}; }
    void noteRecentDocument(const QString&) override {}
    void removeRecentDocument(const QString&) override {}
    QVariantList backupDocumentEntries() override { return {}; }
    void restoreBackupDocument(const QString&) override {}
    miacode::chart_transform::ChartNormalizationOptions normalizationOptions() const override { return {}; }
    void setNormalizationOptions(const miacode::chart_transform::ChartNormalizationOptions&) override {}
    bool requestEditorNavigation(int, int, int, int, bool, bool, bool) override { return true; }
    void setDocumentSaveHandler(std::function<bool(const QString&)> value) override { save = std::move(value); }
    void setChartTextHandler(std::function<bool(const QString&)> value) override { chart = std::move(value); }
    void setLeaveDocumentHandler(std::function<void(std::function<void(bool)>)> value) override
    { leave = std::move(value); }
    void requestLeaveDocument(std::function<void(bool)> decided) override { leave(std::move(decided)); }
    bool applyDocumentDesignerSlots(const QVector<QPair<int, QString>>&, bool, const QString&) override
    { return false; }
    void reconcileUnifiedDocumentDesigner(UnifiedDesignerReconcileReason) override {}
    std::function<bool(const QString&)> save;
    std::function<bool(const QString&)> chart;
    std::function<void(std::function<void(bool)>)> leave;
};

bool expect(bool condition, const char* message)
{
    if (!condition) QTextStream(stderr) << "FAIL: " << message << Qt::endl;
    return condition;
}

QByteArray read(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

bool verifyDifficultyCardProjection()
{
    miacode::ChartWorkspace workspace;
    miacode::ChartWorkspaceFileService files(workspace);
    workspace.openSource("&title=Song\n&artist=Artist\n&des=Shared\n"
                         "&lv_4=12\n&des_4=Expert designer\n&inote_4=(174.5){4}1,E\n"
                         "&lv_5=14+\n&inote_5=(200){4}A1,E\n", {}, 5);
    miacode::AnalysisService analysis(workspace);
    analysis.setDiagnosticsDeferred(true);
    miacode::ShellNotifications notifications;
    miacode::UiRequestService requests;
    FakeBridge fake;
    miacode::DocumentBridge* bridge = &fake;
    miacode::PreviewSurface* preview = nullptr;
    miacode::ui::DocumentModel model(notifications, workspace, files, analysis, requests, bridge, preview);
    const auto revision = workspace.snapshot().revision;
    const auto expert = model.difficultyCardTrack(4);
    const auto master = model.difficultyCardTrack(5);
    bool ok = expect(expert.value("difficulty") == "EXPERT" && expert.value("level") == "12"
        && expert.value("designer") == "Expert designer" && expert.value("bpm") == "174.5"
        && expert.value("mode") == "Standard" && master.value("difficulty") == "MASTER"
        && master.value("level") == "14+" && master.value("designer") == "Shared"
        && master.value("bpm") == "200" && master.value("mode") == "DX",
        "card projection uses the requested difficulty's level, designer, BPM and chart type");
    ok &= expect(model.currentDifficultyId() == 5 && workspace.snapshot().revision == revision
        && !workspace.snapshot().dirty && model.difficultyCardTrack(7).isEmpty(),
        "previewing a card neither selects an editor difficulty nor changes the document");
    workspace.updateDocumentField(miacode::ChartWorkspaceDocumentField::Title, "Renamed");
    workspace.updateDifficultyField(5, miacode::ChartWorkspaceDifficultyField::Level, "15");
    workspace.replaceDifficultyChart(5, "(180){4}1,E");
    const auto updated = model.difficultyCardTrack(5);
    ok &= expect(updated.value("title") == "Renamed" && updated.value("level") == "15"
        && updated.value("bpm") == "180" && updated.value("mode") == "Standard",
        "metadata and chart edits invalidate the card cache");
    workspace.openSource("&title=Next\n&wholebpm=150\n&lv_5=8\n&inote_5=(100){4}1,E\n", {}, 5);
    const auto replacement = model.difficultyCardTrack(5);
    ok &= expect(replacement.value("title") == "Next" && replacement.value("level") == "8"
        && replacement.value("bpm") == "150",
        "replacement documents refresh the card and preserve the whole-BPM priority");
    workspace.removeDifficulty(5);
    ok &= expect(model.difficultyCardTrack(5).isEmpty(), "deleted difficulties have no stale card");
    analysis.shutdown();
    return ok;
}

bool verifyRejectedDraftBlocksPersistenceAndLeave()
{
    QTemporaryDir directory;
    if (!directory.isValid()) return false;
    const QString path = directory.filePath("maidata.txt");
    const QString copy = directory.filePath("copy.txt");
    const QString source = "&title=old\n&custom=old\n&lv_5=12\n&inote_5=(120){4}1,E\n";
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(source.toUtf8()) != source.toUtf8().size()) return false;
    file.close();
    miacode::ChartWorkspace workspace;
    miacode::ChartWorkspaceFileService files(workspace);
    if (!files.open(path).accepted) return false;
    miacode::AnalysisService analysis(workspace);
    analysis.setDiagnosticsDeferred(true);
    miacode::ShellNotifications notifications;
    miacode::UiRequestService requests;
    FakeBridge fake;
    miacode::DocumentBridge* bridge = &fake;
    miacode::PreviewSurface* preview = nullptr;
    miacode::ui::DocumentModel model(notifications, workspace, files, analysis, requests, bridge, preview);
    model.setWholeSourceEditorActive(true);
    const auto revision = workspace.snapshot().revision;
    const QByteArray originalBytes = read(path);
    const QString rejected = "&custom=new\n&missing_equals\n&another=new";
    bool submitted = false;
    const auto submitDraft = QObject::connect(&model, &miacode::ui::DocumentModel::editingFinishedRequested,
        &model, [&] { if (!submitted) { submitted = true; model.setMetadataExtraText(rejected); } });
    bool ok = expect(!model.save() && submitted && model.metadataExtraText() == rejected
        && !model.metadataExtraError().isEmpty() && model.dirty()
        && model.dirtyEditorKeys().contains("metadata") && !workspace.snapshot().dirty
        && workspace.snapshot().revision == revision && read(path) == originalBytes,
        "input rejected during flush stays visible and dirty without changing workspace or file");
    QObject::disconnect(submitDraft);
    model.setMetadataTitle("new title");
    QCoreApplication::processEvents();
    ok &= expect(model.metadataExtraText() == rejected && read(path) == originalBytes,
        "unrelated metadata updates and autosave preserve the rejected draft");
    ok &= expect(!model.saveWholeDocument() && !model.saveDifficultySection(5)
        && !model.saveAs(QUrl::fromLocalFile(copy)) && !fake.save(copy)
        && !QFile::exists(copy) && read(path) == originalBytes,
        "all manual and bridge save entrances reject invalid input without publishing files");
    bool mayLeave = true;
    int decisions = 0;
    model.requestLeaveDocument([&](bool value) { mayLeave = value; ++decisions; });
    ok &= expect(!mayLeave && decisions == 1 && requests.pendingChoiceCount() == 0,
        "invalid input prevents document close before the clean-workspace fast path");
    model.requestLeaveCurrentField([&](bool value) { mayLeave = value; });
    ok &= expect(!mayLeave && model.metadataExtraText() == rejected,
        "invalid input prevents page navigation and retains its draft");
    model.requestCloseDifficulty(5);
    model.closeDocument();
    ok &= expect(requests.pendingChoiceCount() == 0 && model.hasDocument(),
        "invalid metadata prevents a difficulty close from hiding the input");
    model.setMetadataExtraText("&custom=new\n&another=new");
    ok &= expect(model.metadataExtraError().isEmpty() && model.saveWholeDocument()
        && !model.dirty() && read(path).contains("&custom=new") && read(path).contains("&another=new")
        && read(path).contains("&title=new title"),
        "correcting the draft saves every accepted field and clears its dirty state");
    model.setMetadataExtraText(rejected);
    ok &= expect(model.discardChanges() && model.metadataExtraError().isEmpty() && !model.dirty()
        && model.metadataExtraText().contains("&custom=new"),
        "explicit discard restores the save point and clears the rejected draft");
    model.setMetadataExtraText(rejected);
    workspace.openSource(source, path, 5, miacode::ChartDocumentOrigin::NetPreview);
    ok &= expect(model.metadataExtraError().isEmpty() && !model.dirty()
        && model.metadataExtraText().contains("&custom=old"),
        "a new document generation does not inherit another document's draft");
    model.setMetadataExtraText(rejected);
    ok &= expect(!model.save() && requests.pendingFileRequestCount() == 0 && workspace.isNetPreview(),
        "invalid online-preview input is rejected before requesting a save-as path");
    model.setMetadataExtraText("&custom=online copy");
    ok &= expect(model.saveAs(QUrl::fromLocalFile(copy)) && !workspace.isNetPreview()
        && !model.dirty() && read(copy).contains("&custom=online copy"),
        "corrected online-preview input can still be saved as a local document");
    int metadataNotifications = 0;
    QObject::connect(&model, &miacode::ui::DocumentModel::metadataChanged,
                     &model, [&] { ++metadataNotifications; });
    workspace.updateDocumentField(miacode::ChartWorkspaceDocumentField::First,
                                  QStringLiteral("0.375"));
    ok &= expect(metadataNotifications > 0 && model.currentDifficultyOffset() == QLatin1String("0.375"),
                 "a calibration write through the workspace updates the header offset projection");
    analysis.shutdown();
    return ok;
}
} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);
    return verifyDifficultyCardProjection() && verifyRejectedDraftBlocksPersistenceAndLeave() ? 0 : 1;
}
