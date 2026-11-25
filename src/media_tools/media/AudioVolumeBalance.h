#pragma once

#include "common/LocalizedText.h"

#include <atomic>

namespace miacode::media {

enum class AudioVolumeBalanceStatus { Saved, Unchanged, Cancelled, Failed };

struct AudioVolumeBalanceResult {
    AudioVolumeBalanceStatus status;
    LocalizedText message;
    QString details;
};

// Runs on a worker thread.
AudioVolumeBalanceResult runAudioVolumeBalance(
    const QString& ffmpegPath, const QString& trackPath, const QString& backupPath,
    const std::atomic_bool& cancelled);

} // namespace miacode::media
