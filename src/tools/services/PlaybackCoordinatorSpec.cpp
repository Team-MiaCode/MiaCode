#include <QCoreApplication>
#include <QStringList>
#include <QString>
#include <QTextStream>

#include "app/runtime/playback/PlaybackIdentityGate.h"

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

}  // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTextStream err(stderr);
    bool ok = true;
    ok &= verifyPlaybackIdentityGateBehavior(err);
    if (ok) {
        QTextStream(stdout) << "playback_coordinator_spec: OK" << Qt::endl;
    }
    return ok ? 0 : 1;
}
