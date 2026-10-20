#include "core/analysis/MuriSimpleNoteJudge.h"

#include "common/TaskCancellation.h"

#include <algorithm>

#include <QHash>
#include <QMap>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

#include "core/chart/parser/SimaiParser.h"
#include "core/chart/model/TimelineData.h"
#include "core/analysis/MuriConfig.h"
#include "core/analysis/MuriTypes.h"
#include "core/analysis/MuriAnalyzerGeometry.h"     // pointDistance
#include "core/analysis/MuriAnalyzerInternal.h"     // timing / hand-action / pad-event / diagnostic-sink helpers
#include "core/analysis/MuriAnalyzerModel.h"
#include "core/analysis/MuriDiagnosticLabels.h"     // config labels / source anchors / alert text
#include "core/analysis/MuriRuntimeModelBuilder.h"  // buildNoteExpiryBuckets + the Stage-1 model builders
#include "core/analysis/MuriStaticChecker.h"

namespace miacode::muri::detail {

constexpr double kPadTimeEpsilon = 1e-6;

bool judgeSimpleNoteOnPadDown(
    JudgeableSimpleNote* note,
    double nowSecond,
    const QString& pad,
    const RuntimePadEvent* cause)
{
    if (note == nullptr || note->judged) {
        return false;
    }

    const double deltaSecond = nowSecond - note->momentSecond;
    if (deltaSecond < -note->availableSeconds - kPadTimeEpsilon) {
        return false;
    }
    if (pad != note->pad) {
        return false;
    }

    note->judged = true;
    note->judgeSecond = nowSecond;
    note->judgeBad = qAbs(deltaSecond) > note->criticalSeconds + kPadTimeEpsilon;
    if (cause != nullptr) {
        note->cause = *cause;
    } else {
        note->cause = RuntimePadEvent{};
        note->cause.pad = pad;
        note->cause.second = nowSecond;
    }
    return true;
}

void updateSimpleNoteLateBad(JudgeableSimpleNote* note, double nowSecond)
{
    if (note == nullptr || note->judged) {
        return;
    }
    if (nowSecond - note->momentSecond <= note->availableSeconds + kPadTimeEpsilon) {
        return;
    }

    note->judged = true;
    note->judgeBad = true;
    note->lateBad = true;
    note->judgeSecond = nowSecond;
}

bool findOccupyingPadCause(
    const QVector<MuriPadWindow>& padWindows,
    const QHash<QString, MarkerSourceRef>& markerRefs,
    const QString& pad,
    double momentSecond,
    const QString& excludeMarkerKey,
    RuntimePadEvent* cause)
{
    if (cause == nullptr || pad.isEmpty()) {
        return false;
    }

    int bestIndex = -1;
    for (int index = 0; index < padWindows.size(); ++index) {
        miacode::task::CancellationScope::checkpoint();
        const MuriPadWindow& window = padWindows.at(index);
        if (normalizedPadToken(window.pad) != normalizedPadToken(pad)) {
            continue;
        }
        if (window.sourceMarkerKey == excludeMarkerKey) {
            continue;
        }
        if (momentSecond + kPadTimeEpsilon < window.startSecond
            || momentSecond > window.endSecond + kPadTimeEpsilon) {
            continue;
        }

        if (bestIndex < 0) {
            bestIndex = index;
            continue;
        }

        const MuriPadWindow& best = padWindows.at(bestIndex);
        const bool candidateSlideLike =
            window.sourceType == QLatin1String("slide") || window.sourceType == QLatin1String("wifi");
        const bool bestSlideLike =
            best.sourceType == QLatin1String("slide") || best.sourceType == QLatin1String("wifi");
        if (candidateSlideLike != bestSlideLike) {
            if (candidateSlideLike) {
                bestIndex = index;
            }
            continue;
        }
        if (window.startSecond + kPadTimeEpsilon < best.startSecond) {
            bestIndex = index;
            continue;
        }
        if (best.startSecond + kPadTimeEpsilon < window.startSecond) {
            continue;
        }
        const MarkerSourceRef candidateRef = markerRefs.value(window.sourceMarkerKey);
        const MarkerSourceRef bestRef = markerRefs.value(best.sourceMarkerKey);
        if (candidateRef.order > bestRef.order) {
            bestIndex = index;
        }
    }

    if (bestIndex < 0) {
        return false;
    }

    const MuriPadWindow& window = padWindows.at(bestIndex);
    const MarkerSourceRef ref = markerRefs.value(window.sourceMarkerKey);
    cause->pad = normalizedPadToken(window.pad);
    cause->tick = judgeTickForPadActiveStart(window.startSecond);
    cause->second = window.startSecond;
    cause->sourceMarkerKey = window.sourceMarkerKey;
    cause->sourceType = window.sourceType;
    cause->sourceOrder = ref.order;
    cause->line = ref.line;
    cause->col = ref.col;
    cause->extraPadDown = false;
    return true;
}

bool matchesStaticSlideEndOverlap(
    const QVector<MuriStaticReference>& staticReferences,
    const JudgeableSimpleNote& note,
    const RuntimePadEvent& cause)
{
    if (cause.sourceMarkerKey.isEmpty()
        || (cause.sourceType != QLatin1String("slide")
            && cause.sourceType != QLatin1String("wifi"))) {
        return false;
    }

    for (const MuriStaticReference& reference : staticReferences) {
        miacode::task::CancellationScope::checkpoint();
        if (reference.kind != MuriKind::Overlap
            || reference.affected.markerKey != note.markerKey
            || reference.cause.markerKey != cause.sourceMarkerKey) {
            continue;
        }
        return true;
    }
    return false;
}

bool consumeRuntimePadEvent(
    QVector<JudgeableSimpleNote>* notes,
    const QVector<RuntimeTouchGroup>& touchGroups,
    const RuntimeTopLevelNote& topLevel,
    double nowSecond,
    const RuntimePadEvent& event)
{
    if (notes == nullptr) {
        return false;
    }

    if (topLevel.touchGroupIndex >= 0) {
        if (topLevel.touchGroupIndex < 0 || topLevel.touchGroupIndex >= touchGroups.size()) {
            return false;
        }

        const RuntimeTouchGroup& group = touchGroups.at(topLevel.touchGroupIndex);
        for (int childNoteIndex : group.childNoteIndices) {
            miacode::task::CancellationScope::checkpoint();
            if (childNoteIndex < 0 || childNoteIndex >= notes->size()) {
                continue;
            }
            if (judgeSimpleNoteOnPadDown(&(*notes)[childNoteIndex], nowSecond, event.pad, &event)) {
                return true;
            }
        }
        return false;
    }

    if (topLevel.simpleNoteIndex < 0 || topLevel.simpleNoteIndex >= notes->size()) {
        return false;
    }
    return judgeSimpleNoteOnPadDown(&(*notes)[topLevel.simpleNoteIndex], nowSecond, event.pad, &event);
}

void updateRuntimeTopLevelNote(
    QVector<JudgeableSimpleNote>* notes,
    const QVector<RuntimeTouchGroup>& touchGroups,
    const RuntimeTopLevelNote& topLevel,
    double nowSecond)
{
    if (notes == nullptr) {
        return;
    }

    if (topLevel.touchGroupIndex >= 0) {
        if (topLevel.touchGroupIndex < 0 || topLevel.touchGroupIndex >= touchGroups.size()) {
            return;
        }

        const RuntimeTouchGroup& group = touchGroups.at(topLevel.touchGroupIndex);
        int judgedCount = 0;
        for (int childNoteIndex : group.childNoteIndices) {
            miacode::task::CancellationScope::checkpoint();
            if (childNoteIndex < 0 || childNoteIndex >= notes->size()) {
                continue;
            }
            JudgeableSimpleNote& child = (*notes)[childNoteIndex];
            updateSimpleNoteLateBad(&child, nowSecond);
            if (child.judged) {
                ++judgedCount;
            }
        }

        if (judgedCount + kPadTimeEpsilon >= group.threshold) {
            for (int childNoteIndex : group.childNoteIndices) {
                miacode::task::CancellationScope::checkpoint();
                if (childNoteIndex < 0 || childNoteIndex >= notes->size()) {
                    continue;
                }
                JudgeableSimpleNote& child = (*notes)[childNoteIndex];
                if (child.judged) {
                    continue;
                }
                RuntimePadEvent syntheticCause;
                syntheticCause.pad = child.pad;
                syntheticCause.second = nowSecond;
                syntheticCause.sourceMarkerKey = child.markerKey;
                syntheticCause.sourceType = child.type;
                syntheticCause.sourceOrder = child.order;
                syntheticCause.line = child.line;
                syntheticCause.col = child.col;
                judgeSimpleNoteOnPadDown(&child, nowSecond, child.pad, &syntheticCause);
            }
        }
        return;
    }

    if (topLevel.simpleNoteIndex < 0 || topLevel.simpleNoteIndex >= notes->size()) {
        return;
    }
    updateSimpleNoteLateBad(&(*notes)[topLevel.simpleNoteIndex], nowSecond);
}

void simulateSimpleNoteJudgments(
    QVector<JudgeableSimpleNote>* notes,
    const QVector<RuntimeTouchGroup>& touchGroups,
    const QVector<RuntimeTopLevelNote>& topLevelNotes,
    const QMap<int, QMap<QString, RuntimePadEvent>>& eventsByTick)
{
    if (notes == nullptr) {
        return;
    }

    QMap<int, QVector<int>> expiryBuckets = buildNoteExpiryBuckets(*notes);
    QMap<int, bool> timelineTicks;
    for (auto it = eventsByTick.constBegin(); it != eventsByTick.constEnd(); ++it) {
        miacode::task::CancellationScope::checkpoint();
        timelineTicks.insert(it.key(), true);
    }
    for (auto it = expiryBuckets.constBegin(); it != expiryBuckets.constEnd(); ++it) {
        miacode::task::CancellationScope::checkpoint();
        timelineTicks.insert(it.key(), true);
    }

    for (auto tickIt = timelineTicks.constBegin(); tickIt != timelineTicks.constEnd(); ++tickIt) {
        miacode::task::CancellationScope::checkpoint();
        const int tick = tickIt.key();
        const double nowSecond = tickToSecond(tick);

        const QMap<QString, RuntimePadEvent> events = eventsByTick.value(tick);
        for (auto eventIt = events.constBegin(); eventIt != events.constEnd(); ++eventIt) {
            miacode::task::CancellationScope::checkpoint();
            const RuntimePadEvent& event = eventIt.value();
            for (const RuntimeTopLevelNote& topLevel : topLevelNotes) {
                miacode::task::CancellationScope::checkpoint();
                if (consumeRuntimePadEvent(notes, touchGroups, topLevel, nowSecond, event)) {
                    break;
                }
            }
        }

        for (const RuntimeTopLevelNote& topLevel : topLevelNotes) {
            miacode::task::CancellationScope::checkpoint();
            updateRuntimeTopLevelNote(notes, touchGroups, topLevel, nowSecond);
        }
    }
}

QSet<QString> buildSameMomentPadOverlapKeys(const QVector<JudgeableSimpleNote>& notes)
{
    QHash<QString, QVector<QString>> markerKeysByMomentPad;
    markerKeysByMomentPad.reserve(notes.size());

    for (const JudgeableSimpleNote& note : notes) {
        miacode::task::CancellationScope::checkpoint();
        const QString pad = normalizedPadToken(note.pad);
        if (pad.isEmpty()) {
            continue;
        }
        const QString lookupKey =
            QStringLiteral("%1|%2").arg(pad).arg(qRound64(note.momentSecond * 1000000.0));
        markerKeysByMomentPad[lookupKey].append(note.markerKey);
    }

    QSet<QString> overlapKeys;
    overlapKeys.reserve(notes.size());
    for (auto it = markerKeysByMomentPad.constBegin(); it != markerKeysByMomentPad.constEnd(); ++it) {
        miacode::task::CancellationScope::checkpoint();
        if (it.value().size() <= 1) {
            continue;
        }
        for (const QString& markerKey : it.value()) {
            miacode::task::CancellationScope::checkpoint();
            overlapKeys.insert(markerKey);
        }
    }
    return overlapKeys;
}

constexpr double kMultiTouchOverlapCenterEpsilon = 1e-3;
constexpr double kMultiTouchOverlapRadiusEpsilon = 1e-3;

struct MultiTouchActionCluster {
    RuntimeTouchPoint representativePoint;
    QVector<int> actionIndices;
    int handCount = 0;
};

bool shouldMergeMultiTouchCluster(
    const RuntimeTouchPoint& existingPoint,
    const RuntimeHandAction& existingAction,
    const RuntimeTouchPoint& candidatePoint,
    const RuntimeHandAction& candidateAction)
{
    if (existingAction.kind != RuntimeHandActionKind::Press
        || candidateAction.kind != RuntimeHandActionKind::Press) {
        return false;
    }
    return pointDistance(existingPoint.center, candidatePoint.center) <= kMultiTouchOverlapCenterEpsilon
        && qAbs(existingPoint.radius - candidatePoint.radius) <= kMultiTouchOverlapRadiusEpsilon;
}

QVector<MultiTouchActionCluster> buildMultiTouchActionClusters(
    const QVector<RuntimeTouchPoint>& touchPoints,
    const QVector<RuntimeHandAction>& actions)
{
    QVector<MultiTouchActionCluster> clusters;
    clusters.reserve(touchPoints.size());

    for (const RuntimeTouchPoint& touchPoint : touchPoints) {
        miacode::task::CancellationScope::checkpoint();
        if (touchPoint.actionIndex < 0 || touchPoint.actionIndex >= actions.size()) {
            continue;
        }

        const RuntimeHandAction& action = actions.at(touchPoint.actionIndex);
        bool merged = false;
        for (MultiTouchActionCluster& cluster : clusters) {
            miacode::task::CancellationScope::checkpoint();
            if (cluster.representativePoint.actionIndex < 0
                || cluster.representativePoint.actionIndex >= actions.size()) {
                continue;
            }
            const RuntimeHandAction& representativeAction =
                actions.at(cluster.representativePoint.actionIndex);
            if (!shouldMergeMultiTouchCluster(
                    cluster.representativePoint,
                    representativeAction,
                    touchPoint,
                    action)) {
                continue;
            }

            cluster.actionIndices.append(touchPoint.actionIndex);
            cluster.handCount = qMax(cluster.handCount, action.requireTwoHands ? 2 : 1);
            merged = true;
            break;
        }

        if (merged) {
            continue;
        }

        MultiTouchActionCluster cluster;
        cluster.representativePoint = touchPoint;
        cluster.actionIndices.append(touchPoint.actionIndex);
        cluster.handCount = action.requireTwoHands ? 2 : 1;
        clusters.append(cluster);
    }

    return clusters;
}

void collectSimpleNoteMultiTouchDiagnostics(
    const QVector<TimelineNoteMarker>& noteMarkers,
    const QVector<JudgeableSimpleNote>& notes,
    const QVector<RuntimeTouchGroup>& touchGroups,
    const QHash<int, int>& touchGroupByChildNoteIndex,
    const QHash<QString, const TimelineNoteMarker*>& markerLookup,
    const QHash<QString, QString>& syntheticSlideHeadOwnerKeys,
    const MuriRenderOptions& renderOptions,
    QVector<MuriDiagnostic>* diagnostics,
    QSet<QString>* multiTouchMarkerKeys)
{
    if (diagnostics == nullptr) {
        return;
    }

    QVector<RuntimeHandAction> actions =
        buildRuntimeHandActions(
            noteMarkers,
            notes,
            touchGroups,
            touchGroupByChildNoteIndex,
            miacode::muri::handRadiusForOptions(renderOptions),
            true);
    if (renderOptions.excludeTouchFromMultiTouch) {
        actions.erase(
            std::remove_if(actions.begin(), actions.end(), [](const RuntimeHandAction& action) {
                return sourceTypeIsTouchLike(action.sourceType);
            }),
            actions.end());
    }
    if (actions.isEmpty()) {
        return;
    }

    int maxTick = 0;
    for (const RuntimeHandAction& action : actions) {
        miacode::task::CancellationScope::checkpoint();
        maxTick = qMax(maxTick, judgeTickForPadActiveEnd(action.endSecond));
    }
    QVector<int> activeActionIndices;
    QHash<QString, int> diagnosticIndexBySignature;
    QSet<quint64> previousMergedSlidePairs;
    QSet<QPair<QString, QString>> initiallyMergedSameHeadSlides;
    int actionPointer = 0;
    for (int tick = 0; tick <= maxTick; ++tick) {
        miacode::task::CancellationScope::checkpoint();
        const double nowSecond = tickToSecond(tick);
        while (actionPointer < actions.size()) {
            miacode::task::CancellationScope::checkpoint();
            const RuntimeHandAction& action = actions.at(actionPointer);
            if (nowSecond + kPadTimeEpsilon < action.startSecond) {
                break;
            }
            activeActionIndices.append(actionPointer);
            ++actionPointer;
        }

        QSet<quint64> currentMergedSlidePairs;
        const QVector<RuntimeTouchPoint> touchPoints =
            buildRuntimeTouchPoints(
                actions,
                activeActionIndices,
                nowSecond,
                &previousMergedSlidePairs,
                &currentMergedSlidePairs);
        for (quint64 pairKey : currentMergedSlidePairs) {
            const RuntimeHandAction& left = actions.at(static_cast<int>(pairKey >> 32));
            const RuntimeHandAction& right = actions.at(static_cast<int>(pairKey & 0xffffffffULL));
            const TimelineNoteMarker* leftMarker = markerLookup.value(left.markerKey, nullptr);
            const TimelineNoteMarker* rightMarker = markerLookup.value(right.markerKey, nullptr);
            // Only remember a shared start, not later crossings or chained segment starts.
            if (left.sourceType == QLatin1String("slide")
                && right.sourceType == QLatin1String("slide")
                && leftMarker != nullptr && rightMarker != nullptr
                && leftMarker->lane == rightMarker->lane
                && qAbs(leftMarker->second - rightMarker->second) <= kPadTimeEpsilon
                && qAbs(left.startSecond - right.startSecond) <= kPadTimeEpsilon
                && qAbs(left.startSecond - leftMarker->slideTraceSecond) <= kPadTimeEpsilon
                && qAbs(right.startSecond - rightMarker->slideTraceSecond) <= kPadTimeEpsilon
                && tick == judgeTickForPadActiveStart(left.startSecond)) {
                initiallyMergedSameHeadSlides.insert(qMakePair(left.markerKey, right.markerKey));
            }
        }
        const QVector<MultiTouchActionCluster> touchClusters =
            buildMultiTouchActionClusters(touchPoints, actions);
        int handCount = 0;
        QVector<int> touchPointActionIndices;
        touchPointActionIndices.reserve(touchClusters.size());
        int nonTouchHandCount = 0;
        bool involvesTouch = false;
        for (const MultiTouchActionCluster& cluster : touchClusters) {
            miacode::task::CancellationScope::checkpoint();
            if (cluster.representativePoint.actionIndex < 0
                || cluster.representativePoint.actionIndex >= actions.size()) {
                continue;
            }

            const int clusterHandCount = qMax(1, cluster.handCount);
            handCount += clusterHandCount;
            bool clusterHasTouchLike = false;
            bool clusterHasNonTouch = false;
            for (int actionIndex : cluster.actionIndices) {
                miacode::task::CancellationScope::checkpoint();
                if (actionIndex < 0 || actionIndex >= actions.size()) {
                    continue;
                }
                if (sourceTypeIsTouchLike(actions.at(actionIndex).sourceType)) {
                    clusterHasTouchLike = true;
                } else {
                    clusterHasNonTouch = true;
                }
            }
            if (clusterHasTouchLike) {
                involvesTouch = true;
            }
            if (clusterHasNonTouch) {
                nonTouchHandCount += clusterHandCount;
            }
            touchPointActionIndices.append(cluster.representativePoint.actionIndex);
        }
        bool involvesMergedSharedStart = false;
        if (handCount == 2) {
            const bool hasIndependentPress = std::any_of(
                touchPointActionIndices.cbegin(), touchPointActionIndices.cend(),
                [&actions](int index) { return actions.at(index).kind == RuntimeHandActionKind::Press; });
            if (hasIndependentPress) {
                for (quint64 pairKey : currentMergedSlidePairs) {
                    const int leftIndex = static_cast<int>(pairKey >> 32);
                    const int rightIndex = static_cast<int>(pairKey & 0xffffffffULL);
                    if (!initiallyMergedSameHeadSlides.contains(
                            qMakePair(actions.at(leftIndex).markerKey, actions.at(rightIndex).markerKey))) {
                        continue;
                    }
                    involvesMergedSharedStart = true;
                    if (!touchPointActionIndices.contains(leftIndex)) {
                        touchPointActionIndices.append(leftIndex);
                    }
                    if (!touchPointActionIndices.contains(rightIndex)) {
                        touchPointActionIndices.append(rightIndex);
                    }
                }
            }
        }
        if ((handCount > 2 || involvesMergedSharedStart) && !touchPointActionIndices.isEmpty()) {
            std::sort(touchPointActionIndices.begin(), touchPointActionIndices.end(), [&actions](int a, int b) {
                miacode::task::CancellationScope::checkpoint();
                const RuntimeHandAction& left = actions.at(a);
                const RuntimeHandAction& right = actions.at(b);
                if (left.order != right.order) {
                    return left.order < right.order;
                }
                if (left.line != right.line) {
                    return left.line < right.line;
                }
                return left.col < right.col;
            });

            QSet<QString> uniqueSources;
            QVector<int> causeActionIndices;
            causeActionIndices.reserve(touchPointActionIndices.size());
            for (int actionIndex : touchPointActionIndices) {
                miacode::task::CancellationScope::checkpoint();
                const RuntimeHandAction& action = actions.at(actionIndex);
                if (uniqueSources.contains(action.markerKey)) {
                    continue;
                }
                uniqueSources.insert(action.markerKey);
                causeActionIndices.append(actionIndex);
                if (multiTouchMarkerKeys != nullptr) {
                    multiTouchMarkerKeys->insert(action.markerKey);
                }
            }

            QStringList signatureParts;
            QStringList detailParts;
            signatureParts.reserve(causeActionIndices.size());
            detailParts.reserve(causeActionIndices.size());
            for (int actionIndex : causeActionIndices) {
                miacode::task::CancellationScope::checkpoint();
                const RuntimeHandAction& action = actions.at(actionIndex);
                signatureParts.append(action.markerKey);
                detailParts.append(
                    formatMultiTouchActionLabel(action, markerLookup, syntheticSlideHeadOwnerKeys));
            }
            const QString signature = signatureParts.join(QLatin1Char('|'));
            const MuriAlertLevel alertLevel = (involvesMergedSharedStart
                                              || (involvesTouch && nonTouchHandCount <= 2))
                ? MuriAlertLevel::Warning
                : MuriAlertLevel::Muri;
            const int existingIndex = diagnosticIndexBySignature.value(signature, -1);
            if (existingIndex < 0
                || (diagnostics->at(existingIndex).alertLevel == MuriAlertLevel::Warning
                    && alertLevel == MuriAlertLevel::Muri)) {

                int anchorActionIndex = causeActionIndices.constFirst();
                DiagnosticAnchor anchorInfo = diagnosticAnchorFromAction(actions.at(anchorActionIndex));
                for (int actionIndex : causeActionIndices) {
                    miacode::task::CancellationScope::checkpoint();
                    const DiagnosticAnchor candidateAnchor = diagnosticAnchorFromAction(actions.at(actionIndex));
                    if (diagnosticAnchorComesBefore(candidateAnchor, anchorInfo)) {
                        anchorInfo = candidateAnchor;
                        anchorActionIndex = actionIndex;
                    }
                }

                MuriDiagnostic diagnostic;
                diagnostic.kind = MuriKind::MultiTouch;
                diagnostic.second = nowSecond;
                diagnostic.anchorSecond = anchorInfo.valid ? anchorInfo.second : diagnostic.second;
                diagnostic.line = anchorInfo.valid ? anchorInfo.line : 1;
                diagnostic.col = anchorInfo.valid ? anchorInfo.col : 1;
                diagnostic.markerKey = actions.at(anchorActionIndex).markerKey;
                diagnostic.title = muriKindDisplayName(MuriKind::MultiTouch, true);
                diagnostic.alertLevel = alertLevel;
                diagnostic.detailKind = MuriDetailKind::MultiTouchFormedBy;
                diagnostic.detailArgs.actions = detailParts.join(QStringLiteral(", "));
                diagnostic.detailArgs.alert = diagnostic.alertLevel;
                diagnostic.detail = renderMuriDetail(
                    diagnostic.detailKind,
                    diagnostic.detailArgs,
                    SimaiValidationLocale::English);
                if (existingIndex >= 0) {
                    (*diagnostics)[existingIndex] = diagnostic;
                } else {
                    diagnosticIndexBySignature.insert(signature, diagnostics->size());
                    diagnostics->append(diagnostic);
                }
            }
        }

        QVector<int> finishedActionIndices;
        finishedActionIndices.reserve(activeActionIndices.size());
        for (int actionIndex : activeActionIndices) {
            miacode::task::CancellationScope::checkpoint();
            if (actionIndex < 0 || actionIndex >= actions.size()) {
                continue;
            }
            if (nowSecond + kPadTimeEpsilon >= actions.at(actionIndex).endSecond) {
                finishedActionIndices.append(actionIndex);
            }
        }
        for (int actionIndex : finishedActionIndices) {
            miacode::task::CancellationScope::checkpoint();
            activeActionIndices.removeAll(actionIndex);
        }
        previousMergedSlidePairs.swap(currentMergedSlidePairs);
    }
}

bool runtimeCauseIsSlideHeadTap(
    const JudgeableSimpleNote& note,
    const QHash<QString, const TimelineNoteMarker*>& markerLookup)
{
    if (note.cause.extraPadDown || note.cause.sourceMarkerKey.startsWith(QStringLiteral("slide_head_star|"))) {
        return true;
    }
    if (note.pad.isEmpty()) {
        return false;
    }
    const auto it = markerLookup.constFind(note.cause.sourceMarkerKey);
    if (it == markerLookup.constEnd() || it.value() == nullptr || !isSlideLike(*it.value())) {
        return false;
    }
    return normalizedPadToken(note.pad) == slideHeadPad(it.value()->lane);
}

void collectSimpleNoteRuntimeDiagnostics(
    const QVector<TimelineNoteMarker>& noteMarkers,
    const MuriRenderOptions& renderOptions,
    MuriDiagnosticCollector& collector,
    double staticTapOnSlideThresholdSeconds)
{
    const double normalizedStaticTapOnSlideThresholdSeconds = qBound(
        static_cast<double>(miacode::muri::kStaticTapOnSlideThresholdMinMs) / 1000.0,
        staticTapOnSlideThresholdSeconds,
        static_cast<double>(miacode::muri::kStaticTapOnSlideThresholdMaxMs) / 1000.0);
    const QVector<MuriStaticReference> staticReferences = buildStaticMuriReferences(
        noteMarkers,
        normalizedStaticTapOnSlideThresholdSeconds);
    const QHash<QString, MarkerSourceRef> markerRefs = buildMarkerSourceRefs(noteMarkers);
    const QHash<QString, const TimelineNoteMarker*> markerLookup = buildMarkerLookup(noteMarkers);
    const QHash<QString, QString> markerConfigLabels = buildMarkerConfigLabels(noteMarkers);
    const QHash<QString, QString> syntheticSlideHeadOwnerKeys = buildSyntheticSlideHeadOwnerKeys(noteMarkers);
    const QSet<QString> slideKeysWithTapOnSlideHead = buildSlideKeysWithTapOnSlideHead(noteMarkers);
    QHash<QString, int> noteIndexByMarkerKey;
    QVector<JudgeableSimpleNote> notes = buildJudgeableSimpleNotes(noteMarkers, markerRefs, &noteIndexByMarkerKey);
    QHash<int, int> touchGroupByChildNoteIndex;
    const QVector<RuntimeTouchGroup> touchGroups =
        buildRuntimeTouchGroups(noteMarkers, noteIndexByMarkerKey, &touchGroupByChildNoteIndex);
    const QVector<RuntimeTopLevelNote> topLevelNotes =
        buildRuntimeTopLevelNotes(notes, touchGroups, touchGroupByChildNoteIndex);
    const QVector<MuriPadWindow> runtimePadWindows =
        buildRuntimePadWindows(noteMarkers, notes, noteIndexByMarkerKey, touchGroups, touchGroupByChildNoteIndex);
    QSet<QString> multiTouchMarkerKeys;
    collectSimpleNoteMultiTouchDiagnostics(
        noteMarkers,
        notes,
        touchGroups,
        touchGroupByChildNoteIndex,
        markerLookup,
        syntheticSlideHeadOwnerKeys,
        renderOptions,
        &collector.diagnostics,
        &multiTouchMarkerKeys);
    simulateSimpleNoteJudgments(
        &notes,
        touchGroups,
        topLevelNotes,
        buildRuntimePadEvents(noteMarkers, runtimePadWindows, markerRefs));
    const QSet<QString> overlapRenderKeys = buildSameMomentPadOverlapKeys(notes);
    QHash<QString, MuriSimpleJudgeEffect> simpleJudgeEffects;
    simpleJudgeEffects.reserve(notes.size());
    QSet<QString> suppressJudgeSpriteKeys;
    QSet<QString> forceRenderJudgeSpriteKeys;
    suppressJudgeSpriteKeys.reserve(multiTouchMarkerKeys.size());
    forceRenderJudgeSpriteKeys.reserve(notes.size());
    for (const QString& markerKey : multiTouchMarkerKeys) {
        miacode::task::CancellationScope::checkpoint();
        if (!overlapRenderKeys.contains(markerKey)) {
            suppressJudgeSpriteKeys.insert(markerKey);
        }
    }

    for (const RuntimeTopLevelNote& topLevel : topLevelNotes) {
        miacode::task::CancellationScope::checkpoint();
        if (topLevel.touchGroupIndex >= 0) {
            continue;
        }
        if (topLevel.simpleNoteIndex < 0 || topLevel.simpleNoteIndex >= notes.size()) {
            continue;
        }

        const JudgeableSimpleNote& note = notes.at(topLevel.simpleNoteIndex);
        if (!note.judged || !note.judgeBad || note.marker == nullptr) {
            if (!note.judged || !note.judgeBad) {
                continue;
            }
        }

        if (note.judgeSecond + kPadTimeEpsilon < note.momentSecond && !note.cause.sourceMarkerKey.isEmpty()) {
            const bool slideHeadTap = runtimeCauseIsSlideHeadTap(note, markerLookup);
            const QString sourceOwnerKey =
                slideStartOwnerKeyForSource(note.cause.sourceMarkerKey, syntheticSlideHeadOwnerKeys);
            const bool hasTapOnSlideHead = slideKeysWithTapOnSlideHead.contains(sourceOwnerKey);
            const QString causeConfig = markerConfigLabelForSource(
                markerConfigLabels,
                syntheticSlideHeadOwnerKeys,
                note.cause.sourceMarkerKey,
                note.cause.sourceType);
            const double gapSecond = slideHeadTap
                ? qMax(
                      0.0,
                      note.momentSecond
                          - slideStartSecondForSource(
                              note.cause,
                              markerLookup,
                              syntheticSlideHeadOwnerKeys))
                : qMax(0.0, note.momentSecond - note.judgeSecond);
            const double gapMs = gapSecond * 1000.0;
            const MuriAlertLevel baseAlertLevel = slideHeadTap
                ? slideHeadTapAlertLevel(hasTapOnSlideHead, gapSecond)
                : tapOnSlideAlertLevel(gapSecond, normalizedStaticTapOnSlideThresholdSeconds);
            const MuriAlertLevel alertLevel =
                downgradeProtectedSimpleNoteAlertLevel(baseAlertLevel, note.hasProtection);
            const QString affectedTarget = simpleNoteTargetLabel(note);
            const MuriDetailKind detailKind = slideHeadTap
                ? slideHeadTapDetailKind(hasTapOnSlideHead)
                : MuriDetailKind::TapOnSlideCollide;
            const MuriDetailArgs detailArgs =
                simpleGapDetailArgs(alertLevel, causeConfig, affectedTarget, gapMs);
            const QString detail = renderMuriDetail(
                detailKind,
                detailArgs,
                SimaiValidationLocale::English);
            collector.addSimpleNoteDiagnostic(
                slideHeadTap ? MuriKind::SlideHeadTap : MuriKind::TapOnSlide,
                alertLevel,
                note.judgeSecond,
                note,
                detail,
                diagnosticAnchorFromNote(note),
                detailKind,
                detailArgs);
            forceRenderJudgeSpriteKeys.insert(note.markerKey);
            if (alertLevel == MuriAlertLevel::Warning) {
                simpleJudgeEffects.insert(note.markerKey, MuriSimpleJudgeEffect::Perfect);
            }
            continue;
        }

        // Real tap-slide pairs are judged off the slide start in MaiMuriDX.
        // Keep them out of late-overlap reporting until slide runtime is fully ported.
        if (note.slideHead) {
            continue;
        }

        RuntimePadEvent overlapCause = note.cause;
        if (note.lateBad && overlapCause.sourceMarkerKey.isEmpty()) {
            findOccupyingPadCause(
                runtimePadWindows,
                markerRefs,
                note.pad,
                note.momentSecond,
                note.markerKey,
                &overlapCause);
        }

        // A matching static end-area reference owns this slow-slide overlap.
        // The runtime late result would duplicate the panel and anchor it to
        // the earlier slide head.
        if (note.lateBad
            && matchesStaticSlideEndOverlap(staticReferences, note, overlapCause)) {
            continue;
        }

        const QString affectedConfig = markerConfigLabelForKey(markerConfigLabels, note.markerKey, note.type);
        MuriDetailKind overlapDetailKind = MuriDetailKind::FormedOverlapAtSamePosition;
        MuriDetailArgs overlapDetailArgs;
        overlapDetailArgs.left = affectedConfig;
        overlapDetailArgs.alert = MuriAlertLevel::Muri;
        if (!overlapCause.sourceMarkerKey.isEmpty()) {
            const QString causeConfig = markerConfigLabelForSource(
                markerConfigLabels,
                syntheticSlideHeadOwnerKeys,
                overlapCause.sourceMarkerKey,
                overlapCause.sourceType);
            if (!causeConfig.isEmpty() && causeConfig != affectedConfig) {
                overlapDetailKind = MuriDetailKind::FormedOverlapSamePosition;
                overlapDetailArgs.right = causeConfig;
            }
        }
        const QString overlapDetail = renderMuriDetail(
            overlapDetailKind,
            overlapDetailArgs,
            SimaiValidationLocale::English);
        const DiagnosticAnchor anchor = earlierDiagnosticAnchor(
            diagnosticAnchorFromNote(note),
            diagnosticAnchorForCause(overlapCause, markerRefs, syntheticSlideHeadOwnerKeys));
        collector.addSimpleNoteDiagnostic(
            MuriKind::Overlap,
            MuriAlertLevel::Muri,
            note.judgeSecond,
            note,
            overlapDetail,
            anchor,
            overlapDetailKind,
            overlapDetailArgs);
        forceRenderJudgeSpriteKeys.insert(note.markerKey);
    }

    collector.judgeSpriteEvents.reserve(collector.judgeSpriteEvents.size() + notes.size());
    for (const JudgeableSimpleNote& note : notes) {
        miacode::task::CancellationScope::checkpoint();
        if (suppressJudgeSpriteKeys.contains(note.markerKey)
            && !forceRenderJudgeSpriteKeys.contains(note.markerKey)) {
            continue;
        }
        collector.addSimpleJudgeSpriteEvent(
            note,
            simpleJudgeEffects.value(note.markerKey, MuriSimpleJudgeEffect::Good));
    }
}

}  // namespace miacode::muri::detail
