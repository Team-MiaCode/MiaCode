#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

namespace miacode::api {

struct Validation {
    QJsonObject parameters;
    QJsonObject error;
    bool ok() const { return error.isEmpty(); }
};

// Uses the generated catalog; adapters never maintain independent parameter rules.
class ApiCatalog final {
public:
    static QJsonArray operations();
    static QJsonObject operation(const QString& operationId);
    static QJsonObject schema(const QString& name);
    static bool validRequestKey(const QString& value);
    static Validation validate(const QString& operationId, const QJsonObject& parameters);
    static bool validateResult(const QString& schemaName, const QJsonValue& result, QString* field = nullptr);
};

} // namespace miacode::api
