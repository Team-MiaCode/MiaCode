#pragma once

#include <array>

#include <QFont>
#include <QString>
#include <QStringList>
#include <QVector>

#include "core/chart/model/TimelineData.h"

namespace miacode::preview::scene {

struct PreviewHudStats {
    int tapPlayed = 0;
    int holdPlayed = 0;
    int slidePlayed = 0;
    int touchPlayed = 0;
    int breakPlayed = 0;
    int tapTotal = 0;
    int holdTotal = 0;
    int slideTotal = 0;
    int touchTotal = 0;
    int breakTotal = 0;
    int combo = 0;
    int totalNotes = 0;
    int dxScore = 0;
    int dxScoreMax = 0;
    double finaleRate = 0.0;
    double deluxeRate = 0.0;
    int deluxeBreakCurrent = 0;
    int deluxeBreakTotal = 0;
};

enum class PreviewHudFontArea {
    ChartInfo,
    Timestamp,
    CenterDisplay,
    ObjectStats,
    DebugInfo,
};

// Empty paths select the area's bundled font; configuration belongs to a scene/task.
struct PreviewHudFontSettings {
    std::array<QString, 5> paths;
    bool operator==(const PreviewHudFontSettings&) const = default;
    QString path(PreviewHudFontArea area) const { return paths[static_cast<int>(area)]; }
};

struct PreviewHudFontAreaChoice {
    PreviewHudFontArea area;
    const char* labelKey;
    const char* sample;
};

PreviewHudStats computePreviewHudStats(const QVector<TimelineNoteMarker>& noteMarkers, double second);
QString formatPreviewHudTimeLabel(double seconds);
QString previewHudFontFamilyForFile(const QString& path);
QFont previewHudDefaultFontForArea(
    PreviewHudFontArea area,
    int pointSize,
    QFont::Weight weight = QFont::Medium);
QFont previewHudTimestampFontForArea(
    const PreviewHudFontSettings& settings,
    PreviewHudFontArea area,
    int pointSize,
    QFont::Weight weight = QFont::Medium);
QFont previewHudMonoFontForArea(
    const PreviewHudFontSettings& settings,
    PreviewHudFontArea area,
    int pointSize,
    QFont::Weight weight = QFont::Medium);
QVector<PreviewHudFontAreaChoice> previewHudFontAreaChoices();
int previewHudFontAreaId(PreviewHudFontArea area);
PreviewHudFontArea previewHudFontAreaFromId(int areaId);
int previewHudFontAreaIndex(PreviewHudFontArea area);

}  // namespace miacode::preview::scene
