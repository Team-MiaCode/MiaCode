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

} // namespace miacode
