#include "app/ui/theme/UiTheme.h"
#include "app/ui/theme/ThemeVariantResolver.h"
#include "app/services/PreferenceDocument.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>

namespace UiTheme {
namespace {
PreferenceDocument::ThemePalette activeThemePalette()
{
    if (PreferenceDocument::preferredTheme() == PreferenceDocument::ThemePreference::Legacy)
        return PreferenceDocument::ThemePalette::Legacy;
    return miacode::ui::ThemeVariantResolver::resolve(PreferenceDocument::preferredTheme())
            == miacode::ui::ThemeVariant::Dark
        ? PreferenceDocument::preferredDarkTheme() : PreferenceDocument::preferredLightTheme();
}

Colors nativeColors(const QVariantMap& theme)
{
    const QVariantMap native = theme.value(QStringLiteral("native")).toMap();
    Colors result;
    result.dark = theme.value(QStringLiteral("dark")).toBool();
    result.windowBg = QColor(native.value(QStringLiteral("windowBg")).toString());
    result.windowAltBg = QColor(native.value(QStringLiteral("windowAltBg")).toString());
    result.toolbarBg = QColor(native.value(QStringLiteral("toolbarBg")).toString());
    result.statusBg = QColor(native.value(QStringLiteral("statusBg")).toString());
    result.panelBg = QColor(native.value(QStringLiteral("panelBg")).toString());
    result.cardBg = QColor(native.value(QStringLiteral("cardBg")).toString());
    result.cardAltBg = QColor(native.value(QStringLiteral("cardAltBg")).toString());
    result.inputBg = QColor(native.value(QStringLiteral("inputBg")).toString());
    result.inputDisabledBg = QColor(native.value(QStringLiteral("inputDisabledBg")).toString());
    result.canvasBg = QColor(native.value(QStringLiteral("canvasBg")).toString());
    result.textPrimary = QColor(native.value(QStringLiteral("textPrimary")).toString());
    result.textSecondary = QColor(native.value(QStringLiteral("textSecondary")).toString());
    result.textMuted = QColor(native.value(QStringLiteral("textMuted")).toString());
    result.textInverse = QColor(native.value(QStringLiteral("textInverse")).toString());
    result.border = QColor(native.value(QStringLiteral("border")).toString());
    result.borderSoft = QColor(native.value(QStringLiteral("borderSoft")).toString());
    result.borderStrong = QColor(native.value(QStringLiteral("borderStrong")).toString());
    result.accent = QColor(native.value(QStringLiteral("accent")).toString());
    result.accentHover = QColor(native.value(QStringLiteral("accentHover")).toString());
    result.accentPressed = QColor(native.value(QStringLiteral("accentPressed")).toString());
    result.accentText = QColor(native.value(QStringLiteral("accentText")).toString());
    result.selection = QColor(native.value(QStringLiteral("selection")).toString());
    result.selectionText = QColor(native.value(QStringLiteral("selectionText")).toString());
    result.menuBg = QColor(native.value(QStringLiteral("menuBg")).toString());
    result.menuBorder = QColor(native.value(QStringLiteral("menuBorder")).toString());
    result.menuHoverBg = QColor(native.value(QStringLiteral("menuHoverBg")).toString());
    result.menuDisabledText = QColor(native.value(QStringLiteral("menuDisabledText")).toString());
    result.scrollTrack = QColor(native.value(QStringLiteral("scrollTrack")).toString());
    result.scrollHandle = QColor(native.value(QStringLiteral("scrollHandle")).toString());
    result.scrollHandleHover = QColor(native.value(QStringLiteral("scrollHandleHover")).toString());
    result.iconPrimary = QColor(native.value(QStringLiteral("iconPrimary")).toString());
    result.iconSecondary = QColor(native.value(QStringLiteral("iconSecondary")).toString());
    return result;
}
} // namespace

const QVariantMap& themeCatalog()
{
    static const QVariantMap catalog = [] {
        QFile file(QStringLiteral(":/themes/palettes.json"));
        if (!file.open(QIODevice::ReadOnly)) qFatal("Bundled theme catalog is missing");
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
        const QVariantMap themes = document.object().value(QStringLiteral("themes")).toObject().toVariantMap();
        if (!themes.contains(QStringLiteral("dark"))) qFatal("Bundled theme catalog is invalid");
        return themes;
    }();
    return catalog;
}

ResolvedTheme resolvedTheme()
{
    return PreferenceDocument::themePaletteIsDark(activeThemePalette()) ? ResolvedTheme::Dark : ResolvedTheme::Light;
}

bool isDarkTheme() { return resolvedTheme() == ResolvedTheme::Dark; }

const Colors& colors()
{
    static const QMap<QString, Colors> palettes = [] {
        QMap<QString, Colors> result;
        const auto& catalog = themeCatalog();
        for (auto it = catalog.cbegin(); it != catalog.cend(); ++it)
            result.insert(it.key(), nativeColors(it.value().toMap()));
        return result;
    }();
    const QString token = PreferenceDocument::themePaletteToken(activeThemePalette());
    auto found = palettes.constFind(token);
    return found != palettes.cend() ? found.value() : palettes.constFind(QStringLiteral("dark")).value();
}
} // namespace UiTheme
