#include "app/services/HudFontPreferences.h"
#include "common/PreferenceProvider.h"
#include "export/video_export/VideoExportSnapshot.h"
#include "export/session/PreviewQuickExportSession.h"
#include "export/session/PreviewQuickD3D11ExportSession.h"
#include "preview/runtime/PreviewRuntime.h"
#include "preview/quick_scene/PreviewQuickHudLayer.h"

#include <QGuiApplication>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QPainter>
#include <QTextStream>

using namespace miacode::preview::scene;

namespace {
bool require(bool condition, const char* message, QTextStream& err)
{
    if (!condition) err << "FAIL: " << message << Qt::endl;
    return condition;
}

class AmbientPreferences final : public miacode::preferences::PreferenceProvider {
public:
    QJsonObject values;
    QString resolvedLanguageToken() const override { return {}; }
};

QImage paint(const PreviewFrameState& state)
{
    QImage image(800, 800, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    miacode::preview::hud::paintPreviewHudOverlay(painter, state, image.size());
    return image;
}

bool verifyHudViewportConsistency(QTextStream& err)
{
    PreviewFrameState state;
    state.render.showTimestamp = true;
    state.render.showChartInfoHud = true;
    state.render.showObjectStatsHud = true;
    state.render.showDebugInfo = true;
    state.render.centerDisplayMode = miacode::preview_gameplay::CenterDisplayMode::AchievementDxPlus;
    state.chartTitle = QStringLiteral("愛の残滓");
    state.chartArtist = QStringLiteral("藍月なくる");
    state.chartDifficultyLabel = QStringLiteral("MASTER 13+");
    state.chartDesigner = QStringLiteral("テスト譜師 Agjp");
    state.playheadSeconds = 103.262;
    state.hudStatsSnapshot.combo = 320;
    state.hudStatsSnapshot.totalNotes = 567;
    state.hudStatsSnapshot.tapPlayed = 133;
    state.hudStatsSnapshot.tapTotal = 289;
    state.hudStatsSnapshot.finaleRate = 59.83;
    state.hudStatsSnapshot.deluxeRate = 59.522;
    bool ok = true;
    const auto render = [&](const QSize& logicalSize, int dpi) {
        QImage image(1920, 1080, QImage::Format_ARGB32_Premultiplied);
        image.setDotsPerMeterX(qRound(dpi / 0.0254));
        image.setDotsPerMeterY(qRound(dpi / 0.0254));
        image.fill(Qt::transparent);
        {
            QPainter painter(&image);
            // Compare layout at the same raster resolution, removing only the
            // viewport scale. A narrow pane must retain the export's layout.
            painter.scale(1920.0 / logicalSize.width(), 1080.0 / logicalSize.height());
            const auto transform = painter.worldTransform();
            miacode::preview::hud::paintPreviewHudOverlay(painter, state, logicalSize);
            miacode::preview::hud::paintCenterDisplay(painter, state, logicalSize);
            ok &= require(painter.worldTransform() == transform, "HUD restores caller's transform", err);
        }
        image.setDotsPerMeterX(qRound(96 / 0.0254));
        image.setDotsPerMeterY(qRound(96 / 0.0254));
        return image;
    };
    for (const QString& font : {QString(), QStringLiteral(MIACODE_SOURCE_ROOT "/assets/fonts/MapleMonoNormalNL-CN-Regular.ttf")}) {
        state.hudFontSettings.paths.fill(font);
        const QImage exported = render(QSize(1920, 1080), 96);
        for (const QSize& viewport : {QSize(640, 360), QSize(800, 450), QSize(1280, 720), QSize(3840, 2160)}) {
            for (int dpi : {96, 120, 144}) {
                const bool matches = render(viewport, dpi) == exported;
                if (!matches) err << "viewport=" << viewport.width() << "x" << viewport.height()
                    << " dpi=" << dpi << " font=" << font << Qt::endl;
                ok &= require(matches, "preview HUD layout matches export across viewport sizes and DPI", err);
            }
        }
    }
    return ok;
}

bool verifyHudSpacing(QTextStream& err)
{
    struct Canvas { QSize size; qreal scale; };
    const Canvas canvases[] = {{{1920, 1080}, 1}, {{1280, 720}, 1.5},
        {{1280, 720}, 1}, {{854, 480}, 1.25}, {{1440, 1080}, 1}};
    QStringList fonts = {QString(),
        QStringLiteral(MIACODE_SOURCE_ROOT "/assets/fonts/MapleMonoNormalNL-CN-Regular.ttf")};
#ifdef Q_OS_WIN
    const QString systemFonts = qEnvironmentVariable("WINDIR") + QStringLiteral("/Fonts/");
    for (const auto* name : {"simhei.ttf", "simsun.ttc", "bahnschrift.ttf", "Gabriola.ttf", "javatext.ttf"}) {
        const QString path = systemFonts + QString::fromLatin1(name);
        if (QFileInfo::exists(path)) fonts.append(path);
    }
#endif
    const QString outputDir = qEnvironmentVariable("MIACODE_HUD_SPEC_OUTPUT");
    if (!outputDir.isEmpty()) QDir().mkpath(outputDir);
    bool ok = true;
    for (int f = 0; f < fonts.size(); ++f) {
        for (int c = 0; c < 5; ++c) {
            for (int area = 0; area < 4; ++area) {
                PreviewFrameState frame;
                frame.render.showTimestamp = false;
                frame.render.showChartInfoHud = area == 0;
                frame.render.showObjectStatsHud = area == 1;
                frame.render.showDebugInfo = area >= 2;
                if (area == 3) {
                    frame.media.presentationMode = PreviewStageMediaPresentationMode::ExternalQuickMediaItem;
                    frame.media.externalMediaType = PreviewExternalStageMediaType::Video;
                }
                frame.chartTitle = QStringLiteral("测试谱面 Agjp");
                frame.chartArtist = QStringLiteral("测试曲师 Agjp");
                frame.chartDifficultyLabel = QStringLiteral("MASTER 12+");
                frame.chartDesigner = QStringLiteral("测试谱师 Agjp");
                frame.hudFontSettings.paths.fill(fonts[f]);
                QImage image(canvases[c].size * canvases[c].scale, QImage::Format_ARGB32_Premultiplied);
                image.fill(Qt::transparent);
                {
                    QPainter painter(&image);
                    painter.scale(canvases[c].scale, canvases[c].scale);
                    miacode::preview::hud::paintPreviewHudOverlay(painter, frame, canvases[c].size);
                }
                QVector<QPair<int, int>> bands;
                for (int y = 0; y < image.height(); ++y) {
                    const auto* pixels = reinterpret_cast<const QRgb*>(image.constScanLine(y));
                    bool ink = false;
                    for (int x = 0; x < image.width(); ++x) {
                        if (qAlpha(pixels[x]) >= 16) { ink = true; break; }
                    }
                    if (!ink) continue;
                    if (bands.isEmpty() || y > bands.last().second + 1) bands.append({y, y});
                    else bands.last().second = y;
                }
                const int expected = area == 0 ? 4 : area == 1 ? 10 : area == 2 ? 7 : 12;
                // At 4:3 the designer may wrap onto its permitted second row;
                // statistics can use their five-row, two-column layout.
                const bool rowCountOk = bands.size() == expected
                    || (area == 0 && c == 4 && bands.size() == 5)
                    || (area == 1 && bands.size() == 5);
                if (!rowCountOk) {
                    err << "font=" << fonts[f] << " canvas=" << c << " area=" << area
                        << " ink bands=" << bands.size() << " expected=" << expected << Qt::endl;
                }
                ok &= require(rowCountOk, "HUD rows remain separated in rendered pixels", err);
                int minimumGap = image.height();
                for (int i = 1; i < bands.size(); ++i) {
                    minimumGap = qMin(minimumGap, bands[i].first - bands[i - 1].second - 1);
                }
                ok &= require(minimumGap >= 2, "HUD preserves visible spacing including shadows", err);
                if (area == 1 && bands.size() == 10) {
                    const int groupGap = bands[2].first - bands[1].second - 1;
                    const int ordinaryGap = bands[6].first - bands[5].second - 1;
                    ok &= require(groupGap > ordinaryGap, "statistics retain distinct group spacing", err);
                }
                if (!outputDir.isEmpty()) {
                    image.save(outputDir + QStringLiteral("/font%1-canvas%2-area%3.png").arg(f).arg(c).arg(area));
                }
            }
        }
    }
    return ok;
}

}

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    QTextStream err(stderr);
    bool ok = verifyHudSpacing(err);
    ok &= verifyHudViewportConsistency(err);
    VideoExportSnapshot source;
    source.chartTextUtf8 = QStringLiteral("&title=HUD Spec\n&first=0\n&lv_5=12\n&inote_5=(120){4}1,\n");
    source.difficultyId = 5;
    source.originalChartPath = QStringLiteral("C:/charts/hud/maidata.txt");
    source.outputPath = QStringLiteral("C:/charts/hud/out.mp4");
    source.contentDurationSeconds = 1.0;
    for (int area = 0; area < 5; ++area) source.hudFontSettings.paths[area] = QStringLiteral("C:/fonts/area-%1.ttf").arg(area);
    VideoExportSnapshot restored;
    QString error;
    const bool restoredOk = VideoExportSnapshot::fromJson(source.toJson(), &restored, &error);
    if (!restoredOk) err << "snapshot error: " << error << Qt::endl;
    ok &= require(restoredOk, "snapshot restores", err);
    ok &= require(!source.toJson().value(QStringLiteral("render")).toObject()
        .contains(QStringLiteral("fix_hud_text_layout")), "new snapshots omit obsolete HUD layout flag", err);
    ok &= require(restored.hudFontSettings == source.hudFontSettings, "all five area paths round-trip", err);
    VideoExportTask task;
    const bool taskOk = buildVideoExportTaskFromSnapshot(restored, &task, &error);
    if (!taskOk) err << "worker task error: " << error << Qt::endl;
    ok &= require(taskOk, "worker task reconstructs", err);
    ok &= require(task.hudFontSettings == source.hudFontSettings, "worker receives captured fonts", err);

