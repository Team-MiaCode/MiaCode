#include "app/services/update/SemanticVersion.h"

#include <QCoreApplication>
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

using miacode::update::SemanticVersion;

// -1 / 0 / +1，参数是文本，失败的解析在断言里单独覆盖。
int compareText(const QString& lhs, const QString& rhs)
{
    const auto left = SemanticVersion::parse(lhs);
    const auto right = SemanticVersion::parse(rhs);
    if (!left.has_value() || !right.has_value()) {
        return -99;
    }
    return SemanticVersion::compare(*left, *right);
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTextStream err(stderr);
    bool ok = true;

    // ---- 解析 ----
    const auto plain = SemanticVersion::parse(QStringLiteral("2.1.0"));
    ok &= expect(plain.has_value() && plain->major == 2 && plain->minor == 1 && plain->patch == 0
                     && plain->prerelease.isEmpty(),
                 QStringLiteral("a release version parses with an empty prerelease"), err);

    const auto pre = SemanticVersion::parse(QStringLiteral("2.0.0-beta.3"));
    ok &= expect(pre.has_value() && pre->prerelease == QStringList{QStringLiteral("beta"), QStringLiteral("3")},
                 QStringLiteral("prerelease identifiers split on dots"), err);

    const auto tagged = SemanticVersion::parse(QStringLiteral("v2.0.0-alpha"));
    ok &= expect(tagged.has_value() && tagged->major == 2
                     && tagged->prerelease == QStringList{QStringLiteral("alpha")},
                 QStringLiteral("a leading v is accepted so release tags parse"), err);

    const auto built = SemanticVersion::parse(QStringLiteral("2.0.0-alpha+20260915"));
    ok &= expect(built.has_value() && built->prerelease == QStringList{QStringLiteral("alpha")},
                 QStringLiteral("build metadata is discarded"), err);

    const auto upperV = SemanticVersion::parse(QStringLiteral("V2.0.0"));
    ok &= expect(upperV.has_value() && upperV->major == 2 && upperV->minor == 0 && upperV->patch == 0,
                 QStringLiteral("an uppercase leading V is accepted just like lowercase v"), err);

    const auto maxIdentifier = SemanticVersion::parse(QStringLiteral("2.0.0-2147483647"));
    ok &= expect(maxIdentifier.has_value()
                     && maxIdentifier->prerelease == QStringList{QStringLiteral("2147483647")},
                 QStringLiteral("a numeric identifier at INT_MAX still parses"), err);

    const auto zeroIdentifier = SemanticVersion::parse(QStringLiteral("2.0.0-0"));
    ok &= expect(zeroIdentifier.has_value() && zeroIdentifier->prerelease == QStringList{QStringLiteral("0")},
                 QStringLiteral("a bare zero numeric identifier is legal"), err);

    // ---- 解析失败：一律 nullopt，绝不猜 ----
    for (const QString& bad : {QStringLiteral(""),
                               QStringLiteral("2.0"),
                               QStringLiteral("2.0.0.1"),
                               QStringLiteral("2.0.x"),
                               QStringLiteral("2.0.0-"),
                               QStringLiteral("2.0.0-beta..1"),
                               QStringLiteral("-1.0.0"),
                               QStringLiteral("nightly"),
                               QStringLiteral("2.0.0-99999999999999999999"),
                               QStringLiteral("2.0.0-2147483648"),
                               QStringLiteral("2.01.0"),
                               QStringLiteral("2.0.0-01"),
                               QStringLiteral("2.0.0-０")}) {
        ok &= expect(!SemanticVersion::parse(bad).has_value(),
                     QStringLiteral("malformed version is rejected: '%1'").arg(bad), err);
    }

    // ---- 数值段比较 ----
    ok &= expect(compareText(QStringLiteral("2.1.0"), QStringLiteral("2.0.0")) > 0,
                 QStringLiteral("a higher minor wins"), err);
    ok &= expect(compareText(QStringLiteral("2.0.10"), QStringLiteral("2.0.9")) > 0,
                 QStringLiteral("patch compares numerically, not as text"), err);
    ok &= expect(compareText(QStringLiteral("10.0.0"), QStringLiteral("9.9.9")) > 0,
                 QStringLiteral("major compares numerically, not as text"), err);
    ok &= expect(compareText(QStringLiteral("2.0.0"), QStringLiteral("2.0.0")) == 0,
                 QStringLiteral("identical versions compare equal"), err);

    // ---- prerelease 低于正式版 ----
    ok &= expect(compareText(QStringLiteral("2.0.0-alpha"), QStringLiteral("2.0.0")) < 0,
                 QStringLiteral("a prerelease is lower than its own release"), err);
    ok &= expect(compareText(QStringLiteral("2.0.0"), QStringLiteral("2.0.0-alpha")) > 0,
                 QStringLiteral("the comparison is antisymmetric across the prerelease boundary"), err);
    ok &= expect(compareText(QStringLiteral("2.0.0-alpha"), QStringLiteral("1.9.9")) > 0,
                 QStringLiteral("a prerelease still beats a lower release"), err);

    // ---- prerelease 之间 ----
    ok &= expect(compareText(QStringLiteral("2.0.0-alpha"), QStringLiteral("2.0.0-beta.1")) < 0,
                 QStringLiteral("alpha sorts below beta"), err);
    ok &= expect(compareText(QStringLiteral("2.0.0-beta.2"), QStringLiteral("2.0.0-beta.10")) < 0,
                 QStringLiteral("numeric identifiers compare numerically"), err);
    ok &= expect(compareText(QStringLiteral("2.0.0-beta.1"), QStringLiteral("2.0.0-beta")) > 0,
                 QStringLiteral("more identifiers outrank a shared prefix"), err);
    ok &= expect(compareText(QStringLiteral("2.0.0-1"), QStringLiteral("2.0.0-alpha")) < 0,
                 QStringLiteral("a numeric identifier sorts below an alphanumeric one"), err);

    // ---- 真实场景：当前构建是 2.0.0-alpha ----
    ok &= expect(compareText(QStringLiteral("2.0.0-beta.1"), QStringLiteral("2.0.0-alpha")) > 0,
                 QStringLiteral("the shipped 2.0.0-alpha sees 2.0.0-beta.1 as an update"), err);
    ok &= expect(compareText(QStringLiteral("2.0.0-alpha"), QStringLiteral("2.0.0-alpha")) == 0,
                 QStringLiteral("the shipped 2.0.0-alpha does not see itself as an update"), err);

    if (ok) {
        QTextStream(stdout) << "update_version_spec ok\n";
        return 0;
    }
    return 1;
}
