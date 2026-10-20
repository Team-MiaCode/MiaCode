#include "app/services/ShortcutRegistry.h"
#include "app/services/PreferenceVersionGuard.h"
#include "app/services/PreferenceJsonFile.h"

#include "common/InputShortcutGesture.h"

#include <QCoreApplication>
#include <QDir>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

#include <utility>


namespace miacode::ui {
namespace {

QString userOverridePath()
{
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("shortcuts.json"));
}

QString parseShortcutTextValue(const QJsonValue& value)
{
    return value.isString()
        ? miacode::input_shortcut::normalizeGestureText(value.toString())
        : QString();
}

QStringList parseStringList(const QJsonValue& value)
{
    QStringList values;
    const auto appendValue = [&values](const QString& value) {
        const QString trimmed = value.trimmed();
        if (!trimmed.isEmpty() && !values.contains(trimmed)) {
            values.append(trimmed);
        }
    };
    if (value.isString()) {
        appendValue(value.toString());
    } else if (value.isArray()) {
        for (const QJsonValue& entry : value.toArray()) {
            if (entry.isString()) {
                appendValue(entry.toString());
            }
        }
    }
    return values;
}

QString parseShortcutObject(const QJsonObject& object)
{
    if (object.contains(QStringLiteral("shortcut"))) {
        return parseShortcutTextValue(object.value(QStringLiteral("shortcut")));
    }
    return parseShortcutTextValue(object.value(QStringLiteral("default")));
}

QString legacyShortcutId(const QString& id)
{
    if (id == QStringLiteral("ui.zoom_in")) return QStringLiteral("editor.font_increase");
    if (id == QStringLiteral("ui.zoom_out")) return QStringLiteral("editor.font_decrease");
    return {};
}

QJsonObject migratedShortcutEntries(QJsonObject entries)
{
    for (const QString& id : {QStringLiteral("ui.zoom_in"), QStringLiteral("ui.zoom_out")}) {
        const QString legacy = legacyShortcutId(id);
        if (entries.contains(legacy)) {
            // An explicit new entry in the same section takes priority.
            if (!entries.contains(id)) entries.insert(id, entries.value(legacy));
            entries.remove(legacy);
        }
    }
    return entries;
}

}  // namespace

ShortcutRegistry& ShortcutRegistry::instance()
{
    static ShortcutRegistry registry;
    return registry;
}

ShortcutRegistry::ShortcutRegistry()
    : ShortcutRegistry(userOverridePath(), QDir(QDir::currentPath()).filePath(QStringLiteral("shortcuts.json")))
{
    useCurrentCwd_ = true;
}

ShortcutRegistry::ShortcutRegistry(QString overridePath, QString cwdOverridePath)
    : overridePath_(std::move(overridePath)), cwdOverridePath_(std::move(cwdOverridePath))
{
    reload();
}

void ShortcutRegistry::reload()
{
    definitions_.clear();
    defaultShortcuts_.clear();
    shortcuts_.clear();
    defaultShortcutTexts_.clear();
    shortcutTexts_.clear();
    userOverrides_.clear();
    editableShortcutIds_.clear();

    loadDefaults();

    if (useCurrentCwd_) {
        cwdOverridePath_ = QDir(QDir::currentPath()).filePath(QStringLiteral("shortcuts.json"));
    }
    loadOverrideFile(overridePath_, true);
    if (!cwdOverridePath_.isEmpty()
        && QDir::cleanPath(cwdOverridePath_) != QDir::cleanPath(overridePath_)) {
        loadOverrideFile(cwdOverridePath_);
    }
    const auto extensions = extensionDefinitions_.values();
    for (const ShortcutDefinition& definition : extensions) {
        registerExtensionShortcut(definition.id, definition.labelEn, definition.defaultSequence);
    }
    for (auto it = pendingChanges_.constBegin(); it != pendingChanges_.constEnd(); ++it) {
        applyRuntimeShortcut(it.key(), it.value());
    }
}

QKeySequence ShortcutRegistry::sequence(const QString& id, const QKeySequence& fallback) const
{
    return shortcuts_.value(id, fallback);
}

