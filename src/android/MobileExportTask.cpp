#include "MobileExportTask.h"
#include "common/ChartAssetPaths.h"
#include "common/ChartClockCount.h"
#include "common/ContentDurationConfig.h"
#include "core/chart/parser/SimaiNativeParser.h"
#include "timeline/TimelineMarkerOffset.h"
#include "tools/muri/MuriAnalyzer.h"
#include <QFileInfo>
#include <QtMath>

namespace miacode::android {
QString mobileExportFileStem(const QString& title, const QString& fallback) {
    QString name = title.trimmed();
    for (auto& ch : name) if (ch.unicode() < 32 || QStringLiteral("<>:\"/\\|?*").contains(ch)) ch = QLatin1Char('_');
    while (name.endsWith('.') || name.endsWith(' ')) name.chop(1);
    if (name.isEmpty() || name == "." || name == "..") name = fallback;
    // Leave room for the difficulty, a collision suffix and the extension.
    while (name.toUtf8().size() > 180) name.chop(1);
    if (!name.isEmpty() && name.back().isHighSurrogate()) name.chop(1);
    return name;
}
bool buildMobileChartExportTask(const SimaiDocument& document, int id, const QString& chartPath,
    double trackDuration, const VideoExportTask& settings, VideoExportTask* output,
    QString* error, bool validateSyntax) {
    const auto reject = [error](const QString& message) { if (error) *error = message; return false; };
    if (error) error->clear();
    const auto* difficulty = document.difficulty(id);
    if (!output || !difficulty) return reject(qtTrId("dialog.batch_export.error.missing_requested_difficulty"));
    bool validFirst = false;
    const double first = timeline::offset::parsedFirstSeconds(document.first, &validFirst);
    if (!validFirst) return reject(qtTrId("dialog.batch_export.error.invalid_first"));
    if (!qIsFinite(trackDuration) || trackDuration < 0) return reject(qtTrId("dialog.batch_export.error.invalid_duration"));
    const auto timing = simai::buildTimingMetadata(document);
    if (validateSyntax) {
        const auto diagnostics = SimaiNativeParser::buildValidationReport(difficulty->chart,
            SimaiNativeValidationLocale::Chinese, nullptr, timing);
        if (diagnostics.errorCount) return reject(diagnostics.issues.isEmpty()
            ? qtTrId("dialog.batch_export.error.validation_failed_count").arg(diagnostics.errorCount)
            : qtTrId("dialog.batch_export.error.validation_failed_detail").arg(diagnostics.issues.first().displayMessage));
    }
    const auto parsed = SimaiNativeParser::parseForTimeline(difficulty->chart, timing);
    if (validateSyntax && parsed.noteMarkers.isEmpty()) return reject(qtTrId("dialog.batch_export.error.no_markers"));
    auto task = settings;
    task.chartPath = chartPath;
    task.trackPath = chart_assets::resolveTrackPath(chartPath);
    task.backgroundMediaPath = chart_assets::resolveChartVideoPath(chartPath, document.videoPath);
    task.noteMarkers = timeline::offset::shiftedNoteMarkers(parsed.noteMarkers, first,
        timeline::offset::NonFiniteHandling::Propagate);
    double chartEnd = 0;
    for (const auto& marker : task.noteMarkers) {
        chartEnd = qMax(chartEnd, qMax(qMax(marker.second, marker.endSecond),
            qMax(marker.slideTraceSecond, marker.availableSecond)));
        for (const double second : marker.slideSegmentShootSeconds) chartEnd = qMax(chartEnd, second);
    }
    task.contentDurationSeconds = content_duration::totalContentDurationSeconds(chartEnd, trackDuration);
    task.exportStartSeconds = 0; task.fullRangeExport = true; task.previewMaxOutputSeconds = 0;
    task.chartTitle = document.title; task.chartArtist = document.artist;
    task.chartDesigner = difficulty->designer.trimmed().isEmpty() ? document.designer : difficulty->designer;
    task.chartDifficultyLabel = (SimaiDocument::difficultyShortName(id) + " " + difficulty->level).trimmed();
    task.clockCount = chart_clock::clockCountFromDocument(document);
    task.clockBpm = chart_clock::clockBpmForChart(document, difficulty->chart);
    task.intro.title = document.title; task.intro.artist = document.artist; task.intro.designer = task.chartDesigner;
    task.intro.level = difficulty->level;
    task.intro.difficulty = id == 6 ? QStringLiteral("ReMASTER") : SimaiDocument::difficultyName(id).toUpper();
    task.intro.bpm = QString::number(task.clockBpm, 'g', 12);
    task.intro.jacketPath = chart_assets::resolveDisplayBackgroundImagePath(chartPath);
    task.intro.mode = detectedIntroBannerMode(task.noteMarkers);
    copyIntroStyling(settings.intro, &task.intro);
    task.muriAnalysisReport = MuriAnalyzer::analyze(task.noteMarkers, task.muriRenderOptions,
        task.staticTapOnSlideThresholdSeconds);
    *output = std::move(task);
    return true;
}
}
