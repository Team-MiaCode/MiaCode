#include "app/services/EditorTypography.h"
#include "app/ui/preferences/AppearanceLabels.h"
#include "app/ui/preferences/LocaleService.h"

#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTest>
#include <QTextStream>
#include <QXmlStreamReader>
#include <memory>

namespace {
QMap<QString, QString> catalogMessages(const QString& language)
{
    QFile file(QStringLiteral(MIACODE_SOURCE_ROOT "/translations/") + language + QStringLiteral(".ts"));
    if (!file.open(QIODevice::ReadOnly)) return {};
    QXmlStreamReader xml(&file);
    QMap<QString, QString> messages;
    QString id;
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement() && xml.name() == QLatin1String("message"))
            id = xml.attributes().value(QStringLiteral("id")).toString();
        else if (xml.isStartElement() && xml.name() == QLatin1String("translation") && !id.isEmpty())
            messages.insert(id, xml.readElementText());
        else if (xml.isEndElement() && xml.name() == QLatin1String("message")) id.clear();
    }
    return xml.hasError() ? QMap<QString, QString>{} : messages;
}

QQuickItem* visualItem(QQuickItem* parent, const QString& name)
{
    if (parent->objectName() == name) return parent;
    for (auto* child : parent->childItems())
        if (auto* found = visualItem(child, name)) return found;
    return nullptr;
}

QQuickItem* comboItem(QQuickItem* parent)
{
    if (parent->metaObject()->indexOfProperty("displayText") >= 0) return parent;
    for (auto* child : parent->childItems())
        if (auto* found = comboItem(child)) return found;
    return nullptr;
}
}

