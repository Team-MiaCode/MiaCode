#include "audio/bass/OfflineAudioDecoder.h"

#include <algorithm>
#include <climits>
#include <cmath>

#include <QFile>
#include <QFileInfo>
#include <QtMath>

#include "audio/bass/PreviewBassDeviceLease.h"
#include "audio/bass/BassFlacPlugin.h"

#include "bass.h"

namespace miacode::audio_decode {

namespace {

class ScopedBassDecodeDevice
{
public:
    ScopedBassDecodeDevice()
        : previousDevice_(BASS_GetDevice())
        , lease_(miacode::preview_audio::PreviewBassDeviceLease::acquire({
              [] {
                  return BASS_SetDevice(0)
                      ? static_cast<miacode::preview_audio::BassDeviceLeaseApi::DeviceId>(0)
                      : miacode::preview_audio::BassDeviceLeaseApi::kNoDevice;
              },
              [] {
                  return BASS_Init(0, 48000, BASS_DEVICE_NOSPEAKER, nullptr, nullptr) != FALSE;
              },
              [] {
                  BASS_SetDevice(0);
                  BASS_Free();
              },
              miacode::preview_audio::BassDeviceLeaseDomain::NoSound,
          }))
    {
    }

    ~ScopedBassDecodeDevice()
    {
        lease_.release();
        if (previousDevice_ != static_cast<DWORD>(-1)) {
            BASS_SetDevice(previousDevice_);
        }
    }

    bool available() const { return lease_.acquired(); }

private:
    DWORD previousDevice_ = static_cast<DWORD>(-1);
    miacode::preview_audio::PreviewBassDeviceLease lease_;
};

QVector<float> resampleLinear(
    const QVector<float>& source,
    int sourceSampleRate,
    int targetSampleRate)
{
    if (source.isEmpty() || sourceSampleRate <= 0 || targetSampleRate <= 0) {
        return {};
    }
    if (sourceSampleRate == targetSampleRate) {
        return source;
    }

    const qint64 targetCount64 = qRound64(
        static_cast<double>(source.size()) * targetSampleRate / sourceSampleRate);
    const int targetCount = static_cast<int>(qBound<qint64>(
        static_cast<qint64>(1), targetCount64, static_cast<qint64>(INT_MAX)));
    QVector<float> result(targetCount, 0.0f);
    const double sourceStep = static_cast<double>(sourceSampleRate) / targetSampleRate;
    for (int targetIndex = 0; targetIndex < targetCount; ++targetIndex) {
        const double sourcePosition = qMin(
            static_cast<double>(source.size() - 1),
            static_cast<double>(targetIndex) * sourceStep);
        const int left = static_cast<int>(sourcePosition);
        const int right = qMin(left + 1, source.size() - 1);
        const double fraction = sourcePosition - left;
        result[targetIndex] = static_cast<float>(
            source.at(left) * (1.0 - fraction) + source.at(right) * fraction);
    }
    return result;
}

DecodedMonoAudio decodeWithBass(const QString& path, int targetSampleRate)
{
    DecodedMonoAudio decoded;
    ScopedBassDecodeDevice device;
    if (!device.available()) {
        return decoded;
    }
    miacode::audio::ensureBassFlacPluginLoaded();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return decoded;
    }
    const QByteArray bytes = file.readAll();
    if (bytes.isEmpty()) {
        return decoded;
    }

    const HSTREAM stream = BASS_StreamCreateFile(
        BASS_FILE_MEM,
        bytes.constData(),
        0,
        static_cast<QWORD>(bytes.size()),
        BASS_STREAM_DECODE | BASS_STREAM_PRESCAN | BASS_SAMPLE_FLOAT);
    if (stream == 0) {
        return decoded;
    }

    BASS_CHANNELINFO info{};
    if (!BASS_ChannelGetInfo(stream, &info) || info.chans == 0 || info.freq == 0) {
        BASS_StreamFree(stream);
        return decoded;
    }
    const int channels = qBound(1, static_cast<int>(info.chans), 8);
    const int sourceSampleRate = static_cast<int>(info.freq);

    constexpr int kFramesPerChunk = 4096;
    QVector<float> interleaved(kFramesPerChunk * channels, 0.0f);
    QVector<float> mono;
    while (true) {
        const DWORD requestedBytes = static_cast<DWORD>(interleaved.size() * sizeof(float));
        const DWORD bytesRead = BASS_ChannelGetData(
            stream, interleaved.data(), requestedBytes | BASS_DATA_FLOAT);
        if (bytesRead == static_cast<DWORD>(-1) || bytesRead == 0) {
            break;
        }
        const int framesRead = static_cast<int>(bytesRead / sizeof(float)) / channels;
        if (framesRead <= 0) {
            break;
        }
        const int oldSize = mono.size();
        mono.resize(oldSize + framesRead);
        for (int frame = 0; frame < framesRead; ++frame) {
            double sum = 0.0;
            for (int channel = 0; channel < channels; ++channel) {
                sum += interleaved.at(frame * channels + channel);
            }
            mono[oldSize + frame] = static_cast<float>(sum / channels);
        }
    }
    BASS_StreamFree(stream);
    if (mono.isEmpty()) {
        return decoded;
    }

    decoded.samples = resampleLinear(mono, sourceSampleRate, targetSampleRate);
    decoded.sampleRate = targetSampleRate;
    decoded.durationSeconds = static_cast<double>(decoded.samples.size()) / targetSampleRate;
    return decoded;
}

}  // namespace

DecodedMonoAudio decodeFileToMono(
    const QString& path,
    int targetSampleRate)
{
    if (path.isEmpty() || targetSampleRate <= 0 || !QFileInfo::exists(path)) {
        return {};
    }
    return decodeWithBass(path, targetSampleRate);
}

double probeFileDurationSeconds(const QString& path)
{
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        return 0.0;
    }
    ScopedBassDecodeDevice device;
    if (!device.available()) {
        return 0.0;
    }
    miacode::audio::ensureBassFlacPluginLoaded();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return 0.0;
    }
    const QByteArray bytes = file.readAll();
    if (bytes.isEmpty()) {
        return 0.0;
    }
    const HSTREAM stream = BASS_StreamCreateFile(
        BASS_FILE_MEM,
        bytes.constData(),
        0,
        static_cast<QWORD>(bytes.size()),
        BASS_STREAM_DECODE | BASS_STREAM_PRESCAN);
    if (stream == 0) {
        return 0.0;
    }
    const QWORD length = BASS_ChannelGetLength(stream, BASS_POS_BYTE);
    const double seconds = length == static_cast<QWORD>(-1)
        ? 0.0 : BASS_ChannelBytes2Seconds(stream, length);
    BASS_StreamFree(stream);
    return qIsFinite(seconds) && seconds > 0.0 ? seconds : 0.0;
}

namespace {

class BassAudioFileDecoder final : public AudioFileDecoder
{
public:
    QString decoderName() const override { return QStringLiteral("bass"); }
    DecodedMonoAudio decodeFileToMono(const QString& path, int targetSampleRate) const override
    {
        return miacode::audio_decode::decodeFileToMono(path, targetSampleRate);
    }
};

}  // namespace

std::shared_ptr<const AudioFileDecoder> bassAudioFileDecoder()
{
    static const std::shared_ptr<const AudioFileDecoder> decoder =
        std::make_shared<BassAudioFileDecoder>();
    return decoder;
}

}  // namespace miacode::audio_decode