QString ShortcutRegistry::shortcutText(const QString& id, const QString& fallback) const
{
    return shortcutTexts_.value(id, fallback);
}

QList<ShortcutRegistry::ShortcutDefinition> ShortcutRegistry::editableShortcuts() const
{
    QList<ShortcutDefinition> result;
    for (const QString& id : editableShortcutIds_) {
        const ShortcutDefinition definition = definitions_.value(id);
        if (!definition.id.isEmpty()) {
            result.append(definition);
        }
    }
    return result;
}

QKeySequence ShortcutRegistry::defaultSequence(const QString& id) const
{
    return defaultShortcuts_.value(id);
}

QString ShortcutRegistry::defaultShortcutText(const QString& id) const
{
    return defaultShortcutTexts_.value(id);
}

bool ShortcutRegistry::registerExtensionShortcut(
    const QString& id,
    const QString& label,
    const QKeySequence& defaultSequence)
{
    const QString normalizedId = id.trimmed();
    if (!normalizedId.startsWith(QStringLiteral("extension.")) || label.trimmed().isEmpty()) {
        return false;
    }
    const QKeySequence validDefault = defaultSequence;
    const QString defaultText = validDefault.toString(QKeySequence::PortableText);
    definitions_.insert(normalizedId, {
        normalizedId,
        QString(),
        label,
        label,
        validDefault,
        defaultText,
    });
    extensionDefinitions_.insert(normalizedId, definitions_.value(normalizedId));
    if (!validDefault.isEmpty()) {
        defaultShortcuts_.insert(normalizedId, validDefault);
        defaultShortcutTexts_.insert(normalizedId, defaultText);
    } else {
        defaultShortcuts_.remove(normalizedId);
        defaultShortcutTexts_.remove(normalizedId);
    }
    if (!editableShortcutIds_.contains(normalizedId)) {
        editableShortcutIds_.append(normalizedId);
    }
    if (!userOverrides_.contains(normalizedId)) {
        shortcuts_.insert(normalizedId, validDefault);
        shortcutTexts_.insert(normalizedId, defaultText);
    }
    return true;
}

bool ShortcutRegistry::setUserShortcut(const QString& id, const QKeySequence& sequence)
{
    const QKeySequence normalized =
        sequence.toString(QKeySequence::PortableText) == QStringLiteral("Ctrl+Shift++")
            ? QKeySequence(QStringLiteral("Ctrl+Shift+="))
        : sequence.toString(QKeySequence::PortableText) == QStringLiteral("Ctrl+Shift+_")
            ? QKeySequence(QStringLiteral("Ctrl+Shift+-"))
            : sequence;
    return setUserShortcutText(id, normalized.toString(QKeySequence::PortableText));
}

bool ShortcutRegistry::setUserShortcutText(const QString& id, const QString& shortcutText)
{
    const QString normalized = miacode::input_shortcut::normalizeGestureText(shortcutText);
    if (!editableShortcutIds_.contains(id) || normalized.isEmpty()) {
        return false;
    }
    const PendingShortcut change{normalized, false};
    pendingChanges_.insert(id, change);
    applyRuntimeShortcut(id, change);
    saveUserOverrides();
    return true;
}

bool ShortcutRegistry::resetUserShortcut(const QString& id)
{
    if (!editableShortcutIds_.contains(id)) {
        return false;
    }
    const PendingShortcut change{defaultShortcutText(id), true};
    pendingChanges_.insert(id, change);
    applyRuntimeShortcut(id, change);
    saveUserOverrides();
    return true;
}

bool ShortcutRegistry::resetEditableShortcuts()
{
    for (const QString& id : editableShortcutIds_) {
        const PendingShortcut change{defaultShortcutText(id), true};
        pendingChanges_.insert(id, change);
        applyRuntimeShortcut(id, change);
    }
    saveUserOverrides();
    return true;
}

void ShortcutRegistry::loadDefaults()
{
    mergeJsonObject(preference_json_file::read(QStringLiteral(":/config/shortcuts.json")).object);
}

