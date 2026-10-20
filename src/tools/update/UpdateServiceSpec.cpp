#include "app/services/update/UpdateService.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
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

using miacode::update::UpdateEnvironment;
using miacode::update::UpdateFetcher;
using miacode::update::UpdateService;
using miacode::update::UpdateStateStore;

// 同步返回预置响应，并记录被请求的 URL。
class FakeFetcher final : public UpdateFetcher
{
public:
    void fetch(const QUrl& url, Callback callback) override
    {
        requestedUrls.append(url.toString());
        ++fetchCount;
        callback(nextOk, nextPayload, nextReason);
    }

    QStringList requestedUrls;
    int fetchCount = 0;
    bool nextOk = true;
    QByteArray nextPayload;
    QString nextReason;
};

class MemoryStore final : public UpdateStateStore
{
public:
    bool checkEnabled() const override { return enabled; }
    void setCheckEnabled(bool value) override { enabled = value; }
    QDateTime lastCheckAt() const override { return checkedAt; }
    QString lastOutcome() const override { return outcome; }
    void recordCheck(const QDateTime& atUtc, const QString& value) override
    {
        checkedAt = atUtc;
        outcome = value;
        ++recordCount;
    }
    QString skippedVersion() const override { return skipped; }
    void setSkippedVersion(const QString& version) override { skipped = version; }
    QString knownVersion() const override { return known; }
    void setKnownVersion(const QString& version) override { known = version; }

    bool enabled = true;
    QDateTime checkedAt;
    QString outcome;
    QString skipped;
    QString known;
    int recordCount = 0;
};

