#pragma once

#include <QFile>
#include <QSaveFile>

namespace miacode::file_io {

inline bool copyFileContents(QFile& source, QSaveFile& target, QString* detail = nullptr)
{
    QByteArray buffer(1024 * 1024, Qt::Uninitialized);
    while (!source.atEnd()) {
        const qint64 read = source.read(buffer.data(), buffer.size());
        if (read < 0) {
            if (detail != nullptr) *detail = source.errorString();
            return false;
        }
        if (read > 0 && target.write(buffer.constData(), read) != read) {
            if (detail != nullptr) *detail = target.errorString();
            return false;
        }
    }
    return true;
}

inline bool copyFileAtomically(const QString& sourcePath, const QString& targetPath,
                               QString* detail = nullptr)
{
    QFile source(sourcePath);
    if (!source.open(QIODevice::ReadOnly)) {
        if (detail != nullptr) *detail = source.errorString();
        return false;
    }

    QSaveFile target(targetPath);
    target.setDirectWriteFallback(false);
    if (!target.open(QIODevice::WriteOnly)) {
        if (detail != nullptr) *detail = target.errorString();
        return false;
    }
    if (!copyFileContents(source, target, detail)) {
        target.cancelWriting();
        return false;
    }
    if (!target.commit()) {
        if (detail != nullptr) *detail = target.errorString();
        return false;
    }
    return true;
}

} // namespace miacode::file_io
