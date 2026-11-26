#include "core/analysis/MuriTypes.h"

#include "core/chart/parser/SimaiParser.h"
#include "core/chart/model/TimelineData.h"

#include <QCoreApplication>

namespace {

struct MuriDetailTemplateRow {
    MuriDetailKind kind = MuriDetailKind::None;
    const char* muri[3] = {"", "", ""};
    const char* warning[3] = {"", "", ""};
};

constexpr int kEnglishLocaleIndex = 0;
constexpr int kChineseLocaleIndex = 1;
constexpr int kJapaneseLocaleIndex = 2;

int localeTemplateIndex(SimaiValidationLocale locale)
{
    switch (locale) {
    case SimaiValidationLocale::Chinese:
        return kChineseLocaleIndex;
    case SimaiValidationLocale::Japanese:
        return kJapaneseLocaleIndex;
    case SimaiValidationLocale::English:
    default:
        return kEnglishLocaleIndex;
    }
}

QString protectedPrefixText(SimaiValidationLocale locale)
{
    static const char* kProtectedPrefix[3] = {"protected", "保护", "保護"};
    return QString::fromUtf8(kProtectedPrefix[localeTemplateIndex(locale)]);
}

QString localizeMuriEntityText(QString text, SimaiValidationLocale locale)
{
    text = text.trimmed();
    if (text.isEmpty() || locale == SimaiValidationLocale::English) {
        return text;
    }

    static const QString kProtectedPrefix = QStringLiteral("protected ");
    if (text.startsWith(kProtectedPrefix, Qt::CaseInsensitive)) {
        return QStringLiteral("%1 %2")
            .arg(protectedPrefixText(locale), text.mid(kProtectedPrefix.size()).trimmed());
    }
    return text;
}

QString localizeMuriEntityList(QString text, SimaiValidationLocale locale)
{
    text = text.trimmed();
    if (text.isEmpty() || locale == SimaiValidationLocale::English) {
        return text;
    }

    QStringList parts = text.split(QStringLiteral(", "), Qt::SkipEmptyParts);
    if (parts.isEmpty()) {
        return localizeMuriEntityText(text, locale);
    }
    for (QString& part : parts) {
        part = localizeMuriEntityText(part, locale);
    }
    return parts.join(QStringLiteral(", "));
}

const MuriDetailTemplateRow* detailTemplateRow(MuriDetailKind kind)
{
    static const MuriDetailTemplateRow kRows[] = {
        {
            MuriDetailKind::SlideHeadStartEarlyJudgeTap,
            {
                "%1 start will early-judge next tap (%2)",
                "%1 启动提前判定后续 tap（间隔 %2）",
                "%1 始動で後続 tap を早期判定（間隔 %2）",
            },
            {
                "%1 start may early-judge next tap (%2)",
                "%1 启动可能提前判定后续 tap（间隔 %2）",
                "%1 始動で後続 tap を早期判定の恐れ（間隔 %2）",
            },
        },
        {
            MuriDetailKind::SlideHeadJumpStartEarlyJudgeTap,
            {
                "%1 jump-start will early-judge next tap (%2)",
                "%1 偷跑提前判定后续 tap（间隔 %2）",
                "%1 先行始動で後続 tap を早期判定（間隔 %2）",
            },
            {
                "%1 jump-start may early-judge next tap (%2)",
                "%1 偷跑可能提前判定后续 tap（间隔 %2）",
                "%1 先行始動で後続 tap を早期判定の恐れ（間隔 %2）",
            },
        },
        {
            MuriDetailKind::SlideHeadStartEarlyJudge,
            {
                "%1 start will early-judge %2 (%3)",
                "%1 启动提前判定 %2（间隔 %3）",
                "%1 始動で %2 を早期判定（間隔 %3）",
            },
            {
                "%1 start may early-judge %2 (%3)",
                "%1 启动可能提前判定 %2（间隔 %3）",
                "%1 始動で %2 を早期判定の恐れ（間隔 %3）",
            },
        },
        {
            MuriDetailKind::SlideHeadJumpStartEarlyJudge,
            {
                "%1 jump-start will early-judge %2 (%3)",
                "%1 偷跑提前判定 %2（间隔 %3）",
                "%1 先行始動で %2 を早期判定（間隔 %3）",
            },
            {
                "%1 jump-start may early-judge %2 (%3)",
                "%1 偷跑可能提前判定 %2（间隔 %3）",
                "%1 先行始動で %2 を早期判定の恐れ（間隔 %3）",
            },
        },
        {
            MuriDetailKind::TapOnSlideCollide,
            {
                "%1 trajectory will collide with %2 (%3)",
                "%1 轨迹撞到 %2（间隔 %3）",
                "%1 の軌道が %2 に衝突（間隔 %3）",
            },
            {
                "%1 trajectory may collide with %2 (%3)",
                "%1 轨迹可能撞到 %2（间隔 %3）",
                "%1 の軌道が %2 に衝突の恐れ（間隔 %3）",
            },
        },
        {
            MuriDetailKind::EarlyJudgedBy,
            {
                "%1 early-judged by %2 (%3)",
                "%1 被 %2 提前判定（间隔 %3）",
                "%1 が %2 に早期判定（間隔 %3）",
            },
            {"", "", ""},
        },
        {
            MuriDetailKind::ResolvedOutsideWindow,
            {
                "%1 resolved early (%2)",
                "%1 提前完成（间隔 %2）",
                "%1 が早期完了（間隔 %2）",
            },
            {"", "", ""},
        },
        {
            MuriDetailKind::MultiTouchFormedBy,
            {
                "Multi-touch: %1",
                "%1 构成多押",
                "%1 で多点押し",
            },
            {
                "Possible multi-touch: %1",
                "%1 可能构成多押",
                "%1 で多点押しの恐れ",
            },
        },
        {
            MuriDetailKind::FormedOverlapSamePosition,
            {
                "%1 overlaps %2 at same position",
                "%1 与同位置 %2 叠键",
                "%1 と同位置の %2 が重複",
            },
            {"", "", ""},
        },
        {
            MuriDetailKind::FormedOverlapAtSamePosition,
            {
                "%1 overlaps at same position",
                "%1 同位置叠键",
                "%1 が同位置で重複",
            },
            {"", "", ""},
        },
        {
            MuriDetailKind::SlideHeadTriggerEarlyJudged,
            {
                "Early-judged by %1 slide head",
                "被 %1 滑键头提前判定",
                "%1 のスライド頭で早期判定",
            },
            {"", "", ""},
        },
        {
            MuriDetailKind::PadTriggerEarlyJudged,
            {
                "Early-judged by %1 pad trigger",
                "被 %1 按下提前判定",
                "%1 のパッド入力で早期判定",
            },
            {"", "", ""},
        },
        {
            MuriDetailKind::SimpleNoteMissedWindowOverlap,
            {
                "%1 missed critical window (overlap)",
                "%1 错过临界窗，判为叠键",
                "%1 が臨界ウィンドウ外で重複判定",
            },
            {"", "", ""},
        },
        {
            MuriDetailKind::SlideRuntimeOutsideWindow,
            {
                "Slide %1 outside critical window",
                "Slide %1 超出临界窗",
                "Slide %1 が臨界ウィンドウ外",
            },
            {"", "", ""},
        },
        {
            MuriDetailKind::WifiRuntimeOutsideWindow,
            {
                "Wifi %1 outside critical window",
                "Wifi %1 超出临界窗",
                "Wifi %1 が臨界ウィンドウ外",
            },
            {"", "", ""},
        },
        {
            MuriDetailKind::SlideClearedEarly,
            {
                "Slide %1 cleared early",
                "Slide %1 提前完成",
                "Slide %1 が早期完了",
            },
            {"", "", ""},
        },
        {
            MuriDetailKind::WifiClearedEarly,
            {
                "Wifi %1 cleared early",
                "Wifi %1 提前完成",
                "Wifi %1 が早期完了",
            },
            {"", "", ""},
        },
        {
            MuriDetailKind::StaticReference,
            {
                "Static ref %1 (Δ %2)",
                "静态参考 %1（Δ %2）",
                "静的参照 %1（Δ %2）",
            },
            {"", "", ""},
        },
        {
            MuriDetailKind::RuntimeHandMultiTouch,
            {
                "%1-hand multi-touch: %2",
                "%1 手多押：%2",
                "%1 手多点押し：%2",
            },
            {"", "", ""},
        },
    };

    for (const MuriDetailTemplateRow& row : kRows) {
        if (row.kind == kind) {
            return &row;
        }
    }
    return nullptr;
}

QString noGapResolvedOutsideWindowTemplate(SimaiValidationLocale locale)
{
    static const char* kTemplates[3] = {
        "%1 outside critical window",
        "%1 判定超出临界窗",
        "%1 が臨界ウィンドウ外",
    };
    return QString::fromUtf8(kTemplates[localeTemplateIndex(locale)]);
}

QString earlyJudgedByPerfectWindowTemplate(SimaiValidationLocale locale)
{
    static const char* kTemplates[3] = {
        "%1 early-judged by %2, %3 before standard timing (Perfect tolerance %4)",
        "%1 被 %2 提前判定 %3（Perfect 容差 %4）",
        "%1 が %2 に %3 早期判定（Perfect 許容 %4）",
    };
    return QString::fromUtf8(kTemplates[localeTemplateIndex(locale)]);
}

QString resolvedOutsidePerfectWindowTemplate(SimaiValidationLocale locale)
{
    static const char* kTemplates[3] = {
        "%1 resolved %2 before standard timing (Perfect tolerance %3)",
        "%1 提前判定 %2（Perfect 容差 %3）",
        "%1 が %2 早期判定（Perfect 許容 %3）",
    };
    return QString::fromUtf8(kTemplates[localeTemplateIndex(locale)]);
}

QString staticReferenceNoDeltaTemplate(SimaiValidationLocale locale)
{
    static const char* kTemplates[3] = {
        "Static ref %1",
        "静态参考 %1",
        "静的参照 %1",
    };
    return QString::fromUtf8(kTemplates[localeTemplateIndex(locale)]);
}

QString selectedDetailTemplate(
    const MuriDetailTemplateRow& row,
    MuriAlertLevel alert,
    SimaiValidationLocale locale)
{
    const int index = localeTemplateIndex(locale);
    const char* text = alert == MuriAlertLevel::Warning && row.warning[index][0] != '\0'
        ? row.warning[index]
        : row.muri[index];
    if (text[0] == '\0' && index != kEnglishLocaleIndex) {
        text = alert == MuriAlertLevel::Warning && row.warning[kEnglishLocaleIndex][0] != '\0'
            ? row.warning[kEnglishLocaleIndex]
            : row.muri[kEnglishLocaleIndex];
    }
    return QString::fromUtf8(text);
}

QString localizedLeft(const MuriDetailArgs& args, SimaiValidationLocale locale)
{
    return localizeMuriEntityText(args.left, locale);
}

QString localizedRight(const MuriDetailArgs& args, SimaiValidationLocale locale)
{
    return localizeMuriEntityText(args.right, locale);
}

QString localizedActions(const MuriDetailArgs& args, SimaiValidationLocale locale)
{
    return localizeMuriEntityList(args.actions, locale);
}

}  // namespace

