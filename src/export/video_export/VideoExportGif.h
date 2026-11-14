#pragma once

#include "export/video_export/VideoExportController.h"

namespace miacode::video_export {

void applyGifExportSettings(VideoExportTask& task);
VideoExportResult exportGifTask(const VideoExportTask& task,
                                const VideoExportProgressCallback& progressCallback);

} // namespace miacode::video_export
