#pragma once

#include "common/LocalizedText.h"

#include <QFileDevice>
#include <QString>

namespace miacode {

// True when a system or ffmpeg message says the target volume is full.
bool textReportsDiskFull(const QString& text);

// The notice sentence for a failed write of `fileName`: names the file and,
// when the system message makes it certain, why the write failed.
LocalizedText fileWriteFailureText(const QString& fileName,
                                   QFileDevice::FileError error,
                                   const QString& systemError);

} // namespace miacode
