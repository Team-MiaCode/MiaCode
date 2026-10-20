#pragma once

#include "media_tools/media/PvBatchCompressionScanner.h"
#include "common/LocalizedText.h"

#include <QObject>

#include <atomic>

namespace miacode::media {

class PvBatchCompressionWorker : public QObject {
    Q_OBJECT

public:
    PvBatchCompressionWorker(QList<PvCompressionJob> jobs, std::atomic_bool* cancelRequested);

public slots:
    void run();

signals:
    void rowStatus(int row, const miacode::LocalizedText& status);
    void progress(int completed);
    void summary(const miacode::LocalizedText& message);
    void finished(int succeeded, int failed, bool canceled, const miacode::LocalizedText& fatalError);

private:
    bool isCanceled() const;

    QList<PvCompressionJob> jobs_;
    std::atomic_bool* cancelRequested_ = nullptr;
};

QString resolvePvCompressionFfmpegExecutable();

}  // namespace miacode::media