QString makeMarkerAnalysisKey(const TimelineNoteMarker& marker)
{
    QString key = QStringLiteral("%1|%2|%3|%4|%5|%6|%7")
                      .arg(marker.type)
                      .arg(marker.second, 0, 'f', 6)
                      .arg(marker.lane)
                      .arg(marker.endLane)
                      .arg(marker.sourceLine)
                      .arg(marker.sourceCol)
                      .arg(marker.slideTrackKey);
    if (!marker.slideSegmentKeys.isEmpty()) {
        key += QStringLiteral("|%1").arg(marker.slideSegmentKeys.join(QLatin1Char('/')));
    }
    return key;
}

QString muriKindDisplayName(MuriKind kind, bool chineseUi)
{
    switch (kind) {
    case MuriKind::SlideTooFast:
        return chineseUi ? QStringLiteral("内无") : QStringLiteral("Inner");
    case MuriKind::SlideHeadTap:
        return chineseUi ? QStringLiteral("外无") : QStringLiteral("Outer");
    case MuriKind::TapOnSlide:
        return chineseUi ? QStringLiteral("撞尾") : QStringLiteral("Tail");
    case MuriKind::Overlap:
        return chineseUi ? QStringLiteral("叠键") : QStringLiteral("Overlap");
    case MuriKind::MultiTouch:
        return chineseUi ? QStringLiteral("多押") : QStringLiteral("Multi-touch");
    }
    return chineseUi ? QStringLiteral("无理") : QStringLiteral("Muri");
}

