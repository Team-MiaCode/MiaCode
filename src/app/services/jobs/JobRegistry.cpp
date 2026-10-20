#include "app/services/jobs/JobRegistry.h"

#include <QDateTime>
#include <QUuid>

namespace miacode {
namespace {
QString now() { return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs); }
QString id(const char* prefix) { return QString::fromLatin1(prefix) + QUuid::createUuid().toString(QUuid::Id128); }
bool finalItem(const QString& state)
{
    return state == QLatin1String("succeeded") || state == QLatin1String("failed")
        || state == QLatin1String("cancelled") || state == QLatin1String("skipped")
        || state == QLatin1String("outcome_unknown");
}
}

QJsonObject jobError(const QString& code, const QString& message, bool retryable)
{
    return {{QStringLiteral("code"), code}, {QStringLiteral("message"), message.isEmpty() ? code : message},
            {QStringLiteral("retryable"), retryable}};
}

JobRegistry::JobRegistry(QObject* parent) : QObject(parent) {}

bool JobRegistry::isTerminal(const QString& state)
{
    return state == QLatin1String("succeeded") || state == QLatin1String("partial")
        || state == QLatin1String("failed") || state == QLatin1String("cancelled");
}

JobToken JobRegistry::create(const QString& principal, const QString& kind,
                             const QJsonArray& inputs, const QString& parentJobId)
{
    if (principal.isEmpty() || kind.isEmpty() || inputs.size() > 500
        || (!parentJobId.isEmpty() && owned(parentJobId, principal) == nullptr)) return {};
    const QString jobId = id("job_");
    Record record;
    record.principal = principal;
    int index = 0;
    for (const auto& input : inputs) {
        QJsonObject item{{QStringLiteral("itemId"), id("item_")},
            {QStringLiteral("inputOrder"), index++}, {QStringLiteral("state"), QStringLiteral("pending")},
            {QStringLiteral("phase"), QStringLiteral("queued")}, {QStringLiteral("attempt"), 0},
            {QStringLiteral("bytesReceived"), QStringLiteral("0")}, {QStringLiteral("bytesTotal"), QJsonValue::Null},
            {QStringLiteral("artifacts"), QJsonArray{}}, {QStringLiteral("error"), QJsonValue::Null}};
        // Only opaque references cross the registry boundary, never caller request bodies.
        const auto source = input.toObject();
        for (const auto& key : {QStringLiteral("chartRef"), QStringLiteral("materialRef"), QStringLiteral("parentItemId")}) {
            if (source.value(key).isString()) item.insert(key, source.value(key));
        }
        record.items.append(item);
    }
    record.snapshot = {{QStringLiteral("jobId"), jobId}, {QStringLiteral("kind"), kind},
        {QStringLiteral("state"), QStringLiteral("queued")}, {QStringLiteral("phase"), QStringLiteral("queued")},
        {QStringLiteral("createdAt"), now()}, {QStringLiteral("updatedAt"), now()},
        {QStringLiteral("result"), QJsonValue::Null}, {QStringLiteral("error"), QJsonValue::Null},
        {QStringLiteral("progress"), QJsonObject{{QStringLiteral("bytesReceived"), QStringLiteral("0")},
            {QStringLiteral("bytesTotal"), QJsonValue::Null}}}};
    if (!parentJobId.isEmpty()) record.snapshot.insert(QStringLiteral("parentJobId"), parentJobId);
    records_.insert(jobId, std::move(record));
    order_.append(jobId);
    publish(jobId, records_[jobId], QStringLiteral("job.updated"));
    return {jobId, 1};
}

const JobRegistry::Record* JobRegistry::owned(const QString& jobId, const QString& principal) const
{
    const auto it = records_.constFind(jobId);
    return it != records_.cend() && it->principal == principal ? &*it : nullptr;
}

JobRegistry::Record* JobRegistry::active(const JobToken& token)
{
    const auto it = records_.find(token.jobId);
    return it != records_.end() && it->epoch == token.epoch
        && !isTerminal(it->snapshot.value(QStringLiteral("state")).toString()) ? &*it : nullptr;
}

bool JobRegistry::accepts(const JobToken& token) const
{
    const auto it = records_.constFind(token.jobId);
    return it != records_.cend() && it->epoch == token.epoch
        && !isTerminal(it->snapshot.value(QStringLiteral("state")).toString());
}

QJsonObject JobRegistry::snapshot(const QString& jobId, const QString& principal) const
{
    const auto* record = owned(jobId, principal);
    return record ? record->snapshot : QJsonObject{};
}

QJsonArray JobRegistry::list(const QString& principal) const
{
    QJsonArray result;
    for (auto it = order_.crbegin(); it != order_.crend(); ++it) {
        if (const auto* record = owned(*it, principal)) result.append(record->snapshot);
    }
    return result;
}

QJsonArray JobRegistry::items(const QString& jobId, const QString& principal) const
{
    const auto* record = owned(jobId, principal);
    return record ? record->items : QJsonArray{};
}