int main(int argc, char** argv)
{
    QStandardPaths::setTestModeEnabled(true);
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("MiaCodeAppearanceTranslationsSpec"));
    QTextStream out(stdout);
    bool passed = true;
    const auto expect = [&](bool condition, const QString& message) {
        if (!condition) { out << "FAIL: " << message << Qt::endl; passed = false; }
    };
    const QFont font = miacode::editor_typography::resolveFont({}, 13);
    auto& locale = miacode::LocaleService::instance();
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(MIACODE_QML_SPEC_IMPORT_ROOT));
    locale.setQmlEngine(&engine);
    locale.setLanguageToken(QStringLiteral("en"));
    engine.rootContext()->setContextProperty(QStringLiteral("translatedThemeOptions"), miacode::ui::appearance_labels::themeOptions());
    engine.rootContext()->setContextProperty(QStringLiteral("sampleFont"), font);
    engine.rootContext()->setContextProperty(QStringLiteral("translatedFonts"), QVariantList{});
    QQmlComponent component(&engine);
    component.setData(R"(
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MiaCode.UI
ApplicationWindow {
    width: 960; height: 440
    color: Theme.colors.background.surface
    QtObject {
        id: preferences
        property string activeThemeToken: "paper"
        property bool darkTheme: false
        property bool blurMaterialsEnabled: false
        property string uiFontFamily: sampleFont.family
        property font codeFont: sampleFont
        property int fontSize: 13
        property int uiScalePercent: 100
        property int editorBlockSpacing: 2
    }
    Component.onCompleted: Theme.preferences = preferences
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 16; spacing: 12
        RowLayout {
            Layout.fillWidth: true; spacing: 16
            ThemePicker {
                Layout.alignment: Qt.AlignTop
                label: qsTrId("dialog.preferences.theme.light_palette")
                options: translatedThemeOptions; darkPalette: false; currentValue: "paper"
            }
            ThemePicker {
                Layout.alignment: Qt.AlignTop
                label: qsTrId("dialog.preferences.theme.dark_palette")
                options: translatedThemeOptions; darkPalette: true; currentValue: "forest"
            }
        }
        LabeledCombo {
            objectName: "fontPreviewRow"
            Layout.fillWidth: true
            label: qsTrId("dialog.preferences.font.family")
            options: translatedFonts; fontFamilyRole: "family"
            currentValue: sampleFont.family
            defaultFontFamily: sampleFont.family
        }
        Text {
            objectName: "localizedComment"
            Layout.fillWidth: true
            text: "(180){4}1,2,3,4,\n|| " + qsTrId("dialog.preferences.font.sample")
            color: Theme.colors.text.editor; font: Theme.codeFont
        }
        Item { Layout.fillHeight: true }
    }
})", QUrl());
    std::unique_ptr<QObject> object(component.create());
    if (!object) { out << component.errorString() << Qt::endl; return 1; }
    auto* window = qobject_cast<QQuickWindow*>(object.get());
    if (!window) return 1;
    window->show();
    QJsonObject results;
    const QStringList languages{"en", "zh", "ja", "en", "ja", "zh"};
    for (const QString& language : languages) {
        const QString catalog = language == "zh" ? QStringLiteral("zh_CN")
            : language == "ja" ? QStringLiteral("ja_JP") : QStringLiteral("en_US");
        const auto expected = catalogMessages(catalog);
        expect(expected.size() > 1000, catalog + QStringLiteral(" catalog contains ID messages"));
        locale.setLanguageToken(language);
        expect(locale.activeLanguageToken() == language, QStringLiteral("production locale service switches to ") + language);
        for (auto it = expected.cbegin(); it != expected.cend(); ++it)
            expect(qtTrId(it.key().toUtf8().constData()) == it.value(), language + QStringLiteral(" compiled catalog ID: ") + it.key());

        const auto themes = miacode::ui::appearance_labels::themeOptions();
        const QString fontLabel = miacode::ui::appearance_labels::fontLabel(font.family(), true);
        expect(fontLabel.endsWith(expected.value(QStringLiteral("dialog.preferences.font.monospace"))),
               language + QStringLiteral(" font suffix is localized"));
        engine.rootContext()->setContextProperty(QStringLiteral("translatedThemeOptions"), themes);
        engine.rootContext()->setContextProperty(QStringLiteral("translatedFonts"), QVariantList{
            QVariantMap{{"value", ""}, {"label", qtTrId("dialog.preferences.font.default")}},
            QVariantMap{{"value", font.family()}, {"label", fontLabel}, {"family", font.family()}}
        });
        QTest::qWait(60);
        QJsonObject labels;
        for (const auto& option : themes) {
            const auto row = option.toMap();
            const QString token = row.value("value").toString();
            auto* label = visualItem(window->contentItem(), QStringLiteral("themeLabel_") + token);
            auto* comment = visualItem(window->contentItem(), QStringLiteral("themeComment_") + token);
            expect(label && label->property("text").toString() == row.value("label").toString(),
                   language + QStringLiteral(" theme card label: ") + token);
            expect(comment && comment->property("text").toString()
                       == QStringLiteral("|| ") + expected.value(QStringLiteral("dialog.preferences.theme.comment_sample")),
                   language + QStringLiteral(" theme comment: ") + token);
            labels.insert(token, row.value("label").toString());
        }
        auto* combo = comboItem(window->contentItem());
        auto* comment = object->findChild<QObject*>(QStringLiteral("localizedComment"));
        expect(combo && combo->property("displayText").toString() == fontLabel,
               language + QStringLiteral(" rendered font option follows the catalog"));
        expect(comment && comment->property("text").toString().endsWith(expected.value(QStringLiteral("dialog.preferences.font.sample"))),
               language + QStringLiteral(" font preview comment follows the runtime language"));
        results.insert(language, QJsonObject{{"themes", labels}, {"fontOption", fontLabel},
            {"comment", expected.value(QStringLiteral("dialog.preferences.font.sample"))}, {"checkedIds", expected.size()}});
        if (argc > 1) {
            const QDir output(QString::fromLocal8Bit(argv[1]));
            const QImage image = window->grabWindow();
            expect(!image.isNull() && image.save(output.filePath(QStringLiteral("appearance-localized-") + language + QStringLiteral(".png"))),
                   language + QStringLiteral(" localized component screenshot saves"));
        }
        out << language << ": " << expected.size() << " compiled IDs and runtime appearance labels checked" << Qt::endl;
    }
    if (argc > 1) {
        QFile report(QDir(QString::fromLocal8Bit(argv[1])).filePath(QStringLiteral("appearance-localization-results.json")));
        expect(report.open(QIODevice::WriteOnly) && report.write(QJsonDocument(results).toJson()) > 0,
               QStringLiteral("localization report saves"));
    }
    locale.clearTranslator();
    out << (passed ? "PASS" : "FAIL") << " appearance translations" << Qt::endl;
    return passed ? 0 : 1;
}
