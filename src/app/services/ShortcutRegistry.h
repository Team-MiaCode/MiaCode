#pragma once

#include <QHash>
#include <QKeySequence>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

namespace miacode::ui {

class ShortcutRegistry
{
public:
    struct ShortcutDefinition {
        QString id;
        QString labelKey;
        QString labelZh;
        QString labelEn;
        QKeySequence defaultSequence;
        QString defaultShortcutText;
    };

    static ShortcutRegistry& instance();
    // Explicit storage paths allow a host to own a registry without changing
    // the process singleton's portable app/CWD configuration policy.
    explicit ShortcutRegistry(QString overridePath, QString cwdOverridePath = {});

    QKeySequence sequence(const QString& id, const QKeySequence& fallback = QKeySequence()) const;
    QString shortcutText(const QString& id, const QString& fallback = QString()) const;
    QList<ShortcutDefinition> editableShortcuts() const;
    QKeySequence defaultSequence(const QString& id) const;
    QString defaultShortcutText(const QString& id) const;

    bool registerExtensionShortcut(
        const QString& id,
        const QString& label,
        const QKeySequence& defaultSequence);
    bool setUserShortcut(const QString& id, const QKeySequence& sequence);
    // Return whether the input was accepted; persistence failure stays pending
    // while the accepted shortcut takes effect in this session.
    bool setUserShortcutText(const QString& id, const QString& shortcutText);
    bool resetUserShortcut(const QString& id);
    bool resetEditableShortcuts();
    void reload();

private:
    ShortcutRegistry();
    struct PendingShortcut {
        QString text;
        bool reset = false;
    };
    void loadDefaults();
    void loadOverrideFile(const QString& path, bool recoverCorruption = false);
    void mergeJsonObject(const QJsonObject& root);
    void applyRuntimeShortcut(const QString& id, const PendingShortcut& change);
    bool saveUserOverrides();

    QString overridePath_;
    QString cwdOverridePath_;
    bool useCurrentCwd_ = false;
    QHash<QString, PendingShortcut> pendingChanges_;
    QHash<QString, ShortcutDefinition> extensionDefinitions_;

    QHash<QString, ShortcutDefinition> definitions_;
    QHash<QString, QKeySequence> defaultShortcuts_;
    QHash<QString, QKeySequence> shortcuts_;
    QHash<QString, QString> defaultShortcutTexts_;
    QHash<QString, QString> shortcutTexts_;
    QHash<QString, QString> userOverrides_;
    QStringList editableShortcutIds_;
};
} // namespace miacode::ui
