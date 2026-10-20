#include "app/ui/chrome/ShortcutModel.h"

#include "app/services/ShortcutRegistry.h"

#include <QKeySequence>

namespace miacode::ui {
namespace {

QKeySequence resolve(const ShortcutRegistry& registry, const QString& id, const QString& fallback)
{
    const QKeySequence fallbackSequence =
        fallback.isEmpty() ? QKeySequence() : QKeySequence(fallback, QKeySequence::PortableText);
    return registry.sequence(id, fallbackSequence);
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

QString ShortcutModel::displayText(const QString& id, const QString& fallback) const
{
    return resolve(registry_, id, fallback).toString(QKeySequence::NativeText);
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
        const auto nativeText = [](const QString& text) {
            return QKeySequence(text, QKeySequence::PortableText).toString(QKeySequence::NativeText);
        };
        row.insert(QStringLiteral("shortcutText"), nativeText(current));
        row.insert(QStringLiteral("defaultText"), nativeText(defaults));
        row.insert(QStringLiteral("isDefault"), current == defaults);
        rows.append(row);
    }
    return rows;
}

bool ShortcutModel::setShortcutText(const QString& id, const QString& shortcutText)
{
    if (!registry_.setUserShortcutText(id, shortcutText)) {
        return false;
    }
    publishRevision();
    return true;
}

void ShortcutModel::resetShortcut(const QString& id)
{
    if (registry_.resetUserShortcut(id)) {
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
