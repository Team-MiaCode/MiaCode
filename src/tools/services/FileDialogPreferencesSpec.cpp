#include "app/services/FileDialogPreferences.h"

#include <QCoreApplication>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTextStream>

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);
    app.setApplicationName(QStringLiteral("MiaCodeFileDialogPreferencesSpec"));
    namespace history = miacode::file_dialog_preferences;
    using Purpose = history::Purpose;
    QTemporaryDir temp;
    bool ok = true;
    const auto expect = [&](bool condition, const char* message) {
        if (!condition) { QTextStream(stderr) << "FAIL: " << message << Qt::endl; ok = false; }
    };
    expect(temp.isValid(), "isolated directories exist");
    if (!temp.isValid()) return 1;
    PreferenceDocument::savePreferencesObject({{"app", QJsonObject{{"unrelated", 42},
        {"file_dialogs", QJsonObject{{"future_purpose", "preserved"}}}}}});
    const QList<Purpose> purposes{Purpose::IntroSound,
        Purpose::IntroFont, Purpose::CoverFont, Purpose::HudFont,
        Purpose::CoverLayout, Purpose::PvBatchRoot};
    for (const auto purpose : purposes) {
        const QString directory = temp.filePath(history::key(purpose));
        QDir().mkpath(directory);
        expect(history::recentDirectory(purpose, temp.path()) == temp.path(), "missing history uses the caller context");
        expect(purpose == Purpose::PvBatchRoot ? history::rememberDirectory(purpose, directory)
            : history::rememberFile(purpose, QDir(directory).filePath("selected file #1")),
            "a local accepted selection is remembered");
    }
    const auto beforeCancel = PreferenceDocument::loadPreferencesObject();
    for (const auto purpose : purposes) {
        expect(!history::rememberFile(purpose, {}) && !history::rememberDirectory(purpose, {}),
               "cancellation does not choose the process directory");
        expect(history::recentDirectory(purpose) == temp.filePath(history::key(purpose)),
               "each purpose restores its own directory");
    }
    expect(PreferenceDocument::loadPreferencesObject() == beforeCancel, "cancellation does not write any history");
    PreferenceDocument::Repository reopened(PreferenceDocument::preferencesFilePath());
    const auto storedApp = reopened.snapshot().value("app").toObject();
    for (const auto purpose : purposes)
        expect(storedApp.value("file_dialogs").toObject().value(history::key(purpose)).toString()
            == temp.filePath(history::key(purpose)), "all purposes survive a disk reload");
    expect(storedApp.value("unrelated").toInt() == 42
        && storedApp.value("file_dialogs").toObject().value("future_purpose").toString() == "preserved",
        "history writes preserve unrelated settings and future purposes");
    expect(!history::rememberFile(Purpose::IntroFont, "relative.ttf")
        && !history::rememberDirectory(Purpose::IntroFont, temp.filePath("missing")),
        "invalid choices cannot overwrite a valid location");
    QDir(temp.filePath(history::key(Purpose::IntroFont))).removeRecursively();
    expect(history::recentDirectory(Purpose::IntroFont, temp.path()) == temp.path(),
           "deleted history falls back to an existing context");
    expect(history::recentDirectory(Purpose::IntroFont, temp.filePath("missing")) == QDir::homePath(),
           "missing history and context fall back to home");
    expect(history::documentDirectory(temp.filePath("missing"), temp.filePath("chart.txt")) == temp.path(),
           "document history invalidation preserves the chart context");
    return ok ? 0 : 1;
}
