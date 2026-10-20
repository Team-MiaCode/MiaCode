#include "app/services/PreferenceDocument.h"

#include <QCoreApplication>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTextStream>

namespace {

bool expect(bool condition, const QString& message, QTextStream& err)
{
    if (!condition) {
        err << "FAIL: " << message << '\n';
    }
    return condition;
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTextStream err(stderr);
    bool ok = true;

    QJsonObject raw{
        {QStringLiteral("extensionShortcuts"), QJsonObject{{QStringLiteral("demo.run"), QStringLiteral("Ctrl+R")}}},
        {QStringLiteral("vendor.demo.setting"), 42},
        {QStringLiteral("theme"), QStringLiteral("light")},
        {QStringLiteral("master_volume"), 0.5},
    };
    QJsonObject normalized = PreferenceDocument::normalizePreferencesObject(raw);
    ok &= expect(normalized.value(QStringLiteral("vendor.demo.setting")).toInt() == 42,
                 QStringLiteral("unknown extension-owned top-level keys survive normalization"), err);
    ok &= expect(normalized.value(QStringLiteral("extensionShortcuts")).toObject()
                         .value(QStringLiteral("demo.run")).toString() == QLatin1String("Ctrl+R"),
                 QStringLiteral("extension shortcuts survive normalization"), err);
    ok &= expect(PreferenceDocument::themeTokenFromPreferencesObject(normalized) == QLatin1String("light"),
                 QStringLiteral("legacy top-level theme migrates to ui.theme"), err);
    ok &= expect(!normalized.contains(QStringLiteral("theme"))
                     && !normalized.contains(QStringLiteral("master_volume")),
                 QStringLiteral("migrated built-in top-level keys are removed"), err);

    raw.insert(QStringLiteral("ui"), QJsonObject{{QStringLiteral("theme"), QStringLiteral("dark")}});
    normalized = PreferenceDocument::normalizePreferencesObject(raw);
    ok &= expect(PreferenceDocument::themeTokenFromPreferencesObject(normalized) == QLatin1String("dark"),
                 QStringLiteral("canonical ui.theme wins over legacy top-level theme"), err);

    QJsonObject canonicalAudio{
        {QStringLiteral("global_volume"), 0.9},
        {QStringLiteral("tap_volume"), 0.8},
        {QStringLiteral("touch_volume"), 0.15},
    };
    QJsonObject canonicalPreview{
        {QStringLiteral("background_brightness"), 0.7},
        {QStringLiteral("audio"), canonicalAudio},
    };
    QJsonObject mixed{
        {QStringLiteral("app"), QJsonObject{
             {QStringLiteral("last_open_dir"), QStringLiteral("canonical-dir")},
             {QStringLiteral("preview"), canonicalPreview},
         }},
        {QStringLiteral("last_open_dir"), QStringLiteral("legacy-dir")},
        {QStringLiteral("preview_background_brightness"), 0.1},
        {QStringLiteral("preview_audio"), QJsonObject{
             {QStringLiteral("global_volume"), 0.2},
             {QStringLiteral("slide_volume"), 0.3},
         }},
        {QStringLiteral("master_volume"), 0.4},
        {QStringLiteral("judge_volume"), 0.5},
        {QStringLiteral("touchhold_volume"), 0.95},
    };
    const QJsonObject mixedNormalized = PreferenceDocument::normalizePreferencesObject(mixed);
    const QJsonObject mixedApp = mixedNormalized.value(QStringLiteral("app")).toObject();
    const QJsonObject mixedPreview = mixedApp.value(QStringLiteral("preview")).toObject();
    const QJsonObject mixedAudio = mixedPreview.value(QStringLiteral("audio")).toObject();
    ok &= expect(mixedApp.value(QStringLiteral("last_open_dir")).toString() == QLatin1String("canonical-dir")
                     && qFuzzyCompare(mixedPreview.value(QStringLiteral("background_brightness")).toDouble(), 0.7)
                     && qFuzzyCompare(mixedAudio.value(QStringLiteral("global_volume")).toDouble(), 0.9)
                     && qFuzzyCompare(mixedAudio.value(QStringLiteral("tap_volume")).toDouble(), 0.8)
                     && qFuzzyCompare(mixedAudio.value(QStringLiteral("touch_volume")).toDouble(), 0.15),
                 QStringLiteral("canonical app/preview/audio values win over legacy aliases"), err);
    ok &= expect(qFuzzyCompare(mixedAudio.value(QStringLiteral("slide_volume")).toDouble(), 0.3),
                 QStringLiteral("missing canonical audio members are backfilled from legacy container"), err);

    const QJsonObject legacyTouchNormalized = PreferenceDocument::normalizePreferencesObject(QJsonObject{
        {QStringLiteral("master_volume"), 0.6},
        {QStringLiteral("touch_volume"), 0.2},
        {QStringLiteral("touchhold_volume"), 0.8},
    });
    const QJsonObject legacyTouchAudio = legacyTouchNormalized.value(QStringLiteral("app")).toObject()
        .value(QStringLiteral("preview")).toObject()
        .value(QStringLiteral("audio")).toObject();
    ok &= expect(qFuzzyCompare(legacyTouchAudio.value(QStringLiteral("touch_volume")).toDouble(), 0.8),
                 QStringLiteral("legacy touch and touchhold volumes retain their maximum-value migration"), err);

    QJsonObject written = normalized;
    PreferenceDocument::setThemeTokenInPreferencesObject(&written, QStringLiteral("system"));
    ok &= expect(PreferenceDocument::themeTokenFromPreferencesObject(written) == QLatin1String("system")
                     && !written.contains(QStringLiteral("theme")),
                 QStringLiteral("canonical theme helper writes ui.theme only"), err);

    ok &= expect(PreferenceDocument::themePreferenceFromToken(QStringLiteral("legacy"))
                     == PreferenceDocument::ThemePreference::Legacy
                 && PreferenceDocument::themePreferenceToken(PreferenceDocument::ThemePreference::Legacy)
                        == QLatin1String("legacy"),
                 QStringLiteral("legacy token round-trips without falling back to system"), err);
    ok &= expect(PreferenceDocument::themePreferenceFromToken(QStringLiteral("unknown"))
                     == PreferenceDocument::ThemePreference::System,
                 QStringLiteral("unknown theme tokens still fall back to system"), err);

    QTemporaryDir scaleDirectory;
    ok &= expect(scaleDirectory.isValid(), QStringLiteral("temporary UI scale preferences directory"), err);
    if (scaleDirectory.isValid()) {
        const QString path = scaleDirectory.filePath(QStringLiteral("preferences.json"));
        PreferenceDocument::Repository repository(path);
        QJsonObject scaleRoot = repository.snapshot();
        QJsonObject scaleUi = scaleRoot.value(QStringLiteral("ui")).toObject();
        scaleUi.insert(QStringLiteral("ui_scale_percent"), 137);
        scaleUi.insert(QStringLiteral("editor_text_font_size"), 17);
        scaleRoot.insert(QStringLiteral("ui"), scaleUi);
        ok &= expect(repository.replace(scaleRoot), QStringLiteral("UI scale preferences persist"), err);

        PreferenceDocument::Repository reopened(path);
        QJsonObject reopenedRoot = reopened.snapshot();
        QJsonObject reopenedUi = reopenedRoot.value(QStringLiteral("ui")).toObject();
        ok &= expect(reopenedUi.value(QStringLiteral("ui_scale_percent")).toInt() == 137
                         && reopenedUi.value(QStringLiteral("editor_text_font_size")).toInt() == 17,
                     QStringLiteral("UI scale and editor font size retain independent values across launches"), err);
        reopenedUi.insert(QStringLiteral("ui_scale_percent"), 100);
        reopenedRoot.insert(QStringLiteral("ui"), reopenedUi);
        ok &= expect(reopened.replace(reopenedRoot), QStringLiteral("reset UI scale preferences persist"), err);

        PreferenceDocument::Repository resetLifetime(path);
        const QJsonObject resetUi = resetLifetime.snapshot().value(QStringLiteral("ui")).toObject();
        ok &= expect(resetUi.value(QStringLiteral("ui_scale_percent")).toInt() == 100
                         && resetUi.value(QStringLiteral("editor_text_font_size")).toInt() == 17,
                     QStringLiteral("resetting UI scale preserves the editor font size"), err);
    }

    if (ok) {
        QTextStream(stdout) << "ui_text_preferences_spec ok\n";
        return 0;
    }
    return 1;
}
