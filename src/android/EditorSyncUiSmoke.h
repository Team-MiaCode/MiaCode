#pragma once

#include "MobileTimeline.h"
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QTimer>
#include <QDebug>
#include <cstdio>
#include <memory>

namespace miacode::android {
inline void startEditorSyncUiSmoke(QGuiApplication& app, QQmlApplicationEngine& engine,
    AndroidDocumentSession& document, MobilePreview& preview, MobileTimeline& timeline,
    EditorSyncController& sync)
{
    struct Proof {
        QElapsedTimer elapsed;
        int phase = 0;
        quint64 sequence = 0;
        quint64 acknowledged = 0;
        bool applied = false;
        int caretEvents = 0;
        int caretBefore = 0;
        int cursorBefore = 0;
        double cursorSecond = 0;
        EditorFollowState deliveredFollow;
    };
    const auto proof = std::make_shared<Proof>();
    proof->elapsed.start();
    auto* timer = new QTimer(&app);
    timer->setInterval(50);
    QObject::connect(&sync, &EditorSyncController::navigationFinished, timer,
        [proof](qulonglong sequence, bool applied) {
            proof->acknowledged = sequence;
            proof->applied = applied;
        });
    QObject::connect(&sync, &EditorSyncController::caretLocationPublished, timer,
        [proof](int, quint64, int, int) { ++proof->caretEvents; });
    QObject::connect(&sync, &EditorSyncController::followChanged, timer, [proof, &sync] {
        proof->deliveredFollow = {sync.followDifficultyId(), sync.followRevision(),
            sync.followStart(), sync.followEnd(), sync.followCaret(), sync.followActive(),
            sync.followReveal(), sync.followPlaybackActive()};
    });
    QObject::connect(timer, &QTimer::timeout, timer,
        [&, timer, proof] {
            auto finish = [&](bool passed, const char* reason) {
                timer->stop();
                preview.setPlaying(false);
                qInfo("Editor sync UI: %s; phase=%d; %s", passed ? "passed" : "failed",
                    proof->phase, reason);
                std::fprintf(stderr, "Editor sync UI: %s; phase=%d; %s\n",
                    passed ? "passed" : "failed", proof->phase, reason);
                app.exit(passed ? 0 : 28);
            };
            if (proof->elapsed.elapsed() > 15000) {
                finish(false, "timed out waiting for the actual UI synchronization");
                return;
            }
            if (engine.rootObjects().isEmpty()) return;
            auto* root = engine.rootObjects().first();
            auto* view = root->findChild<QQuickItem*>("v2SourceEditor");
            auto* editor = root->findChild<QQuickItem*>("sourceArea");
            auto* bridge = qobject_cast<TimelineQuickStateBridge*>(timeline.stateBridge());
            if (!view || !editor || !bridge) return;
            switch (proof->phase) {
            case 0:
                if (proof->elapsed.elapsed() < 500) return;
                if (view->property("syncController").value<QObject*>() != &sync) {
                    finish(false, "shared controller binding is missing");
                    return;
                }
                if (!sync.editorContextActive() || document.chartText().size() < 5) return;
                proof->sequence = sync.requestNavigation(document.activeDifficulty(),
                    document.documentRevision(), 1, 4, true, true);
                if (!proof->sequence) { finish(false, "visible editor rejected navigation"); return; }
                ++proof->phase;
                break;
            case 1: {
                if (proof->acknowledged != proof->sequence) return;
                const auto selection = sync.editorSelection();
                if (!proof->applied || editor->property("selectionStart").toInt() != 1
                    || editor->property("selectionEnd").toInt() != 4
                    || !selection.valid || selection.anchor != 1 || selection.position != 4) {
                    finish(false, "navigation selection or acknowledgment is incorrect"); return;
                }
                timeline.headerNavigate(10);
                ++proof->phase;
                break;
            }
            case 2:
                if (proof->acknowledged <= proof->sequence) return;
                if (!proof->applied || editor->property("cursorPosition").toInt() <= 4
                    || qAbs(preview.positionSeconds() - 10) > 0.01
                    || qAbs(bridge->playheadSeconds() - 10) > 0.01) {
                    finish(false, "timeline navigation did not reach the editor and preview"); return;
                }
                proof->caretBefore = proof->caretEvents;
                editor->setProperty("cursorPosition", 1);
                ++proof->phase;
                break;
            case 3:
                if (proof->caretEvents <= proof->caretBefore) return;
                if (sync.editorSelection().position != 1 || bridge->cursorSeconds() >= 10) {
                    finish(false, "user caret did not return to the timeline"); return;
                }
                proof->cursorSecond = bridge->cursorSeconds();
                if (!sync.seekPreviewToEditorLocation(document.activeDifficulty(),
                        document.documentRevision(), 1, 2)) {
                    finish(false, "editor preview seek was rejected"); return;
                }
                ++proof->phase;
                break;
            case 4:
                if (qAbs(preview.positionSeconds() - proof->cursorSecond) > 0.01) return;
                if (sync.requestNavigation(document.activeDifficulty(), document.documentRevision() + 1,
                        1, 4, true, true) != 0) {
                    finish(false, "stale revision navigation was accepted"); return;
                }
                sync.publishFollow({document.activeDifficulty(), document.documentRevision() + 1,
                    1, 4, 4, true, false, false});
                ++proof->phase;
                break;
            case 5:
                if (view->property("followDecorationActive").toBool()) return;
                proof->cursorBefore = editor->property("cursorPosition").toInt();
                timeline.followPreviewToggled(true);
                preview.setPositionSeconds(10);
                preview.setPlaying(true);
                ++proof->phase;
                break;
            case 6:
                if (!proof->deliveredFollow.playbackActive || !view->property("followDecorationActive").toBool()) return;
                // Follow delivery is queued and coalesced. Compare the UI to the
                // delivered state, rather than a newer transport sample awaiting delivery.
                if (!proof->deliveredFollow.active
                    || view->property("followDecorationStart").toInt() != proof->deliveredFollow.start
                    || view->property("followDecorationEnd").toInt() != proof->deliveredFollow.end
                    || view->property("followDecorationCursor").toInt() != proof->deliveredFollow.caret
                    || editor->property("cursorPosition").toInt() != proof->cursorBefore) {
                    std::fprintf(stderr, "Follow mismatch: ui=%d/%d/%d delivered=%d/%d/%d caret=%d expected=%d\n",
                        view->property("followDecorationStart").toInt(), view->property("followDecorationEnd").toInt(),
                        view->property("followDecorationCursor").toInt(), proof->deliveredFollow.start,
                        proof->deliveredFollow.end, proof->deliveredFollow.caret,
                        editor->property("cursorPosition").toInt(), proof->cursorBefore);
                    finish(false, "playback follow projection changed the editing caret or lost its span"); return;
                }
                finish(true, "navigation, timeline, caret, preview seek, stale revision, playback follow");
                break;
            }
        });
    timer->start();
}
}
