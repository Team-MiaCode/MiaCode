#pragma once

#include <QCoreApplication>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

namespace miacode::ui::appearance_labels {

inline QVariantList themeOptions()
{
    return {
        QVariantMap{{"value", "dark"}, {"label", qtTrId("dialog.preferences.theme.default_dark")}},
        QVariantMap{{"value", "light"}, {"label", qtTrId("dialog.preferences.theme.default_light")}},
        QVariantMap{{"value", "legacy"}, {"label", qtTrId("dialog.preferences.theme.legacy_dark")}},
        QVariantMap{{"value", "legacy_light"}, {"label", qtTrId("dialog.preferences.theme.legacy_light")}},
        QVariantMap{{"value", "midnight"}, {"label", qtTrId("dialog.preferences.theme.midnight")}},
        QVariantMap{{"value", "forest"}, {"label", qtTrId("dialog.preferences.theme.forest")}},
        QVariantMap{{"value", "paper"}, {"label", qtTrId("dialog.preferences.theme.paper")}},
        QVariantMap{{"value", "lavender"}, {"label", qtTrId("dialog.preferences.theme.lavender")}},
        QVariantMap{{"value", "catppuccin_mocha"}, {"label", qtTrId("dialog.preferences.theme.catppuccin_mocha")}},
        QVariantMap{{"value", "catppuccin_latte"}, {"label", qtTrId("dialog.preferences.theme.catppuccin_latte")}},
        QVariantMap{{"value", "nord"}, {"label", qtTrId("dialog.preferences.theme.nord")}},
        QVariantMap{{"value", "noah"}, {"label", qtTrId("dialog.preferences.theme.noah")}},
        QVariantMap{{"value", "yuzu"}, {"label", qtTrId("dialog.preferences.theme.yuzu")}},
        QVariantMap{{"value", "sky"}, {"label", qtTrId("dialog.preferences.theme.sky")}},
        QVariantMap{{"value", "dracula"}, {"label", qtTrId("dialog.preferences.theme.dracula")}},
        QVariantMap{{"value", "tokyo_night"}, {"label", qtTrId("dialog.preferences.theme.tokyo_night")}},
    };
}

inline QString fontLabel(const QString& family, bool fixedPitch)
{
    return fixedPitch
        ? family + QStringLiteral(" · ") + qtTrId("dialog.preferences.font.monospace") : family;
}

} // namespace miacode::ui::appearance_labels
