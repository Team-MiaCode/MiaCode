#include "app/services/jobs/JobRegistry.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QTextStream>

namespace {
bool check(bool condition, const char* message)
{
    if (!condition) QTextStream(stderr) << "FAIL: " << message << '\n';
    return condition;
}
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    using namespace miacode;
    JobRegistry registry;
    const QString owner = QStringLiteral("local");
    const QString other = QStringLiteral("other");
    const auto job = registry.create(owner, QStringLiteral("net.upload"), QJsonArray{
        QJsonObject{{"chartRef", "chart-a"}, {"password", "secret"}, {"path", "private-path"}},
        QJsonObject{{"chartRef", "chart-b"}}, QJsonObject{{"chartRef", "chart-c"}}});
    bool ok = check(job.isValid(), "job is created with an opaque identity");
    const auto ids = registry.itemIds(job);
    ok &= check(ids.size() == 3 && ids[0] != ids[1], "items have stable distinct identities");
    ok &= check(registry.snapshot(job.jobId, other).isEmpty()
        && registry.items(job.jobId, other).isEmpty() && !registry.cancel(job.jobId, other),
        "another principal cannot read or cancel a task");
    const auto json = QJsonDocument(registry.items(job.jobId, owner)).toJson();
    ok &= check(!json.contains("secret") && !json.contains("private-path"), "registry accepts only opaque input references");
    ok &= check(registry.snapshot(job.jobId, owner).value("version").isString(), "versions use decimal strings");
    ok &= check(registry.setState(job, QStringLiteral("running"), QStringLiteral("upload")), "queued transitions to running");
    ok &= check(!registry.setState(job, QStringLiteral("succeeded"), QStringLiteral("finished")), "terminal state is derived from items");
    registry.updateItem(job, ids[0], QStringLiteral("succeeded"), QStringLiteral("uploaded"));
    registry.updateItem(job, ids[1], QStringLiteral("running"), QStringLiteral("upload"));
    int cancellations = 0;
    registry.setCancelHandler(job, [&] { ++cancellations; });
    registry.cancel(job.jobId, owner);
    registry.cancel(job.jobId, owner);
    ok &= check(cancellations == 1 && registry.snapshot(job.jobId, owner).value("state") == "cancelling",
        "cancel is idempotent and waits for owner completion");
    registry.updateItem(job, ids[1], QStringLiteral("outcome_unknown"), QStringLiteral("cancelled_after_send"));
    registry.finish(job);
    const auto final = registry.snapshot(job.jobId, owner);
    const auto counts = final.value("counts").toObject();
    ok &= check(final.value("state") == "cancelled" && counts.value("succeeded") == 1
        && counts.value("unknown") == 1 && counts.value("cancelled") == 1
        && counts.value("pending") == 0, "cancel preserves success and uncertain uploads and settles pending items");
    ok &= check(!registry.updateItem(job, ids[0], QStringLiteral("failed"), QStringLiteral("late"))
        && !registry.finish(job, QJsonObject{}), "late events cannot rewrite terminal results");
    registry.cancel(job.jobId, owner);
    ok &= check(registry.snapshot(job.jobId, owner) == final, "cancel of completed task preserves snapshot");

    const auto blocked = registry.create(owner, QStringLiteral("net.download"), QJsonArray{QJsonObject{}});
    const auto blockedId = registry.itemIds(blocked).first();
    registry.setState(blocked, QStringLiteral("running"), QStringLiteral("download"));
    registry.updateItem(blocked, blockedId, QStringLiteral("retry_wait"), QStringLiteral("download"), {},
        {{"retryAt", "2026-10-10T00:00:05.000Z"}});
    ok &= check(registry.items(blocked.jobId, owner).first().toObject().value("retryAt") == "2026-10-10T00:00:05.000Z",
        "retry deadline is available to task consumers");
    registry.updateItem(blocked, blockedId, QStringLiteral("running"), QStringLiteral("download"));
    ok &= check(!registry.items(blocked.jobId, owner).first().toObject().contains("retryAt"),
        "running item clears its expired retry deadline");
    registry.updateItem(blocked, blockedId, QStringLiteral("blocked"), QStringLiteral("challenge"));
    registry.setState(blocked, QStringLiteral("blocked"), QStringLiteral("challenge"));
    const quint64 version = registry.snapshot(blocked.jobId, owner).value("version").toString().toULongLong();
    ok &= check(!registry.resume(blocked.jobId, owner, version - 1).isValid(), "resume rejects a stale version");
    const auto resumed = registry.resume(blocked.jobId, owner, version);
    ok &= check(resumed.isValid() && resumed.epoch != blocked.epoch, "resume starts a new execution epoch");
    ok &= check(!registry.updateItem(blocked, blockedId, QStringLiteral("failed"), QStringLiteral("late")),
        "old execution cannot change resumed items");
    registry.setState(resumed, QStringLiteral("running"), QStringLiteral("download"));
    registry.updateItem(resumed, blockedId, QStringLiteral("failed"), QStringLiteral("network"));
    registry.finish(resumed);
    ok &= check(registry.snapshot(resumed.jobId, owner).value("state") == "failed", "all failed derives failed state");
    const auto parent = registry.create(owner, QStringLiteral("net.download"), {}, job.jobId);
    ok &= check(parent.isValid() && registry.snapshot(parent.jobId, owner).value("parentJobId") == job.jobId,
        "retry lineage references existing owned task");
    ok &= check(!registry.create(other, QStringLiteral("net.download"), {}, job.jobId).isValid(),
        "retry cannot reference another owner's task");

    const auto events = registry.create(owner, QStringLiteral("net.probe"));
    registry.setState(events, QStringLiteral("running"), QStringLiteral("probe"));
    for (int i = 0; i < 1005; ++i) registry.setState(events, QStringLiteral("running"), QString::number(i));
    ok &= check(registry.events(events.jobId, owner, 1).value("error").toObject().value("code") == "job.cursor_expired",
        "event retention reports expired cursors");
    const auto page = registry.events(events.jobId, owner, 0, 3);
    ok &= check(page.value("events").toArray().size() == 3 && page.value("nextAfter").isString(),
        "events page respects limit and string sequence");
    registry.cancelAll();
    ok &= check(registry.snapshot(events.jobId, owner).value("state") == "cancelled", "shutdown settles an unowned active execution");
    return ok ? 0 : 1;
}
