#pragma once

#include <QString>

namespace miacode {

// True when a system or ffmpeg message says the target volume is full.
bool textReportsDiskFull(const QString& text);

} // namespace miacode
