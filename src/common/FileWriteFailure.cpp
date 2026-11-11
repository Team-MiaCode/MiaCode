#include "common/FileWriteFailure.h"

#include <QStringList>

namespace miacode {

namespace {

bool containsAny(const QString& text, const QStringList& markers)
{
    for (const QString& marker : markers) {
        if (text.contains(marker, Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

} // namespace

// ffmpeg and POSIX report errno text in English; Windows system messages follow
// the system language. Non-ASCII markers are the zh-CN and ja-JP Windows texts,
// escaped because MSVC does not read these targets as UTF-8.
bool textReportsDiskFull(const QString& text)
{
    static const QStringList markers{
        QStringLiteral("No space left on device"),
        QStringLiteral("There is not enough space on the disk"),
        QStringLiteral("The disk is full"),
        QStringLiteral("\u78c1\u76d8\u7a7a\u95f4\u4e0d\u8db3"),
        QStringLiteral("\u78c1\u76d8\u5df2\u6ee1"),
        QStringLiteral("\u30c7\u30a3\u30b9\u30af\u306b\u5341\u5206\u306a\u7a7a\u304d\u9818\u57df\u304c\u3042\u308a\u307e\u305b\u3093"),
        QStringLiteral("\u30c7\u30a3\u30b9\u30af\u304c\u3044\u3063\u3071\u3044\u3067\u3059"),
    };
    return containsAny(text, markers);
}

LocalizedText fileWriteFailureText(const QString& fileName,
                                   QFileDevice::FileError error,
                                   const QString& systemError)
{
    static const QStringList accessDeniedMarkers{
        QStringLiteral("Access is denied"),
        QStringLiteral("Permission denied"),
        QStringLiteral("Read-only file system"),
        QStringLiteral("Operation not permitted"),
        QStringLiteral("\u62d2\u7edd\u8bbf\u95ee"),
        QStringLiteral("\u30a2\u30af\u30bb\u30b9\u304c\u62d2\u5426\u3055\u308c\u307e\u3057\u305f"),
    };
    static const QStringList inUseMarkers{
        QStringLiteral("being used by another process"),
        QStringLiteral("\u53e6\u4e00\u4e2a\u7a0b\u5e8f\u6b63\u5728\u4f7f\u7528\u6b64\u6587\u4ef6"),
        QStringLiteral("\u5225\u306e\u30d7\u30ed\u30bb\u30b9\u304c\u4f7f\u7528\u4e2d"),
    };
    if (textReportsDiskFull(systemError)) {
        return localizedText("file_write.error.disk_full").arg(fileName);
    }
    if (containsAny(systemError, inUseMarkers)) {
        return localizedText("file_write.error.in_use").arg(fileName);
    }
    if (error == QFileDevice::PermissionsError || containsAny(systemError, accessDeniedMarkers)) {
        return localizedText("file_write.error.access_denied").arg(fileName);
    }
    return localizedText("file_write.error.generic").arg(fileName);
}

} // namespace miacode
