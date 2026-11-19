#pragma once

#include "common/LocalizedText.h"

#include <QString>

#include <atomic>
#include <functional>
#include <memory>

namespace miacode::media {

enum class VideoAudioAlignmentStage {
    Probe,
    Decode,
    Analyze,
    Encode,
    Finalize,
};

enum class VideoAudioAlignmentStatus {
    Aligned,
    AlreadyAligned,
    Cancelled,
    Failed,
};

struct VideoAudioAlignmentRequest {
    QString ffmpegPath;
    QString videoPath;
    QString trackPath;
};

struct VideoAudioAlignmentResult {
    VideoAudioAlignmentStatus status = VideoAudioAlignmentStatus::Failed;
    VideoAudioAlignmentStage stage = VideoAudioAlignmentStage::Probe;
    double offsetSeconds = 0.0;
    miacode::LocalizedText error;
};

using VideoAudioAlignmentCancellation = std::shared_ptr<std::atomic_bool>;
// A negative percent denotes a stage without measurable progress. Called on the worker thread.
using VideoAudioAlignmentProgress = std::function<void(VideoAudioAlignmentStage, int)>;

// Run on a worker thread. The caller releases the preview's file handles before starting.
VideoAudioAlignmentResult runVideoAudioAlignment(
    const VideoAudioAlignmentRequest& request,
    const VideoAudioAlignmentCancellation& cancellation,
    const VideoAudioAlignmentProgress& progress = {});

} // namespace miacode::media
