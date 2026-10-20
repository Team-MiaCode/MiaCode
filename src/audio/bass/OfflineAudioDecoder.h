#pragma once

#include <memory>

#include <QString>
#include <QVector>

#include "audio/AudioFileDecoder.h"

namespace miacode::audio_decode {

DecodedMonoAudio decodeFileToMono(
    const QString& path,
    int targetSampleRate);
double probeFileDurationSeconds(const QString& path);

// decodeFileToMono() behind the AudioFileDecoder interface ("bass").
std::shared_ptr<const AudioFileDecoder> bassAudioFileDecoder();

}  // namespace miacode::audio_decode
