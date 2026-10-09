#pragma once

#include "app/services/update/UpdateFetcher.h"
#include "app/services/update/UpdateManifest.h"
#include "app/services/update/UpdateStateStore.h"

#include <QObject>
#include <QString>
#include <QVariantMap>

namespace miacode::update {

// 本次运行的身份。由调用方从编译期常量与运行期查询拼好后传入，这样 spec
// 可以固定住它，断言不随真实构建版本漂移。
struct UpdateEnvironment {
    QString versionText;
    int major = 0;
    QString platformKey;
    QString languageToken;
};

// 更新检查的编排者：读偏好 -> 判节流 -> 组 URL -> 取 manifest -> 比版本 ->
// 更新状态并发信号。不含任何 UI 代码，也不自己弹任何东西。
class UpdateService final : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool checkEnabled READ checkEnabled WRITE setCheckEnabled NOTIFY settingsChanged)
    Q_PROPERTY(QString channelToken READ channelToken WRITE setChannelToken NOTIFY settingsChanged)
    Q_PROPERTY(QString lastCheckText READ lastCheckText NOTIFY settingsChanged)
    Q_PROPERTY(bool checkInFlight READ checkInFlight NOTIFY findingChanged)
    Q_PROPERTY(bool updateAvailable READ updateAvailable NOTIFY findingChanged)
    Q_PROPERTY(QString availableVersion READ availableVersion NOTIFY findingChanged)
    Q_PROPERTY(QString currentVersion READ currentVersion CONSTANT)

public:
    UpdateService(UpdateFetcher& fetcher,
                  UpdateStateStore& store,
                  UpdateEnvironment environment,
                  QObject* parent = nullptr);

    bool checkEnabled() const;
    void setCheckEnabled(bool enabled);
    QString channelToken() const;
    void setChannelToken(const QString& token);
    void setLanguageToken(const QString& token);
    QString lastCheckText() const;
    bool checkInFlight() const { return inFlight_; }
    bool updateAvailable() const { return updateAvailable_; }
    QString availableVersion() const { return availableVersion_; }
    QString currentVersion() const { return environment_.versionText; }

    // 有效通道：用户显式选择优先，否则按本构建是否为 prerelease 决定。
    // QML 的偏好设置页直接调用它，所以必须是 Q_INVOKABLE —— 普通公有方法
    // QML 看不见。
    Q_INVOKABLE QString effectiveChannel() const;

    // manual=true 绕过节流、绕过「已跳过版本」，且无论结果都发
    // manualCheckFinished。manual=false 是启动后的自动检查。
    Q_INVOKABLE void checkNow(bool manual);

    // 启动后延迟一段时间做一次自动检查。延迟是为了避开启动期的 I/O 高峰。
    Q_INVOKABLE void scheduleStartupCheck();

    // 把用户送到 Release 页面。没有可用更新时什么都不做。
    Q_INVOKABLE void openDownloadPage();

    // 记下当前可用版本为「已跳过」，并清掉指示标记。
    Q_INVOKABLE void skipAvailableVersion();

    // 提示要显示的全部字段：version / releasedAt / notes / sizeText /
    // releasePageUrl / mandatory。
    Q_INVOKABLE QVariantMap availableDetail() const;

signals:
    void settingsChanged();
    void findingChanged();
    // outcome ∈ "available" / "up-to-date" / "no-package" / "failed"。
    // 只在手动检查时发；自动检查静默。
    void manualCheckFinished(const QString& outcome, const QVariantMap& detail);

private:
    void restoreKnownFinding();
    bool throttleAllows() const;
    void handlePayload(bool fetchOk, const QByteArray& payload, const QString& reason, bool manual);
    void finish(const QString& outcome, const QString& logReason, bool manual);
    void setFinding(const UpdateManifest& manifest);
    void clearFinding();

    UpdateFetcher& fetcher_;
    UpdateStateStore& store_;
    UpdateEnvironment environment_;
    SemanticVersion currentVersion_;
    bool currentVersionParsed_ = false;
    bool inFlight_ = false;
    bool updateAvailable_ = false;
    QString availableVersion_;
    UpdateManifest finding_;
};

} // namespace miacode::update
