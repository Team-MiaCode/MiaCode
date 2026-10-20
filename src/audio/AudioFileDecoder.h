#pragma once

#include <QString>
#include <QVector>

namespace miacode::audio_decode {

struct DecodedMonoAudio {
    QVector<float> samples;
    int sampleRate = 0;
    double durationSeconds = 0.0;

    bool isEmpty() const { return samples.isEmpty() || sampleRate <= 0; }
};

// Decodes an audio file to mono float samples. An audio backend supplies the
// implementation (audio/bass/OfflineAudioDecoder.h on desktop); consumers such
// as the waveform cache receive it by injection.
class AudioFileDecoder
{
public:
    virtual ~AudioFileDecoder() = default;

    // Short backend label written to diagnostics ("bass").
    virtual QString decoderName() const = 0;
    virtual DecodedMonoAudio decodeFileToMono(const QString& path, int targetSampleRate) const = 0;

protected:
    AudioFileDecoder() = default;
    AudioFileDecoder(const AudioFileDecoder&) = default;
    AudioFileDecoder& operator=(const AudioFileDecoder&) = default;
};

}  // namespace miacode::audio_decode