QByteArray manifestPayload(const QString& version)
{
    const QJsonObject root{
        {QStringLiteral("schema"), 1},
        {QStringLiteral("major"), 2},
        {QStringLiteral("version"), version},
        {QStringLiteral("releasedAt"), QStringLiteral("2026-09-20")},
        {QStringLiteral("notes"), QJsonObject{{QStringLiteral("en_US"), QStringLiteral("notes")}}},
        {QStringLiteral("platforms"), QJsonObject{
             {QStringLiteral("macos-arm64"), QJsonObject{
                  {QStringLiteral("file"), QStringLiteral("mac.7z")},
                  {QStringLiteral("bytes"), 84231168},
                  {QStringLiteral("sha256"), QStringLiteral("abc")},
                  {QStringLiteral("url"), QStringLiteral("https://example.invalid/mac.7z")},
              }},
         }},
    };
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

// 当前构建固定当作 2.0.0-alpha / major 2 / macos-arm64 / zh_CN，
// 这样断言不随真实构建版本漂移。
UpdateEnvironment testEnvironment()
{
    UpdateEnvironment environment;
    environment.versionText = QStringLiteral("2.0.0-alpha");
    environment.major = 2;
    environment.platformKey = QStringLiteral("macos-arm64");
    environment.languageToken = QStringLiteral("zh_CN");
    return environment;
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTextStream err(stderr);
    bool ok = true;

    // ---- 每个 major 只有一份 OSS manifest，预发布与正式构建读同一个地址 ----
    for (const QString& version : {QStringLiteral("2.0.0-alpha"), QStringLiteral("2.0.0")}) {
        FakeFetcher fetcher;
        MemoryStore store;
        UpdateEnvironment environment = testEnvironment();
        environment.versionText = version;
        fetcher.nextPayload = manifestPayload(QStringLiteral("2.1.0"));
        UpdateService service(fetcher, store, environment);
        service.checkNow(true);
        ok &= expect(fetcher.requestedUrls == QStringList{QStringLiteral(
                         "https://miacode.oss-cn-beijing.aliyuncs.com/update/2/latest.json")},
                     QStringLiteral("every %1 build reads the major's single OSS manifest").arg(version), err);
    }

    // ---- 找到更新 ----
    {
        FakeFetcher fetcher;
        MemoryStore store;
        fetcher.nextPayload = manifestPayload(QStringLiteral("2.1.0"));
        UpdateService service(fetcher, store, testEnvironment());
        QSignalSpy reported(&service, &UpdateService::manualCheckFinished);
        service.checkNow(true);
        ok &= expect(service.updateAvailable() && service.availableVersion() == QLatin1String("2.1.0"),
                     QStringLiteral("a higher manifest version becomes the available update"), err);
        ok &= expect(store.known == QLatin1String("2.1.0"),
                     QStringLiteral("the found version is remembered for the next launch"), err);
        ok &= expect(store.outcome == QLatin1String("ok") && store.recordCount == 1,
                     QStringLiteral("a successful check is recorded once"), err);
        ok &= expect(reported.count() == 1
                         && reported.first().at(0).toString() == QLatin1String("available"),
                     QStringLiteral("a manual check reports that an update is available"), err);
    }

    // ---- 已是最新 ----
    {
        FakeFetcher fetcher;
        MemoryStore store;
        fetcher.nextPayload = manifestPayload(QStringLiteral("2.0.0-alpha"));
        UpdateService service(fetcher, store, testEnvironment());
        QSignalSpy reported(&service, &UpdateService::manualCheckFinished);
        service.checkNow(true);
        ok &= expect(!service.updateAvailable(),
                     QStringLiteral("the same version is not an update"), err);
        ok &= expect(reported.count() == 1
                         && reported.first().at(0).toString() == QLatin1String("up-to-date"),
                     QStringLiteral("a manual check says so when already current"), err);
    }
    {
        FakeFetcher fetcher;
        MemoryStore store;
        fetcher.nextPayload = manifestPayload(QStringLiteral("1.9.9"));
        UpdateService service(fetcher, store, testEnvironment());
        service.checkNow(true);
        ok &= expect(!service.updateAvailable(),
                     QStringLiteral("an older manifest version never downgrades the user"), err);
    }

    // ---- 本平台无包 ----
    {
        FakeFetcher fetcher;
        MemoryStore store;
        UpdateEnvironment environment = testEnvironment();
        environment.platformKey = QString();
        fetcher.nextPayload = manifestPayload(QStringLiteral("2.1.0"));
        UpdateService service(fetcher, store, environment);
        QSignalSpy reported(&service, &UpdateService::manualCheckFinished);
        service.checkNow(true);
        ok &= expect(!service.updateAvailable()
                         && reported.count() == 1
                         && reported.first().at(0).toString() == QLatin1String("no-package"),
                     QStringLiteral("no package for this platform is reported as such, not as latest"), err);
    }

    // ---- 失败路径 ----
    {
        FakeFetcher fetcher;
        MemoryStore store;
        fetcher.nextOk = false;
        fetcher.nextReason = QStringLiteral("host unreachable");
        UpdateService service(fetcher, store, testEnvironment());
        QSignalSpy reported(&service, &UpdateService::manualCheckFinished);
        service.checkNow(true);
        ok &= expect(reported.count() == 1
                         && reported.first().at(0).toString() == QLatin1String("failed"),
                     QStringLiteral("a manual check surfaces a network failure"), err);
        ok &= expect(store.outcome == QLatin1String("error"),
                     QStringLiteral("a failed check is recorded as an error"), err);
    }
    {
        // 先成功找到一个更新，再让同一个 fetcher 转为失败：失败信号不能带着
        // 上一次找到的版本，但状态栏标记本身必须原样留着。
        FakeFetcher fetcher;
        MemoryStore store;
        fetcher.nextPayload = manifestPayload(QStringLiteral("2.1.0"));
        UpdateService service(fetcher, store, testEnvironment());
        service.checkNow(true);

        fetcher.nextOk = false;
        fetcher.nextReason = QStringLiteral("host unreachable");
        QSignalSpy reportedAgain(&service, &UpdateService::manualCheckFinished);
        service.checkNow(true);
        ok &= expect(reportedAgain.count() == 1
                         && reportedAgain.first().at(0).toString() == QLatin1String("failed")
                         && reportedAgain.first().at(1).toMap().value(QStringLiteral("version")).toString().isEmpty(),
                     QStringLiteral("a failed check does not report the previous find's version as its own result"),
                     err);
        ok &= expect(service.updateAvailable(),
                     QStringLiteral("a transient failure does not clear the status-bar badge"), err);
    }
    {
        FakeFetcher fetcher;
        MemoryStore store;
        fetcher.nextPayload = QByteArray("not json");
        UpdateService service(fetcher, store, testEnvironment());
        QSignalSpy reported(&service, &UpdateService::manualCheckFinished);
        service.checkNow(true);
        ok &= expect(reported.first().at(0).toString() == QLatin1String("failed"),
                     QStringLiteral("a malformed manifest is a failure, not silence"), err);
    }
    {
        // major 不匹配属于「不适用」，不是失败。
        FakeFetcher fetcher;
        MemoryStore store;
        UpdateEnvironment environment = testEnvironment();
        environment.major = 1;
        environment.versionText = QStringLiteral("1.1.0");
        fetcher.nextPayload = manifestPayload(QStringLiteral("2.1.0"));
        UpdateService service(fetcher, store, environment);
        QSignalSpy reported(&service, &UpdateService::manualCheckFinished);
        service.checkNow(true);
        ok &= expect(!service.updateAvailable()
                         && reported.first().at(0).toString() == QLatin1String("up-to-date"),
                     QStringLiteral("a 1.x build is never pushed a 2.x manifest"), err);
    }

    // ---- 节流 ----
    {
        FakeFetcher fetcher;
        MemoryStore store;
        store.checkedAt = QDateTime::currentDateTimeUtc().addSecs(-3600);
        store.outcome = QStringLiteral("ok");
        fetcher.nextPayload = manifestPayload(QStringLiteral("2.1.0"));
        UpdateService service(fetcher, store, testEnvironment());
        service.checkNow(false);
        ok &= expect(fetcher.fetchCount == 0,
                     QStringLiteral("an automatic check inside the 24h success window is skipped"), err);
        service.checkNow(true);
        ok &= expect(fetcher.fetchCount == 1,
                     QStringLiteral("a manual check ignores the throttle"), err);
    }
    {
        FakeFetcher fetcher;
        MemoryStore store;
        store.checkedAt = QDateTime::currentDateTimeUtc().addSecs(-5 * 3600);
        store.outcome = QStringLiteral("error");
        fetcher.nextPayload = manifestPayload(QStringLiteral("2.1.0"));
        UpdateService service(fetcher, store, testEnvironment());
        service.checkNow(false);
        ok &= expect(fetcher.fetchCount == 1,
                     QStringLiteral("after a failure the window is 4h, so 5h later retries"), err);
    }
    {
        // 存下来的时间在未来（时钟被改过又调回来之类）：按窗口算永远算不出
        // 「已经过了窗口」，节流会一直卡死。必须把它当成「窗口已过」处理。
        FakeFetcher fetcher;
        MemoryStore store;
        store.checkedAt = QDateTime::currentDateTimeUtc().addSecs(3600);
        store.outcome = QStringLiteral("ok");
        fetcher.nextPayload = manifestPayload(QStringLiteral("2.1.0"));
        UpdateService service(fetcher, store, testEnvironment());
        service.checkNow(false);
        ok &= expect(fetcher.fetchCount == 1,
                     QStringLiteral("a stored check time in the future does not wedge automatic checks"), err);
    }
    {
        // 记住的版本只够点亮标记，详情要等一次检查才有。节流是为了不反复打扰
        // 服务器，不是为了让一个亮着的标记一整天点不开 —— 那比不提示更糟：
        // 用户点开对话框，只看到一个禁用的下载按钮。
        FakeFetcher fetcher;
        MemoryStore store;
        store.known = QStringLiteral("2.1.0");
        store.checkedAt = QDateTime::currentDateTimeUtc().addSecs(-60);
        store.outcome = QStringLiteral("ok");
        fetcher.nextPayload = manifestPayload(QStringLiteral("2.1.0"));
        UpdateService service(fetcher, store, testEnvironment());
        ok &= expect(service.updateAvailable()
                         && service.availableDetail()
                                .value(QStringLiteral("downloadUrl")).toString().isEmpty(),
                     QStringLiteral("a remembered version lights the badge before any detail exists"), err);
        service.checkNow(false);
        ok &= expect(fetcher.fetchCount == 1,
                     QStringLiteral("the throttle gives way while the badge has no detail to show"), err);
        ok &= expect(service.availableDetail().value(QStringLiteral("downloadUrl")).toString()
                         == QLatin1String("https://example.invalid/mac.7z"),
                     QStringLiteral("the refetch fills in the detail the badge promised"), err);
    }
    {
        // 上面那条例外不能退化成「每次启动都检查」：详情到手之后节流照常生效。
        FakeFetcher fetcher;
        MemoryStore store;
        fetcher.nextPayload = manifestPayload(QStringLiteral("2.1.0"));
        UpdateService service(fetcher, store, testEnvironment());
        service.checkNow(true);
        service.checkNow(false);
        ok &= expect(fetcher.fetchCount == 1,
                     QStringLiteral("once the detail is in hand the throttle applies again"), err);
    }
    {
        FakeFetcher fetcher;
        MemoryStore store;
        store.enabled = false;
        UpdateService service(fetcher, store, testEnvironment());
        service.checkNow(false);
        ok &= expect(fetcher.fetchCount == 0,
                     QStringLiteral("a disabled automatic check never reaches the network"), err);
        service.checkNow(true);
        ok &= expect(fetcher.fetchCount == 1,
                     QStringLiteral("the manual button still works while automatic checks are off"), err);
    }

    // ---- 跳过版本 ----
    {
        FakeFetcher fetcher;
        MemoryStore store;
        store.skipped = QStringLiteral("2.1.0");
        fetcher.nextPayload = manifestPayload(QStringLiteral("2.1.0"));
        UpdateService service(fetcher, store, testEnvironment());
        service.checkNow(false);
        ok &= expect(!service.updateAvailable(),
                     QStringLiteral("an automatic check honours a skipped version"), err);
        service.checkNow(true);
        ok &= expect(service.updateAvailable(),
                     QStringLiteral("a manual check reports even a skipped version"), err);
    }
    {
        FakeFetcher fetcher;
        MemoryStore store;
        store.skipped = QStringLiteral("2.1.0");
        fetcher.nextPayload = manifestPayload(QStringLiteral("2.2.0"));
        UpdateService service(fetcher, store, testEnvironment());
        service.checkNow(false);
        ok &= expect(service.updateAvailable(),
                     QStringLiteral("skipping one version does not mute the next one"), err);
    }
    {
        FakeFetcher fetcher;
        MemoryStore store;
        fetcher.nextPayload = manifestPayload(QStringLiteral("2.1.0"));
        UpdateService service(fetcher, store, testEnvironment());
        service.checkNow(true);
        service.skipAvailableVersion();
        ok &= expect(store.skipped == QLatin1String("2.1.0") && !service.updateAvailable(),
                     QStringLiteral("skipping clears the indicator and remembers the version"), err);
    }

    // ---- 启动时清理陈旧状态 ----
    {
        FakeFetcher fetcher;
        MemoryStore store;
        // 上次记下 2.0.0-alpha，而现在跑的就是它：标记必须自己消失。
        store.known = QStringLiteral("2.0.0-alpha");
        store.skipped = QStringLiteral("1.5.0");
        UpdateService service(fetcher, store, testEnvironment());
        ok &= expect(!service.updateAvailable(),
                     QStringLiteral("a known version the build has reached no longer shows as available"), err);
        ok &= expect(store.known.isEmpty(),
                     QStringLiteral("a reached known version is cleared from storage"), err);
        ok &= expect(store.skipped.isEmpty(),
                     QStringLiteral("a skipped version below the running build is cleared"), err);
    }
    {
        FakeFetcher fetcher;
        MemoryStore store;
        store.known = QStringLiteral("2.1.0");
        UpdateService service(fetcher, store, testEnvironment());
        ok &= expect(service.updateAvailable() && service.availableVersion() == QLatin1String("2.1.0"),
                     QStringLiteral("a known higher version shows immediately without the network"), err);
        ok &= expect(fetcher.fetchCount == 0,
                     QStringLiteral("restoring the known version does not touch the network"), err);
    }
    {
        // 已知陷阱回归测试：known 和 skipped 是同一个版本时，「先清 skipped 再
        // 读 known」的顺序必须保住——用户已经跳过的版本不该在下次启动时又亮起来。
        FakeFetcher fetcher;
        MemoryStore store;
        store.known = QStringLiteral("2.1.0");
        store.skipped = QStringLiteral("2.1.0");
        UpdateService service(fetcher, store, testEnvironment());
        ok &= expect(!service.updateAvailable(),
                     QStringLiteral("a version already skipped by the user does not light the badge on the next launch"),
                     err);
    }

    // ---- 并发保护 ----
    {
        // 假 fetcher 是同步的，所以这里用「正在进行」的语义来验证：
        // 一次 fetch 未回调完成前不得发起第二次。用一个不回调的 fetcher。
        class NeverCallsBack final : public UpdateFetcher
        {
        public:
            void fetch(const QUrl&, Callback) override { ++fetchCount; }
            int fetchCount = 0;
        };
        NeverCallsBack fetcher;
        MemoryStore store;
        UpdateService service(fetcher, store, testEnvironment());
        service.checkNow(true);
        service.checkNow(true);
        ok &= expect(fetcher.fetchCount == 1,
                     QStringLiteral("a second check while one is in flight is dropped"), err);
    }
    {
        // 端口没有承诺回调只来一次；这里模拟一个（错误地）调用两次的实现，
        // 照 NeverCallsBack 的形状写，只是方向相反。
        class CallsBackTwice final : public UpdateFetcher
        {
        public:
            void fetch(const QUrl&, Callback callback) override
            {
                callback(true, payload, QString());
                callback(true, payload, QString());
            }
            QByteArray payload;
        };
        CallsBackTwice fetcher;
        fetcher.payload = manifestPayload(QStringLiteral("2.1.0"));
        MemoryStore store;
        UpdateService service(fetcher, store, testEnvironment());
        service.checkNow(true);
        ok &= expect(store.recordCount == 1,
                     QStringLiteral("a fetcher that calls its callback twice does not double-record the check"), err);
    }

    if (ok) {
        QTextStream(stdout) << "update_service_spec ok\n";
        return 0;
    }
    return 1;
}
