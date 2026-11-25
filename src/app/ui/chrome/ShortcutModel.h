#pragma once

#include <QObject>
#include <QVariantList>
#include <QString>
#include <QStringList>

namespace miacode::ui {

class ShortcutRegistry;

// QML's view of ShortcutRegistry. The registry itself is QtCore + QtGui only —
// its QtWidgets-looking helpers take QAction / QShortcut, both of which moved to
// QtGui in Qt 6 — so v2 reuses it rather than growing a second binding table
// that would drift from the user's shortcuts.json and from v1.
//
// Two spellings of the same binding are exposed because they are not
// interchangeable: QML `Shortcut.sequence` parses portable text ("Ctrl+Z"),
// while a menu row must display the platform spelling ("⌘Z" on macOS).
class ShortcutModel final : public QObject
{
    Q_OBJECT
    // Bumped when the registry reloads, so every QML binding that resolved a
    // sequence re-evaluates after the user edits their shortcuts.
    Q_PROPERTY(qulonglong revision READ revision NOTIFY revisionChanged)
    Q_PROPERTY(bool captureActive READ captureActive WRITE setCaptureActive NOTIFY captureActiveChanged)

public:
    explicit ShortcutModel(QObject* parent = nullptr);
    explicit ShortcutModel(ShortcutRegistry& registry, QObject* parent = nullptr);

    qulonglong revision() const;
    bool captureActive() const;
    void setCaptureActive(bool active);

    // Portable text for `Shortcut { sequence: ... }`. Empty when the id is
    // unknown and no fallback is given, which leaves the Shortcut inert rather
    // than binding something arbitrary.
    Q_INVOKABLE QString sequence(const QString& id, const QString& fallback = QString()) const;
    // Unlike sequence(), this preserves wheel gestures for non-keyboard consumers.
    Q_INVOKABLE QString gestureText(const QString& id, const QString& fallback = QString()) const;
    // True when another editable command starts with, or is a prefix of, this binding.
    // Qt would treat both as ambiguous and run neither.
    Q_INVOKABLE bool overlapsOtherShortcut(const QString& id) const;
    // Label key of the fixed file/edit command (MainMenu, WindowTitleBar) whose binding
    // overlaps this one, or empty. Editable commands may not take these bindings.
    Q_INVOKABLE QString reservedShortcutOwner(const QString& shortcutText) const;
    // Platform spelling for menu rows and tooltips.
    Q_INVOKABLE QString displayText(const QString& id, const QString& fallback = QString()) const;
    // Preferences spelling of a portable binding, such as a key just recorded:
    // the platform text, with "+" after each macOS modifier symbol ("⌘+C").
    Q_INVOKABLE QString nativeText(const QString& shortcutText) const;
    // Menu rows whose binding is a Qt standard key (Open/Save/Undo/...) rather
    // than a registry id still need the platform spelling to display.
    Q_INVOKABLE QString standardDisplayText(int standardKey) const;
    Q_INVOKABLE void reload();

    // Shortcut editing. editableShortcuts() returns one row per command. Assigning or
    // resetting a binding unbinds every other command that overlaps it.
    Q_INVOKABLE QVariantList editableShortcuts() const;
    Q_INVOKABLE bool setShortcutText(const QString& id, const QString& shortcutText);
    Q_INVOKABLE void resetShortcut(const QString& id);
    Q_INVOKABLE void resetAllShortcuts();
    // Converts a captured key event into Qt's portable storage syntax.
    Q_INVOKABLE QString shortcutTextForKeyEvent(int key, int modifiers) const;

signals:
    void revisionChanged();
    void captureActiveChanged();

private:
    void publishRevision();
    void releaseOverlaps(const QString& id);
    ShortcutRegistry& registry_;
    qulonglong revision_ = 1;
    bool captureActive_ = false;
};

} // namespace miacode::ui