void ShortcutRegistry::loadOverrideFile(const QString& path, bool recoverCorruption)
{
    // Only the app-owned write target is repaired. A CWD input remains an
    // external higher-priority source, with errors logged and bytes preserved.
    mergeJsonObject(recoverCorruption ? preference_json_file::load(path)
                                     : preference_json_file::read(path).object);
}

void ShortcutRegistry::mergeJsonObject(const QJsonObject& root)
{
    if (root.contains(QStringLiteral("editable"))) {
        editableShortcutIds_ = parseStringList(root.value(QStringLiteral("editable")));
        bool migrated = false;
        for (const QString& id : {QStringLiteral("ui.zoom_in"), QStringLiteral("ui.zoom_out")}) {
            const QString legacy = legacyShortcutId(id);
            if (editableShortcutIds_.contains(legacy)) {
                for (QString& editableId : editableShortcutIds_) {
                    if (editableId == legacy) editableId = id;
                }
                migrated = true;
            }
        }
        editableShortcutIds_.removeDuplicates();
        if (migrated && !editableShortcutIds_.contains(QStringLiteral("ui.zoom_reset"))) {
            editableShortcutIds_.append(QStringLiteral("ui.zoom_reset"));
        }
    }
    const QJsonObject actions = migratedShortcutEntries(root.value(QStringLiteral("actions")).toObject());
    for (auto it = actions.constBegin(); it != actions.constEnd(); ++it) {
        const QJsonObject actionObject = it.value().toObject();
        const QString parsed = parseShortcutObject(actionObject);
        if (!defaultShortcuts_.contains(it.key()) && actionObject.contains(QStringLiteral("default"))) {
            const QString defaultText = parseShortcutTextValue(actionObject.value(QStringLiteral("default")));
            if (!defaultText.isEmpty()) {
                defaultShortcutTexts_.insert(it.key(), defaultText);
                defaultShortcuts_.insert(it.key(), QKeySequence(defaultText, QKeySequence::PortableText));
            }
        }
        if (!definitions_.contains(it.key())) {
            definitions_.insert(it.key(), {
                it.key(),
                actionObject.value(QStringLiteral("label_key")).toString(),
                actionObject.value(QStringLiteral("label_zh")).toString(),
                actionObject.value(QStringLiteral("label_en")).toString(),
                defaultShortcuts_.value(it.key()),
                defaultShortcutTexts_.value(it.key()),
            });
        }
        if (!parsed.isEmpty()) {
            shortcutTexts_.insert(it.key(), parsed);
            shortcuts_.insert(it.key(), QKeySequence(parsed, QKeySequence::PortableText));
            if (actionObject.contains(QStringLiteral("shortcut"))) {
                userOverrides_.insert(it.key(), parsed);
            }
        }
    }

    const QJsonObject contextual = migratedShortcutEntries(root.value(QStringLiteral("contextual")).toObject());
    for (auto it = contextual.constBegin(); it != contextual.constEnd(); ++it) {
        const QJsonObject shortcutObject = it.value().toObject();
        const QString parsed = parseShortcutObject(shortcutObject);
        if (parsed.isEmpty()) {
            continue;
        }
        if (!defaultShortcuts_.contains(it.key()) && shortcutObject.contains(QStringLiteral("default"))) {
            const QString defaultText = parseShortcutTextValue(shortcutObject.value(QStringLiteral("default")));
            if (!defaultText.isEmpty()) {
                defaultShortcutTexts_.insert(it.key(), defaultText);
                defaultShortcuts_.insert(it.key(), QKeySequence(defaultText, QKeySequence::PortableText));
            }
        }
        if (editableShortcutIds_.contains(it.key()) && !definitions_.contains(it.key())) {
            definitions_.insert(it.key(), {
                it.key(),
                shortcutObject.value(QStringLiteral("label_key")).toString(),
                shortcutObject.value(QStringLiteral("label_zh")).toString(),
                shortcutObject.value(QStringLiteral("label_en")).toString(),
                defaultShortcuts_.value(it.key()),
                defaultShortcutTexts_.value(it.key()),
            });
        }
        shortcutTexts_.insert(it.key(), parsed);
        shortcuts_.insert(it.key(), QKeySequence(parsed, QKeySequence::PortableText));
        if (shortcutObject.contains(QStringLiteral("shortcut"))) {
            userOverrides_.insert(it.key(), parsed);
        }
    }
}

