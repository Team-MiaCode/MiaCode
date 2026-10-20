#pragma once

#include <QVector>

#include "core/analysis/MuriConfig.h"
#include "core/analysis/MuriRenderOptions.h"
#include "core/analysis/MuriTypes.h"

struct TimelineNoteMarker;

class MuriAnalyzer
{
public:
    static MuriAnalysisReport analyze(
        const QVector<TimelineNoteMarker>& noteMarkers,
        const MuriRenderOptions& renderOptions = {});
    static MuriAnalysisReport analyze(
        const QVector<TimelineNoteMarker>& noteMarkers,
        const MuriRenderOptions& renderOptions,
        double staticTapOnSlideThresholdSeconds);
};
