#pragma once

#include "core/scene/PreviewFrameState.h"
#include "core/scene/PreviewHeadLayerState.h"
#include "core/scene/PreviewTrackLayerState.h"
#include "core/scene/PreviewSlideMotionLayerState.h"
#include "core/scene/PreviewTouchLayerState.h"
#include "core/scene/PreviewTouchHoldLayerState.h"
#include "core/scene/PreviewSceneGeometry.h"
#include "preview/quick_scene/PreviewQuickSpriteNodes.h"
#include <QSGNode>

// Live editor decoration only. Root invokes this solely for its runtime-backed
// preview; explicit-frame export/cover roots never enter this layer.
class PreviewQuickSelectionLayer {
public:
    QSGNode* updateNode(QSGNode* oldNode,
        const miacode::preview::scene::PreviewFrameState& state,
        const QSize& renderSize, QQuickWindow* window, PreviewTextureRepository* textures)
    {
        using namespace miacode::preview::scene;
        if (state.selectedNoteLocations.isEmpty()) { delete oldNode; return nullptr; }
        if (sourceRevision_ != state.sceneContentRevision || sourceStorage_ != state.sceneStorageGeneration
            || locations_ != state.selectedNoteLocations) {
            selectedMarkers_.clear();
            for (const auto& marker : state.noteMarkers) {
                const quint64 location = (quint64(quint32(marker.sourceLine)) << 32) | quint32(marker.sourceCol);
                if (state.selectedNoteLocations.contains(location)) selectedMarkers_.append(marker);
            }
            sourceRevision_ = state.sceneContentRevision;
            sourceStorage_ = state.sceneStorageGeneration;
            locations_ = state.selectedNoteLocations;
            ++selectionRevision_;
        }
        PreviewFrameState selected = state;
        selected.noteMarkers = selectedMarkers_;
        selected.sceneContentRevision = selectionRevision_;
        if (prepared_.sync(selected)) {
            headCursor_.reset(); trackCursor_.reset(); touchCursor_.reset(); holdCursor_.reset();
        }
        syncPreviewLayerWindowCursor(prepared_.headLayer(), state.playheadSeconds, &headCursor_);
        syncPreviewLayerWindowCursor(prepared_.slideLikeLayer(), state.playheadSeconds, &trackCursor_);
        syncPreviewLayerWindowCursor(prepared_.touchLayer(), state.playheadSeconds, &touchCursor_);
        syncPreviewLayerWindowCursor(prepared_.touchHoldLayer(), state.playheadSeconds, &holdCursor_);
        const PreviewActiveMarkerView heads(selected.noteMarkers, prepared_.headLayer(), headCursor_);
        const PreviewActiveMarkerView tracks(selected.noteMarkers, prepared_.slideLikeLayer(), trackCursor_);
        const PreviewActiveMarkerView touches(selected.noteMarkers, prepared_.touchLayer(), touchCursor_);
        const PreviewActiveMarkerView holds(selected.noteMarkers, prepared_.touchHoldLayer(), holdCursor_);
        const auto field = playfieldRectForStage(stageRectForSize(renderSize), state.render.layoutSquareScale);
        const auto head = buildPreviewHeadLayerState(selected, heads, field, &headCache_);
        const auto track = buildPreviewTrackLayerState(selected, tracks, field);
        const auto motion = buildPreviewSlideMotionLayerState(selected, tracks, field);
        const auto touch = buildPreviewTouchLayerState(selected, touches, field);
        const auto hold = buildPreviewTouchHoldLayerState(selected, holds, field);
        PreviewSpriteDescriptors outlines;
        outlines.reserve(head.sprites.size() + track.sprites.size() + motion.sprites.size()
            + touch.sprites.size() + hold.sprites.size());
        const auto append = [&](const PreviewSpriteDescriptors& sprites) {
            for (auto sprite : sprites) {
                sprite.effect = PreviewAnimatedSpriteEffect::SelectionOutline;
                outlines.append(sprite);
            }
        };
        append(track.sprites); append(head.sprites); append(motion.sprites);
        append(touch.sprites); append(hold.sprites);
        return buildPreviewSpriteNodeTree(oldNode, outlines, window, textures, state.playheadSeconds, "selection");
    }
private:
    QSet<quint64> locations_;
    QVector<TimelineNoteMarker> selectedMarkers_;
    quint64 sourceRevision_ = ~quint64(0), sourceStorage_ = ~quint64(0), selectionRevision_ = 0;
    miacode::preview::scene::PreviewPreparedSceneCache prepared_;
    miacode::preview::scene::PreviewLayerWindowCursor headCursor_, trackCursor_, touchCursor_, holdCursor_;
    miacode::preview::scene::PreviewHeadRenderAssetCache headCache_;
};
