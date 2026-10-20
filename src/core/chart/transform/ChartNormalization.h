#pragma once

#include <QString>

#include "core/chart/document/SimaiTimingMetadata.h"

namespace miacode::chart_transform {

enum class ChartNormalizationSyntax {
    SegmentPreserving,
    CompactSingleLine,
};

struct ChartNormalizationOptions {
    bool startAtNewMeasure = true;
    bool reduceTo384Grid = true;
    bool splitEveryFourMeasures = true;
    ChartNormalizationSyntax syntax = ChartNormalizationSyntax::SegmentPreserving;
    int sectionMeasureCount = 4;
};

struct ChartNormalizationResult {
    bool ok = false;
    QString text;
    QString errorMessage;
    int changedCount = 0;
    int measureLineCount = 0;
};

ChartNormalizationResult normalizeChartText(
    const QString& input,
    const miacode::simai::SimaiTimingMetadata& timingMetadata = miacode::simai::SimaiTimingMetadata(),
    const ChartNormalizationOptions& options = ChartNormalizationOptions());

ChartNormalizationResult normalizeChartSelectionText(
    const QString& fullText,
    int selectionStart,
    int selectionEnd,
    const miacode::simai::SimaiTimingMetadata& timingMetadata = miacode::simai::SimaiTimingMetadata(),
    const ChartNormalizationOptions& options = ChartNormalizationOptions());

// Normalized output is always whole measure lines. When the replaced selection
// did not begin at a line start, or did not end at a line boundary, splice in
// the separators that keep the surrounding text intact.
QString composeNormalizedSelectionReplacement(
    const QString& original,
    int selectionStart,
    int selectionEnd,
    const QString& normalizedText);

}  // namespace miacode::chart_transform
