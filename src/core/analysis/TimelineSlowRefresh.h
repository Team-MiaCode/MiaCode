#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>

#include "core/analysis/MuriRenderOptions.h"
#include "core/analysis/MuriTypes.h"
#include "core/chart/document/SimaiTimingMetadata.h"
#include "core/chart/parser/SimaiParser.h"
#include "core/chart/model/TimelineData.h"

struct TimelineSlowRefreshRequest {
    quint64 revision = 0;
    int difficultyId = 0;
    QString chartText;
    double firstSeconds = 0.0;
    miacode::simai::SimaiTimingMetadata timingMetadata;
    SimaiValidationLocale validationLocale = SimaiValidationLocale::English;
};

struct TimelinePreviewRefreshResult {
    quint64 revision = 0;
    int difficultyId = 0;
    QString chartText;
    double firstSeconds = 0.0;
    SimaiParseResult parseResult;
    QVector<TimelineBeatMarker> shiftedBeatMarkers;
    QVector<TimelineNoteMarker> shiftedNoteMarkers;
    QByteArray noteMarkerSignature;
    double durationSeconds = 0.0;
};

struct TimelinePreviewRefreshState {
    QVector<TimelineNoteMarker> shiftedNoteMarkers;
    QByteArray noteMarkerSignature;
};

struct TimelineAnalysisRefreshRequest {
    quint64 revision = 0;
    int difficultyId = 0;
    QString chartText;
    SimaiValidationLocale validationLocale = SimaiValidationLocale::English;
    miacode::simai::SimaiTimingMetadata timingMetadata;
    QByteArray noteMarkerSignature;
    QVector<TimelineNoteMarker> noteMarkers;
    MuriRenderOptions renderOptions;
    double staticTapOnSlideThresholdSeconds = 0.0;
};

struct TimelineAnalysisRefreshResult {
    quint64 revision = 0;
    int difficultyId = 0;
    QString chartText;
    SimaiValidationLocale validationLocale = SimaiValidationLocale::English;
    miacode::simai::SimaiTimingMetadata timingMetadata;
    QByteArray noteMarkerSignature;
    SimaiValidationReport validationReport;
    MuriAnalysisReport analysisReport;
    QVector<MuriStaticReference> staticReferences;
};

TimelinePreviewRefreshState buildTimelinePreviewRefreshState(
    const SimaiParseResult& parseResult,
    double firstSeconds);
TimelinePreviewRefreshResult buildTimelinePreviewRefreshResult(
    const TimelineSlowRefreshRequest& request,
    const SimaiParseResult& parseResult,
    const TimelinePreviewRefreshState& previewState);
TimelinePreviewRefreshResult buildTimelinePreviewRefreshResult(const TimelineSlowRefreshRequest& request);
TimelinePreviewRefreshState buildTimelinePreviewRefreshState(const QString& chartText, double firstSeconds);
TimelineAnalysisRefreshResult buildTimelineAnalysisRefreshResult(const TimelineAnalysisRefreshRequest& request);
