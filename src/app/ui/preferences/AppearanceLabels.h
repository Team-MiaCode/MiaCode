#pragma once

#include <QCoreApplication>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

namespace miacode::ui::appearance_labels {

inline QVariantList themeOptions()
{
    return {
        QVariantMap{{"value", "dark"}, {"label", qtTrId("dialog.preferences.theme.default")}},
        QVariantMap{{"value", "light"}, {"label", qtTrId("dialog.preferences.theme.default")}},
        QVariantMap{{"value", "legacy"}, {"label", qtTrId("dialog.preferences.theme.legacy")}},
        QVariantMap{{"value", "legacy_light"}, {"label", qtTrId("dialog.preferences.theme.legacy")}},
        QVariantMap{{"value", "midnight"}, {"label", qtTrId("dialog.preferences.theme.midnight")}},
        QVariantMap{{"value", "forest"}, {"label", qtTrId("dialog.preferences.theme.forest")}},
        QVariantMap{{"value", "paper"}, {"label", qtTrId("dialog.preferences.theme.paper")}},
        QVariantMap{{"value", "lavender"}, {"label", qtTrId("dialog.preferences.theme.lavender")}},
    };
}

inline QString fontLabel(const QString& family, bool fixedPitch)
{
    return fixedPitch
        ? family + QStringLiteral(" · ") + qtTrId("dialog.preferences.font.monospace") : family;
}

} // namespace miacode::ui::appearance_labels
