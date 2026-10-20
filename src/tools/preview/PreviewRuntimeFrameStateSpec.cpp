#include "core/scene/PreviewPreparedSceneCache.h"
#include "preview/runtime/PreviewRuntime.h"

#include <QGuiApplication>
#include <QTextStream>

using miacode::preview::scene::PreviewPreparedSceneCache;

namespace {

bool require(bool condition, const QString& message, QTextStream& err)
{
    if (!condition) {
        err << "FAIL: " << message << Qt::endl;
    }
    return condition;
}

QVector<TimelineNoteMarker> tapMarkers(int count)
{
    QVector<TimelineNoteMarker> markers;
    for (int index = 0; index < count; ++index) {
        TimelineNoteMarker marker;
        marker.type = QStringLiteral("tap");
        marker.second = 1.0 + index * 0.5;
        marker.lane = 1 + index % 8;
        marker.endLane = marker.lane;
        markers.append(marker);
    }
    return markers;
}

// Issue #48: each PreviewQuickSceneRoot keeps its own prepared cache keyed on
// (sceneContentRevision, sceneStorageGeneration). A root that cached an empty
// marker set must rebuild once the runtime publishes a chart, even when a
// non-releasing reset() sits between the two revisions.
bool verifyResetNeverReplaysACachedRevision(QTextStream& err)
{
    PreviewRuntime runtime;
    PreviewPreparedSceneCache rootCache;

    runtime.setNoteMarkers({});
    rootCache.sync(*runtime.frameStateSnapshot());
    bool ok = require(rootCache.headLayer().entries.isEmpty(),
                      QStringLiteral("root caches the empty marker set"), err);

    const quint64 revisionBeforeReset = runtime.frameState().sceneContentRevision;
    const quint64 generationBeforeReset = runtime.frameState().sceneStorageGeneration;
    runtime.reset(false);
    ok &= require(runtime.frameState().sceneStorageGeneration == generationBeforeReset,
                  QStringLiteral("non-releasing reset keeps the storage generation"), err);
    ok &= require(runtime.frameState().sceneContentRevision > revisionBeforeReset,
                  QStringLiteral("reset advances the scene revision instead of rewinding it (%1 -> %2)")
                      .arg(revisionBeforeReset)
                      .arg(runtime.frameState().sceneContentRevision), err);

    runtime.setNoteMarkers(tapMarkers(3));
    ok &= require(rootCache.sync(*runtime.frameStateSnapshot()),
                  QStringLiteral("root cache rebuilds for the chart published after reset"), err);
    ok &= require(rootCache.headLayer().entries.size() == 3,
                  QStringLiteral("rebuilt head layer holds the new chart's notes (got %1)")
                      .arg(rootCache.headLayer().entries.size()), err);
    return ok;
}

}  // namespace

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    QTextStream err(stderr);
    bool ok = verifyResetNeverReplaysACachedRevision(err);
    if (!ok) {
        return 1;
    }
    QTextStream(stdout) << "preview_runtime_frame_state_spec ok" << Qt::endl;
    return 0;
}
