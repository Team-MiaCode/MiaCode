#include "app/services/ShortcutRegistry.h"
#include "app/services/PreferenceJsonFile.h"
#include "app/ui/chrome/ShortcutModel.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QSet>
#include <QTemporaryDir>
#include <QTextStream>

namespace {
bool put(const QString& path, const QByteArray& bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}
QByteArray bytes(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir temporary;
    if (!temporary.isValid()) {
        return 1;
    }
    miacode::debug_log::setSessionProjectLogDirectory(temporary.path());
    QTextStream err(stderr);
    bool ok = true;
    const auto expect = [&](bool condition, const char* message) {
        if (!condition) { err << "FAIL: " << message << '\n'; ok = false; }
    };
    using miacode::ui::ShortcutRegistry;
    using miacode::ui::ShortcutModel;
    using namespace miacode::preference_json_file;
    const QString id = QStringLiteral("transform.mirror_lr");
    const QString second = QStringLiteral("preview.stop_or_play");
    const QString path = temporary.filePath(QStringLiteral("shortcuts.json"));
    const QJsonObject fixture{
        {QStringLiteral("schema"), 1},
        {QStringLiteral("vendor"), QJsonObject{{QStringLiteral("keep"), true}}},
        {QStringLiteral("editable"), QJsonArray{id, second}},
        {QStringLiteral("actions"), QJsonObject{
             {id, QJsonObject{{QStringLiteral("shortcut"), QStringLiteral("Ctrl+1")},
                             {QStringLiteral("default"), QStringLiteral("Ctrl+A")},
                             {QStringLiteral("metadata"), 42}}},
             {QStringLiteral("vendor.noneditable"), QJsonObject{{QStringLiteral("shortcut"), QStringLiteral("Ctrl+B")}}}}},
        {QStringLiteral("contextual"), QJsonObject{
             {id, QJsonObject{{QStringLiteral("shortcut"), QStringLiteral("Ctrl+2")},
                             {QStringLiteral("context"), QStringLiteral("keep")}}},
             {QStringLiteral("vendor.context"), QJsonObject{{QStringLiteral("unknown"), true}}}}},
    };
    expect(put(path, QJsonDocument(fixture).toJson()), "create independent override fixture");
    ShortcutRegistry registry(path);
    ShortcutModel model(registry);
    expect(registry.shortcutText(id) == "Ctrl+2", "contextual retains precedence over app actions");
    int notifications = 0;
    QObject::connect(&model, &ShortcutModel::revisionChanged, [&] { ++notifications; });
    expect(model.setShortcutText(id, QStringLiteral("Ctrl+3")) && notifications == 1
               && registry.shortcutText(id) == "Ctrl+3", "accepted edit applies and notifies without reload");
    const QJsonObject saved = read(path).object;
    expect(saved.value("schema") == 1 && saved.value("vendor") == fixture.value("vendor")
               && saved.value("editable") == fixture.value("editable"), "save preserves unknown root fields");
    expect(saved.value("actions") == fixture.value("actions")
               && saved.value("contextual").toObject().value("vendor.context")
                   == fixture.value("contextual").toObject().value("vendor.context")
               && saved.value("contextual").toObject().value(id).toObject().value("context") == "keep",
           "save preserves action metadata, noneditable bindings and contextual fields");
    ShortcutRegistry restart(path);
    expect(restart.shortcutText(id) == "Ctrl+3", "ordinary edit survives a new registry lifetime");
    const QString defaults = registry.defaultShortcutText(id);
    model.resetShortcut(id);
    ShortcutRegistry resetRestart(path);
    expect(registry.shortcutText(id) == defaults && resetRestart.shortcutText(id) == defaults,
           "reset restores the declared default even beside preserved hand-authored metadata");
    expect(read(path).object.value("actions").toObject().value(id).toObject().value("metadata") == 42,
           "reset does not discard metadata");

    const QString extension = QStringLiteral("extension.test.persist");
    expect(registry.registerExtensionShortcut(extension, QStringLiteral("Extension"), QKeySequence("Ctrl+6")),
           "register extension shortcut");
    expect(model.setShortcutText(extension, QStringLiteral("Ctrl+7")), "edit extension shortcut");
    registry.reload();
    bool extensionRetained = false;
    for (const auto& definition : registry.editableShortcuts()) {
        extensionRetained |= definition.id == extension && definition.labelEn == "Extension";
    }
    expect(extensionRetained && registry.shortcutText(extension) == "Ctrl+7",
           "editing and explicit reload retain extension definitions and overrides");

    const QString previous = temporary.filePath(QStringLiteral("previous.json"));
    expect(QFile::rename(path, previous) && QDir().mkdir(path), "block atomic persistence target");
    const int beforeFailure = notifications;
    expect(model.setShortcutText(id, QStringLiteral("Ctrl+8")) && notifications == beforeFailure + 1
               && registry.shortcutText(id) == "Ctrl+8", "failed disk save still applies and notifies runtime edit");
    model.reload();
    expect(registry.shortcutText(id) == "Ctrl+8", "explicit reload cannot discard unsaved edits");
    expect(QDir().rmdir(path) && QFile::rename(previous, path), "restore persistence target");
    expect(model.setShortcutText(second, QStringLiteral("Ctrl+9")), "next edit retries pending persistence");
    ShortcutRegistry retried(path);
    expect(retried.shortcutText(id) == "Ctrl+8" && retried.shortcutText(second) == "Ctrl+9"
               && read(path).object.value("vendor") == fixture.value("vendor"),
           "retry retains every pending edit and unrelated original fields");
    const int beforeInvalid = notifications;
    expect(!model.setShortcutText(id, QString()) && notifications == beforeInvalid
               && registry.shortcutText(id) == "Ctrl+8", "invalid input is rejected without changing runtime state");

    const QString zoomIn = QStringLiteral("ui.zoom_in");
    const QString zoomOut = QStringLiteral("ui.zoom_out");
    const QString legacyIn = QStringLiteral("editor.font_increase");
    const QString legacyOut = QStringLiteral("editor.font_decrease");
    const QString legacyPath = temporary.filePath(QStringLiteral("legacy-font-shortcuts.json"));
    const QJsonObject legacyFixture{
        {QStringLiteral("schema"), 1},
        {QStringLiteral("editable"), QJsonArray{legacyIn, legacyOut, zoomIn}},
        {QStringLiteral("actions"), QJsonObject{
            {legacyIn, QJsonObject{{QStringLiteral("shortcut"), QStringLiteral("Ctrl+Alt+8")},
                                  {QStringLiteral("metadata"), 42}}},
            {legacyOut, QJsonObject{{QStringLiteral("default"), QStringLiteral("Ctrl+Alt+9")}}}}},
        {QStringLiteral("contextual"), QJsonObject{
            {legacyIn, QJsonObject{{QStringLiteral("shortcut"), QStringLiteral("Ctrl+Alt+7")},
                                  {QStringLiteral("default"), QStringLiteral("Ctrl+Alt+3")},
                                  {QStringLiteral("context"), QStringLiteral("keep")}}}}},
    };
    expect(put(legacyPath, QJsonDocument(legacyFixture).toJson()), "legacy font shortcut fixture");
    const QByteArray originalLegacy = bytes(legacyPath);
    ShortcutRegistry upgraded(legacyPath);
    ShortcutModel upgradedModel(upgraded);
    QSet<QString> upgradedIds;
    for (const auto& definition : upgraded.editableShortcuts()) upgradedIds.insert(definition.id);
    expect(upgraded.shortcutText(zoomIn) == "Ctrl+Alt+7"
               && upgraded.shortcutText(zoomOut) == "Ctrl+Alt+9"
               && upgraded.shortcutText(legacyIn).isEmpty() && upgraded.shortcutText(legacyOut).isEmpty()
               && upgradedIds == QSet<QString>{zoomIn, zoomOut, QStringLiteral("ui.zoom_reset")}
               && bytes(legacyPath) == originalLegacy,
           "legacy custom keys and editable ids upgrade once without modifying the source");
    expect(upgradedModel.setShortcutText(zoomIn, QStringLiteral("Ctrl+Alt+6")), "edit upgraded UI scale shortcut");
    ShortcutRegistry upgradedRestart(legacyPath);
    const QJsonObject upgradedSaved = read(legacyPath).object;
    expect(upgradedRestart.shortcutText(zoomIn) == "Ctrl+Alt+6"
               && upgradedSaved.value("actions").toObject().value(legacyIn).toObject().value("metadata") == 42
               && !upgradedSaved.value("actions").toObject().value(legacyIn).toObject().contains("shortcut")
               && !upgradedSaved.value("contextual").toObject().value(legacyIn).toObject().contains("shortcut")
               && upgradedSaved.value("contextual").toObject().value(legacyIn).toObject().value("context") == "keep",
           "editing writes the new identity, retires legacy owned fields, and preserves metadata");
    upgradedModel.resetShortcut(zoomIn);
    upgradedModel.resetShortcut(zoomOut);
    ShortcutRegistry upgradedReset(legacyPath);
    expect(upgradedReset.shortcutText(zoomIn) == upgraded.defaultShortcutText(zoomIn)
               && upgradedReset.shortcutText(zoomOut) == upgraded.defaultShortcutText(zoomOut),
           "reset cannot resurrect a legacy custom key or copied default");

    QJsonObject mixed = legacyFixture;
    QJsonObject mixedActions = mixed.value("actions").toObject();
    mixedActions.insert(zoomOut, QJsonObject{{QStringLiteral("shortcut"), QStringLiteral("Ctrl+Alt+5")}});
    mixed.insert(QStringLiteral("actions"), mixedActions);
    expect(put(legacyPath, QJsonDocument(mixed).toJson()), "mixed legacy and new shortcut fixture");
    ShortcutRegistry mixedRegistry(legacyPath);
    expect(mixedRegistry.shortcutText(zoomOut) == "Ctrl+Alt+5", "new identity wins over its legacy alias in the same section");

    const QString legacyCwd = temporary.filePath(QStringLiteral("legacy-cwd-shortcuts.json"));
    expect(put(legacyCwd, "{\"contextual\":{\"editor.font_decrease\":{\"shortcut\":\"Ctrl+Alt+4\"}}}"),
           "legacy external shortcut fixture");
    const QByteArray originalLegacyCwd = bytes(legacyCwd);
    ShortcutRegistry upgradedLayered(legacyPath, legacyCwd);
    expect(upgradedLayered.shortcutText(zoomOut) == "Ctrl+Alt+4"
               && bytes(legacyCwd) == originalLegacyCwd,
           "legacy aliases preserve external source priority without rewriting external input");

    const QString cwd = temporary.filePath(QStringLiteral("cwd-shortcuts.json"));
    expect(put(cwd, "{\"actions\":{\"transform.mirror_lr\":{\"shortcut\":\"Ctrl+4\"}}}"), "CWD override fixture");
    const QByteArray cwdOriginal = bytes(cwd);
    ShortcutRegistry layered(path, cwd);
    ShortcutModel layeredModel(layered);
    expect(layered.shortcutText(id) == "Ctrl+4", "CWD retains the existing higher source priority");
    expect(layeredModel.setShortcutText(id, QStringLiteral("Ctrl+5")) && layered.shortcutText(id) == "Ctrl+5",
           "UI edit applies immediately despite an existing CWD override");
    layeredModel.reload();
    ShortcutRegistry layeredRestart(path, cwd);
    expect(layered.shortcutText(id) == "Ctrl+4" && layeredRestart.shortcutText(id) == "Ctrl+4"
               && bytes(cwd) == cwdOriginal, "explicit reload/restart retain original CWD precedence and write target");

    const QString corrupt = temporary.filePath(QStringLiteral("corrupt-shortcuts.json"));
    const QByteArray broken("{\r\n\"actions\":");
    expect(put(corrupt, broken), "corrupt owned override fixture");
    ShortcutRegistry recovered(corrupt);
    const QDir directory(temporary.path());
    const QStringList backups = directory.entryList({QStringLiteral("corrupt-shortcuts.json.corrupt-*")}, QDir::Files);
    expect(read(corrupt).status == ReadStatus::Valid && backups.size() == 1
               && bytes(directory.filePath(backups.value(0))) == broken, "corrupt owned override is backed up byte-for-byte and rebuilt");
    expect(put(cwd, broken), "corrupt external CWD fixture");
    ShortcutRegistry external(path, cwd);
    expect(bytes(cwd) == broken && directory.entryList({QStringLiteral("cwd-shortcuts.json.corrupt-*")}, QDir::Files).isEmpty(),
           "invalid external input is logged without modifying it");
    for (const QJsonValue version : {QJsonValue(23), QJsonValue(QStringLiteral("1")), QJsonValue(QJsonValue::Null)}) {
        QJsonObject future{{QStringLiteral("schema"), version}};
        const QByteArray originalFuture = QJsonDocument(future).toJson();
        const QString futurePath = temporary.filePath(QStringLiteral("future-shortcuts.json"));
        expect(put(futurePath, originalFuture), "future shortcut fixture");
        ShortcutRegistry guarded(futurePath);
        expect(guarded.setUserShortcutText(id, QStringLiteral("Ctrl+8"))
                   && guarded.shortcutText(id) == "Ctrl+8" && bytes(futurePath) == originalFuture,
               "unsupported shortcut schema accepts runtime edit without overwriting disk");
        guarded.reload();
        expect(guarded.shortcutText(id) == "Ctrl+8" && bytes(futurePath) == originalFuture,
               "unsupported shortcut schema retains pending override after reload");
    }
    miacode::debug_log::shutdownAsyncLogWriter();
    const QByteArray log = bytes(miacode::debug_log::runtimeLogPath());
    expect(log.contains("invalid-json") && log.contains("read-"), "silent recovery and IO failures leave log evidence");
    return ok ? 0 : 1;
}
