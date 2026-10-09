#pragma once
#include "MobileSfxMixer.h"
#include <QAudioSink>
#include <QAudioDecoder>
#include <QTimer>
#include <QMediaDevices>
#include <QMutex>
#include <QObject>
#include <QVariantMap>
#include <atomic>
#include <memory>
#include <thread>

namespace miacode::android {
class MobileSfxOutput final : public QObject {
    Q_OBJECT
public:
    explicit MobileSfxOutput(QObject* parent = nullptr);
    ~MobileSfxOutput() override;
    bool ready() const { return ready_ && (!introEnabled_ || introReady_); }
    QString assetError() const { return introEnabled_ && !introAssetError_.isEmpty() ? introAssetError_ : assetError_; }
    bool running() const { return running_; }
    void configure(const QVector<TimelineNoteMarker>& markers, double rate);
    void configureExportAudition(bool introEnabled, int clockCount, double clockBpm);
    void setIntroSoundFile(const QString& fileName);
    void setLevels(const PreviewAudioSettings& levels);
    bool start(double second, double rate);
    void pause();
    void seek(double second, double rate);
    void stop();
    void synchronize(double second);
    void setEndSecond(double second) { QMutexLocker guard(&mutex_); mixer_.setEndSecond(second); }
    double audibleSecond() const;
    QVariantMap diagnostics() const;
signals:
    void readyChanged();
    void failed(const QString& message);
    void outputDeviceChanged();
private:
    void render(QSpan<float> output);
    QMediaDevices devices_;
    QByteArray deviceId_;
    mutable QMutex mutex_;
    MobileSfxMixer mixer_;
    QVector<TimelineNoteMarker> markers_;
    PreviewAudioSettings levels_;
    std::unique_ptr<QAudioSink> sink_;
    std::atomic_bool stopping_{false};
    std::thread loader_;
    bool ready_ = false;
    bool introReady_ = true;
    QString introSource_;
    QString introAssetError_;
    quint64 introDecodeGeneration_ = 0;
    MobileSfxClip introClip_;
    std::unique_ptr<QAudioDecoder> introDecoder_;
    QTimer introDecodeTimeout_;
    QString assetError_;
    bool running_ = false;
    bool suspended_ = false;
    double rate_ = 1;
    bool introEnabled_ = false;
    int clockCount_ = 0;
    double clockBpm_ = 0;
    double introVolume_ = 1;
    double anchor_ = 0;
    double reference_ = 0;
    int divergence_ = 0;
    int outputSampleRate_ = 48000;
    int outputChannels_ = 2;
    QVector<float> scratch_;
    std::atomic_bool renderEnabled_{false};
    std::atomic<quint64> renderedFrames_{0};
    std::atomic<quint64> nonzeroFrames_{0};
};
}
