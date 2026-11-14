#include "app/ui/chrome/ShortcutModel.h"

#include "app/services/ShortcutRegistry.h"
#include "common/InputShortcutGesture.h"

#include <QKeySequence>

#include <utility>

namespace miacode::ui {
namespace {

QKeySequence resolve(const ShortcutRegistry& registry, const QString& id, const QString& fallback)
{
    const QKeySequence fallbackSequence =
        fallback.isEmpty() ? QKeySequence() : QKeySequence(fallback, QKeySequence::PortableText);
    return registry.sequence(id, fallbackSequence);
}

// The same binding, or one key sequence starting the other: Qt would run neither.
bool bindingsOverlap(const QString& first, const QString& second)
{
    const auto a = miacode::input_shortcut::parseGesture(first);
    const auto b = miacode::input_shortcut::parseGesture(second);
    if (!a.isValid() || !b.isValid()) return false;
    if (a.isWheel() || b.isWheel()) return a.canonicalText == b.canonicalText;
    return a.keySequence.matches(b.keySequence) != QKeySequence::NoMatch
        || b.keySequence.matches(a.keySequence) != QKeySequence::NoMatch;
}

// Preferences spell macOS bindings as "⌘+C" rather than Qt's native "⌘C".
QString preferenceText(const QString& shortcutText)
{
    QString text = miacode::input_shortcut::gestureDisplayText(shortcutText);
#ifdef Q_OS_MACOS
    static const QString modifierSymbols = QString::fromUtf16(u"⌃⌥⇧⌘");
    QString joined;
    for (const QChar symbol : text) {
        joined += symbol;
        if (modifierSymbols.contains(symbol)) joined += QLatin1Char('+');
    }
    text = joined;
#endif
    return text;
}

} // namespace

ShortcutModel::ShortcutModel(QObject* parent)
    : ShortcutModel(ShortcutRegistry::instance(), parent)
{}

ShortcutModel::ShortcutModel(ShortcutRegistry& registry, QObject* parent)
    : QObject(parent), registry_(registry)
{}

qulonglong ShortcutModel::revision() const { return revision_; }

bool ShortcutModel::captureActive() const { return captureActive_; }

void ShortcutModel::setCaptureActive(bool active)
{
    if (captureActive_ == active) return;
    captureActive_ = active;
    emit captureActiveChanged();
}

QString ShortcutModel::sequence(const QString& id, const QString& fallback) const
{
    return resolve(registry_, id, fallback).toString(QKeySequence::PortableText);
}

QString ShortcutModel::gestureText(const QString& id, const QString& fallback) const
{
    return registry_.shortcutText(id, fallback);
}

bool ShortcutModel::overlapsOtherShortcut(const QString& id) const
{
    const QString own = registry_.shortcutText(id);
    for (const ShortcutRegistry::ShortcutDefinition& other : registry_.editableShortcuts()) {
        if (other.id != id && bindingsOverlap(own, registry_.shortcutText(other.id))) return true;
    }
    return false;
}

QString ShortcutModel::reservedShortcutOwner(const QString& shortcutText) const
{
    // Keep in step with the fixed bindings in MainMenu.qml and WindowTitleBar.qml.
    static const std::pair<QKeySequence::StandardKey, const char*> standardKeys[] = {
        {QKeySequence::Save, "action.save"}, {QKeySequence::SaveAs, "action.save_as"},
        {QKeySequence::Close, "qml.close_document"}, {QKeySequence::Quit, "shortcut.file.quit"},
        {QKeySequence::Undo, "qml.undo"}, {QKeySequence::Redo, "action.redo"},
        {QKeySequence::Cut, "action.cut"}, {QKeySequence::Copy, "action.copy"},
        {QKeySequence::Paste, "action.paste"}, {QKeySequence::Find, "metadata.find"},
        {QKeySequence::SelectAll, "net.select_all"},
    };
    for (const auto& [standardKey, labelKey] : standardKeys) {
        for (const QKeySequence& sequence : QKeySequence::keyBindings(standardKey)) {
            if (bindingsOverlap(shortcutText, sequence.toString(QKeySequence::PortableText)))
                return QString::fromLatin1(labelKey);
        }
    }
    struct FixedCommand { const char* id; const char* fallback; const char* labelKey; };
    static const FixedCommand fixedCommands[] = {
        {"file.new", "Ctrl+Shift+N", "action.new"},
        {"file.open", "Ctrl+Shift+O", "action.open"},
        {"file.quit", "Ctrl+Esc", "shortcut.file.quit"},
    };
    for (const FixedCommand& command : fixedCommands) {
        const QString binding = registry_.shortcutText(QString::fromLatin1(command.id),
                                                       QString::fromLatin1(command.fallback));
        if (bindingsOverlap(shortcutText, binding)) return QString::fromLatin1(command.labelKey);
    }
    return {};
}

void ShortcutModel::releaseOverlaps(const QString& id)
{
    // The command that was just given a binding takes it from every other command.
    const QString own = registry_.shortcutText(id);
    for (const ShortcutRegistry::ShortcutDefinition& other : registry_.editableShortcuts()) {
        if (other.id != id && bindingsOverlap(own, registry_.shortcutText(other.id)))
            registry_.unbindUserShortcut(other.id);
    }
}

QString ShortcutModel::displayText(const QString& id, const QString& fallback) const
{
    return resolve(registry_, id, fallback).toString(QKeySequence::NativeText);
}

QString ShortcutModel::nativeText(const QString& shortcutText) const
{
    return preferenceText(shortcutText);
}

QString ShortcutModel::standardDisplayText(int standardKey) const
{
    return QKeySequence(static_cast<QKeySequence::StandardKey>(standardKey))
        .toString(QKeySequence::NativeText);
}

void ShortcutModel::reload()
{
    registry_.reload();
    publishRevision();
}

void ShortcutModel::publishRevision()
{
    ++revision_;
    emit revisionChanged();
}

} // namespace miacode::ui

