#include "core/scene/PreviewHudState.h"

#include <QFontDatabase>
#include <QFontInfo>
#include <QHash>
#include <QFileInfo>
#include <QDateTime>
#include <QMutex>
#include <QMutexLocker>
#include "common/DebugLog.h"
#include <QtMath>

namespace {

QString slideHeadEventKey(const TimelineNoteMarker& marker)
{
    return QStringLiteral("slide_head_star|%1|%2|%3|%4|%5")
        .arg(marker.second, 0, 'f', 6)
        .arg(marker.lane)
        .arg(marker.sourceLine)
        .arg(marker.sourceCol)
        .arg(marker.eachGroupId);
}

double slideJudgeSecondFor(const TimelineNoteMarker& marker)
{
    if (marker.endSecond > marker.second) {
        return marker.endSecond;
    }
    if (marker.slideTraceSecond > marker.second) {
        return marker.slideTraceSecond;
    }
    return marker.second;
}

bool eventPlayed(double second, double judgeSecond)
{
    return judgeSecond <= (second + 1e-6);
}

struct CachedHudFont {
    qint64 modifiedMs = 0;
    qint64 size = -1;
    bool readable = false;
    int fontId = -1;
    QString family;
};

struct HudFontCache {
    QMutex mutex;
    QHash<QString, CachedHudFont> fonts;
};

HudFontCache& hudFontCache()
{
    // Registrations remain alive while their file is unchanged, including while
    // other scene snapshots use them. QFontDatabase owns process shutdown.
    static HudFontCache cache;
    return cache;
}

}  // namespace

