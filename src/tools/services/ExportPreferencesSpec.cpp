#include "app/services/VideoExportPreferences.h"
#include "app/services/CoverExportPreferences.h"
#include "app/services/HudFontPreferences.h"
#include "export/video_export/EncoderProbeCache.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QTemporaryDir>
#include <QTextStream>

namespace {
class MemoryEncoderCache final : public miacode::video_export::EncoderProbeCache {
public:
    QString preferredHardwareEncoder() const override { return codec; }
    void rememberPreferredHardwareEncoder(const QString& value) override { codec = value; }
    QString codec;
};
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir temporary;
    miacode::debug_log::setSessionProjectLogDirectory(temporary.path());
    QTextStream err(stderr);
    bool ok = true;
    const auto expect = [&](bool condition, const char* message) {
        if (!condition) { err << "FAIL: " << message << '\n'; ok = false; }
    };
    QJsonObject section{
        {QStringLiteral("hud_font_paths"), QJsonObject{{QStringLiteral("timestamp"), QStringLiteral("font.ttf")}}},
        {QStringLiteral("unknown_sibling"), QJsonObject{{QStringLiteral("value"), 42}}},
        {QStringLiteral("intro_card_type"), QStringLiteral("standard")},
    };
    const QJsonObject original = section;
    bool failWrites = false;
    miacode::app_preferences::VideoExportPreferences preferences(
        [&] { return section; }, [&](const QJsonObject& value) {
            if (failWrites) return false;
            section = value;
            return true;
        });
    QJsonObject next = preferences.load();
    expect(!next.contains(QStringLiteral("intro_card_type")), "existing migration remains in app adapter");
    next.insert(QStringLiteral("fps"), 60);
    expect(preferences.save(next), "load modify save succeeds");
    expect(section.value(QStringLiteral("hud_font_paths")) == original.value(QStringLiteral("hud_font_paths"))
               && section.value(QStringLiteral("unknown_sibling")) == original.value(QStringLiteral("unknown_sibling")),
           "export task update preserves HUD and unknown sibling fields");
    const QJsonObject successful = section;
    failWrites = true;
    next.insert(QStringLiteral("fps"), 30);
    expect(!preferences.save(next) && section == successful, "adapter propagates storage failure");

