#include "app/services/update/UpdateManifest.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QTextStream>

namespace {

bool expect(bool condition, const QString& message, QTextStream& err)
{
    if (!condition) {
        err << "FAIL: " << message << '\n';
    }
    return condition;
}

using miacode::update::ManifestStatus;
using miacode::update::parseManifest;

QJsonObject platformEntry(const QString& file, qint64 bytes)
{
    return QJsonObject{
        {QStringLiteral("file"), file},
        {QStringLiteral("bytes"), bytes},
        {QStringLiteral("sha256"), QStringLiteral("0123456789abcdef")},
        {QStringLiteral("url"), QStringLiteral("https://example.invalid/") + file},
    };
}

QJsonObject validManifest()
{
    return QJsonObject{
        {QStringLiteral("schema"), 1},
        {QStringLiteral("major"), 2},
        {QStringLiteral("version"), QStringLiteral("2.1.0-beta.3")},
        {QStringLiteral("releasedAt"), QStringLiteral("2026-09-20")},
        {QStringLiteral("mandatory"), false},
        {QStringLiteral("notes"), QJsonObject{
             {QStringLiteral("zh_CN"), QStringLiteral("中文说明")},
             {QStringLiteral("en_US"), QStringLiteral("English notes")},
         }},
        {QStringLiteral("platforms"), QJsonObject{
             {QStringLiteral("macos-arm64"), platformEntry(QStringLiteral("mac.7z"), 84231168)},
             {QStringLiteral("windows-x64"), platformEntry(QStringLiteral("win.7z"), 81002496)},
         }},
    };
}

QByteArray toPayload(const QJsonObject& root)
{
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTextStream err(stderr);
    bool ok = true;

    const QString mac = QStringLiteral("macos-arm64");

    // ---- 正常路径 ----
    {
        const auto result = parseManifest(toPayload(validManifest()), 2, mac, QStringLiteral("zh_CN"));
        ok &= expect(result.status == ManifestStatus::Ok,
                     QStringLiteral("a well-formed manifest parses"), err);
        ok &= expect(result.manifest.versionText == QLatin1String("2.1.0-beta.3")
                         && result.manifest.version.minor == 1,
                     QStringLiteral("version is kept both as text and parsed"), err);
        ok &= expect(result.manifest.notes == QStringLiteral("中文说明"),
                     QStringLiteral("notes follow the requested language"), err);
        ok &= expect(result.manifest.package.bytes == 84231168
                         && result.manifest.package.file == QLatin1String("mac.7z"),
                     QStringLiteral("the platform entry is selected by key"), err);
        ok &= expect(result.manifest.package.url == QLatin1String("https://example.invalid/mac.7z"),
                     QStringLiteral("the platform download url survives"), err);
    }

    // ---- 语言回退 ----
    {
        const auto result = parseManifest(toPayload(validManifest()), 2, mac, QStringLiteral("ja_JP"));
        ok &= expect(result.status == ManifestStatus::Ok
                         && result.manifest.notes == QLatin1String("English notes"),
                     QStringLiteral("a missing language falls back to en_US"), err);
    }
    {
        QJsonObject root = validManifest();
        root.insert(QStringLiteral("notes"), QJsonObject{});
        const auto result = parseManifest(toPayload(root), 2, mac, QStringLiteral("zh_CN"));
        ok &= expect(result.status == ManifestStatus::Ok && result.manifest.notes.isEmpty(),
                     QStringLiteral("no notes at all is still a valid manifest"), err);
    }

    // ---- NotApplicable：不是错误，是「这份 manifest 不适用于你」 ----
    {
        QJsonObject root = validManifest();
        root.insert(QStringLiteral("schema"), 2);
        const auto result = parseManifest(toPayload(root), 2, mac, QStringLiteral("zh_CN"));
        ok &= expect(result.status == ManifestStatus::NotApplicable,
                     QStringLiteral("an unknown schema is ignored, not an error"), err);
    }
    {
        const auto result = parseManifest(toPayload(validManifest()), 1, mac, QStringLiteral("zh_CN"));
        ok &= expect(result.status == ManifestStatus::NotApplicable,
                     QStringLiteral("a 1.x build ignores a major-2 manifest"), err);
    }
    {
        const auto result = parseManifest(toPayload(validManifest()), 2,
                                          QStringLiteral("linux-x64"), QStringLiteral("zh_CN"));
        ok &= expect(result.status == ManifestStatus::NotApplicable,
                     QStringLiteral("a platform with no package is not applicable"), err);
    }
    {
        const auto result = parseManifest(toPayload(validManifest()), 2,
                                          QString(), QStringLiteral("zh_CN"));
        ok &= expect(result.status == ManifestStatus::NotApplicable,
                     QStringLiteral("an unresolvable platform key is not applicable"), err);
    }
    {
        QJsonObject root = validManifest();
        QJsonObject platforms = root.value(QStringLiteral("platforms")).toObject();
        platforms.insert(mac, QStringLiteral("oops"));
        root.insert(QStringLiteral("platforms"), platforms);
        const auto result = parseManifest(toPayload(root), 2, mac, QStringLiteral("zh_CN"));
        ok &= expect(result.status == ManifestStatus::NotApplicable,
                     QStringLiteral("a platform entry that is a string is not applicable"), err);
    }
    {
        QJsonObject root = validManifest();
        QJsonObject platforms = root.value(QStringLiteral("platforms")).toObject();
        platforms.insert(mac, QJsonValue());
        root.insert(QStringLiteral("platforms"), platforms);
        const auto result = parseManifest(toPayload(root), 2, mac, QStringLiteral("zh_CN"));
        ok &= expect(result.status == ManifestStatus::NotApplicable,
                     QStringLiteral("a platform entry that is null is not applicable"), err);
    }
    {
        QJsonObject root = validManifest();
        QJsonObject platforms = root.value(QStringLiteral("platforms")).toObject();
        platforms.insert(mac, QJsonObject{});
        root.insert(QStringLiteral("platforms"), platforms);
        const auto result = parseManifest(toPayload(root), 2, mac, QStringLiteral("zh_CN"));
        ok &= expect(result.status == ManifestStatus::NotApplicable,
                     QStringLiteral("a platform entry that is an empty object is not applicable"), err);
    }
    {
        QJsonObject root = validManifest();
        QJsonObject platforms = root.value(QStringLiteral("platforms")).toObject();
        platforms.insert(mac, QJsonObject{
                              {QStringLiteral("file"), QStringLiteral("mac.7z")},
                          });
        root.insert(QStringLiteral("platforms"), platforms);
        const auto result = parseManifest(toPayload(root), 2, mac, QStringLiteral("zh_CN"));
        ok &= expect(result.status == ManifestStatus::NotApplicable,
                     QStringLiteral("a platform entry with a file but no url is not applicable"), err);
    }

    // ---- Invalid：结构坏了 ----
    {
        const auto result = parseManifest(QByteArray("not json at all"), 2, mac, QStringLiteral("zh_CN"));
        ok &= expect(result.status == ManifestStatus::Invalid,
                     QStringLiteral("non-JSON payload is invalid"), err);
    }
    {
        const auto result = parseManifest(QByteArray("[1,2,3]"), 2, mac, QStringLiteral("zh_CN"));
        ok &= expect(result.status == ManifestStatus::Invalid,
                     QStringLiteral("a JSON array is invalid"), err);
    }
    {
        QJsonObject root = validManifest();
        root.insert(QStringLiteral("version"), QStringLiteral("2.x"));
        const auto result = parseManifest(toPayload(root), 2, mac, QStringLiteral("zh_CN"));
        ok &= expect(result.status == ManifestStatus::Invalid,
                     QStringLiteral("an unparseable version is invalid"), err);
    }
    for (const QString& url : {QStringLiteral("javascript:alert(1)"), QStringLiteral("http://example.invalid/mac.7z")}) {
        QJsonObject root = validManifest();
        QJsonObject platforms = root.value(QStringLiteral("platforms")).toObject();
        QJsonObject entry = platforms.value(mac).toObject();
        entry.insert(QStringLiteral("url"), url);
        platforms.insert(mac, entry);
        root.insert(QStringLiteral("platforms"), platforms);
        const auto result = parseManifest(toPayload(root), 2, mac, QStringLiteral("zh_CN"));
        ok &= expect(result.status == ManifestStatus::Invalid,
                     QStringLiteral("a non-https download url is rejected: %1").arg(url), err);
    }
    {
        QJsonObject root = validManifest();
        root.remove(QStringLiteral("platforms"));
        const auto result = parseManifest(toPayload(root), 2, mac, QStringLiteral("zh_CN"));
        ok &= expect(result.status == ManifestStatus::Invalid,
                     QStringLiteral("a manifest without a platforms object is invalid"), err);
    }
    {
        QJsonObject root = validManifest();
        root.remove(QStringLiteral("major"));
        const auto result = parseManifest(toPayload(root), 2, mac, QStringLiteral("zh_CN"));
        ok &= expect(result.status == ManifestStatus::Invalid,
                     QStringLiteral("a manifest without a major is invalid"), err);
    }
    {
        QJsonObject root = validManifest();
        root.insert(QStringLiteral("schema"), 99.5);
        const auto result = parseManifest(toPayload(root), 2, mac, QStringLiteral("zh_CN"));
        ok &= expect(result.status == ManifestStatus::Invalid,
                     QStringLiteral("a non-integral schema is invalid, not silently coerced to 0"), err);
    }
    {
        QJsonObject root = validManifest();
        root.insert(QStringLiteral("schema"), 1.9);
        const auto result = parseManifest(toPayload(root), 2, mac, QStringLiteral("zh_CN"));
        ok &= expect(result.status == ManifestStatus::Invalid,
                     QStringLiteral("a fractional schema is invalid, not truncated"), err);
    }
    {
        QJsonObject root = validManifest();
        root.insert(QStringLiteral("major"), 2.5);
        const auto result = parseManifest(toPayload(root), 2, mac, QStringLiteral("zh_CN"));
        ok &= expect(result.status == ManifestStatus::Invalid,
                     QStringLiteral("a non-integral major is invalid, not a family mismatch"), err);
    }

    // ---- 每条失败路径都要带上可写进日志的原因 ----
    {
        const auto result = parseManifest(QByteArray("not json"), 2, mac, QStringLiteral("zh_CN"));
        ok &= expect(!result.reason.isEmpty(),
                     QStringLiteral("a failure carries a loggable reason"), err);
    }

    // ---- bytes：类型错、越界或负数都要记 0，而不是相信或做未定义行为 ----
    {
        QJsonObject root = validManifest();
        QJsonObject platforms = root.value(QStringLiteral("platforms")).toObject();
        platforms.insert(mac, platformEntry(QStringLiteral("mac.7z"), -1000000));
        root.insert(QStringLiteral("platforms"), platforms);
        const auto result = parseManifest(toPayload(root), 2, mac, QStringLiteral("zh_CN"));
        ok &= expect(result.status == ManifestStatus::Ok && result.manifest.package.bytes == 0,
                     QStringLiteral("a negative byte count is dropped to 0, not kept negative"), err);
    }
    {
        QJsonObject root = validManifest();
        QJsonObject platforms = root.value(QStringLiteral("platforms")).toObject();
        QJsonObject entry = platformEntry(QStringLiteral("mac.7z"), 84231168);
        entry.insert(QStringLiteral("bytes"), QStringLiteral("84231168"));
        platforms.insert(mac, entry);
        root.insert(QStringLiteral("platforms"), platforms);
        const auto result = parseManifest(toPayload(root), 2, mac, QStringLiteral("zh_CN"));
        ok &= expect(result.status == ManifestStatus::Ok && result.manifest.package.bytes == 0,
                     QStringLiteral("a byte count given as a JSON string is dropped to 0"), err);
    }

    if (ok) {
        QTextStream(stdout) << "update_manifest_spec ok\n";
        return 0;
    }
    return 1;
}
