#pragma once

#include "app/services/update/SemanticVersion.h"

#include <QByteArray>
#include <QJsonObject>
#include <QString>

namespace miacode::update {

// 客户端认识的 manifest schema。更高的 schema 被安静忽略（NotApplicable），
// 不向用户报错 —— 旧客户端遇到未来格式时无事可做，报错只会制造噪音。
constexpr int kSupportedManifestSchema = 1;

enum class ManifestStatus {
    // 可用，且适用于本构建与本平台。
    Ok,
    // 这份 manifest 不适用于你：schema 不识别、major 不匹配、没有本平台的包。
    // 这不是错误，调用方按「无可用更新」处理。
    NotApplicable,
    // 结构坏了：JSON 非法、必需字段缺失、版本不可用、本平台包的 URL 不是 https。
    Invalid,
};

struct PlatformPackage {
    QString file;
    qint64 bytes = 0;
    QString sha256;
    QString url;
    QString minOsVersion;
};

// 已经按平台和语言挑选完毕的一份 manifest —— 调用方不需要再做任何挑选。
struct UpdateManifest {
    int major = 0;
    QString versionText;
    SemanticVersion version;
    QString releasedAt;
    bool mandatory = false;
    QString notes;
    QJsonObject notesByLanguage;
    QString localizedNotes(const QString& languageToken) const;
    PlatformPackage package;
};

struct ManifestParseResult {
    ManifestStatus status = ManifestStatus::Invalid;
    // 写日志用的原因，永远不展示给用户。
    QString reason;
    // 仅当 status == Ok 时有意义。
    UpdateManifest manifest;
};

// platformKey 空串表示当前平台无法识别，按 NotApplicable 处理。
// languageToken 形如 "zh_CN"，缺失时回退 "en_US"，再缺失则 notes 为空。
ManifestParseResult parseManifest(const QByteArray& payload,
                                  int selfMajor,
                                  const QString& platformKey,
                                  const QString& languageToken);

// 编译期平台 + 运行期架构。无法映射到发布矩阵时返回空串。
QString currentPlatformKey();

} // namespace miacode::update
