#pragma once

#include <QString>
#include <QStringList>

#include <optional>

namespace miacode::update {

// semver 2.0.0 的一个子集：major.minor.patch 加可选的 prerelease 标识符列表。
// 构建元数据（+...）在解析时丢弃，因为它不参与比较。
//
// 解析失败一律返回 nullopt，调用方据此放弃提示（fail closed）。宁可漏一次
// 更新提示，也不要因为一个畸形字符串把用户推到错误的版本上。
struct SemanticVersion {
    int major = 0;
    int minor = 0;
    int patch = 0;
    // 空表示正式版。正式版高于同 major.minor.patch 的任何 prerelease。
    QStringList prerelease;

    // 允许一个前导 'v'，这样 GitHub 的 release tag（v2.1.0）可以直接喂进来。
    static std::optional<SemanticVersion> parse(const QString& text);

    // -1 / 0 / +1
    static int compare(const SemanticVersion& lhs, const SemanticVersion& rhs);
};

} // namespace miacode::update
