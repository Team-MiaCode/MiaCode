#include "MobileSfxOutput.h"
#include "audio/BassPreviewSfxSchedulerPolicy.h"
#include "common/AssetPaths.h"
#include "common/PreviewSfxAssets.h"
#include <QFile>
#include <QAudioBuffer>
#include <QCryptographicHash>
#include <QSaveFile>
#include <QStandardPaths>
#include <QMutexLocker>
#include <QVariantMap>
#include <cmath>

namespace miacode::android {
MobileSfxOutput::MobileSfxOutput(QObject* parent) : QObject(parent) {
    introSource_ = QDir(assets::assetPath("SFX")).filePath("track_start.wav");
    connect(&devices_, &QMediaDevices::audioOutputsChanged, this, [this] {
        if (!sink_ || deviceId_ == QMediaDevices::defaultAudioOutput().id()) return;
        stop(); sink_.reset(); emit outputDeviceChanged();
    });
    // Reading bundled WAVs never performs multimedia decoder work on the UI.
    loader_ = std::thread([this] {
        QHash<QString, MobileSfxClip> clips;
        QString failure;
        const QString directory = assets::assetPath("SFX");
        for (const auto* kind : {"answer", "judge", "judge_break", "slide", "break", "break_slide_start",
                "break_slide_finish", "break_slide_tail_break", "judge_break_slide", "ex", "touch", "touchhold", "firework", "clock", "track_start"}) {
            if (stopping_.load()) return;
            QFile file(QString::fromLatin1(kind) == "track_start" ? QDir(directory).filePath("track_start.wav")
                : preview_sfx::assetFilePathForKind(directory, QString::fromLatin1(kind)));
            MobileSfxClip clip;
            if (!file.open(QIODevice::ReadOnly) || file.size() > 64 * 1024 * 1024
                || !decodeMobileSfxWav(file.readAll(), &clip, &failure)) {
                failure = QStringLiteral("Cannot load preview SFX %1: %2").arg(QString::fromLatin1(kind), failure.isEmpty() ? file.errorString() : failure);
                break;
            }
            clips.insert(QString::fromLatin1(kind), std::move(clip));
        }
        QMetaObject::invokeMethod(this, [this, clips = std::move(clips), failure] () mutable {
            if (!failure.isEmpty()) { assetError_ = failure; emit failed(failure); return; }
            { QMutexLocker guard(&mutex_);
                if (!introClip_.samples.isEmpty()) clips.insert(QStringLiteral("track_start"), introClip_);
                mixer_.setClips(std::move(clips)); }
            ready_ = true; emit readyChanged();
        }, Qt::QueuedConnection);
    });
}
MobileSfxOutput::~MobileSfxOutput() {
    stop(); stopping_.store(true);
    introDecodeTimeout_.stop(); introDecoder_.reset();
    if (loader_.joinable()) loader_.join();
}
void MobileSfxOutput::setIntroSoundFile(const QString& fileName) {
    const QString requested = preview_sfx::assetFilePathForKind(assets::assetPath("SFX"), "track_start", fileName);
    if (requested == introSource_) return;
    const quint64 generation = ++introDecodeGeneration_;
    introSource_ = requested; introReady_ = false; introAssetError_.clear(); introClip_ = {};
    stop();
    introDecodeTimeout_.stop(); introDecodeTimeout_.disconnect(this);
    introDecoder_.reset();
    const auto fail = [this, generation](const QString& error) {
        if (generation != introDecodeGeneration_ || !introAssetError_.isEmpty()) return;
        introAssetError_ = error; introDecodeTimeout_.stop();
        if (introDecoder_) introDecoder_->stop();
        emit failed(error);
    };
    QString source = requested;
    if (source.startsWith("assets:") || source.startsWith(":")) {
        QFile input(source);
        if (!input.open(QIODevice::ReadOnly) || input.size() > 64 * 1024 * 1024) {
            fail(tr("无法读取片头音源：%1").arg(source)); return;
        }
        const auto data = input.readAll();
        if (input.error() != QFile::NoError || data.isEmpty()) { fail(tr("片头音源读取失败。")); return; }
        const QString directory = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/audio-audition";
        source = directory + '/' + QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex())
            + '.' + QFileInfo(requested).suffix();
        if (!QFile::exists(source)) {
            QSaveFile output(source);
            if (!QDir().mkpath(directory) || !output.open(QIODevice::WriteOnly)
                || output.write(data) != data.size() || !output.commit()) {
                fail(tr("无法准备本机片头音源：%1").arg(output.errorString())); return;
            }
        }
    }
    introDecoder_ = std::make_unique<QAudioDecoder>();
    auto* decoder = introDecoder_.get();
    QAudioFormat format;
    format.setSampleRate(48000); format.setChannelCount(2); format.setSampleFormat(QAudioFormat::Float);
    decoder->setAudioFormat(format); decoder->setSource(QUrl::fromLocalFile(source));
    auto clip = std::make_shared<MobileSfxClip>(); clip->sampleRate = 48000; clip->channels = 2;
    connect(decoder, &QAudioDecoder::bufferReady, this, [this, decoder, clip, format, fail, generation] {
        if (generation != introDecodeGeneration_ || !introAssetError_.isEmpty()) return;
        const auto buffer = decoder->read();
        if (!buffer.isValid()) return;
        if (buffer.format() != format || buffer.sampleCount() % 2 || clip->samples.size() + buffer.sampleCount() > 16 * 1024 * 1024) {
            fail(tr("片头音源 PCM 格式不支持或超过内存上限。")); return;
        }
        const auto* values = buffer.constData<float>();
        for (int i = 0; i < buffer.sampleCount(); ++i) {
            if (!std::isfinite(values[i])) { fail(tr("片头音源包含无效采样。")); return; }
        }
        const qsizetype previousSize = clip->samples.size();
        clip->samples.resize(previousSize + buffer.sampleCount());
        std::copy_n(values, buffer.sampleCount(), clip->samples.data() + previousSize);
    });
    connect(decoder, &QAudioDecoder::finished, this, [this, clip, fail, generation] {
        if (generation != introDecodeGeneration_ || !introAssetError_.isEmpty()) return;
        if (clip->samples.isEmpty()) { fail(tr("片头音源没有可播放的采样。")); return; }
        introDecodeTimeout_.stop(); introClip_ = std::move(*clip);
        { QMutexLocker guard(&mutex_); mixer_.setIntroClip(introClip_); }
        introReady_ = true; emit readyChanged();
    });
    connect(decoder, qOverload<QAudioDecoder::Error>(&QAudioDecoder::error), this, [this, decoder, fail, generation](QAudioDecoder::Error) {
        if (generation == introDecodeGeneration_) fail(decoder->errorString());
    });
    introDecodeTimeout_.setSingleShot(true);
    connect(&introDecodeTimeout_, &QTimer::timeout, this, [fail] { fail(tr("片头音源解码超时。")); });
    introDecodeTimeout_.start(30000); decoder->start();
}
void MobileSfxOutput::configure(const QVector<TimelineNoteMarker>& markers, double rate) {
    markers_ = markers;
    auto program = buildMobileExportAuditionProgram(markers, rate, levels_.mineSfxEnabled,
        introEnabled_, clockCount_, clockBpm_, introVolume_);
    const double second = running_ ? audibleSecond() : reference_;
    const bool resume = running_;
    stop();
    rate_ = preview_sfx_timing::normalizedPlaybackRate(rate);
    { QMutexLocker guard(&mutex_); mixer_.setProgram(std::move(program)); mixer_.reset(second, rate); }
    reference_ = second;
    if (resume) start(second, rate);
}
void MobileSfxOutput::configureExportAudition(bool introEnabled, int clockCount, double clockBpm) {
    if (introEnabled_ == introEnabled && clockCount_ == clockCount && clockBpm_ == clockBpm) return;
    introEnabled_ = introEnabled; clockCount_ = clockCount; clockBpm_ = clockBpm;
    configure(markers_, rate_);
}
void MobileSfxOutput::setLevels(const PreviewAudioSettings& levels) {
    const double introVolume = preview_sfx::selectedIntroSoundVolume();
    const bool rebuild = levels_.mineSfxEnabled != levels.mineSfxEnabled || introVolume_ != introVolume;
    introVolume_ = introVolume;
    levels_ = levels; levels_.normalize();
    { QMutexLocker guard(&mutex_); mixer_.setLevels(levels_); }
    if (rebuild) configure(markers_, rate_);
}
bool MobileSfxOutput::start(double second, double rate) {
    if (!ready()) return false;
    if (suspended_ && sink_ && qAbs(second - reference_) < 0.001 && rate == rate_) {
        suspended_ = false; running_ = true; renderEnabled_.store(true); sink_->resume(); return true;
    }
    stop();
    const auto device = QMediaDevices::defaultAudioOutput();
    if (device.isNull()) { emit failed(tr("没有可用的预览音频输出设备。")); return false; }
    QAudioFormat format;
    format.setSampleRate(48000); format.setChannelCount(2); format.setSampleFormat(QAudioFormat::Float);
    if (!device.isFormatSupported(format)) format = device.preferredFormat();
    if (format.sampleFormat() != QAudioFormat::Float && format.sampleFormat() != QAudioFormat::Int16) {
        format.setSampleFormat(QAudioFormat::Int16);
        if (!device.isFormatSupported(format)) { emit failed(tr("音频设备不支持预览 PCM 输出。")); return false; }
    }
    outputSampleRate_ = format.sampleRate(); outputChannels_ = format.channelCount();
    deviceId_ = device.id();
    rate_ = preview_sfx_timing::normalizedPlaybackRate(rate);
    reference_ = anchor_ = second; divergence_ = 0;
    { QMutexLocker guard(&mutex_); mixer_.reset(second, rate_); }
    renderedFrames_.store(0); nonzeroFrames_.store(0);
    sink_ = std::make_unique<QAudioSink>(device, format);
    connect(sink_.get(), &QAudioSink::stateChanged, this, [this](QtAudio::State state) {
        if (running_ && state == QtAudio::StoppedState && sink_->error() != QtAudio::NoError) {
            stop(); emit failed(tr("谱面音效输出设备已停止。"));
        }
    });
    sink_->setBufferFrameCount(1024);
    // Scratch is fixed before the device starts; larger backend callbacks are
    // filled in chunks without allocation on the audio thread.
    scratch_.resize(4096 * outputChannels_);
    running_ = true; renderEnabled_.store(true);
    if (format.sampleFormat() == QAudioFormat::Float) {
        sink_->start([this](QSpan<float> samples) { render(samples); });
    } else {
        sink_->start([this](QSpan<qint16> samples) {
            for (qsizetype at = 0; at < samples.size();) {
                const qsizetype count = qMin(samples.size() - at, scratch_.size());
                auto block = QSpan<float>(scratch_.data(), count); render(block);
                for (qsizetype i = 0; i < count; ++i) samples[at + i] = qint16(qRound(qBound(-1.0f, block[i], 1.0f) * 32767));
                at += count;
            }
        });
    }
    if (sink_->error() != QtAudio::NoError) { stop(); emit failed(tr("无法启动谱面音效输出。")); return false; }
    return true;
}
void MobileSfxOutput::render(QSpan<float> output) {
    if (!renderEnabled_.load()) { std::fill(output.begin(), output.end(), 0); return; }
    { QMutexLocker guard(&mutex_); mixer_.render(output, outputSampleRate_, outputChannels_); }
    quint64 nonzero = 0;
    for (qsizetype frame = 0; frame < output.size() / outputChannels_; ++frame) {
        for (int channel = 0; channel < outputChannels_; ++channel) {
            if (std::abs(output[frame * outputChannels_ + channel]) > 0.000001f) { ++nonzero; break; }
        }
    }
    renderedFrames_.fetch_add(output.size() / outputChannels_); nonzeroFrames_.fetch_add(nonzero);
}
void MobileSfxOutput::pause() {
    if (!running_) return;
    renderEnabled_.store(false); running_ = false; suspended_ = true;
    if (sink_) sink_->suspend();
}
void MobileSfxOutput::seek(double second, double rate) {
    const bool resume = running_; const bool rateChanged = rate != rate_;
    stop(); reference_ = second; rate_ = rate;
    // Answer timing depends on playback rate, so changing speed also replaces
    // the scheduled program, rather than only advancing the old one faster.
    if (rateChanged) {
        auto program = buildMobileExportAuditionProgram(markers_, rate, levels_.mineSfxEnabled,
            introEnabled_, clockCount_, clockBpm_, introVolume_);
        QMutexLocker guard(&mutex_); mixer_.setProgram(std::move(program)); mixer_.reset(second, rate);
    } else { QMutexLocker guard(&mutex_); mixer_.reset(second, rate); }
    if (resume) start(second, rate);
}
void MobileSfxOutput::stop() {
    renderEnabled_.store(false); running_ = false; suspended_ = false;
    if (sink_) sink_->reset();
}
double MobileSfxOutput::audibleSecond() const {
    // Qt 6.11.1 callback backends do not update the ringbuffer's processed
    // duration. Count frames requested by the device instead. This is the
    // submitted PCM clock; physical output latency remains a separate concern.
    return sink_ ? anchor_ + double(renderedFrames_.load()) / outputSampleRate_ * rate_ : reference_;
}
void MobileSfxOutput::synchronize(double second) {
    reference_ = second;
    if (!running_) return;
    using namespace preview_audio::bass;
    divergence_ = chainClockDiverged(audibleSecond(), second) ? divergence_ + 1 : 0;
    if (divergence_ >= kChainClockDivergenceStrikes) start(second, rate_);
}
QVariantMap MobileSfxOutput::diagnostics() const {
    QMutexLocker guard(&mutex_);
    return {{"ready", ready()}, {"running", running_}, {"suspended", suspended_},
        {"introSource", introSource_}, {"introReady", introReady_}, {"introVolume", introVolume_},
        {"introEnabled", introEnabled_}, {"clockCount", clockCount_}, {"clockBpm", clockBpm_},
        {"renderedFrames", qulonglong(renderedFrames_.load())}, {"nonzeroFrames", qulonglong(nonzeroFrames_.load())},
        {"processedUSecs", sink_ ? sink_->processedUSecs() : 0}, {"audioSecond", audibleSecond()},
        {"sampleRate", outputSampleRate_}, {"channels", outputChannels_},
        {"deviceState", sink_ ? int(sink_->state()) : int(QtAudio::StoppedState)},
        {"mixerSecond", mixer_.nextSecond()}, {"triggeredEvents", qulonglong(mixer_.triggeredEvents())},
        {"tapGain", previewSfxVolumeForKind(levels_, "judge")}, {"touchGain", previewSfxVolumeForKind(levels_, "touchhold")}};
}
}