namespace miacode::preview::scene {

QString previewHudFontFamilyForFile(const QString& path)
{
    if (path.isEmpty()) return {};
    HudFontCache& cache = hudFontCache();
    const QMutexLocker lock(&cache.mutex);
    const QFileInfo file(path);
    const QString absolute = file.absoluteFilePath();
    const bool readable = file.isFile() && file.isReadable();
    const qint64 modifiedMs = file.lastModified().toMSecsSinceEpoch();
    const qint64 size = readable ? file.size() : -1;
    const auto found = cache.fonts.constFind(absolute);
    if (found != cache.fonts.constEnd() && found->modifiedMs == modifiedMs
        && found->size == size && found->readable == readable) return found->family;

    if (found != cache.fonts.constEnd() && found->fontId >= 0) {
        QFontDatabase::removeApplicationFont(found->fontId);
    }
    CachedHudFont entry;
    entry.modifiedMs = modifiedMs;
    entry.size = size;
    entry.readable = readable;
    if (readable) {
        entry.fontId = QFontDatabase::addApplicationFont(absolute);
        if (entry.fontId >= 0) {
            const QStringList families = QFontDatabase::applicationFontFamilies(entry.fontId);
            entry.family = families.isEmpty() ? QString() : families.first();
        }
    }
    if (entry.family.isEmpty()) {
        if (entry.fontId >= 0) QFontDatabase::removeApplicationFont(entry.fontId);
        entry.fontId = -1;
        miacode::debug_log::appendLine(miacode::debug_log::Channel::Runtime,
            QStringLiteral("hud_font_unavailable"),
            QStringLiteral("path=%1 reason=%2").arg(absolute, readable
                ? QStringLiteral("font-registration-failed") : QStringLiteral("file-unreadable-or-missing")),
            true, miacode::debug_log::Level::Warn);
    }
    cache.fonts.insert(absolute, entry);
    return entry.family;
}

PreviewHudStats computePreviewHudStats(const QVector<TimelineNoteMarker>& noteMarkers, double second)
{
    struct StatsAccumulator {
        int tapTotal = 0;
        int tapPlayed = 0;
        int holdTotal = 0;
        int holdPlayed = 0;
        int slideTotal = 0;
        int slidePlayed = 0;
        int touchTotal = 0;
        int touchPlayed = 0;
        int breakTotal = 0;
        int breakPlayed = 0;
        double finaleBaseTotal = 0.0;
        double finaleCurrent = 0.0;
        double deluxeBaseTotal = 0.0;
        double deluxeBaseCurrent = 0.0;
        int deluxeBreakTotal = 0;
        int deluxeBreakCurrent = 0;
    } stats;

    QHash<QString, bool> slideHeadSeen;
    slideHeadSeen.reserve(noteMarkers.size());

    const auto addNormalEvent = [&stats, second](
        double judgeSecond,
        int finaleScore,
        int deluxeValue,
        int* totalCounter,
        int* playedCounter
    ) {
        const bool played = eventPlayed(second, judgeSecond);
        ++(*totalCounter);
        stats.finaleBaseTotal += static_cast<double>(finaleScore);
        stats.deluxeBaseTotal += static_cast<double>(deluxeValue);
        if (played) {
            ++(*playedCounter);
            stats.finaleCurrent += static_cast<double>(finaleScore);
            stats.deluxeBaseCurrent += static_cast<double>(deluxeValue);
        }
    };

    const auto addBreakEvent = [&stats, second](double judgeSecond) {
        const bool played = eventPlayed(second, judgeSecond);
        ++stats.breakTotal;
        ++stats.deluxeBreakTotal;
        stats.finaleBaseTotal += 2500.0;
        stats.deluxeBaseTotal += 5.0;
        if (played) {
            ++stats.breakPlayed;
            ++stats.deluxeBreakCurrent;
            stats.finaleCurrent += 2600.0;
            stats.deluxeBaseCurrent += 5.0;
        }
    };

    for (const TimelineNoteMarker& marker : noteMarkers) {
        const QString type = marker.type.toLower();
        if (type == QLatin1String("tap")) {
            if (marker.isBreak) {
                addBreakEvent(marker.second);
            } else {
                addNormalEvent(marker.second, 500, 1, &stats.tapTotal, &stats.tapPlayed);
            }
            continue;
        }
        if (type == QLatin1String("hold")) {
            if (marker.isBreak) {
                addBreakEvent(marker.endSecond > marker.second ? marker.endSecond : marker.second);
            } else {
                addNormalEvent(
                    marker.endSecond > marker.second ? marker.endSecond : marker.second,
                    1000,
                    2,
                    &stats.holdTotal,
                    &stats.holdPlayed
                );
            }
            continue;
        }
        if (type == QLatin1String("touch")) {
            if (marker.isBreak) {
                addBreakEvent(marker.second);
            } else {
                addNormalEvent(marker.second, 500, 1, &stats.touchTotal, &stats.touchPlayed);
            }
            continue;
        }
        if (type == QLatin1String("touch_hold")) {
            if (marker.isBreak) {
                addBreakEvent(marker.endSecond > marker.second ? marker.endSecond : marker.second);
            } else {
                addNormalEvent(
                    marker.endSecond > marker.second ? marker.endSecond : marker.second,
                    1000,
                    2,
                    &stats.holdTotal,
                    &stats.holdPlayed
                );
            }
            continue;
        }
        if (type == QLatin1String("slide") || type == QLatin1String("wifi")) {
            if (marker.hasHeadStar) {
                const QString key = slideHeadEventKey(marker);
                if (!slideHeadSeen.contains(key)) {
                    slideHeadSeen.insert(key, true);
                    if (marker.headBreak) {
                        addBreakEvent(marker.second);
                    } else {
                        addNormalEvent(marker.second, 500, 1, &stats.tapTotal, &stats.tapPlayed);
                    }
                }
            }
            const double judgeSecond = slideJudgeSecondFor(marker);
            if (marker.trackBreak) {
                addBreakEvent(judgeSecond);
            } else {
                addNormalEvent(judgeSecond, 1500, 3, &stats.slideTotal, &stats.slidePlayed);
            }
        }
    }

    PreviewHudStats result;
    result.tapPlayed = stats.tapPlayed;
    result.holdPlayed = stats.holdPlayed;
    result.slidePlayed = stats.slidePlayed;
    result.touchPlayed = stats.touchPlayed;
    result.breakPlayed = stats.breakPlayed;
    result.tapTotal = stats.tapTotal;
    result.holdTotal = stats.holdTotal;
    result.slideTotal = stats.slideTotal;
    result.touchTotal = stats.touchTotal;
    result.breakTotal = stats.breakTotal;
    result.combo =
        stats.tapPlayed + stats.holdPlayed + stats.slidePlayed + stats.touchPlayed + stats.breakPlayed;
    result.totalNotes =
        stats.tapTotal + stats.holdTotal + stats.slideTotal + stats.touchTotal + stats.breakTotal;
    result.dxScore = result.combo * 3;
    result.dxScoreMax = result.totalNotes * 3;
    result.finaleRate = stats.finaleBaseTotal > 0.0
        ? (stats.finaleCurrent / stats.finaleBaseTotal) * 100.0
        : 0.0;
    result.deluxeRate = (stats.deluxeBaseTotal > 0.0
        ? (stats.deluxeBaseCurrent / stats.deluxeBaseTotal) * 100.0
        : 0.0)
        + (stats.deluxeBreakTotal > 0
            ? (static_cast<double>(stats.deluxeBreakCurrent) / static_cast<double>(stats.deluxeBreakTotal))
            : 0.0);
    result.deluxeBreakCurrent = stats.deluxeBreakCurrent;
    result.deluxeBreakTotal = stats.deluxeBreakTotal;
    return result;
}

QString formatPreviewHudTimeLabel(double seconds)
{
    const qint64 signedMs = qRound64(seconds * 1000.0);
    const bool negative = signedMs < 0;
    const qint64 totalMs = qAbs(signedMs);
    const qint64 minutes = totalMs / 60000;
    const qint64 sec = (totalMs / 1000) % 60;
    const qint64 ms = totalMs % 1000;
    return QString("%1%2:%3:%4")
        .arg(negative ? QStringLiteral("-") : QString())
        .arg(minutes, 2, 10, QChar('0'))
        .arg(sec, 2, 10, QChar('0'))
        .arg(ms, 3, 10, QChar('0'));
}

QFont previewHudDefaultFontForArea(PreviewHudFontArea area, int pointSize, QFont::Weight weight)
{
    if (area == PreviewHudFontArea::DebugInfo) {
        static const QString embeddedMapleMonoFamily = []() -> QString {
            const int fontId = QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/maple_mono_cn.ttf"));
            const QStringList families = QFontDatabase::applicationFontFamilies(fontId);
            return families.isEmpty() ? QStringLiteral("Maple Mono Normal NL CN") : families.first();
        }();
        QFont font(embeddedMapleMonoFamily);
        font.setPointSize(pointSize);
        font.setWeight(weight);
        font.setStyleHint(QFont::Monospace);
        font.setFixedPitch(true);
        return font;
    }

    static const QString embeddedXiaolaiMonoFamily = []() -> QString {
        const int fontId = QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/xiaolai_mono.ttf"));
        if (fontId < 0) {
            return QString();
        }
        const QStringList families = QFontDatabase::applicationFontFamilies(fontId);
        return families.isEmpty() ? QString() : families.first();
    }();

    QFont font;
    if (!embeddedXiaolaiMonoFamily.isEmpty()) {
        font.setFamily(embeddedXiaolaiMonoFamily);
    } else {
        font.setFamily(QStringLiteral("Xiaolai Mono"));
    }
    if (font.family().isEmpty() || QFontInfo(font).family().compare(font.family(), Qt::CaseInsensitive) != 0) {
        font = previewHudDefaultFontForArea(PreviewHudFontArea::DebugInfo, pointSize, weight);
    } else {
        font.setPointSize(pointSize);
        font.setWeight(weight);
    }
    font.setStyleHint(QFont::Monospace);
    font.setFixedPitch(true);
    return font;
}

QFont previewHudTimestampFontForArea(
    const PreviewHudFontSettings& settings, PreviewHudFontArea area, int pointSize, QFont::Weight weight)
{
    const QString customFamily = previewHudFontFamilyForFile(settings.path(area));
    if (!customFamily.isEmpty()) {
        QFont font(customFamily);
        font.setPointSize(pointSize);
        font.setWeight(weight);
        return font;
    }
    return previewHudDefaultFontForArea(PreviewHudFontArea::Timestamp, pointSize, weight);
}

QFont previewHudMonoFontForArea(
    const PreviewHudFontSettings& settings, PreviewHudFontArea area, int pointSize, QFont::Weight weight)
{
    const QString customFamily = previewHudFontFamilyForFile(settings.path(area));
    if (!customFamily.isEmpty()) {
        QFont font(customFamily);
        font.setPointSize(pointSize);
        font.setWeight(weight);
        return font;
    }

    return previewHudDefaultFontForArea(PreviewHudFontArea::DebugInfo, pointSize, weight);
}

QVector<PreviewHudFontAreaChoice> previewHudFontAreaChoices()
{
    return {
        {PreviewHudFontArea::ChartInfo,
         "dialog.video_export.option.hud_font_area.chart_info",
         "Title / Artist / MASTER 13+ / Designer"},
        {PreviewHudFontArea::Timestamp,
         "dialog.video_export.option.hud_font_area.timestamp",
         "12:34:567"},
        {PreviewHudFontArea::CenterDisplay,
         "dialog.video_export.option.hud_font_area.center_display",
         "ACHIEVEMENT DX  100.0000%"},
        {PreviewHudFontArea::ObjectStats,
         "dialog.video_export.option.hud_font_area.object_stats",
         "DELUXE Rate: 101.0000%  TAP: 128/128"},
        {PreviewHudFontArea::DebugInfo,
         "dialog.video_export.option.hud_font_area.debug",
         "Present: 60.0 FPS  max=17ms"},
    };
}

int previewHudFontAreaId(PreviewHudFontArea area)
{
    return static_cast<int>(area);
}

PreviewHudFontArea previewHudFontAreaFromId(int areaId)
{
    const QVector<PreviewHudFontAreaChoice> choices = previewHudFontAreaChoices();
    for (const PreviewHudFontAreaChoice& choice : choices) {
        if (previewHudFontAreaId(choice.area) == areaId) {
            return choice.area;
        }
    }
    return PreviewHudFontArea::ChartInfo;
}

int previewHudFontAreaIndex(PreviewHudFontArea area)
{
    const QVector<PreviewHudFontAreaChoice> choices = previewHudFontAreaChoices();
    for (int index = 0; index < choices.size(); ++index) {
        if (choices.at(index).area == area) {
            return index;
        }
    }
    return 0;
}

}  // namespace miacode::preview::scene
