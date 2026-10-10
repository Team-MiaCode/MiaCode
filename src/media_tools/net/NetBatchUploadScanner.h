#pragma once

#include <QList>
#include <QMetaType>
#include <QString>

namespace miacode::net {

struct NetUploadJob {
    QString directoryPath;
    QString displayName;
    QString chartPath;
    QString backgroundPath;
    QString trackPath;
    QString videoPath;
    bool selected = true;
};

struct NetUploadScanRejection {
    QString displayName;
    QString reason;
};

QList<NetUploadJob> scanNetUploadFolders(const QString& rootDirectory, QList<NetUploadScanRejection>* rejected = nullptr);
int appendUniqueNetUploadJobs(QList<NetUploadJob>* queue, const QList<NetUploadJob>& candidates);

}  // namespace miacode::net

Q_DECLARE_METATYPE(miacode::net::NetUploadJob)
