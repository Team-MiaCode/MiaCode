#pragma once

#include "core/chart/transform/ChartNormalization.h"
#include <QJsonObject>

namespace miacode::app_preferences {
using miacode::chart_transform::ChartNormalizationOptions;
using miacode::chart_transform::ChartNormalizationSyntax;

inline constexpr auto kChartNormalizeStartAtNewMeasurePreferenceKey =
    "chart_normalize_start_at_new_measure";
inline constexpr auto kChartNormalizeReduceTo384GridPreferenceKey =
    "chart_normalize_reduce_to_384_grid";
inline constexpr auto kChartNormalizeSplitEveryFourMeasuresPreferenceKey =
    "chart_normalize_split_every_four_measures";
inline constexpr auto kChartNormalizeSyntaxPreferenceKey =
    "chart_normalize_syntax";
inline constexpr auto kChartNormalizeSectionMeasureCountPreferenceKey =
    "chart_normalize_section_measure_count";

inline ChartNormalizationOptions chartNormalizationOptionsFromPreferences(
    const QJsonObject& preview,
    const ChartNormalizationOptions& defaults = ChartNormalizationOptions())
{
    ChartNormalizationOptions options = defaults;
    if (preview.value(kChartNormalizeStartAtNewMeasurePreferenceKey).isBool()) {
        options.startAtNewMeasure =
            preview.value(kChartNormalizeStartAtNewMeasurePreferenceKey).toBool(options.startAtNewMeasure);
    }
    if (preview.value(kChartNormalizeReduceTo384GridPreferenceKey).isBool()) {
        options.reduceTo384Grid =
            preview.value(kChartNormalizeReduceTo384GridPreferenceKey).toBool(options.reduceTo384Grid);
    }
    if (preview.value(kChartNormalizeSplitEveryFourMeasuresPreferenceKey).isBool()) {
        options.splitEveryFourMeasures =
            preview.value(kChartNormalizeSplitEveryFourMeasuresPreferenceKey).toBool(options.splitEveryFourMeasures);
    }
    const QString syntax = preview.value(kChartNormalizeSyntaxPreferenceKey).toString().trimmed().toLower();
    if (syntax == QStringLiteral("compact_single_line") || syntax == QStringLiteral("hinata")) {
        options.syntax = ChartNormalizationSyntax::CompactSingleLine;
    } else if (syntax == QStringLiteral("segment_preserving") || syntax == QStringLiteral("fpd")) {
        options.syntax = ChartNormalizationSyntax::SegmentPreserving;
    }
    options.sectionMeasureCount = options.splitEveryFourMeasures ? 4 : 0;
    if (preview.value(kChartNormalizeSectionMeasureCountPreferenceKey).isDouble()) {
        options.sectionMeasureCount = qMax(0, preview.value(kChartNormalizeSectionMeasureCountPreferenceKey).toInt(4));
    }
    return options;
}

inline void saveChartNormalizationOptionsToPreferences(
    QJsonObject* preview,
    const ChartNormalizationOptions& options)
{
    if (preview == nullptr) {
        return;
    }
    preview->insert(kChartNormalizeStartAtNewMeasurePreferenceKey, options.startAtNewMeasure);
    preview->insert(kChartNormalizeReduceTo384GridPreferenceKey, options.reduceTo384Grid);
    preview->insert(kChartNormalizeSplitEveryFourMeasuresPreferenceKey, options.splitEveryFourMeasures);
    preview->insert(
        kChartNormalizeSyntaxPreferenceKey,
        options.syntax == ChartNormalizationSyntax::CompactSingleLine
            ? QStringLiteral("compact_single_line")
            : QStringLiteral("segment_preserving"));
    preview->insert(kChartNormalizeSectionMeasureCountPreferenceKey, options.sectionMeasureCount);
}

} // namespace miacode::app_preferences
