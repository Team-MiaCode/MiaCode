#include "app/services/EditorTypography.h"
#include "app/services/PreferenceDocument.h"
#include "app/ui/theme/UiTheme.h"
#include "app/ui/preferences/AppearanceLabels.h"

#include <QDir>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QTest>
#include <QTextStream>
#include <QTranslator>
#include <cmath>
#include <memory>

namespace {
double luminance(const QColor& color)
{
    auto linear = [](double c) { return c <= .04045 ? c / 12.92 : std::pow((c + .055) / 1.055, 2.4); };
    return .2126 * linear(color.redF()) + .7152 * linear(color.greenF()) + .0722 * linear(color.blueF());
}
double contrast(const QColor& a, const QColor& b)
{
    const double x = luminance(a), y = luminance(b);
    return (qMax(x, y) + .05) / (qMin(x, y) + .05);
}
QQuickItem* findVisualItem(QQuickItem* parent, const QString& name)
{
    if (parent->objectName() == name) return parent;
    for (auto* child : parent->childItems()) {
        if (auto* found = findVisualItem(child, name)) return found;
    }
    return nullptr;
}
}

int main(int argc, char** argv)
{
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
    QGuiApplication app(argc, argv);
    QTextStream out(stdout);
    bool passed = true;
    const auto expect = [&](bool condition, const char* message) {
        if (!condition) { out << "FAIL: " << message << Qt::endl; passed = false; }
    };
    QTranslator translator;
    expect(translator.load(QStringLiteral(":/i18n/en_US.qm")), "compiled English catalog loads");
    app.installTranslator(&translator);
    const auto& catalog = UiTheme::themeCatalog();
    const QStringList tokens{"dark", "light", "legacy", "legacy_light", "midnight", "forest", "paper", "lavender"};
    expect(catalog.size() == tokens.size(), "eight bundled themes");
    const QVariantList options = miacode::ui::appearance_labels::themeOptions();
    for (const QString& token : tokens) {
        const auto palette = PreferenceDocument::themePaletteFromToken(token);
        const QVariantMap theme = catalog.value(token).toMap();
        expect(PreferenceDocument::themePaletteToken(palette) == token, "theme id survives persistence mapping");
        expect(PreferenceDocument::themePaletteIsDark(palette) == theme.value("dark").toBool(), "native and QML appearance agree");
        const QVariantMap colors = theme.value("colors").toMap();
        for (const auto& role : colors) {
            for (const auto& value : role.toMap())
                expect(QColor(value.toString()).isValid(), "every semantic role is a valid color");
        }
        if (tokens.indexOf(token) >= 4) {
            const auto text = colors.value("text").toMap();
            const auto background = colors.value("background").toMap();
            expect(contrast(QColor(text.value("editor").toString()), QColor(background.value("surface").toString())) >= 4.5,
                   "new theme chart text contrast >= 4.5");
            expect(contrast(QColor(text.value("secondary").toString()), QColor(background.value("panel").toString())) >= 4.5,
                   "new theme secondary text contrast >= 4.5");
            if (!theme.value("dark").toBool()) {
                const auto syntax = colors.value("syntax").toMap();
                const QColor accent(colors.value("accent").toMap().value("primary").toString());
                for (const QString role : {QStringLiteral("keyword"), QStringLiteral("duration")}) {
                    const QColor ink(syntax.value(role).toString());
                    expect(ink.lightnessF() < .5
                               && contrast(ink, QColor(background.value("surface").toString())) >= 4.5,
                           "light theme syntax uses contrasting dark ink");
                    const double hueGap = std::abs(ink.hslHueF() - accent.hslHueF());
                    expect(qMin(hueGap, 1 - hueGap) > .13, "syntax hue is distinct from the theme accent");
                }
            }
        }
    }
    const QFont defaultFont = miacode::editor_typography::resolveFont({}, 14);
    // The offscreen platform has no system font database; register a second
    // bundled family to exercise an actual family change deterministically.
    const int alternateFontId = QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/xiaolai_mono.ttf"));
    expect(alternateFontId >= 0, "alternate bundled font loads");
    const QStringList installedFamilies = QFontDatabase::families();
    expect(!installedFamilies.isEmpty(), "installed fonts can be enumerated");
    const QString available = QFontDatabase::applicationFontFamilies(alternateFontId).value(0);
    const QFont customFont = miacode::editor_typography::resolveFont(available, 18);
    out << "font requested=" << available << " resolved=" << customFont.family()
        << " pointSize=" << customFont.pointSize() << Qt::endl;
    expect(customFont.family().compare(available, Qt::CaseInsensitive) == 0 && customFont.pointSize() == 18,
           "installed font family and point size are applied");
    expect(miacode::editor_typography::resolveFont(QStringLiteral("MiaCode missing font 7913"), 14).family() == defaultFont.family(),
           "missing font falls back to the default");

    QTemporaryDir temp;
    const QString path = temp.filePath(QStringLiteral("preferences.json"));
    {
        PreferenceDocument::Repository repository(path);
        QJsonObject root = repository.snapshot();
        QJsonObject ui = root.value("ui").toObject();
        ui.insert("editor_text_font_family", available);
        ui.insert("light_theme", "paper");
        ui.insert("dark_theme", "forest");
        root.insert("ui", ui);
        expect(repository.replace(root), "appearance preferences persist");
    }
    const QJsonObject restored = PreferenceDocument::Repository(path).snapshot().value("ui").toObject();
    expect(restored.value("editor_text_font_family").toString() == available
               && restored.value("light_theme").toString() == "paper"
               && restored.value("dark_theme").toString() == "forest", "appearance choices survive a new repository lifetime");

    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(MIACODE_QML_SPEC_IMPORT_ROOT));
    engine.rootContext()->setContextProperty(QStringLiteral("appearanceThemeOptions"), options);
    engine.rootContext()->setContextProperty(QStringLiteral("appearanceSampleFont"), defaultFont);
    QQmlComponent component(&engine);
    component.setData(R"(
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MiaCode.UI
ApplicationWindow {
    width: 840; height: 360
    color: Theme.colors.background.surface
    QtObject {
        id: previewPreferences
        objectName: "previewPreferences"
        property string activeThemeToken: "dark"
        property string lightThemeToken: "light"
        property string darkThemeToken: "dark"
        property bool darkTheme: Theme.themeCatalog[activeThemeToken].dark
        property bool blurMaterialsEnabled: false
        property string uiFontFamily: Qt.application.font.family
        property font codeFont: appearanceSampleFont
        property int fontSize: 13
        property int uiScalePercent: 100
        property int editorBlockSpacing: 2
    }
    Component.onCompleted: Theme.preferences = previewPreferences
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 16; spacing: 12
        RowLayout {
            Layout.fillWidth: true
            spacing: 16
            ThemePicker {
                objectName: "lightPicker"
                Layout.alignment: Qt.AlignTop
                label: qsTrId("dialog.preferences.theme.light_palette")
                darkPalette: false
                options: appearanceThemeOptions
                currentValue: previewPreferences.lightThemeToken
                onPicked: function(value) {
                    previewPreferences.lightThemeToken = value
                    previewPreferences.activeThemeToken = value
                }
            }
            ThemePicker {
                objectName: "darkPicker"
                Layout.alignment: Qt.AlignTop
                label: qsTrId("dialog.preferences.theme.dark_palette")
                darkPalette: true
                options: appearanceThemeOptions
                currentValue: previewPreferences.darkThemeToken
                onPicked: function(value) {
                    previewPreferences.darkThemeToken = value
                    previewPreferences.activeThemeToken = value
                }
            }
        }
        Text {
            text: "(180){4}1,2,3,4,  || 中文注释 · 日本語"
            font: Theme.codeFont
            color: Theme.colors.text.editor
        }
        Item { Layout.fillHeight: true }
    }
})", QUrl());
    std::unique_ptr<QObject> object(component.create());
    if (!object) { out << component.errorString() << Qt::endl; return 1; }
    auto* window = qobject_cast<QQuickWindow*>(object.get());
    expect(window != nullptr, "theme picker window creates");
    if (!window) return 1;
    window->show();
    QTest::qWait(150);
    auto* preferences = object->findChild<QObject*>(QStringLiteral("previewPreferences"));
    for (const QString& token : tokens) {
        const bool dark = catalog.value(token).toMap().value("dark").toBool();
        auto* matchingPicker = findVisualItem(window->contentItem(), dark ? QStringLiteral("darkPicker") : QStringLiteral("lightPicker"));
        auto* oppositePicker = findVisualItem(window->contentItem(), dark ? QStringLiteral("lightPicker") : QStringLiteral("darkPicker"));
        expect(matchingPicker && oppositePicker, "both appearance groups create");
        if (!matchingPicker || !oppositePicker) continue;
        expect(findVisualItem(oppositePicker, QStringLiteral("themeTile_") + token) == nullptr,
               "palette never appears in the opposite appearance group");
        auto* tile = findVisualItem(matchingPicker, QStringLiteral("themeTile_") + token);
        if (tile) out << "tile=" << token << " size=" << tile->width() << 'x' << tile->height() << Qt::endl;
        expect(tile != nullptr && tile->width() >= 100, "theme tile has a usable hit area");
        if (!tile) continue;
        const QPoint point = tile->mapToScene(QPointF(tile->width() / 2, tile->height() / 2)).toPoint();
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, point);
        QTest::qWait(25);
        expect(preferences->property("activeThemeToken").toString() == token, "click switches the selected theme immediately");
    }
    const QImage image = window->grabWindow();
    expect(!image.isNull(), "theme gallery renders");
    if (argc > 1) expect(image.save(QString::fromLocal8Bit(argv[1])), "theme gallery screenshot saves");
    out << (passed ? "PASS" : "FAIL") << " appearance preferences" << Qt::endl;
    return passed ? 0 : 1;
}
