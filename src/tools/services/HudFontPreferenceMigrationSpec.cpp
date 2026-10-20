#include "app/services/HudFontPreferences.h"
#include "app/services/PreferenceJsonFile.h"

#include <QCoreApplication>
#include <QTemporaryDir>
#include <QTextStream>

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir temporary;
    if (!temporary.isValid()) return 1;
    miacode::debug_log::setSessionProjectLogDirectory(temporary.path());
    QTextStream err(stderr);
    bool ok = true;
    const auto expect = [&](bool condition, const char* message) {
        if (!condition) { err << "FAIL: " << message << Qt::endl; ok = false; }
    };
    using namespace miacode::hud_preferences;
    using namespace miacode::preview::scene;
    const QString legacyFont = temporary.filePath(QStringLiteral("legacy.ttf"));
    const QString regionFont = temporary.filePath(QStringLiteral("region.otf"));
    expect(QFile::copy(QStringLiteral(MIACODE_SOURCE_ROOT "/assets/fonts/XiaolaiMono-Regular.subset.ttf"), legacyFont), "legacy font fixture");
    expect(QFile::copy(QStringLiteral(MIACODE_SOURCE_ROOT "/assets/fonts/MapleMonoNormalNL-CN-Regular.ttf"), regionFont), "region font fixture");
    const QJsonObject legacyOnly{{QStringLiteral("hud_font_path"), legacyFont}, {QStringLiteral("fps"), 120}};
    const QJsonObject expanded = migrateSection(legacyOnly);
    const auto expandedSettings = fromSection(expanded);
    expect(!expanded.contains(QStringLiteral("hud_font_path")) && expanded.value(QStringLiteral("fps")) == 120,
        "migration removes legacy key and preserves export fields");
    for (const auto& choice : previewHudFontAreaChoices()) {
        expect(expandedSettings.path(choice.area) == legacyFont, "legacy-only font expands to all five areas");
    }
    expect(migrateSection(expanded) == expanded, "migration is idempotent");

    QJsonObject mixed = legacyOnly;
    mixed.insert(QStringLiteral("hud_font_paths"), QJsonObject{
        {QStringLiteral("timestamp"), regionFont}, {QStringLiteral("chart_info"), QString()},
        {QStringLiteral("debug_info"), QStringLiteral("missing-explicit.ttf")},
        {QStringLiteral("future_area"), QStringLiteral("untouched")}});
    const QJsonObject migratedMixed = migrateSection(mixed);
    const auto mixedSettings = fromSection(migratedMixed);
    expect(mixedSettings.path(PreviewHudFontArea::Timestamp) == regionFont, "explicit region selection survives migration");
    expect(mixedSettings.path(PreviewHudFontArea::ChartInfo).isEmpty(), "explicit empty region remains bundled default");
    expect(mixedSettings.path(PreviewHudFontArea::DebugInfo).isEmpty(), "invalid explicit region never inherits legacy");
    expect(mixedSettings.path(PreviewHudFontArea::CenterDisplay) == legacyFont
        && mixedSettings.path(PreviewHudFontArea::ObjectStats) == legacyFont, "only missing region choices inherit historical appearance");
    expect(migratedMixed.value(QStringLiteral("hud_font_paths")).toObject().value(QStringLiteral("future_area")) == "untouched",
        "unknown region keys survive migration");
    const QJsonObject reset = withPath(migratedMixed, PreviewHudFontArea::Timestamp, {});
    const auto resetSettings = fromSection(reset);
    expect(resetSettings.path(PreviewHudFontArea::Timestamp).isEmpty()
        && resetSettings.path(PreviewHudFontArea::CenterDisplay) == legacyFont,
        "reset selects only this area's bundled default and preserves other areas");
    expect(migrateSection(reset) == reset && !reset.contains(QStringLiteral("hud_font_path")), "reset cannot resurrect legacy fallback");

    const QJsonObject noLegacy{{QStringLiteral("fps"), 60}};
    expect(migrateSection(noLegacy) == noLegacy, "absent legacy key creates no selections");
    for (const QJsonValue invalid : {QJsonValue(QString()), QJsonValue(QStringLiteral("missing.ttf")), QJsonValue(3)}) {
        QJsonObject bad = noLegacy;
        bad.insert(QStringLiteral("hud_font_path"), invalid);
        expect(migrateSection(bad) == noLegacy, "invalid legacy key is removed without inventing selections");
    }

    const QString repositoryPath = temporary.filePath(QStringLiteral("preferences.json"));
    PreferenceDocument::Repository repository(repositoryPath);
    QJsonObject root = repository.snapshot();
    QJsonObject appValues = root.value(QStringLiteral("app")).toObject();
    appValues.insert(QStringLiteral("video_export"), legacyOnly);
    root.insert(QStringLiteral("app"), appValues);
    expect(repository.replace(root), "persist legacy repository fixture");
    expect(QFile::remove(repositoryPath) && QDir().mkdir(repositoryPath), "block persistence destination");
    appValues.insert(QStringLiteral("video_export"), expanded);
    root.insert(QStringLiteral("app"), appValues);
    expect(!repository.replace(root) && repository.isDirty()
        && repository.snapshot().value(QStringLiteral("app")).toObject().value(QStringLiteral("video_export")).toObject() == expanded,
        "failed migration save keeps pending migrated runtime document");
    expect(QDir().rmdir(repositoryPath) && repository.flush(), "pending migration can be persisted after IO recovery");
    expect(miacode::preference_json_file::read(repositoryPath).object == repository.snapshot(), "recovered disk matches migrated runtime document");
    miacode::debug_log::shutdownAsyncLogWriter();
    if (ok) QTextStream(stdout) << "hud_font_preference_migration_spec ok" << Qt::endl;
    return ok ? 0 : 1;
}
