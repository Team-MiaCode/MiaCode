#include <QCoreApplication>
#include <QStringList>
#include <QString>
#include <QTextStream>

#include "app/runtime/playback/PlaybackIdentityGate.h"
#include "app/runtime/playback/SelectionPlaybackRange.h"

namespace {

bool require(bool condition, const QString& message, QTextStream& err)
{
    if (!condition) {
        err << "FAIL: " << message << Qt::endl;
    }
    return condition;
}

bool verifyPlaybackIdentityGateBehavior(QTextStream& err)
{
    miacode::runtime::PlaybackIdentityGate gate(41);
    gate.setDocumentRevision(9);
    const miacode::PlaybackCallbackStamp initial = gate.currentStamp();
    bool ok = require(gate.active()
                          && initial.sessionGeneration == 41
                          && initial.documentRevision == 9
                          && initial.playbackSequence == 0,
                      QStringLiteral("the identity gate starts with an explicit session and revision"), err);

    ok &= require(gate.advanceSequence() == 1,
                  QStringLiteral("one playback command advances exactly one sequence"), err);
    const miacode::PlaybackCallbackStamp current = gate.currentStamp();
    ok &= require(gate.accepts(current),
                  QStringLiteral("the current callback stamp is accepted"), err);
    ok &= require(!gate.accepts(initial),
                  QStringLiteral("a stale command sequence is rejected"), err);

    const miacode::PlaybackCallbackStamp wrongRevision{41, 10, 1};
    const miacode::PlaybackCallbackStamp wrongGeneration{42, 9, 1};
    ok &= require(!gate.accepts(wrongRevision) && !gate.accepts(wrongGeneration),
                  QStringLiteral("revision and generation mismatches are rejected"), err);

    gate.invalidate();
    const miacode::PlaybackCallbackStamp invalidated = gate.currentStamp();
    ok &= require(!gate.active()
                      && invalidated.sessionGeneration == 42
                      && invalidated.playbackSequence == 2
                      && !gate.accepts(invalidated)
                      && gate.advanceSequence() == 2,
                  QStringLiteral("invalidation rejects callbacks and freezes command sequencing"), err);
    gate.setDocumentRevision(11);
    ok &= require(!gate.active() && !gate.accepts(gate.currentStamp()),
                  QStringLiteral("a revision update cannot reactivate an invalidated session"), err);
    return ok;
}

bool verifySelectionAuditionBounds(QTextStream& err)
{
    miacode::runtime::SelectionPlaybackRange range;
    bool ok = require(!range.begin(3,7,4.0,3.0) && !range.active(),
        QStringLiteral("invalid selection bounds cannot arm an audition"),err);
    ok &= require(range.begin(3,7,2.5,8.75) && range.matches(3,7)
        && !range.matches(4,7) && !range.matches(3,8),
        QStringLiteral("selection playback retains the exact difficulty and chart revision"),err);
    ok &= require(!range.finished(8.7) && range.finished(8.75) && range.finished(9.0)
        && range.startSecond() == 2.5 && range.endSecond() == 8.75,
        QStringLiteral("one frozen range ends only when the shared chart clock reaches its complete tail"),err);
    range.cancel();
    ok &= require(!range.active() && !range.finished(9.0),
        QStringLiteral("stop or seek cancellation removes the selection terminal bound"),err);
    return ok;
}

}  // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTextStream err(stderr);
    bool ok = true;
    ok &= verifyPlaybackIdentityGateBehavior(err);
    ok &= verifySelectionAuditionBounds(err);
    if (ok) {
        QTextStream(stdout) << "playback_coordinator_spec: OK" << Qt::endl;
    }
    return ok ? 0 : 1;
}
