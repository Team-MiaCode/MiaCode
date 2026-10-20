#include "app/services/update/SemanticVersion.h"

#include <algorithm>

namespace miacode::update {
namespace {

// 纯数字且无前导零（"0" 本身允许）。semver 的数字标识符规则。
bool isNumericIdentifier(const QString& identifier)
{
    if (identifier.isEmpty()) {
        return false;
    }
    for (const QChar c : identifier) {
        if (!c.isDigit()) {
            return false;
        }
    }
    // 前导零不是合法的数字标识符（"0" 本身允许）。
    if (identifier.size() > 1 && identifier.startsWith(QLatin1Char('0'))) {
        return false;
    }
    return true;
}

// 全是数字字符，不管有没有前导零。用来判断一个标识符"本该"走数值比较，
// 从而在 parse() 里把不合法的数字段（前导零、溢出 int）拒之门外。
bool isDigitsOnly(const QString& identifier)
{
    if (identifier.isEmpty()) {
        return false;
    }
    for (const QChar c : identifier) {
        if (!c.isDigit()) {
            return false;
        }
    }
    return true;
}

bool isValidIdentifier(const QString& identifier)
{
    if (identifier.isEmpty()) {
        return false;
    }
    for (const QChar c : identifier) {
        if (!c.isLetterOrNumber() && c != QLatin1Char('-')) {
            return false;
        }
        // 非 ASCII 字母数字（全角数字之类）不算合法标识符。
        if (c.unicode() > 0x7F) {
            return false;
        }
    }
    return true;
}

std::optional<int> parseCoreNumber(const QString& text)
{
    if (text.isEmpty() || !isNumericIdentifier(text)) {
        return std::nullopt;
    }
    bool okNumber = false;
    const int value = text.toInt(&okNumber);
    if (!okNumber || value < 0) {
        return std::nullopt;
    }
    return value;
}

// 两个 prerelease 标识符的比较：数字段按数值，数字段低于非数字段，
// 否则按 ASCII。
int compareIdentifier(const QString& lhs, const QString& rhs)
{
    const bool leftNumeric = isNumericIdentifier(lhs);
    const bool rightNumeric = isNumericIdentifier(rhs);
    if (leftNumeric && rightNumeric) {
        // parse() 保证数字标识符不溢出 int，所以这里的 toInt() 无需再查 ok。
        const int left = lhs.toInt();
        const int right = rhs.toInt();
        if (left == right) {
            return 0;
        }
        return left < right ? -1 : 1;
    }
    if (leftNumeric != rightNumeric) {
        return leftNumeric ? -1 : 1;
    }
    const int ordering = QString::compare(lhs, rhs, Qt::CaseSensitive);
    if (ordering == 0) {
        return 0;
    }
    return ordering < 0 ? -1 : 1;
}

} // namespace

std::optional<SemanticVersion> SemanticVersion::parse(const QString& text)
{
    QString body = text.trimmed();
    if (body.startsWith(QLatin1Char('v')) || body.startsWith(QLatin1Char('V'))) {
        body = body.mid(1);
    }
    // 构建元数据整段丢弃。
    const int plusIndex = body.indexOf(QLatin1Char('+'));
    if (plusIndex >= 0) {
        body = body.left(plusIndex);
    }

    QString core = body;
    QString prereleaseText;
    const int dashIndex = body.indexOf(QLatin1Char('-'));
    if (dashIndex >= 0) {
        core = body.left(dashIndex);
        prereleaseText = body.mid(dashIndex + 1);
        if (prereleaseText.isEmpty()) {
            return std::nullopt;
        }
    }

    const QStringList coreParts = core.split(QLatin1Char('.'));
    if (coreParts.size() != 3) {
        return std::nullopt;
    }
    SemanticVersion version;
    const std::optional<int> major = parseCoreNumber(coreParts.at(0));
    const std::optional<int> minor = parseCoreNumber(coreParts.at(1));
    const std::optional<int> patch = parseCoreNumber(coreParts.at(2));
    if (!major.has_value() || !minor.has_value() || !patch.has_value()) {
        return std::nullopt;
    }
    version.major = *major;
    version.minor = *minor;
    version.patch = *patch;

    if (!prereleaseText.isEmpty()) {
        const QStringList identifiers = prereleaseText.split(QLatin1Char('.'));
        for (const QString& identifier : identifiers) {
            if (!isValidIdentifier(identifier)) {
                return std::nullopt;
            }
            if (isDigitsOnly(identifier)) {
                // 数字标识符必须无前导零，且必须能真的参与数值比较。放行一个
                // 溢出 int 的标识符会让 compare() 把它当成 0，从而把顺序判反。
                if (!isNumericIdentifier(identifier)) {
                    return std::nullopt;
                }
                bool fits = false;
                identifier.toInt(&fits);
                if (!fits) {
                    return std::nullopt;
                }
            }
        }
        version.prerelease = identifiers;
    }
    return version;
}

int SemanticVersion::compare(const SemanticVersion& lhs, const SemanticVersion& rhs)
{
    if (lhs.major != rhs.major) {
        return lhs.major < rhs.major ? -1 : 1;
    }
    if (lhs.minor != rhs.minor) {
        return lhs.minor < rhs.minor ? -1 : 1;
    }
    if (lhs.patch != rhs.patch) {
        return lhs.patch < rhs.patch ? -1 : 1;
    }
    // 正式版高于任何 prerelease。
    if (lhs.prerelease.isEmpty() != rhs.prerelease.isEmpty()) {
        return lhs.prerelease.isEmpty() ? 1 : -1;
    }
    const int shared = std::min(lhs.prerelease.size(), rhs.prerelease.size());
    for (int index = 0; index < shared; ++index) {
        const int ordering = compareIdentifier(lhs.prerelease.at(index), rhs.prerelease.at(index));
        if (ordering != 0) {
            return ordering;
        }
    }
    if (lhs.prerelease.size() == rhs.prerelease.size()) {
        return 0;
    }
    // 前缀相同时，标识符更多的更高。
    return lhs.prerelease.size() < rhs.prerelease.size() ? -1 : 1;
}

} // namespace miacode::update