QString muriKindText(MuriKind kind)
{
    switch (kind) {
    case MuriKind::SlideTooFast:
        return qtTrId("validation.muri.kind.slide_too_fast");
    case MuriKind::SlideHeadTap:
        return qtTrId("validation.muri.kind.slide_head_tap");
    case MuriKind::TapOnSlide:
        return qtTrId("validation.muri.kind.tap_on_slide");
    case MuriKind::Overlap:
        return qtTrId("validation.muri.kind.overlap");
    case MuriKind::MultiTouch:
        return qtTrId("validation.muri.kind.multi_touch");
    }
    return QString();
}

QString muriDetailKindKey(MuriDetailKind kind)
{
    switch (kind) {
    case MuriDetailKind::None:
        return QStringLiteral("none");
    case MuriDetailKind::SlideHeadStartEarlyJudgeTap:
        return QStringLiteral("slide_head_start_early_judge_tap");
    case MuriDetailKind::SlideHeadJumpStartEarlyJudgeTap:
        return QStringLiteral("slide_head_jump_start_early_judge_tap");
    case MuriDetailKind::SlideHeadStartEarlyJudge:
        return QStringLiteral("slide_head_start_early_judge");
    case MuriDetailKind::SlideHeadJumpStartEarlyJudge:
        return QStringLiteral("slide_head_jump_start_early_judge");
    case MuriDetailKind::TapOnSlideCollide:
        return QStringLiteral("tap_on_slide_collide");
    case MuriDetailKind::EarlyJudgedBy:
        return QStringLiteral("early_judged_by");
    case MuriDetailKind::ResolvedOutsideWindow:
        return QStringLiteral("resolved_outside_window");
    case MuriDetailKind::MultiTouchFormedBy:
        return QStringLiteral("multi_touch_formed_by");
    case MuriDetailKind::FormedOverlapSamePosition:
        return QStringLiteral("formed_overlap_same_position");
    case MuriDetailKind::FormedOverlapAtSamePosition:
        return QStringLiteral("formed_overlap_at_same_position");
    case MuriDetailKind::SlideHeadTriggerEarlyJudged:
        return QStringLiteral("slide_head_trigger_early_judged");
    case MuriDetailKind::PadTriggerEarlyJudged:
        return QStringLiteral("pad_trigger_early_judged");
    case MuriDetailKind::SimpleNoteMissedWindowOverlap:
        return QStringLiteral("simple_note_missed_window_overlap");
    case MuriDetailKind::SlideRuntimeOutsideWindow:
        return QStringLiteral("slide_runtime_outside_window");
    case MuriDetailKind::WifiRuntimeOutsideWindow:
        return QStringLiteral("wifi_runtime_outside_window");
    case MuriDetailKind::SlideClearedEarly:
        return QStringLiteral("slide_cleared_early");
    case MuriDetailKind::WifiClearedEarly:
        return QStringLiteral("wifi_cleared_early");
    case MuriDetailKind::StaticReference:
        return QStringLiteral("static_reference");
    case MuriDetailKind::RuntimeHandMultiTouch:
        return QStringLiteral("runtime_hand_multi_touch");
    }
    return QStringLiteral("none");
}

