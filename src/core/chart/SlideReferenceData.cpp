#include "core/chart/SlideReferenceData.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonParseError>

namespace miacode::slide_reference {

const QJsonObject& root()
{
    static const QJsonObject root = []() {
        QFile file(QStringLiteral(":/data/slide_data.json"));
        if (!file.open(QIODevice::ReadOnly)) {
            return QJsonObject();
        }
        QJsonParseError error;
        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
        if (error.error != QJsonParseError::NoError || !doc.isObject()) {
            return QJsonObject();
        }
        return doc.object();
    }();
    return root;
}

}  // namespace miacode::slide_reference
