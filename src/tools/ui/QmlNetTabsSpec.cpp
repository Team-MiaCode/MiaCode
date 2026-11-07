#include <QCoreApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QTextStream>
#include <memory>

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData(R"(
import QtQml
import "." as Mia
QtObject {
    id: root
    property string failure: ""
    property int netActivations: 0
    property int difficultyActivations: 0
    property bool confirmNetActivations: true
    property Mia.ViewState state: Mia.ViewState {
        onApplicationEditorActivationRequested: function(key) {
            root.netActivations++
            if (root.confirmNetActivations)
                confirmApplicationEditor(key)
        }
        onDifficultyEditorActivationRequested: function(id) { root.difficultyActivations++ }
    }
    function require(condition, message) { if (!condition && !failure) failure = message }
    function run() {
        state.openEditor(state.netDownloadEditorKey)
        require(state.netEditorActive && state.openEditorTabs.length === 1, "Download opens by default without a document")
        state.openEditor(state.netUploadEditorKey)
        require(state.openEditorTabs.length === 1 && root.netActivations === 1, "Upload stays hidden without configuration")
        state.setActiveEditor(state.netUploadEditorKey)
        require(state.activeEditorKey === state.netDownloadEditorKey && root.netActivations === 1, "Direct upload activation requires configuration")
        state.netUploadEnabled = true
        state.openEditor(state.netUploadEditorKey)
        require(state.activeEditorKey === state.netUploadEditorKey && state.openEditorTabs.length === 2, "Configuration enables upload")
        state.closeEditor(state.netUploadEditorKey)
        state.openEditor(state.netDownloadEditorKey)
        require(state.netEditorActive && state.openEditorTabs.length === 1, "Net opens without a document")
        state.openEditor(state.netDownloadEditorKey)
        require(state.openEditorTabs.length === 1, "Repeated opening reuses its tab")
        state.resetEditorTabs(3)
        require(state.containsEditor(state.netDownloadEditorKey) && state.activeEditorKey === "difficulty:3", "Document replacement preserves Net tab and selects the chart")
        require(root.difficultyActivations === 0, "Replacement remains silent")
        state.activateEditor(state.netDownloadEditorKey)
        state.syncDifficultyEditors([{id: 4}], 4)
        require(state.netEditorActive && state.containsEditor("difficulty:4"), "Chart synchronization preserves active Net")
        state.swapEditorTabs(state.netDownloadEditorKey, "difficulty:4")
        require(state.netEditorActive, "Reordering preserves page identity")
        state.closeEditor(state.netDownloadEditorKey)
        require(state.activeEditorKey === "difficulty:4" && root.difficultyActivations === 1, "Closing Net returns to document owner")
        state.openEditor(state.netDownloadEditorKey)
        state.resetEditorTabs(0)
        require(state.netEditorActive && state.openEditorTabs.length === 1, "Closing the document keeps the independent Net page")
        state.closeEditor(state.netDownloadEditorKey)
        require(!state.hasActiveEditor && state.openEditorTabs.length === 0, "Closing last tab clears presentation")
        require(root.netActivations >= 4, "User Net activations reach the page host")

        state.resetEditorTabs(3)
        root.confirmNetActivations = false
        state.openEditor(state.netDownloadEditorKey)
        require(state.activeEditorKey === "difficulty:3" && state.openEditorTabs.length === 1,
                "Pending Net navigation retains the active chart and tab list")
        state.cancelApplicationEditorActivation()
        require(state.activeEditorKey === "difficulty:3", "Rejected navigation retains the active chart")
        state.confirmApplicationEditor(state.netDownloadEditorKey)
        state.openEditor(state.netUploadEditorKey)
        require(state.activeEditorKey === state.netDownloadEditorKey && !state.containsEditor(state.netUploadEditorKey),
                "Pending upload navigation retains download")
        state.confirmApplicationEditor(state.netUploadEditorKey)
        state.closeEditor(state.netUploadEditorKey)
        require(state.pendingApplicationCloseKey === state.netUploadEditorKey
                && state.containsEditor(state.netUploadEditorKey) && state.activeEditorKey === state.netUploadEditorKey,
                "Closing to another Net tab waits for its navigation decision")
        state.cancelApplicationEditorActivation()
        require(state.containsEditor(state.netUploadEditorKey) && state.activeEditorKey === state.netUploadEditorKey,
                "Rejected close retains the current tab")
        state.closeEditor(state.netUploadEditorKey)
        state.confirmApplicationEditor(state.netDownloadEditorKey)
        require(!state.containsEditor(state.netUploadEditorKey) && state.activeEditorKey === state.netDownloadEditorKey,
                "Confirmed close activates download and removes upload")
        state.activateEditor("difficulty:3")
        state.closeEditor("difficulty:3")
        require(state.containsEditor("difficulty:3") && state.activeEditorKey === "difficulty:3",
                "Closing the chart to Net retains its view until confirmation")
        state.resetEditorTabs(4)
        require(state.pendingApplicationCloseKey.length === 0 && state.activeEditorKey === "difficulty:4",
                "Document replacement releases a pending close")
        return failure.length === 0
    }
}
)", QUrl::fromLocalFile(QStringLiteral(MIACODE_SOURCE_ROOT "/src/app/ui/NetTabsHarness.qml")));
    if (component.isError()) {
        for (const auto& error : component.errors()) QTextStream(stderr) << error.toString() << '\n';
        return 1;
    }
    std::unique_ptr<QObject> harness(component.create());
    QVariant result;
    if (!harness || !QMetaObject::invokeMethod(harness.get(), "run", Q_RETURN_ARG(QVariant, result)) || !result.toBool()) {
        QTextStream(stderr) << "FAIL: " << (harness ? harness->property("failure").toString() : QStringLiteral("Cannot create harness")) << '\n';
        return 1;
    }
    return 0;
}
