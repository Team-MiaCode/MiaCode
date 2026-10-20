#include "app/services/api/ApiCatalog.h"
#include "app/services/api/generated/NetApiCatalogData.h"

#include <QDate>
#include <QDateTime>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QTimeZone>
#include <QUrl>

#include <cmath>

namespace miacode::api {
namespace {
const QJsonObject& data()
{
    static const auto value = [] {
        QByteArray json;
        for (const auto* chunk : generated::catalogChunks) json.append(chunk);
        return QJsonDocument::fromJson(json).object();
    }();
    return value;
}

QJsonObject invalid(const QString& field)
{
    return {{QStringLiteral("code"), QStringLiteral("request.invalid")},
        {QStringLiteral("message"), QStringLiteral("Request does not match the operation contract.")},
        {QStringLiteral("retryable"), false}, {QStringLiteral("field"), field}};
}

bool matchesType(const QJsonValue& value, const QString& type)
{
    if (type == QLatin1String("object")) return value.isObject();
    if (type == QLatin1String("array")) return value.isArray();
    if (type == QLatin1String("string")) return value.isString();
    if (type == QLatin1String("boolean")) return value.isBool();
    if (type == QLatin1String("null")) return value.isNull();
    if (type == QLatin1String("number")) return value.isDouble() && std::isfinite(value.toDouble());
    if (type == QLatin1String("integer")) return value.isDouble() && std::isfinite(value.toDouble())
        && std::floor(value.toDouble()) == value.toDouble();
    return false;
}

bool validateValue(const QJsonObject& rules, QJsonValue& value, const QString& path,
                   bool normalize, QString& field, int depth = 0)
{
    const auto fail = [&] { field = path; return false; };
    if (depth > 64) return fail();
    if (rules.contains(QStringLiteral("$ref"))) {
        const auto name = rules.value(QStringLiteral("$ref")).toString().mid(8);
        const auto target = ApiCatalog::schema(name);
        return !target.isEmpty() && validateValue(target, value, path, normalize, field, depth + 1);
    }
    for (const auto& keyword : {QStringLiteral("oneOf"), QStringLiteral("anyOf")}) {
        if (!rules.contains(keyword)) continue;
        int matched = 0;
        QJsonValue accepted;
        for (const auto& branch : rules.value(keyword).toArray()) {
            auto candidate = value;
            QString ignored;
            if (validateValue(branch.toObject(), candidate, path, normalize, ignored, depth + 1)) {
                ++matched;
                accepted = candidate;
            }
        }
        if (matched == 0 || (keyword == QLatin1String("oneOf") && matched != 1)) return fail();
        value = accepted;
    }
    if (normalize && value.isString()) {
        QString text = value.toString();
        if (rules.value(QStringLiteral("x-trim")).toBool()) text = text.trimmed();
        if (rules.value(QStringLiteral("x-normalize")) == QLatin1String("tag")
            && text.startsWith(QStringLiteral("tag:"), Qt::CaseInsensitive)) text = text.mid(4).trimmed();
        if (rules.value(QStringLiteral("x-format")) == QLatin1String("time-zone")
            && text == QLatin1String("system")) text = QString::fromUtf8(QTimeZone::systemTimeZoneId());
        value = text;
    }
    const auto type = rules.value(QStringLiteral("type"));
    if (type.isString() && !matchesType(value, type.toString())) return fail();
    if (type.isArray()) {
        bool found = false;
        for (const auto& candidate : type.toArray()) found |= matchesType(value, candidate.toString());
        if (!found) return fail();
    }
    if (rules.contains(QStringLiteral("const")) && value != rules.value(QStringLiteral("const"))) return fail();
    if (rules.contains(QStringLiteral("enum")) && !rules.value(QStringLiteral("enum")).toArray().contains(value)) return fail();
    if (value.isObject()) {
        auto object = value.toObject();
        const auto properties = rules.value(QStringLiteral("properties")).toObject();
        for (const auto& required : rules.value(QStringLiteral("required")).toArray()) {
            if (!object.contains(required.toString())) { field = path + "." + required.toString(); return false; }
        }
        for (auto it = properties.begin(); it != properties.end(); ++it) {
            const auto property = it.value().toObject();
            if (!object.contains(it.key()) && normalize && property.contains(QStringLiteral("default")))
                object.insert(it.key(), property.value(QStringLiteral("default")));
        }
        if (object.size() < rules.value(QStringLiteral("minProperties")).toInt(0)) return fail();
        for (auto it = object.begin(); it != object.end(); ++it) {
            if (!properties.contains(it.key())) {
                if (rules.value(QStringLiteral("additionalProperties")) == QJsonValue(false)) {
                    field = path + "." + it.key(); return false;
                }
                continue;
            }
            QJsonValue member = it.value();
            if (!validateValue(properties.value(it.key()).toObject(), member, path + "." + it.key(), normalize, field, depth + 1)) return false;
            it.value() = member;
        }
        value = object;
    }
    if (value.isArray()) {
        auto array = value.toArray();
        if (array.size() < rules.value(QStringLiteral("minItems")).toInt(0)
            || (rules.contains(QStringLiteral("maxItems")) && array.size() > rules.value(QStringLiteral("maxItems")).toInt())) return fail();
        for (int index = 0; index < array.size(); ++index) {
            QJsonValue element = array[index];
            if (!validateValue(rules.value(QStringLiteral("items")).toObject(), element,
                               path + QStringLiteral("[%1]").arg(index), normalize, field, depth + 1)) return false;
            array[index] = element;
            if (rules.value(QStringLiteral("uniqueItems")).toBool()) {
                for (int previous = 0; previous < index; ++previous) if (array[previous] == element) return fail();
            }
        }
        value = array;
    }
    if (value.isDouble()) {
        const double number = value.toDouble();
        if ((rules.contains(QStringLiteral("minimum")) && number < rules.value(QStringLiteral("minimum")).toDouble())
            || (rules.contains(QStringLiteral("maximum")) && number > rules.value(QStringLiteral("maximum")).toDouble())) return fail();
    }
    if (value.isString()) {
        const QString text = value.toString();
        const auto length = text.toUcs4().size();
        if (length < rules.value(QStringLiteral("minLength")).toInt(0)
            || (rules.contains(QStringLiteral("maxLength")) && length > rules.value(QStringLiteral("maxLength")).toInt())) return fail();
        if (rules.contains(QStringLiteral("pattern"))
            && !QRegularExpression(rules.value(QStringLiteral("pattern")).toString()).match(text).hasMatch()) return fail();
        if (rules.value(QStringLiteral("x-uint64")).toBool()) {
            bool valid = false;
            text.toULongLong(&valid);
            if (!valid) return fail();
        }
        if (rules.value(QStringLiteral("x-format")) == QLatin1String("time-zone")
            && !QTimeZone(text.toUtf8()).isValid()) return fail();
        const QString format = rules.value(QStringLiteral("format")).toString();
        if (format == QLatin1String("date")) {
            const auto date = QDate::fromString(text, Qt::ISODate);
            if (!date.isValid() || date.toString(Qt::ISODate) != text) return fail();
        } else if (format == QLatin1String("date-time")) {
            if (!text.endsWith(QLatin1Char('Z')) || !QDateTime::fromString(text, Qt::ISODateWithMs).isValid()) return fail();
        } else if (format == QLatin1String("uri")) {
            const QUrl url(text, QUrl::StrictMode);
            if (!url.isValid() || url.isRelative() || url.host().isEmpty()) return fail();
        }
    }
    return true;
}
}

QJsonArray ApiCatalog::operations()
{
    return data().value(QStringLiteral("catalog")).toObject().value(QStringLiteral("operations")).toArray();
}

QJsonObject ApiCatalog::operation(const QString& operationId)
{
    for (const auto& value : operations()) {
        const auto item = value.toObject();
        if (item.value(QStringLiteral("id")) == operationId) return item;
    }
    return {};
}

QJsonObject ApiCatalog::schema(const QString& name)
{
    return data().value(QStringLiteral("schemas")).toObject().value(QStringLiteral("$defs")).toObject().value(name).toObject();
}

bool ApiCatalog::validRequestKey(const QString& value)
{
    static const QRegularExpression safe(QStringLiteral("^[A-Za-z0-9_.:-]{1,128}$"));
    return safe.match(value).hasMatch();
}

Validation ApiCatalog::validate(const QString& operationId, const QJsonObject& parameters)
{
    const auto descriptor = operation(operationId);
    if (descriptor.isEmpty()) return {{}, {{QStringLiteral("code"), QStringLiteral("capability.unavailable")},
        {QStringLiteral("message"), QStringLiteral("Operation is not registered.")}, {QStringLiteral("retryable"), false}}};
    if (QJsonDocument(parameters).toJson(QJsonDocument::Compact).size() > 1048576) return {{}, invalid(QStringLiteral("$"))};
    QJsonValue normalized = parameters;
    QString field;
    if (!validateValue(schema(descriptor.value(QStringLiteral("requestSchema")).toString()), normalized,
                       QStringLiteral("$"), true, field)) return {{}, invalid(field)};
    auto values = normalized.toObject();
    if (operationId == QLatin1String("net.queries.create")) {
        if (values.value(QStringLiteral("uploader")).toString().isEmpty()
            && values.value(QStringLiteral("tag")).toString().isEmpty()
            && values.value(QStringLiteral("title")).toString().isEmpty()) return {{}, invalid(QStringLiteral("$.title"))};
        if (values.contains(QStringLiteral("startDate")) != values.contains(QStringLiteral("endDate"))) return {{}, invalid(QStringLiteral("$.endDate"))};
        if (values.value(QStringLiteral("startDate")).toString() > values.value(QStringLiteral("endDate")).toString()) {
            const auto start = values.value(QStringLiteral("startDate"));
            values.insert(QStringLiteral("startDate"), values.value(QStringLiteral("endDate")));
            values.insert(QStringLiteral("endDate"), start);
        }
        if (values.contains(QStringLiteral("endDate"))
            && !QDate::fromString(values.value(QStringLiteral("endDate")).toString(), Qt::ISODate).addDays(1).isValid())
            return {{}, invalid(QStringLiteral("$.endDate"))};
    }
    if (operationId.startsWith(QStringLiteral("network.http."))) {
        const QUrl url(values.value(QStringLiteral("url")).toString(), QUrl::StrictMode);
        if ((url.scheme() != QLatin1String("http") && url.scheme() != QLatin1String("https"))
            || !url.userInfo().isEmpty() || url.hasFragment()) return {{}, invalid(QStringLiteral("$.url"))};
    }
    if (operationId == QLatin1String("network.proxy.set")) {
        const QString mode = values.value(QStringLiteral("mode")).toString();
        const bool remote = mode == QLatin1String("http_connect") || mode == QLatin1String("socks5");
        if (remote != values.contains(QStringLiteral("host")) || remote != values.contains(QStringLiteral("port")))
            return {{}, invalid(QStringLiteral("$.host"))};
        const bool existing = values.value(QStringLiteral("credentialMode")) == QLatin1String("existing");
        if (existing != values.contains(QStringLiteral("credentialRef"))
            || (!remote && values.value(QStringLiteral("credentialMode")) != QLatin1String("none")))
            return {{}, invalid(QStringLiteral("$.credentialRef"))};
    }
    return {values, {}};
}

bool ApiCatalog::validateResult(const QString& schemaName, const QJsonValue& result, QString* field)
{
    const auto rules = schema(schemaName);
    if (rules.isEmpty()) return false;
    auto value = result;
    QString failure;
    const bool valid = validateValue(rules, value, QStringLiteral("$"), false, failure);
    if (field) *field = failure;
    return valid;
}

} // namespace miacode::api