    for (const QJsonValue token : {QJsonValue(99), QJsonValue(0), QJsonValue(1.5), QJsonValue(QStringLiteral("future")), QJsonValue(true), QJsonValue(QJsonValue::Null), QJsonValue(QJsonArray{1}), QJsonValue(QJsonObject{{QStringLiteral("v"), 2}})}) {
        QJsonObject stored{{QStringLiteral("schema_version"), token},
                           {QStringLiteral("hud_font_path"), QStringLiteral("historical.ttf")},
                           {QStringLiteral("fps"), 24}};
        const QJsonObject untouched = stored;
        int writes = 0;
        miacode::app_preferences::VideoExportPreferences guarded([&] { return stored; },
            [&](const QJsonObject& value) { ++writes; stored = value; return true; });
        expect(guarded.load() == untouched, "unsupported video schema is not migrated");
        QJsonObject stale{{QStringLiteral("schema_version"), 2}, {QStringLiteral("fps"), 60}};
        expect(!guarded.save(stale) && writes == 0 && stored == untouched,
               "latest unsupported video section blocks a stale current-version save");
        stored = QJsonObject{{QStringLiteral("schema_version"), 2}};
        expect(!guarded.save(untouched) && writes == 0,
               "unsupported incoming video section is never downgraded");
        expect(miacode::hud_preferences::migrateSection(untouched) == untouched
                   && miacode::hud_preferences::withPath(untouched, miacode::preview::scene::PreviewHudFontArea::Timestamp, {}) == untouched,
               "HUD migration and codec preserve unsupported shared video schema");

        QJsonObject cover{{QStringLiteral("version"), token}, {QStringLiteral("vendor"), 7}};
        const QJsonObject originalCover = cover;
        miacode::app_preferences::CoverExportPreferences guardedCover([&] { return cover; },
            [&](const QJsonObject& value) { ++writes; cover = value; return true; });
        const QJsonObject currentCover = miacode::cover_export::CoverCompositionState{}.toJson();
        expect(!guardedCover.savePreferences(currentCover), "latest unsupported cover blocks composition save");
        guardedCover.pushRecentFile(QStringLiteral("layout.miacover"));
        guardedCover.clearRecentFiles();
        guardedCover.saveUserPreset(QStringLiteral("preset"), currentCover);
        guardedCover.removeUserPreset(QStringLiteral("preset"));
        guardedCover.renameUserPreset(QStringLiteral("preset"), QStringLiteral("renamed"));
        expect(writes == 0 && cover == originalCover, "all cover writer paths preserve unsupported section");
        cover = currentCover;
        expect(!guardedCover.savePreferences(originalCover), "unsupported incoming cover is rejected");
        guardedCover.saveUserPreset(QStringLiteral("future"), originalCover);
        expect(writes == 0, "unsupported preset composition is rejected");
        miacode::cover_export::CoverCompositionState parsed;
        QJsonObject unsupportedComposition = currentCover;
        unsupportedComposition.insert(QStringLiteral("version"), token);
        expect(!miacode::cover_export::CoverCompositionState::fromJson(unsupportedComposition, &parsed)
                   && miacode::cover_export::CoverCompositionState::migrateToCurrent(unsupportedComposition) == unsupportedComposition,
               "cover domain rejects unsupported composition without migration");
    }
    QJsonObject latestCover = miacode::cover_export::CoverCompositionState{}.toJson();
    bool coverWriteFails = true;
    miacode::app_preferences::CoverExportPreferences coverWriter([&] { return latestCover; },
        [&](const QJsonObject& value) { if (coverWriteFails) return false; latestCover = value; return true; });
    const QJsonObject pendingComposition = latestCover;
    QJsonObject legacyCover = latestCover;
    legacyCover.insert(QStringLiteral("outputFile"), QStringLiteral("other-chart/cover.png"));
    legacyCover.insert(QStringLiteral("background"), QJsonObject{
        {QStringLiteral("mode"), QStringLiteral("custom")},
        {QStringLiteral("customPath"), QStringLiteral("other-chart/bg.png")},
        {QStringLiteral("brightness"), 0.7}});
    QJsonObject isolatedCover = legacyCover;
    miacode::app_preferences::CoverExportPreferences isolatedWriter([&] { return isolatedCover; },
        [&](const QJsonObject& value) { isolatedCover = value; return true; });
    const QJsonObject sanitized = isolatedWriter.loadPreferences();
    expect(!sanitized.contains(QStringLiteral("outputFile"))
               && sanitized.value("background").toObject().value("mode") == QStringLiteral("jacket")
               && !sanitized.value("background").toObject().contains("customPath"),
           "legacy global paths cannot seed another chart's output or custom background");
    expect(isolatedWriter.savePreferences(legacyCover) && isolatedCover == sanitized,
           "global cover saves exclude project paths while preserving appearance");
    expect(!coverWriter.savePreferences(pendingComposition), "cover persistence failure is returned to dirty session");
    latestCover.insert(QStringLiteral("recentFiles"), QJsonArray{QStringLiteral("new-layout.miacover")});
    latestCover.insert(QStringLiteral("vendor"), 9);
    coverWriteFails = false;
    expect(coverWriter.savePreferences(pendingComposition)
               && latestCover.value("recentFiles").toArray().size() == 1 && latestCover.value("vendor") == 9,
           "deferred composition merges intervening sibling updates at actual save time");

    for (const bool futureWrapper : {false, true}) {
        const QJsonObject futurePreset{{QStringLiteral("name"), QStringLiteral("future")},
            {QStringLiteral("version"), futureWrapper ? 2 : 1},
            {QStringLiteral("composition"), QJsonObject{{QStringLiteral("version"), futureWrapper ? 3 : 4}, {QStringLiteral("future-key"), true}}}};
        QJsonObject presetStore{{QStringLiteral("version"), 3}, {QStringLiteral("presets"), QJsonArray{futurePreset}}};
        int presetWrites = 0;
        miacode::app_preferences::CoverExportPreferences presetWriter([&] { return presetStore; },
            [&](const QJsonObject& value) { ++presetWrites; presetStore = value; return true; });
        presetWriter.saveUserPreset(QStringLiteral("future"), pendingComposition);
        expect(presetWrites == 0 && presetStore.value("presets").toArray().first() == futurePreset,
               "same-name save cannot replace an unsupported existing preset fragment");
        presetWriter.saveUserPreset(QStringLiteral("new-name"), pendingComposition);
        expect(presetWrites == 1 && presetStore.value("presets").toArray().last() == futurePreset,
               "saving another preset preserves the unsupported fragment verbatim");
    }

    using namespace miacode::video_export;
    installEncoderProbeCache({});
    expect(!encoderProbeCache(), "export can run without a machine cache");
    const auto first = std::make_shared<MemoryEncoderCache>();
    installEncoderProbeCache(first);
    const auto inFlight = encoderProbeCache();
    const auto replacement = std::make_shared<MemoryEncoderCache>();
    installEncoderProbeCache(replacement);
    inFlight->rememberPreferredHardwareEncoder(QStringLiteral("h264_nvenc"));
    expect(first->codec == QStringLiteral("h264_nvenc") && replacement->codec.isEmpty(),
           "replacement does not redirect an in-flight cache operation");
    installEncoderProbeCache({});
    expect(!encoderProbeCache() && inFlight->preferredHardwareEncoder() == QStringLiteral("h264_nvenc"),
           "uninstall preserves lifetime of active callers");
    miacode::debug_log::shutdownAsyncLogWriter();
    return ok ? 0 : 1;
}
