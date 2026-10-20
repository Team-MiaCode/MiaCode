#pragma once

#include <QJsonObject>
#include <QList>
#include <QSize>
#include <QStringList>

namespace miacode::cover_export {

class CoverLayoutModel;

struct CoverCompositionState {
    static constexpr int kCurrentVersion = 3;
    static constexpr char kDefaultOutputFile[] = "card.jpg";

    QSize size;
    QJsonObject background;
    QJsonObject card;
    QJsonObject layout;
    // Output destination retained in session snapshots and legacy compositions.
    // CoverExportSession persists it per chart project; shared layouts and
    // application preferences omit it. Relative paths use the chart folder.
    QString outputFile;

    QJsonObject toJson() const;
    static bool fromJson(const QJsonObject& root, CoverCompositionState* out, QString* errorMessage = nullptr);
    static bool supportsVersion(const QJsonObject& root);
    static QJsonObject migrateToCurrent(const QJsonObject& root);

};

}  // namespace miacode::cover_export