void ShortcutRegistry::applyRuntimeShortcut(const QString& id, const PendingShortcut& change)
{
    shortcutTexts_.insert(id, change.text);
    shortcuts_.insert(id, QKeySequence(change.text, QKeySequence::PortableText));
    if (change.reset) {
        userOverrides_.remove(id);
    } else {
        userOverrides_.insert(id, change.text);
    }
}

bool ShortcutRegistry::saveUserOverrides()
{
    if (pendingChanges_.isEmpty()) {
        return true;
    }
    const auto existing = preference_json_file::read(overridePath_);
    if (existing.status == preference_json_file::ReadStatus::ReadError) {
        return false;
    }
    // Preserve both known metadata and extension/vendor fields. Only shortcut
    // fields explicitly edited in this session belong to this writer.
    QJsonObject root = existing.object;
    if (!app_preferences::allowVersionWrite(root, QStringLiteral("schema"), 1, QStringLiteral("shortcuts"))) return false;
    if (!root.contains(QStringLiteral("schema"))) {
        root.insert(QStringLiteral("schema"), 1);
    }
    QJsonObject actions = root.value(QStringLiteral("actions")).toObject();
    QJsonObject contextual = root.value(QStringLiteral("contextual")).toObject();
    bool actionsChanged = false;
    bool contextualChanged = false;
    for (auto it = pendingChanges_.constBegin(); it != pendingChanges_.constEnd(); ++it) {
        const QString& id = it.key();
        const PendingShortcut& change = it.value();
        // When the user edits an upgraded command, retire the old owned field
        // so resetting the new binding cannot resurrect a stale font shortcut.
        // Keep legacy metadata and defaults, as with all other override edits.
        const QString legacy = legacyShortcutId(id);
        if (!legacy.isEmpty()) {
            for (const bool context : {false, true}) {
                QJsonObject& entries = context ? contextual : actions;
                QJsonObject object = entries.value(legacy).toObject();
                if (object.contains(QStringLiteral("shortcut"))) {
                    object.remove(QStringLiteral("shortcut"));
                    entries.insert(legacy, object);
                    (context ? contextualChanged : actionsChanged) = true;
                }
            }
        }
        if (change.reset) {
            // Remove both owned fields so a lower app entry cannot resurrect an
            // old UI override. Metadata and hand-authored defaults survive.
            for (const bool context : {false, true}) {
                QJsonObject& entries = context ? contextual : actions;
                QJsonObject object = entries.value(id).toObject();
                if (object.contains(QStringLiteral("shortcut"))) {
                    object.remove(QStringLiteral("shortcut"));
                    entries.insert(id, object);
                    (context ? contextualChanged : actionsChanged) = true;
                }
            }
            QString fallback = defaultShortcutText(id);
            for (const QJsonObject& entries : {actions, contextual}) {
                const QString candidate = parseShortcutObject(migratedShortcutEntries(entries).value(id).toObject());
                if (!candidate.isEmpty()) {
                    fallback = candidate;
                }
            }
            if (fallback == change.text) {
                continue;
            }
        }
        // contextual is applied after actions by the existing load contract.
        const bool context = contextual.value(id).isObject()
            || (!legacy.isEmpty() && contextual.value(legacy).isObject());
        QJsonObject& entries = context ? contextual : actions;
        QJsonObject object = entries.value(id).toObject();
        object.insert(QStringLiteral("shortcut"), change.text);
        entries.insert(id, object);
        (context ? contextualChanged : actionsChanged) = true;
    }
    if (actionsChanged) {
        root.insert(QStringLiteral("actions"), actions);
    }
    if (contextualChanged) {
        root.insert(QStringLiteral("contextual"), contextual);
    }
    if (!preference_json_file::write(overridePath_, root)) {
        return false;
    }
    pendingChanges_.clear();
    return true;
}
} // namespace miacode::ui
