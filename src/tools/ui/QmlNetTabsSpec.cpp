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
    property Mia.ViewState state: Mia.ViewState {
        onApplicationEditorActivationRequested: function(key) { root.netActivations++ }
        onDifficultyEditorActivationRequested: function(id) { root.difficultyActivations++ }
    }
    function require(condition, message) { if (!condition && !failure) failure = message }
    function run() {
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
