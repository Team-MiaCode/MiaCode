#pragma once

#include "common/LocalizedText.h"

#include <QString>

#include <atomic>
#include <functional>
#include <memory>

namespace miacode::media {

enum class VideoCompressionStage {
    Backup,
    Probe,
    Encode,
    Finalize,
};

enum class VideoCompressionStatus {
    Compressed,
    Cancelled,
    Failed,
    PreservedCompressed,
};

struct VideoCompressionRequest {
    QString ffmpegPath;
    QString videoPath;
};

struct VideoCompressionResult {
    VideoCompressionStatus status = VideoCompressionStatus::Failed;
    VideoCompressionStage stage = VideoCompressionStage::Backup;
    miacode::LocalizedText error;
};

using VideoCompressionCancellation = std::shared_ptr<std::atomic_bool>;
// A negative percent denotes a stage without measurable progress. Called on the worker thread.
using VideoCompressionProgress = std::function<void(VideoCompressionStage, int)>;

// Run on a worker thread. The caller releases the preview's file handles before starting.
VideoCompressionResult runVideoCompression(
    const VideoCompressionRequest& request,
    const VideoCompressionCancellation& cancellation,
    const VideoCompressionProgress& progress = {});

} // namespace miacode::media
