#include "app/services/update/UpdateManifest.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QSysInfo>
#include <QUrl>

#include <cmath>
#include <limits>
#include <optional>

namespace miacode::update {
namespace {

ManifestParseResult failure(ManifestStatus status, const QString& reason)
{
    ManifestParseResult result;
    result.status = status;
    result.reason = reason;
    return result;
}

// 读一个必须是整数的字段。QJsonValue::toInt() 对小数或超出 int 范围的值直接
// 返回默认值，而不是截断 —— 所以 "schema": 99.5 会被读成 0，从而绕过下面
// 「版本过新就安静忽略」的闸门。必须自己确认它真的是个整数。
std::optional<int> readWholeNumber(const QJsonValue& value)
{
    if (!value.isDouble()) {
        return std::nullopt;
    }
    const double raw = value.toDouble();
    if (!std::isfinite(raw) || raw != std::floor(raw)
        || raw < static_cast<double>(std::numeric_limits<int>::min())
        || raw > static_cast<double>(std::numeric_limits<int>::max())) {
        return std::nullopt;
    }
    return static_cast<int>(raw);
}

PlatformPackage readPackage(const QJsonObject& entry)
{
    PlatformPackage package;
    package.file = entry.value(QStringLiteral("file")).toString();
    // 只接受一个正的、有限的、能安全落进 qint64 的字节数。其余一律记 0，
    // 调用方对 0 的处理是「不显示大小」，比显示一个负数或未定义值要好。
    package.bytes = 0;
    const QJsonValue bytesValue = entry.value(QStringLiteral("bytes"));
    if (bytesValue.isDouble()) {
        const double raw = bytesValue.toDouble();
        if (std::isfinite(raw) && raw > 0.0 && raw < 9.0e15) {
            package.bytes = static_cast<qint64>(raw);
        }
    }
    package.sha256 = entry.value(QStringLiteral("sha256")).toString();
    package.url = entry.value(QStringLiteral("url")).toString();
    package.minOsVersion = entry.value(QStringLiteral("minOsVersion")).toString();
    return package;
}

QString selectNotes(const QJsonObject& notes, const QString& languageToken)
{
    if (notes.contains(languageToken)) {
        return notes.value(languageToken).toString();
    }
    const QString language = languageToken.section(QLatin1Char('_'), 0, 0).section(QLatin1Char('-'), 0, 0);
    const QString catalog = language == QStringLiteral("zh") ? QStringLiteral("zh_CN")
        : language == QStringLiteral("ja") ? QStringLiteral("ja_JP") : QStringLiteral("en_US");
    if (notes.contains(catalog)) return notes.value(catalog).toString();
    return notes.value(QStringLiteral("en_US")).toString();
}

} // namespace

QString UpdateManifest::localizedNotes(const QString& languageToken) const
{
    return notesByLanguage.isEmpty() ? notes : selectNotes(notesByLanguage, languageToken);
}

ManifestParseResult parseManifest(const QByteArray& payload,
                                  int selfMajor,
                                  const QString& platformKey,
                                  const QString& languageToken)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return failure(ManifestStatus::Invalid,
                       QStringLiteral("manifest is not a JSON object: %1").arg(parseError.errorString()));
    }
    const QJsonObject root = document.object();

    const std::optional<int> schema = readWholeNumber(root.value(QStringLiteral("schema")));
    if (!schema.has_value()) {
        return failure(ManifestStatus::Invalid, QStringLiteral("manifest has no integral schema"));
    }
    if (*schema > kSupportedManifestSchema) {
        return failure(ManifestStatus::NotApplicable,
                       QStringLiteral("manifest schema %1 is newer than this build understands")
                           .arg(*schema));
    }

    const std::optional<int> major = readWholeNumber(root.value(QStringLiteral("major")));
    if (!major.has_value()) {
        return failure(ManifestStatus::Invalid, QStringLiteral("manifest has no integral major"));
    }
    if (*major != selfMajor) {
        // 版本族隔离：1.x 与 2.x 是两套独立应用，互不推送。
        return failure(ManifestStatus::NotApplicable,
                       QStringLiteral("manifest targets major %1, this build is major %2")
                           .arg(*major).arg(selfMajor));
    }

    const QString versionText = root.value(QStringLiteral("version")).toString();
    const auto version = SemanticVersion::parse(versionText);
    if (!version.has_value()) {
        return failure(ManifestStatus::Invalid,
                       QStringLiteral("manifest version is unparseable: '%1'").arg(versionText));
    }

    const QJsonValue platformsValue = root.value(QStringLiteral("platforms"));
    if (!platformsValue.isObject()) {
        return failure(ManifestStatus::Invalid, QStringLiteral("manifest has no platforms object"));
    }
    if (platformKey.isEmpty()) {
        return failure(ManifestStatus::NotApplicable,
                       QStringLiteral("the running platform has no release key"));
    }
    const QJsonObject platforms = platformsValue.toObject();
    if (!platforms.contains(platformKey)) {
        return failure(ManifestStatus::NotApplicable,
                       QStringLiteral("no package for platform '%1'").arg(platformKey));
    }
    const QJsonValue entryValue = platforms.value(platformKey);
    if (!entryValue.isObject()) {
        return failure(ManifestStatus::NotApplicable,
                       QStringLiteral("no package for platform '%1': the entry is not an object")
                           .arg(platformKey));
    }
    const PlatformPackage package = readPackage(entryValue.toObject());
    if (package.file.isEmpty() || package.url.isEmpty()) {
        // 条目在，但没有能用的东西 —— 与「本平台没有包」是同一回事，
        // 不要谎报成一个可用的更新。
        return failure(ManifestStatus::NotApplicable,
                       QStringLiteral("no package for platform '%1': the entry has no file or url")
                           .arg(platformKey));
    }
    const QUrl packageUrl(package.url);
    if (!packageUrl.isValid() || packageUrl.scheme() != QLatin1String("https")) {
        // 下载按钮把这个 URL 交给系统浏览器打开，所以 scheme 必须是 https。
        return failure(ManifestStatus::Invalid,
                       QStringLiteral("package url is not https: '%1'").arg(package.url));
    }

    ManifestParseResult result;
    result.status = ManifestStatus::Ok;
    result.manifest.major = *major;
    result.manifest.versionText = versionText;
    result.manifest.version = *version;
    result.manifest.releasedAt = root.value(QStringLiteral("releasedAt")).toString();
    result.manifest.mandatory = root.value(QStringLiteral("mandatory")).toBool(false);
    result.manifest.notesByLanguage = root.value(QStringLiteral("notes")).toObject();
    result.manifest.notes = result.manifest.localizedNotes(languageToken);
    result.manifest.package = package;
    return result;
}

QString currentPlatformKey()
{
    // 取值必须与 .github/workflows/package.yml 矩阵里的 platform 字段一致。
    const QString architecture = QSysInfo::currentCpuArchitecture();
#if defined(Q_OS_MACOS)
    // macOS 发布版只有 Apple Silicon 一种包（CMakeLists 强制 arm64）。
    // Rosetta 下跑出来的 x86_64 会走到空串，调用方按「本平台无包」处理。
    if (architecture == QLatin1String("arm64")) {
        return QStringLiteral("macos-arm64");
    }
    return QString();
#elif defined(Q_OS_WIN)
    if (architecture == QLatin1String("arm64")) {
        return QStringLiteral("windows-arm64");
    }
    if (architecture == QLatin1String("x86_64")) {
        return QStringLiteral("windows-x64");
    }
    return QString();
#else
    return QString();
#endif
}

} // namespace miacode::update
