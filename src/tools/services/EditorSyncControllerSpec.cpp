// Behaviour regression for the (difficultyId, revision) identity gate that every
// preview→editor handoff passes through.
//
// This replaces a source-string contract that asserted `appliedQmlWorkspaceRevision_ > 0`
// appeared in two MainWindow files. The gate has since moved into this controller,
// and the scan went red without anything actually being broken — a scan cannot tell
// a moved guard from a deleted one. What follows drives the real object instead.
//
// The target deliberately links Qt6::Core + Qt6::Test only: a Widgets type creeping
// back into this boundary fails the build rather than a string match.

#include "app/services/EditorSyncController.h"

#include <QCoreApplication>
#include <QSignalSpy>
#include <QTextStream>

namespace {

using miacode::EditorFollowState;
using miacode::EditorSyncController;

constexpr int kDifficulty = 3;
constexpr qulonglong kRevision = 7;

bool require(bool condition, const QString& message, QTextStream& err)
{
    if (!condition) {
        err << "FAIL: " << message << Qt::endl;
    }
    return condition;
}

// Every publish is queued, so nothing this controller decides is observable
// until the event loop turns. Each check that expects a delivery drains here.
void flush()
{
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
}

void makeReady(EditorSyncController& controller)
{
    controller.setEditorReadiness(kDifficulty, kRevision, true);
    flush();
}

bool verifyNavigationRequiresTheReadyIdentity(QTextStream& err)
{
    EditorSyncController controller;
    QSignalSpy requested(&controller, &EditorSyncController::navigationRequested);

    // No editor has reported readiness yet.
    bool ok = require(controller.requestNavigation(kDifficulty, kRevision, 0, 4, false, false) == 0,
                      QStringLiteral("navigation is refused before an editor reports readiness"), err);

    makeReady(controller);
    ok = ok
        && require(controller.requestNavigation(kDifficulty, kRevision + 1, 0, 4, false, false) == 0
                       && controller.requestNavigation(kDifficulty + 1, kRevision, 0, 4, false, false) == 0,
                   QStringLiteral("a stale revision or a different difficulty is refused outright"), err);

    const qulonglong accepted = controller.requestNavigation(kDifficulty, kRevision, 2, 6, true, true);
    ok = ok && require(accepted != 0 && requested.isEmpty(),
                       QStringLiteral("an accepted navigation returns a sequence and delivers nothing synchronously"), err);
    flush();
    ok = ok && require(requested.count() == 1
                           && requested.first().at(0).toULongLong() == accepted
                           && requested.first().at(1).toInt() == kDifficulty
                           && requested.first().at(2).toULongLong() == kRevision,
                       QStringLiteral("the delivered navigation carries the sequence and the identity it was accepted on"), err);
    return ok;
}

bool verifyIdentityIsRecheckedAtDelivery(QTextStream& err)
{
    EditorSyncController controller;
    makeReady(controller);
    QSignalSpy requested(&controller, &EditorSyncController::navigationRequested);
    QSignalSpy finished(&controller, &EditorSyncController::navigationFinished);

    // Accepted against the identity that was current at submission, then the
    // document moves on before the queued delivery runs. Delivering it would
    // place a caret using positions measured in the previous revision's text.
    const qulonglong sequence = controller.requestNavigation(kDifficulty, kRevision, 2, 6, false, false);
    controller.setEditorReadiness(kDifficulty, kRevision + 1, true);
    flush();

    return require(sequence != 0 && requested.isEmpty(),
                   QStringLiteral("a navigation invalidated between submit and delivery is never emitted"), err)
        && require(finished.count() >= 1
                       && finished.first().at(0).toULongLong() == sequence
                       && !finished.first().at(1).toBool(),
                   QStringLiteral("the caller is told the navigation did not apply, rather than left waiting"), err);
}

bool verifyLosingTheEditorCancelsPendingWork(QTextStream& err)
{
    EditorSyncController controller;
    makeReady(controller);
    QSignalSpy requested(&controller, &EditorSyncController::navigationRequested);
    QSignalSpy finished(&controller, &EditorSyncController::navigationFinished);

    const qulonglong sequence = controller.requestNavigation(kDifficulty, kRevision, 2, 6, false, false);
    // The editor is hidden (a tab closed, or an overlay page took the pane).
    controller.setEditorReadiness(kDifficulty, kRevision, false);
    flush();

    bool ok = require(sequence != 0 && requested.isEmpty() && finished.count() >= 1
                          && !finished.first().at(1).toBool(),
                      QStringLiteral("hiding the editor finishes the pending navigation instead of stranding it"), err);

    return ok;
}

bool verifyLocationPublishersShareTheGate(QTextStream& err)
{
    EditorSyncController controller;
    makeReady(controller);

    bool ok = require(!controller.beginPointerInteraction(kDifficulty, kRevision + 1)
                          && !controller.setTouchPadPreviewAnchor(kDifficulty, kRevision + 1,
                                                                  1, 3)
                          && !controller.seekPreviewToEditorLocation(kDifficulty, kRevision + 1, 1, 1),
                      QStringLiteral("pointer, touch anchor, and preview seek all refuse a stale revision"), err);

    QSignalSpy seeks(&controller, &EditorSyncController::previewSeekPublished);
    QSignalSpy anchors(&controller, &EditorSyncController::touchPadPreviewAnchorPublished);
    ok = ok
        && require(controller.seekPreviewToEditorLocation(kDifficulty, kRevision, 4, 9)
                       && controller.setTouchPadPreviewAnchor(kDifficulty, kRevision,
                                                              2, 2),
                   QStringLiteral("the same calls are accepted on the ready identity"), err);
    flush();
    ok = ok
        && require(seeks.count() == 1 && seeks.first().at(0).toInt() == kDifficulty
                       && seeks.first().at(1).toInt() == 4 && seeks.first().at(2).toInt() == 9,
                   QStringLiteral("an accepted preview seek is delivered with its line and column"), err)
        && require(anchors.count() == 1 && anchors.first().at(1).toInt() == 2
                       && anchors.first().at(2).toInt() == 2,
                   QStringLiteral("a touch anchor resolves its offset to a line and column in C++"), err);

    // Same submission, invalidated before the queue runs.
    QSignalSpy lateSeeks(&controller, &EditorSyncController::previewSeekPublished);
    const bool submitted = controller.seekPreviewToEditorLocation(kDifficulty, kRevision, 2, 2);
    controller.setEditorReadiness(kDifficulty, kRevision + 2, true);
    flush();
    ok = ok && require(submitted && lateSeeks.isEmpty(),
                       QStringLiteral("a preview seek invalidated before delivery is dropped, not published late"), err);
    return ok;
}

bool verifyCaretPublishingFollowsTheSameRule(QTextStream& err)
{
    EditorSyncController controller;
    makeReady(controller);
    QSignalSpy carets(&controller, &EditorSyncController::caretLocationPublished);

    controller.setEditorContext(kDifficulty, kRevision + 1, 0, 0, true, false, 5, 3, true);
    flush();
    bool ok = require(carets.isEmpty(),
                      QStringLiteral("a caret published on an unready revision never reaches the preview"), err);

    controller.setEditorContext(kDifficulty, kRevision, 0, 0, true, false, 5, 3, true);
    flush();
    ok = ok && require(carets.count() == 1 && carets.first().at(2).toInt() == 5
                           && carets.first().at(3).toInt() == 3,
                       QStringLiteral("a caret on the ready identity is published once"), err);

    // Republishing the same location is not news; the preview would re-seek to
    // where it already is.
    controller.setEditorContext(kDifficulty, kRevision, 0, 0, true, false, 5, 3, true);
    flush();
    ok = ok && require(carets.count() == 1,
                       QStringLiteral("an unchanged caret location is not published twice"), err);
    return ok;
}

bool verifyTouchAuthoringDoesNotNeedEditorFocus(QTextStream& err)
{
    EditorSyncController controller;
    makeReady(controller);
    QSignalSpy holds(&controller, &EditorSyncController::touchPadControlHoldChanged);
    QSignalSpy requests(&controller, &EditorSyncController::touchPadAuthoringRequested);

    // Pressing the preview takes focus from the editor between the Ctrl press
    // and the click's release; the hold and the click must both survive it and
    // land at the caret the editor last published (a backward selection's
    // anchor is past its caret).
    controller.setEditorContext(kDifficulty, kRevision, 9, 4, true, false, 1, 5, false);
    controller.setTouchPadControlHold(true);
    flush();
    controller.setEditorContext(kDifficulty, kRevision, 9, 4, false, false, 1, 5, false);
    flush();
    bool ok = require(holds.count() == 1 && holds.first().at(0).toBool(),
                      QStringLiteral("losing editor focus does not drop a held Ctrl"), err);
    ok = ok
        && require(controller.requestTouchPadAuthoring(QStringLiteral("B7"), QLatin1Char(',')),
                   QStringLiteral("a touch click is accepted while the editor is unfocused"), err);
    flush();
    ok = ok
        && require(requests.count() == 1 && requests.first().at(4).toInt() == 9
                       && requests.first().at(5).toInt() == 4,
                   QStringLiteral("the touch request carries the published anchor and caret"), err);

    // IME composition still owns the caret, focused or not.
    controller.setEditorContext(kDifficulty, kRevision, 4, 4, false, true, 1, 5, false);
    flush();
    ok = ok
        && require(holds.count() == 2 && !holds.last().at(0).toBool()
                       && !controller.requestTouchPadAuthoring(QStringLiteral("B7"), QLatin1Char('/')),
                   QStringLiteral("IME composition releases the hold and refuses touch clicks"), err);
    return ok;
}

bool verifyFollowCarriesItsIdentityForTheEditorToGateOn(QTextStream& err)
{
    EditorSyncController controller;
    QSignalSpy changed(&controller, &EditorSyncController::followChanged);

    // Follow is a projection, not a request: the controller does not gate it,
    // it carries the publisher's identity through so the editor can refuse a
    // decoration that belongs to a revision it is no longer showing. That only
    // works while the publisher sends the same revision the editor compares
    // against — sending a neighbouring counter silently disables 代码跟随.
    EditorFollowState state;
    state.difficultyId = kDifficulty;
    state.revision = kRevision;
    state.start = 4;
    state.end = 9;
    state.caret = 6;
    state.active = true;
    controller.publishFollow(state);
    flush();

    bool ok = require(changed.count() == 1
                          && controller.followActive()
                          && controller.followDifficultyId() == kDifficulty
                          && controller.followRevision() == kRevision
                          && controller.followStart() == 4
                          && controller.followEnd() == 9
                          && controller.followCaret() == 6,
                      QStringLiteral("follow state reaches the editor with the publisher's identity intact"), err);

    controller.publishFollow(state);
    flush();
    ok = ok && require(changed.count() == 1,
                       QStringLiteral("an identical follow state does not wake the editor again"), err);

    controller.setPlaybackActive(true);
    flush();
    ok = ok && require(changed.count() == 2 && controller.followPlaybackActive(),
                       QStringLiteral("playback state rides the same projection"), err);
    return ok;
}

bool verifySelectionExportDelivery(QTextStream& err)
{
    EditorSyncController controller;
    QSignalSpy requested(&controller, &EditorSyncController::selectionRangeExportRequested);
    bool ok = require(!controller.requestSelectionRangeExport(kDifficulty, kRevision, 2, 9),
                      QStringLiteral("selection export requires the current editor"), err);
    makeReady(controller);
    ok &= require(controller.requestSelectionRangeExport(kDifficulty, kRevision, 2, 9),
                  QStringLiteral("ready selection export is accepted"), err);
    flush();
    ok &= require(requested.size() == 1 && requested.at(0).at(1).toInt() == 2
                      && requested.at(0).at(2).toInt() == 9,
                  QStringLiteral("selection export delivers the requested bounds"), err);
    requested.clear();
    controller.requestSelectionRangeExport(kDifficulty, kRevision, 4, 12);
    controller.setEditorReadiness(kDifficulty, kRevision + 1, true);
    flush();
    ok &= require(requested.isEmpty(),
                  QStringLiteral("revision changes discard pending selection exports"), err);
    return ok;
}

bool verifySelectionPlaybackUsesLiveIdentity(QTextStream& err)
{
    EditorSyncController controller;
    QSignalSpy context(&controller, &EditorSyncController::editorContextChanged);
    QSignalSpy requested(&controller, &EditorSyncController::selectionPlaybackRequested);
    makeReady(controller);
    controller.setEditorContext(kDifficulty,kRevision,9,2,true,false,1,3,false);
    controller.setSelectionPlayable(true);
    flush();
    context.clear();
    controller.setEditorContext(kDifficulty,kRevision,8,2,true,false,1,3,false);
    flush();
    bool ok = require(context.size() == 1,
        QStringLiteral("selection-only changes publish a fresh note-selection projection"),err);
    ok &= require(controller.playSelection() && requested.size() == 1
        && requested.first().at(0).toInt() == kDifficulty
        && requested.first().at(1).toULongLong() == kRevision
        && requested.first().at(2).toInt() == 2 && requested.first().at(3).toInt() == 8,
        QStringLiteral("Play Selection carries current revision and ordered UTF-16 bounds"),err);
    controller.setEditorReadiness(kDifficulty,kRevision+1,true);
    ok &= require(!controller.playSelection(),QStringLiteral("a stale source selection cannot start playback"),err);
    controller.setEditorContext(kDifficulty,kRevision+1,2,2,true,false,1,3,false);
    ok &= require(!controller.playSelection(),QStringLiteral("collapsed selections cannot start playback"),err);
    return ok;
}

bool verifyNoteCollectionsAreRevisionBound(QTextStream& err)
{
    EditorSyncController controller;
    makeReady(controller);
    controller.setEditorContext(kDifficulty, kRevision, 0, 0, true, false, 1, 1, false);
    using miacode::chart_selection::NoteRanges;
    const NoteRanges catalogue{{8,9},{10,11},{14,27}};
    controller.setNoteCatalogue(kDifficulty, kRevision, catalogue);
    QSignalSpy navigation(&controller, &EditorSyncController::navigationRequested);
    controller.requestNavigation(kDifficulty, kRevision, 0, 1, false, false);
    bool ok = require(controller.requestNoteSelection(kDifficulty, kRevision, {{16,17}}, false)
        && controller.editorSelection().ranges == NoteRanges{{14,27}}
        && controller.editorSelection().noteSelection,
        QStringLiteral("preview inputs snap to the complete chained Slide token"), err);
    flush();
    ok &= require(navigation.isEmpty(), QStringLiteral("a user note collection supersedes older queued caret navigation"), err);
    ok &= require(controller.requestNoteSelection(kDifficulty, kRevision, {{8,9}}, true)
        && controller.editorSelection().ranges == NoteRanges{{8,9},{14,27}},
        QStringLiteral("additive clicks preserve a noncontiguous collection without its intervening note"), err);
    ok &= require(controller.requestNoteSelection(kDifficulty, kRevision, {{8,9}}, true)
        && controller.editorSelection().ranges == NoteRanges{{14,27}},
        QStringLiteral("Shift on an already selected note removes only that note"), err);
    ok &= require(controller.requestNoteSelection(kDifficulty, kRevision, {{8,9},{14,27}}, true)
        && controller.editorSelection().ranges == NoteRanges{{8,9}},
        QStringLiteral("one rectangle toggles selected and unselected notes atomically"), err);
    ok &= require(controller.requestNoteSelection(kDifficulty, kRevision, {}, true)
        && controller.editorSelection().ranges == NoteRanges{{8,9}},
        QStringLiteral("an empty Shift rectangle leaves the collection intact"), err);
    ok &= require(controller.requestNoteSelection(kDifficulty, kRevision, {}, false)
        && controller.editorSelection().ranges.isEmpty(),
        QStringLiteral("an empty replacement selection clears the collection"), err);
    controller.setEditorReadiness(kDifficulty, kRevision + 1, true);
    ok &= require(!controller.requestNoteSelection(kDifficulty, kRevision, {{8,9}}, true)
        && !controller.requestNoteSelection(kDifficulty, kRevision + 1, {{8,9}}, true)
        && !controller.editorSelection().valid,
        QStringLiteral("revision changes discard old selections and reject the old token catalogue"), err);
    controller.setEditorReadiness(kDifficulty + 1, kRevision, true);
    ok &= require(!controller.requestNoteSelection(kDifficulty, kRevision, catalogue, false),
        QStringLiteral("a collection from another difficulty cannot enter the current editor"), err);
    return ok;
}

}  // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTextStream err(stderr);
    QTextStream out(stdout);
    const bool ok = verifyNoteCollectionsAreRevisionBound(err)
        && verifySelectionPlaybackUsesLiveIdentity(err)
        && verifySelectionExportDelivery(err)
        && verifyNavigationRequiresTheReadyIdentity(err)
        && verifyIdentityIsRecheckedAtDelivery(err)
        && verifyLosingTheEditorCancelsPendingWork(err)
        && verifyLocationPublishersShareTheGate(err)
        && verifyCaretPublishingFollowsTheSameRule(err)
        && verifyTouchAuthoringDoesNotNeedEditorFocus(err)
        && verifyFollowCarriesItsIdentityForTheEditorToGateOn(err);
    if (ok) {
        out << "editor_sync_controller_spec ok" << Qt::endl;
    }
    return ok ? 0 : 1;
}