    AmbientPreferences ambient;
    miacode::preferences::installPreferenceProvider(&ambient);
    ambient.values.insert(QStringLiteral("hud_font_path"), QStringLiteral(MIACODE_SOURCE_ROOT "/assets/fonts/MapleMonoNormalNL-CN-Regular.ttf"));
    QJsonObject legacyJson = source.toJson();
    legacyJson.remove(QStringLiteral("hud_fonts"));
    QJsonObject oldRender = legacyJson.value(QStringLiteral("render")).toObject();
    oldRender.insert(QStringLiteral("fix_hud_text_layout"), true);
    legacyJson.insert(QStringLiteral("render"), oldRender);
    VideoExportSnapshot legacy;
    const bool legacyOk = VideoExportSnapshot::fromJson(legacyJson, &legacy, &error);
    if (!legacyOk) err << "old snapshot error: " << error << Qt::endl;
    ok &= require(legacyOk, "old snapshot restores", err);
    ok &= require(legacy.hudFontSettings == PreviewHudFontSettings{}, "old snapshot uses bundled defaults regardless of ambient preferences", err);

    const QString fontA = QStringLiteral(MIACODE_SOURCE_ROOT "/assets/fonts/XiaolaiMono-Regular.subset.ttf");
    const QString fontB = QStringLiteral(MIACODE_SOURCE_ROOT "/assets/fonts/MapleMonoNormalNL-CN-Regular.ttf");
    PreviewFrameState frame;
    frame.render.showTimestamp = true;
    frame.playheadSeconds = 12.345;
    frame.hudFontSettings.paths[static_cast<int>(PreviewHudFontArea::Timestamp)] = fontA;
    PreviewQuickExportSession gl;
    PreviewQuickD3D11ExportSession d3d;
    gl.setFrameState(frame);
    d3d.setFrameState(frame);
    const QImage before = paint(gl.frameState());
    const QFont fontBefore = previewHudTimestampFontForArea(frame.hudFontSettings, PreviewHudFontArea::Timestamp, 20);
    PreviewRuntime live;
    live.setHudFontSettings(frame.hudFontSettings);
    const auto oldLive = live.frameStateSnapshot();
    frame.hudFontSettings.paths[static_cast<int>(PreviewHudFontArea::Timestamp)] = fontB;
    live.setHudFontSettings(frame.hudFontSettings);
    ambient.values.insert(QStringLiteral("hud_font_paths"), QJsonObject{{QStringLiteral("timestamp"), fontB}});
    gl.applyExportFrameTick(12.345, true, false, false, 0, 60);
    d3d.applyExportFrameTick(12.345, true, false, false, 0, 60);
    ok &= require(gl.frameState().hudFontSettings == oldLive->hudFontSettings
        && d3d.frameState().hudFontSettings == oldLive->hudFontSettings, "both render sessions retain captured fonts after live changes and frame ticks", err);
    ok &= require(live.frameStateSnapshot()->hudFontSettings == frame.hudFontSettings, "live preview accepts new settings independently", err);
    ok &= require(paint(gl.frameState()) == before && paint(d3d.frameState()) == before, "HUD output ignores ambient preference and live scene changes", err);
    ok &= require(fontBefore.family() != previewHudTimestampFontForArea(frame.hudFontSettings, PreviewHudFontArea::Timestamp, 20).family(), "different explicit font configurations resolve independently", err);
    ok &= require(paint(frame) != before, "explicit font changes affect rendered HUD", err);
    miacode::preferences::installPreferenceProvider(nullptr);
    if (ok) QTextStream(stdout) << "video_export_hud_font_snapshot_spec ok" << Qt::endl;
    return ok ? 0 : 1;
}
