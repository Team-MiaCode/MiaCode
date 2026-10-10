#include "app/services/api/ApiDispatcher.h"
#include "app/services/api/ApiCatalog.h"

#include <QJsonDocument>
#include <QSysInfo>
#include <QTimeZone>
#include <QUuid>

namespace miacode::api {
namespace {
QString reference(const char* prefix)
{
    return QString::fromLatin1(prefix) + QUuid::createUuid().toString(QUuid::Id128);
}
int errorStatus(const QString& code)
{
    if (code == QLatin1String("auth.invalid_token")) return 401;
    if (code == QLatin1String("permission.denied")) return 403;
    if (code == QLatin1String("capability.unavailable")) return 501;
    if (code == QLatin1String("job.cursor_expired") || code == QLatin1String("query.expired")) return 410;
    if (code == QLatin1String("idempotency.conflict") || code == QLatin1String("job.invalid_state")) return 409;
    if (code == QLatin1String("internal.contract_violation")) return 500;
    if (code == QLatin1String("request.too_large")) return 413;
    return 400;
}
}

ApiDispatcher::ApiDispatcher(NetService& net, QString applicationVersion)
    : net_(net), applicationVersion_(std::move(applicationVersion)), hostInstanceId_(reference("host_"))
{
    clock_.start();
}

bool ApiDispatcher::authorized(const ApiContext& context, const QJsonObject& operation) const
{
    const auto scopes = operation.value(QStringLiteral("scopes")).toObject();
    for (const auto& scope : scopes.value(QStringLiteral("allOf")).toArray())
        if (!context.scopes.contains(scope.toString())) return false;
    const auto any = scopes.value(QStringLiteral("anyOf")).toArray();
    bool match = any.isEmpty();
    for (const auto& scope : any) match |= context.scopes.contains(scope.toString());
    return match;
}

bool ApiDispatcher::available(const QString& operation) const
{
    if (!ApiCatalog::operation(operation).value(QStringLiteral("adapters")).toObject().value(QStringLiteral("internal")).toBool()) return false;
    if (operation == QLatin1String("api.capabilities")) return true;
    if (operation.startsWith(QStringLiteral("net.")) || operation == QLatin1String("document.snapshot")) return net_.supports(operation);
    return operation == QLatin1String("jobs.list") || operation == QLatin1String("jobs.get")
        || operation == QLatin1String("jobs.items") || operation == QLatin1String("jobs.events")
        || operation == QLatin1String("jobs.cancel");
}

QJsonObject ApiDispatcher::capabilities(const ApiContext& context) const
{
    QJsonArray operations;
    for (const auto& value : ApiCatalog::operations()) {
        const auto descriptor = value.toObject();
        if (!authorized(context, descriptor)) continue;
        const QString id = descriptor.value(QStringLiteral("id")).toString();
        const bool supported = available(id);
        auto adapters = descriptor.value(QStringLiteral("adapters")).toObject();
        adapters.insert(QStringLiteral("internal"), supported);
        operations.append(QJsonObject{{QStringLiteral("id"), id}, {QStringLiteral("available"), supported},
            {QStringLiteral("reason"), supported ? QJsonValue(QJsonValue::Null) : QJsonValue(QStringLiteral("capability.unavailable"))},
            {QStringLiteral("scopes"), descriptor.value(QStringLiteral("scopes"))},
            {QStringLiteral("schemaVersion"), descriptor.value(QStringLiteral("schemaVersion"))},
            {QStringLiteral("adapters"), adapters}});
    }
    return {{QStringLiteral("apiVersion"), QStringLiteral("1.0")}, {QStringLiteral("hostInstanceId"), hostInstanceId_},
        {QStringLiteral("applicationVersion"), applicationVersion_}, {QStringLiteral("platform"), QSysInfo::productType()},
        {QStringLiteral("timeZone"), QString::fromUtf8(QTimeZone::systemTimeZoneId())}, {QStringLiteral("operations"), operations},
        {QStringLiteral("limits"), QJsonObject{{QStringLiteral("controlRequestBytes"), 1048576},
            {QStringLiteral("batchItems"), 500}, {QStringLiteral("pageItems"), 200},
            {QStringLiteral("jobRecordRetention"), QStringLiteral("process_lifetime")}}}};
}

ApiReply ApiDispatcher::dispatch(const ApiContext& context, const QString& operation, const QJsonObject& parameters)
{
    const QString requestId = ApiCatalog::validRequestKey(context.requestId) ? context.requestId : reference("req_");
    const auto failure = [&](const QJsonObject& error) -> ApiReply {
        return {errorStatus(error.value(QStringLiteral("code")).toString()),
            {{QStringLiteral("apiVersion"), QStringLiteral("1.0")}, {QStringLiteral("requestId"), requestId},
             {QStringLiteral("ok"), false}, {QStringLiteral("error"), error}}};
    };
    if ((!context.requestId.isEmpty() && !ApiCatalog::validRequestKey(context.requestId)) || (!context.idempotencyKey.isEmpty()
        && !ApiCatalog::validRequestKey(context.idempotencyKey))) return failure(jobError(QStringLiteral("request.invalid")));
    if (context.principal.isEmpty()) return failure(jobError(QStringLiteral("auth.invalid_token")));
    const auto descriptor = ApiCatalog::operation(operation);
    if (descriptor.isEmpty()) return failure(jobError(QStringLiteral("capability.unavailable")));
    if (!authorized(context, descriptor)) return failure(jobError(QStringLiteral("permission.denied")));
    if (QJsonDocument(parameters).toJson(QJsonDocument::Compact).size() > 1048576)
        return failure(jobError(QStringLiteral("request.too_large")));
    const auto validation = ApiCatalog::validate(operation, parameters);
    if (!validation.ok()) return failure(validation.error);
    if (descriptor.value(QStringLiteral("idempotency")) == QLatin1String("required") && context.idempotencyKey.isEmpty())
        return failure(jobError(QStringLiteral("request.invalid")));
    if (!available(operation)) return failure(jobError(QStringLiteral("capability.unavailable")));
    auto result = invoke(context, operation, validation.parameters);
    if (!result.ok()) return failure(result.error);
    if (!ApiCatalog::validateResult(descriptor.value(QStringLiteral("resultSchema")).toString(), result.result))
        return failure(jobError(QStringLiteral("internal.contract_violation")));
    return {descriptor.value(QStringLiteral("async")).toBool() ? 202 : 200,
        {{QStringLiteral("apiVersion"), QStringLiteral("1.0")}, {QStringLiteral("requestId"), requestId},
         {QStringLiteral("ok"), true}, {QStringLiteral("result"), result.result}}};
}

net::NetCallResult ApiDispatcher::invoke(const ApiContext& context, const QString& operation, const QJsonObject& parameters)
{
    if (operation == QLatin1String("api.capabilities")) return {capabilities(context), {}};
    if (operation == QLatin1String("document.snapshot")) return net_.call(context.principal, operation, parameters);
    if (operation.startsWith(QStringLiteral("net."))) {
        if (ApiCatalog::operation(operation).value(QStringLiteral("async")).toBool())
            return net_.start(context.principal, operation, parameters, context.idempotencyKey);
        return net_.call(context.principal, operation, parameters);
    }
    if (operation == QLatin1String("jobs.list") || operation == QLatin1String("jobs.items")) return page(context, operation, parameters);
    const QString jobId = parameters.value(QStringLiteral("jobId")).toString();
    if (net_.jobs().snapshot(jobId, context.principal).isEmpty()) return {{}, jobError(QStringLiteral("permission.denied"))};
    if (operation == QLatin1String("jobs.get")) return {net_.jobs().snapshot(jobId, context.principal), {}};
    if (operation == QLatin1String("jobs.cancel")) {
        net_.jobs().cancel(jobId, context.principal);
        return {net_.jobs().snapshot(jobId, context.principal), {}};
    }
    if (operation == QLatin1String("jobs.events")) {
        const auto events = net_.jobs().events(jobId, context.principal,
            parameters.value(QStringLiteral("after")).toString().toULongLong(), parameters.value(QStringLiteral("limit")).toInt());
        if (events.contains(QStringLiteral("error"))) return {{}, events.value(QStringLiteral("error")).toObject()};
        return {events, {}};
    }
    return {{}, jobError(QStringLiteral("capability.unavailable"))};
}

net::NetCallResult ApiDispatcher::page(const ApiContext& context, const QString& operation, const QJsonObject& parameters)
{
    const qint64 now = clock_.elapsed();
    for (auto it = cursors_.begin(); it != cursors_.end();) {
        if (it->expiresAt <= now) it = cursors_.erase(it);
        else ++it;
    }
    const QString jobId = parameters.value(QStringLiteral("jobId")).toString();
    const QString kind = parameters.value(QStringLiteral("kind")).toString();
    const QString state = parameters.value(QStringLiteral("state")).toString();
    const QString cursorRef = parameters.value(QStringLiteral("cursor")).toString();
    Cursor cursor;
    if (cursorRef.isEmpty()) {
        cursor = {context.principal, operation, jobId, kind, state, {}, {}, 0, now + 900000};
        if (operation == QLatin1String("jobs.list")) {
            for (const auto& value : net_.jobs().list(context.principal)) {
                const auto item = value.toObject();
                if ((!kind.isEmpty() && item.value(QStringLiteral("kind")) != kind)
                    || (!state.isEmpty() && item.value(QStringLiteral("state")) != state)) continue;
                cursor.items.append(item);
            }
        } else {
            const auto snapshot = net_.jobs().snapshot(jobId, context.principal);
            if (snapshot.isEmpty()) return {{}, jobError(QStringLiteral("permission.denied"))};
            cursor.items = net_.jobs().items(jobId, context.principal);
            cursor.version = snapshot.value(QStringLiteral("version"));
        }
    } else {
        const auto it = cursors_.constFind(cursorRef);
        if (it == cursors_.cend()) return {{}, jobError(QStringLiteral("job.cursor_expired"))};
        if (it->principal != context.principal) return {{}, jobError(QStringLiteral("permission.denied"))};
        if (it->operation != operation || it->jobId != jobId || it->kind != kind || it->state != state)
            return {{}, jobError(QStringLiteral("job.cursor_expired"))};
        cursor = *it;
    }
    QJsonArray items;
    const int limit = parameters.value(QStringLiteral("limit")).toInt();
    while (cursor.offset < cursor.items.size() && items.size() < limit) items.append(cursor.items[cursor.offset++]);
    QJsonValue next = QJsonValue::Null;
    if (cursor.offset < cursor.items.size()) {
        if (cursors_.size() >= 100) return {{}, jobError(QStringLiteral("job.cursor_expired"))};
        next = reference("cursor_");
        cursors_.insert(next.toString(), cursor);
    }
    QJsonObject result{{QStringLiteral("items"), items}, {QStringLiteral("nextCursor"), next}};
    if (operation == QLatin1String("jobs.items")) {
        result.insert(QStringLiteral("jobId"), jobId);
        result.insert(QStringLiteral("snapshotVersion"), cursor.version);
    }
    return {result, {}};
}

} // namespace miacode::api