namespace miacode::ui {

QVariantList ShortcutModel::editableShortcuts() const
{
    QVariantList rows;
    ShortcutRegistry& registry = registry_;
    for (const ShortcutRegistry::ShortcutDefinition& definition : registry.editableShortcuts()) {
        const QString current = registry.shortcutText(definition.id);
        const QString defaults = registry.defaultShortcutText(definition.id);
        QVariantMap row;
        row.insert(QStringLiteral("id"), definition.id);
        // Labels stay raw here. The shell resolves labelKey with qsTrId,
        // falling back to labelEn and then the id.
        row.insert(QStringLiteral("labelKey"), definition.labelKey);
        row.insert(
            QStringLiteral("labelFallback"),
            definition.labelEn.isEmpty() ? definition.id : definition.labelEn);
        row.insert(QStringLiteral("shortcutText"), preferenceText(current));
        row.insert(QStringLiteral("defaultText"), preferenceText(defaults));
        row.insert(QStringLiteral("isDefault"), current == defaults);
        rows.append(row);
    }
    return rows;
}

bool ShortcutModel::setShortcutText(const QString& id, const QString& shortcutText)
{
    if (!reservedShortcutOwner(shortcutText).isEmpty()
        || !registry_.setUserShortcutText(id, shortcutText)) {
        return false;
    }
    releaseOverlaps(id);
    publishRevision();
    return true;
}

void ShortcutModel::resetShortcut(const QString& id)
{
    if (registry_.resetUserShortcut(id)) {
        releaseOverlaps(id);
        publishRevision();
    }
}

QString ShortcutModel::shortcutTextForKeyEvent(int key, int modifiers) const
{
    const auto keyboardModifiers = Qt::KeyboardModifiers(modifiers)
        & (Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier);
    return QKeySequence(QKeyCombination(keyboardModifiers, Qt::Key(key)))
        .toString(QKeySequence::PortableText);
}

void ShortcutModel::resetAllShortcuts()
{
    if (registry_.resetEditableShortcuts()) {
        publishRevision();
    }
}

}  // namespace miacode::ui
