#pragma once

#include <QHash>
#include <QImage>
#include <QObject>
#include <QString>
#include <QStringList>

class QProcess;

namespace miacode::cover_export {

// PV frames for chart-frame layers in frameBgMode "pv". The PV is aligned to
// the chart timeline (chart second == media second, as in the video export),
// so a layer's frameSeconds picks its frame directly.
//
// Each decode is one input-seeked ffmpeg run (~150 ms for a 720p H.264 PV),
// uncropped, its shorter side scaled to at most `sidePx` (never upscaled).
// The async path runs in the ffmpeg child process, so the GUI thread only pays
// for the QImage load of a small BMP. Requests are latest-wins per layer key:
// scrubbing queues at most one pending decode behind the running one, and only
// the newest result per key is emitted.
//
// GUI-thread object.
class CoverPvFrameSource final : public QObject
{
    Q_OBJECT

public:
    explicit CoverPvFrameSource(QObject* parent = nullptr);
    ~CoverPvFrameSource() override;

    // Blocking decode for the export render. Null image + *errorMessage on failure.
    static QImage decodeFrame(const QString& mediaPath, double seconds, int sidePx,
                              QString* errorMessage = nullptr);

    void request(const QString& key, const QString& mediaPath, double seconds, int sidePx);
    void cancel(const QString& key);
    void cancelAll();

signals:
    void frameReady(const QString& key, double seconds, const QImage& image);
    void frameFailed(const QString& key, const QString& errorMessage);

private:
    struct Request {
        QString mediaPath;
        double seconds = 0.0;
        int sidePx = 0;
    };
    struct Slot {
        QProcess* process = nullptr;
        Request running;
        bool hasPending = false;
        Request pending;
    };

    static QStringList decodeArguments(const Request& request);
    static QString cacheKey(const Request& request);
    void start(const QString& key, const Request& request);
    void onFinished(const QString& key, QProcess* process);
    void remember(const QString& cacheKey, const QImage& image);

    QHash<QString, Slot> slots_;
    QHash<QString, QImage> cache_;
    QStringList cacheOrder_;
};

}  // namespace miacode::cover_export
