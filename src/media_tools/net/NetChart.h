#pragma once

#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>

namespace miacode::net {

struct NetChartSummary {
    QString id;
    QString title;
    QString artist;
    QString designer;
    QString uploader;
    QString hash;
    QStringList levels;
    QStringList publicTags;
    QDateTime timestampUtc;
};

} // namespace miacode::net
