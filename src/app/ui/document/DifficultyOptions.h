#pragma once

#include "core/chart/document/SimaiDocument.h"

#include <QList>
#include <QVariantList>
#include <QVariantMap>

namespace miacode::ui {

inline QVariantList difficultyOptions(const QList<int>& ids)
{
    QVariantList options;
    options.reserve(ids.size());
    for (int id : ids) {
        options.append(QVariantMap{
            {QStringLiteral("id"), id},
            {QStringLiteral("name"), SimaiDocument::difficultyName(id)},
        });
    }
    return options;
}

} // namespace miacode::ui