QString renderMuriDetail(
    MuriDetailKind kind,
    const MuriDetailArgs& args,
    SimaiValidationLocale locale)
{
    if (kind == MuriDetailKind::None) {
        return QString();
    }

    if (kind == MuriDetailKind::ResolvedOutsideWindow && args.gapText.trimmed().isEmpty()) {
        return noGapResolvedOutsideWindowTemplate(locale).arg(localizedLeft(args, locale));
    }
    if (kind == MuriDetailKind::ResolvedOutsideWindow && !args.perfectWindowText.trimmed().isEmpty()) {
        return resolvedOutsidePerfectWindowTemplate(locale)
            .arg(localizedLeft(args, locale), args.gapText, args.perfectWindowText);
    }
    if (kind == MuriDetailKind::EarlyJudgedBy && !args.perfectWindowText.trimmed().isEmpty()) {
        return earlyJudgedByPerfectWindowTemplate(locale)
            .arg(localizedLeft(args, locale), localizedRight(args, locale), args.gapText, args.perfectWindowText);
    }
    if (kind == MuriDetailKind::StaticReference && args.deltaText.trimmed().isEmpty()) {
        return staticReferenceNoDeltaTemplate(locale).arg(localizedLeft(args, locale));
    }

    const MuriDetailTemplateRow* row = detailTemplateRow(kind);
    if (row == nullptr) {
        return QString();
    }

    const QString text = selectedDetailTemplate(*row, args.alert, locale);
    switch (kind) {
    case MuriDetailKind::SlideHeadStartEarlyJudgeTap:
    case MuriDetailKind::SlideHeadJumpStartEarlyJudgeTap:
    case MuriDetailKind::ResolvedOutsideWindow:
        return text.arg(localizedLeft(args, locale), args.gapText);
    case MuriDetailKind::SlideHeadStartEarlyJudge:
    case MuriDetailKind::SlideHeadJumpStartEarlyJudge:
    case MuriDetailKind::TapOnSlideCollide:
    case MuriDetailKind::EarlyJudgedBy:
        return text.arg(localizedLeft(args, locale), localizedRight(args, locale), args.gapText);
    case MuriDetailKind::MultiTouchFormedBy:
    case MuriDetailKind::FormedOverlapAtSamePosition:
    case MuriDetailKind::SlideHeadTriggerEarlyJudged:
    case MuriDetailKind::PadTriggerEarlyJudged:
    case MuriDetailKind::SimpleNoteMissedWindowOverlap:
    case MuriDetailKind::SlideRuntimeOutsideWindow:
    case MuriDetailKind::WifiRuntimeOutsideWindow:
    case MuriDetailKind::SlideClearedEarly:
    case MuriDetailKind::WifiClearedEarly: {
        const QString actions = localizedActions(args, locale);
        return text.arg(actions.isEmpty() ? localizedLeft(args, locale) : actions);
    }
    case MuriDetailKind::FormedOverlapSamePosition:
        return text.arg(localizedLeft(args, locale), localizedRight(args, locale));
    case MuriDetailKind::StaticReference:
        return text.arg(localizedLeft(args, locale), args.deltaText);
    case MuriDetailKind::RuntimeHandMultiTouch:
        return text.arg(args.handCount, localizedActions(args, locale));
    case MuriDetailKind::None:
        return QString();
    }
    return QString();
}

QString renderMuriDiagnosticDetail(
    const MuriDiagnostic& diagnostic,
    SimaiValidationLocale locale)
{
    const QString rendered = renderMuriDetail(diagnostic.detailKind, diagnostic.detailArgs, locale);
    return rendered.isEmpty() ? diagnostic.detail : rendered;
}

QString muriAlertLevelDisplayName(MuriAlertLevel level, bool chineseUi)
{
    switch (level) {
    case MuriAlertLevel::Muri:
        return chineseUi ? QStringLiteral("无理") : QStringLiteral("Muri");
    case MuriAlertLevel::Warning:
        return chineseUi ? QStringLiteral("警告") : QStringLiteral("Warning");
    }
    return chineseUi ? QStringLiteral("无理") : QStringLiteral("Muri");
}
