#pragma once

#include "core/chart/document/ChartClockCount.h"
#include "core/chart/parser/SimaiParser.h"
#include "export/video_export/VideoExportController.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

namespace miacode::chart_banner {

inline QVariantMap loadTemplate()
{
    QFile file(QStringLiteral(":/intro/templates/maimai_banner.json"));
    if (!file.open(QIODevice::ReadOnly)) return {};
    return QJsonDocument::fromJson(file.readAll()).object().toVariantMap();
}

inline QString difficultyAtlasKey(int difficultyId)
{
    switch (difficultyId) {
    case 1: return QStringLiteral("EASY");
    case 2: return QStringLiteral("BASIC");
    case 3: return QStringLiteral("ADVANCED");
    case 4: return QStringLiteral("EXPERT");
    case 5: return QStringLiteral("MASTER");
    case 6: return QStringLiteral("ReMASTER");
    case 7: return QStringLiteral("UTAGE");
    default: return QStringLiteral("MASTER");
    }
}

inline QString bpmText(double bpm)
{
    if (!(bpm > 0.0)) return {};
    QString text = QString::number(bpm, 'f', 2);
    while (text.endsWith(QLatin1Char('0'))) text.chop(1);
    if (text.endsWith(QLatin1Char('.'))) text.chop(1);
    return text;
}

// Shared chart-derived payload for metadata, cover export and video intros.
// Rendering options and media paths remain with each consumer.
inline IntroBannerSpec forDifficulty(const SimaiDocument& document, int difficultyId)
{
    IntroBannerSpec banner;
    const auto* difficulty = document.difficulty(difficultyId);
    const QString chartBody = difficulty != nullptr ? difficulty->chart : QString();
    banner.title = document.title;
    banner.artist = document.artist;
    banner.difficulty = difficultyAtlasKey(difficultyId);
    banner.level = difficulty != nullptr ? difficulty->level : QString();
    banner.designer = document.effectiveDesignerForSlot(difficultyId);
    banner.bpm = bpmText(miacode::chart_clock::clockBpmForChart(document, chartBody));
    banner.mode = chartBody.trimmed().isEmpty() ? QStringLiteral("Standard")
        : detectedIntroBannerMode(SimaiParser::parseForTimeline(
            chartBody, miacode::simai::buildTimingMetadata(document)).noteMarkers);
    return banner;
}

} // namespace miacode::chart_banner