QJsonObject JobRegistry::events(const QString& jobId, const QString& principal, quint64 after, int limit) const
{
    const auto* record = owned(jobId, principal);
    if (!record) return {{QStringLiteral("error"), jobError(QStringLiteral("permission.denied"))}};
    if (limit < 1 || limit > 200 || after > record->sequence) {
        return {{QStringLiteral("error"), jobError(QStringLiteral("request.invalid"))}};
    }
    const quint64 first = record->events.isEmpty() ? 1
        : record->events.first().toObject().value(QStringLiteral("sequence")).toString().toULongLong();
    if (after > 0 && after < first - 1) {
        return {{QStringLiteral("error"), jobError(QStringLiteral("job.cursor_expired"))}};
    }
    QJsonArray result;
    quint64 last = after;
    for (const auto& value : record->events) {
        const auto event = value.toObject();
        const quint64 sequence = event.value(QStringLiteral("sequence")).toString().toULongLong();
        if (sequence <= after) continue;
        result.append(event);
        last = sequence;
        if (result.size() == limit) break;
    }
    return {{QStringLiteral("events"), result}, {QStringLiteral("nextAfter"), QString::number(last)},
            {QStringLiteral("snapshotVersion"), QString::number(record->version)}};
}

QStringList JobRegistry::itemIds(const JobToken& token) const
{
    QStringList result;
    const auto it = records_.constFind(token.jobId);
    if (it == records_.cend() || it->epoch != token.epoch) return result;
    for (const auto& item : it->items) result.append(item.toObject().value(QStringLiteral("itemId")).toString());
    return result;
}

bool JobRegistry::setState(const JobToken& token, const QString& state, const QString& phase, const QJsonObject& error)
{
    auto* record = active(token);
    if (!record) return false;
    const QString previous = record->snapshot.value(QStringLiteral("state")).toString();
    const bool allowed = (previous == QLatin1String("queued") && state == QLatin1String("running"))
        || (previous == QLatin1String("running") && (state == QLatin1String("running")
            || state == QLatin1String("retry_wait") || state == QLatin1String("awaiting_user")
            || state == QLatin1String("blocked")))
        || ((previous == QLatin1String("retry_wait") || previous == QLatin1String("awaiting_user"))
            && state == QLatin1String("running"));
    if (!allowed) return false;
    record->snapshot.insert(QStringLiteral("state"), state);
    record->snapshot.insert(QStringLiteral("phase"), phase);
    record->snapshot.insert(QStringLiteral("error"), error.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(error));
    publish(token.jobId, *record, QStringLiteral("job.updated"));
    return true;
}

bool JobRegistry::updateItem(const JobToken& token, const QString& itemId, const QString& state,
    const QString& phase, const QJsonObject& error, const QJsonObject& progress, const QJsonArray& artifacts)
{
    auto* record = active(token);
    if (!record) return false;
    const QStringList validStates{QStringLiteral("pending"), QStringLiteral("running"), QStringLiteral("retry_wait"),
        QStringLiteral("blocked"), QStringLiteral("succeeded"), QStringLiteral("failed"),
        QStringLiteral("cancelled"), QStringLiteral("skipped"), QStringLiteral("outcome_unknown")};
    if (!validStates.contains(state)) return false;
    const bool cancelling = record->snapshot.value(QStringLiteral("state")) == QLatin1String("cancelling");
    if (cancelling && !finalItem(state)) return false;
    for (int index = 0; index < record->items.size(); ++index) {
        auto item = record->items[index].toObject();
        if (item.value(QStringLiteral("itemId")) != itemId) continue;
        if (finalItem(item.value(QStringLiteral("state")).toString())) return false;
        item.insert(QStringLiteral("state"), state);
        item.insert(QStringLiteral("phase"), phase);
        item.insert(QStringLiteral("error"), error.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(error));
        if (state != QLatin1String("retry_wait")) item.remove(QStringLiteral("retryAt"));
        for (const auto& key : {QStringLiteral("bytesReceived"), QStringLiteral("bytesTotal"), QStringLiteral("attempt"), QStringLiteral("retryAt")}) {
            if (progress.contains(key)) item.insert(key, progress.value(key));
        }
        if (!artifacts.isEmpty()) item.insert(QStringLiteral("artifacts"), artifacts);
        record->items[index] = item;
        publish(token.jobId, *record, QStringLiteral("item.updated"), itemId);
        return true;
    }
    return false;
}

QJsonObject JobRegistry::counts(const QJsonArray& items)
{
    QJsonObject result{{QStringLiteral("total"), items.size()}, {QStringLiteral("succeeded"), 0},
        {QStringLiteral("failed"), 0}, {QStringLiteral("pending"), 0}, {QStringLiteral("cancelled"), 0},
        {QStringLiteral("skipped"), 0}, {QStringLiteral("unknown"), 0}};
    for (const auto& value : items) {
        const QString state = value.toObject().value(QStringLiteral("state")).toString();
        const QString key = state == QLatin1String("outcome_unknown") ? QStringLiteral("unknown")
            : finalItem(state) ? state : QStringLiteral("pending");
        result.insert(key, result.value(key).toInt() + 1);
    }
    return result;
}

