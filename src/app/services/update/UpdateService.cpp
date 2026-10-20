#include "app/services/update/UpdateService.h"

#include <QDateTime>
#include <QDebug>
#include <QDesktopServices>
#include <QLocale>
#include <QPointer>
#include <QTimer>
#include <QUrl>

namespace miacode::update {
namespace {

constexpr int kSuccessThrottleHours = 24;
constexpr int kFailureThrottleHours = 4;
constexpr int kStartupDelayMs = 8000;

QUrl manifestUrl(int major)
{
    // 每个 major 一份手动发布到 OSS 的 manifest，不分通道：发布什么版本由
    // 发布者决定，客户端只在它比自己新时提示。
    return QUrl(QStringLiteral(
                    "https://miacode.oss-cn-beijing.aliyuncs.com/update/%1/latest.json")
                    .arg(major));
}

QString formatSize(qint64 bytes)
{
    if (bytes <= 0) {
        return QString();
    }
    return QLocale::system().formattedDataSize(bytes);
}

} // namespace

UpdateService::UpdateService(UpdateFetcher& fetcher,
                             UpdateStateStore& store,
                             UpdateEnvironment environment,
                             QObject* parent)
    : QObject(parent)
    , fetcher_(fetcher)
    , store_(store)
    , environment_(std::move(environment))
{
    const auto parsed = SemanticVersion::parse(environment_.versionText);
    currentVersionParsed_ = parsed.has_value();
    if (currentVersionParsed_) {
        currentVersion_ = *parsed;
    }
    restoreKnownFinding();
}

bool UpdateService::checkEnabled() const
{
    return store_.checkEnabled();
}

void UpdateService::setCheckEnabled(bool enabled)
{
    if (enabled == store_.checkEnabled()) {
        return;
    }
    store_.setCheckEnabled(enabled);
    emit settingsChanged();
}

QString UpdateService::lastCheckText() const
{
    const QDateTime at = store_.lastCheckAt();
    if (!at.isValid()) {
        return QString();
    }
    return QLocale::system().toString(at.toLocalTime(), QLocale::ShortFormat);
}

void UpdateService::restoreKnownFinding()
{
    // 启动时清理陈旧状态，否则会出现「已经升级了还在提示」。
    const QString skipped = store_.skippedVersion();
    if (!skipped.isEmpty() && currentVersionParsed_) {
        const auto parsed = SemanticVersion::parse(skipped);
        if (!parsed.has_value() || SemanticVersion::compare(currentVersion_, *parsed) >= 0) {
            store_.setSkippedVersion(QString());
        }
    }

    const QString known = store_.knownVersion();
    if (known.isEmpty() || !currentVersionParsed_) {
        return;
    }
    const auto parsed = SemanticVersion::parse(known);
    if (!parsed.has_value() || SemanticVersion::compare(currentVersion_, *parsed) >= 0) {
        store_.setKnownVersion(QString());
        return;
    }
    if (known == store_.skippedVersion()) {
        return;
    }
    // 不等网络就先把标记亮起来；这一版只知道版本号，详情等本次检查回来再补。
    updateAvailable_ = true;
    availableVersion_ = known;
    finding_ = UpdateManifest();
    finding_.versionText = known;
    finding_.version = *parsed;
    emit findingChanged();
}

bool UpdateService::throttleAllows() const
{
    const QDateTime last = store_.lastCheckAt();
    if (!last.isValid()) {
        return true;
    }
    const int window = store_.lastOutcome() == QLatin1String("error") ? kFailureThrottleHours
                                                                     : kSuccessThrottleHours;
    const qint64 elapsed = last.secsTo(QDateTime::currentDateTimeUtc());
    if (elapsed < 0) {
        // 存下来的时间在未来（用户改过系统时钟之类）。继续按窗口算的话自动
        // 检查会一直停到真实时间追上那个假时间为止，所以直接放行一次，让这次
        // 检查把时间戳改回一个正常值。
        return true;
    }
    return elapsed >= static_cast<qint64>(window) * 3600;
}

void UpdateService::scheduleStartupCheck()
{
    if (!store_.checkEnabled()) {
        return;
    }
    QTimer::singleShot(kStartupDelayMs, this, [this]() { checkNow(false); });
}

void UpdateService::checkNow(bool manual)
{
    if (inFlight_) {
        return;
    }
    if (!manual) {
        // restoreKnownFinding() 只恢复得出版本号，详情要靠一次检查补上。节流
        // 是为了不反复打扰服务器，不是为了让一个已经亮起的标记一整天点不开 ——
        // 那比不提示更糟：用户点开对话框，只看到一个禁用的下载按钮。
        const bool needsDetail = updateAvailable_ && finding_.package.url.isEmpty();
        if (!store_.checkEnabled() || (!needsDetail && !throttleAllows())) {
            return;
        }
    }
    inFlight_ = true;
    emit findingChanged();
    // 回调可能在真实 fetcher 那边异步很久之后才回来，而这中间用户完全可能
    // 已经退出。QPointer 在对象析构后自动置空，照 AnalysisService 的既有写法。
    QPointer<UpdateService> guard(this);
    fetcher_.fetch(manifestUrl(environment_.major),
                   [guard, manual](bool fetchOk, QByteArray payload, QString reason) {
                       if (guard.isNull()) {
                           return;
                       }
                       guard->handlePayload(fetchOk, payload, reason, manual);
                   });
}

void UpdateService::handlePayload(bool fetchOk,
                                  const QByteArray& payload,
                                  const QString& reason,
                                  bool manual)
{
    if (!inFlight_) {
        // 端口没有承诺回调只来一次。重复的回调必须原地丢掉，否则会重复记录
        // 检查时间、重复发信号，让 UI 收到两次结果。
        return;
    }
    inFlight_ = false;
    if (!fetchOk) {
        finish(QStringLiteral("failed"), reason, manual);
        return;
    }
    const ManifestParseResult parsed = parseManifest(
        payload, environment_.major, environment_.platformKey, environment_.languageToken);
    switch (parsed.status) {
    case ManifestStatus::Invalid:
        finish(QStringLiteral("failed"), parsed.reason, manual);
        return;
    case ManifestStatus::NotApplicable:
        // 「这份 manifest 不适用于你」不是错误。没有本平台的包要单独说，
        // 否则会把「我们没给你这个平台出包」谎报成「你已经是最新版」。
        clearFinding();
        finish(parsed.reason.contains(QLatin1String("no package"))
                       || parsed.reason.contains(QLatin1String("no release key"))
                   ? QStringLiteral("no-package")
                   : QStringLiteral("up-to-date"),
               parsed.reason, manual);
        return;
    case ManifestStatus::Ok:
        break;
    }

    if (!currentVersionParsed_) {
        // 自身版本都解析不出来，不敢比较（fail closed）。
        finish(QStringLiteral("failed"),
               QStringLiteral("running version is unparseable: '%1'").arg(environment_.versionText),
               manual);
        return;
    }
    if (SemanticVersion::compare(parsed.manifest.version, currentVersion_) <= 0) {
        clearFinding();
        store_.setKnownVersion(QString());
        finish(QStringLiteral("up-to-date"), QString(), manual);
        return;
    }
    if (!manual && parsed.manifest.versionText == store_.skippedVersion()) {
        clearFinding();
        finish(QStringLiteral("up-to-date"), QStringLiteral("version skipped by the user"), false);
        return;
    }

    setFinding(parsed.manifest);
    store_.setKnownVersion(parsed.manifest.versionText);
    finish(QStringLiteral("available"), QString(), manual);
}

void UpdateService::finish(const QString& outcome, const QString& logReason, bool manual)
{
    store_.recordCheck(QDateTime::currentDateTimeUtc(),
                       outcome == QLatin1String("failed") ? QStringLiteral("error")
                                                          : QStringLiteral("ok"));
    if (!logReason.isEmpty()) {
        // 原因只进日志，不进 UI。
        qWarning("update check: %s", qUtf8Printable(logReason));
    }
    emit settingsChanged();
    emit findingChanged();
    if (manual) {
        // "failed" 时不要把上一次的发现当成这次的结果送出去。finding_ 本身
        // 保持不变（状态栏标记不该被一次网络抖动抹掉），但这个信号只描述
        // 「这次检查」，所以失败时它必须是空的。
        emit manualCheckFinished(outcome,
                                 outcome == QLatin1String("failed") ? QVariantMap()
                                                                    : availableDetail());
    }
}

void UpdateService::setFinding(const UpdateManifest& manifest)
{
    finding_ = manifest;
    updateAvailable_ = true;
    availableVersion_ = manifest.versionText;
}

void UpdateService::clearFinding()
{
    finding_ = UpdateManifest();
    updateAvailable_ = false;
    availableVersion_.clear();
}

void UpdateService::setLanguageToken(const QString& token)
{
    if (environment_.languageToken == token) return;
    environment_.languageToken = token;
    emit findingChanged();
    emit settingsChanged();
}

QVariantMap UpdateService::availableDetail() const
{
    return QVariantMap{
        {QStringLiteral("version"), finding_.versionText},
        {QStringLiteral("releasedAt"), finding_.releasedAt},
        {QStringLiteral("notes"), finding_.localizedNotes(environment_.languageToken)},
        {QStringLiteral("sizeText"), formatSize(finding_.package.bytes)},
        {QStringLiteral("downloadUrl"), finding_.package.url},
        {QStringLiteral("mandatory"), finding_.mandatory},
    };
}

void UpdateService::openDownloadPage()
{
    if (!updateAvailable_ || finding_.package.url.isEmpty()) {
        return;
    }
    // OSS 上没有发布页面：直接下载 manifest 按本机平台挑好的包，更新说明
    // 已经在提示对话框里展示过。
    QDesktopServices::openUrl(QUrl(finding_.package.url));
}

void UpdateService::skipAvailableVersion()
{
    if (availableVersion_.isEmpty()) {
        return;
    }
    store_.setSkippedVersion(availableVersion_);
    clearFinding();
    emit findingChanged();
}

} // namespace miacode::update
