#pragma once

#include "common/DebugLog.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QUuid>

// Shared disk policy for application and project preferences. Keep recovery
// independent of the in-memory preference owner.
namespace miacode::preference_json_file {

enum class ReadStatus { Missing, Valid, Corrupt, ReadError };
struct ReadResult {
    ReadStatus status = ReadStatus::Missing;
    QJsonObject object;
};

inline void log(const QString& path, const QString& detail,
                debug_log::Level level = debug_log::Level::Error)
{
    debug_log::appendLine(debug_log::Channel::Runtime, QStringLiteral("preferences"),
                          QStringLiteral("path=%1 %2").arg(path, detail), true, level);
}

inline ReadResult read(const QString& path)
{
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        return {};
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        log(path, QStringLiteral("read-open-failed error=%1").arg(file.errorString()));
        return {ReadStatus::ReadError, {}};
    }
    const QByteArray bytes = file.readAll();
    if (file.error() != QFileDevice::NoError) {
        log(path, QStringLiteral("read-failed error=%1").arg(file.errorString()));
        return {ReadStatus::ReadError, {}};
    }
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(bytes, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        log(path, QStringLiteral("invalid-json offset=%1 error=%2 object=%3")
                      .arg(error.offset).arg(error.errorString()).arg(document.isObject()),
            debug_log::Level::Warn);
        return {ReadStatus::Corrupt, {}};
    }
    return {ReadStatus::Valid, document.object()};
}

inline bool backupCorrupt(const QString& path)
{
    const QString backup = path + QStringLiteral(".corrupt-")
        + QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz"))
        + QLatin1Char('-') + QUuid::createUuid().toString(QUuid::WithoutBraces);
    QFile source(path);
    if (!source.copy(backup)) {
        log(path, QStringLiteral("backup-failed original-preserved error=%1").arg(source.errorString()));
        return false;
    }
    log(path, QStringLiteral("corrupt-backup=%1").arg(backup), debug_log::Level::Warn);
    return true;
}

inline bool write(const QString& path, const QJsonObject& object)
{
    if (path.isEmpty()) {
        log(path, QStringLiteral("write-failed empty-path"));
        return false;
    }
    // Guard direct saves too: callers need not have loaded the file first.
    const ReadResult existing = read(path);
    if (existing.status == ReadStatus::ReadError
        || (existing.status == ReadStatus::Corrupt && !backupCorrupt(path))) {
        return false;
    }
    const QDir parent = QFileInfo(path).dir();
    if (!parent.exists() && !QDir().mkpath(parent.absolutePath())) {
        log(path, QStringLiteral("write-failed cannot-create-parent"));
        return false;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        log(path, QStringLiteral("write-open-failed error=%1").arg(file.errorString()));
        return false;
    }
    const QByteArray payload = QJsonDocument(object).toJson(QJsonDocument::Indented);
    if (file.write(payload) != payload.size()) {
        log(path, QStringLiteral("write-failed error=%1").arg(file.errorString()));
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        log(path, QStringLiteral("commit-failed error=%1").arg(file.errorString()));
        return false;
    }
    if (existing.status == ReadStatus::Corrupt) {
        log(path, QStringLiteral("corrupt-file-rebuilt"), debug_log::Level::Warn);
    }
    return true;
}

inline QJsonObject load(const QString& path, const QJsonObject& defaults = {})
{
    const ReadResult result = read(path);
    if (result.status == ReadStatus::Valid) {
        return result.object;
    }
    if (result.status == ReadStatus::Corrupt) {
        write(path, defaults);
    }
    return defaults;
}

} // namespace miacode::preference_json_file