bool JobRegistry::finish(const JobToken& token, const QJsonValue& result, const QJsonObject& error)
{
    auto* record = active(token);
    if (!record) return false;
    const bool cancelled = record->snapshot.value(QStringLiteral("state")) == QLatin1String("cancelling");
    if (cancelled) {
        for (int index = 0; index < record->items.size(); ++index) {
            auto item = record->items[index].toObject();
            if (finalItem(item.value(QStringLiteral("state")).toString())) continue;
            item.insert(QStringLiteral("state"), QStringLiteral("cancelled"));
            record->items[index] = item;
        }
    }
    const auto summary = counts(record->items);
    const int succeeded = summary.value(QStringLiteral("succeeded")).toInt();
    const int unresolved = summary.value(QStringLiteral("failed")).toInt()
        + summary.value(QStringLiteral("unknown")).toInt() + summary.value(QStringLiteral("pending")).toInt();
    const QString state = cancelled ? QStringLiteral("cancelled")
        : error.isEmpty() && unresolved == 0 ? QStringLiteral("succeeded")
        : succeeded > 0 ? QStringLiteral("partial") : QStringLiteral("failed");
    record->snapshot.insert(QStringLiteral("state"), state);
    record->snapshot.insert(QStringLiteral("phase"), QStringLiteral("finished"));
    record->snapshot.insert(QStringLiteral("result"), result);
    record->snapshot.insert(QStringLiteral("error"), error.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(error));
    record->cancel = {};
    publish(token.jobId, *record, QStringLiteral("job.completed"));
    return true;
}

bool JobRegistry::setCancelHandler(const JobToken& token, std::function<void()> cancel)
{
    auto* record = active(token);
    if (!record) return false;
    record->cancel = std::move(cancel);
    return true;
}

bool JobRegistry::cancel(const QString& jobId, const QString& principal)
{
    auto it = records_.find(jobId);
    if (it == records_.end() || it->principal != principal) return false;
    const QString state = it->snapshot.value(QStringLiteral("state")).toString();
    if (isTerminal(state) || state == QLatin1String("cancelling")) return true;
    const JobToken token{jobId, it->epoch};
    const auto cancelHandler = it->cancel;
    it->snapshot.insert(QStringLiteral("state"), QStringLiteral("cancelling"));
    publish(jobId, *it, QStringLiteral("job.updated"));
    if (cancelHandler) cancelHandler();
    else finish(token);
    return true;
}

JobToken JobRegistry::resume(const QString& jobId, const QString& principal, quint64 expectedVersion)
{
    auto it = records_.find(jobId);
    if (it == records_.end() || it->principal != principal || it->version != expectedVersion
        || it->snapshot.value(QStringLiteral("state")) != QLatin1String("blocked")) return {};
    ++it->epoch;
    it->snapshot.insert(QStringLiteral("state"), QStringLiteral("queued"));
    it->snapshot.insert(QStringLiteral("error"), QJsonValue::Null);
    for (int index = 0; index < it->items.size(); ++index) {
        auto item = it->items[index].toObject();
        if (item.value(QStringLiteral("state")) == QLatin1String("blocked")) {
            item.insert(QStringLiteral("state"), QStringLiteral("pending"));
            it->items[index] = item;
        }
    }
    const JobToken token{jobId, it->epoch};
    publish(jobId, *it, QStringLiteral("job.updated"));
    return token;
}

void JobRegistry::cancelAll()
{
    const auto ids = order_;
    for (const auto& jobId : ids) {
        const QString principal = records_.value(jobId).principal;
        cancel(jobId, principal);
    }
}

void JobRegistry::publish(const QString& jobId, Record& record, const QString& type, const QString& itemId)
{
    record.snapshot.insert(QStringLiteral("version"), QString::number(++record.version));
    record.snapshot.insert(QStringLiteral("sequence"), QString::number(++record.sequence));
    record.snapshot.insert(QStringLiteral("updatedAt"), now());
    record.snapshot.insert(QStringLiteral("counts"), counts(record.items));
    QJsonObject event{{QStringLiteral("jobId"), jobId}, {QStringLiteral("sequence"), QString::number(record.sequence)},
        {QStringLiteral("type"), type}, {QStringLiteral("at"), now()},
        {QStringLiteral("payload"), record.snapshot}};
    if (!itemId.isEmpty()) event.insert(QStringLiteral("itemId"), itemId);
    if (!itemId.isEmpty()) {
        for (const auto& value : record.items) {
            const auto item = value.toObject();
            if (item.value(QStringLiteral("itemId")) == itemId) {
                event.insert(QStringLiteral("item"), item);
                break;
            }
        }
    }
    record.events.append(event);
    while (record.events.size() > 1000) record.events.removeAt(0);
    emit changed(jobId);
    emit eventPublished(event);
}

} // namespace miacode
