#pragma once

#include <QDateTime>
#include <QString>

namespace miacode::update {

// UpdateService 的持久化端口。生产实现写偏好 JSON 的 `updates` 对象；
// spec 注入一个内存实现，这样节流与「跳过版本」的行为可以离线测试。
class UpdateStateStore
{
public:
    virtual ~UpdateStateStore() = default;

    virtual bool checkEnabled() const = 0;
    virtual void setCheckEnabled(bool enabled) = 0;

    virtual QDateTime lastCheckAt() const = 0;
    // "ok" 或 "error"，决定下次节流窗口的长短。
    virtual QString lastOutcome() const = 0;
    virtual void recordCheck(const QDateTime& atUtc, const QString& outcome) = 0;

    virtual QString skippedVersion() const = 0;
    virtual void setSkippedVersion(const QString& version) = 0;

    // 上次发现的版本，用于启动时不等网络就能亮状态栏标记。
    virtual QString knownVersion() const = 0;
    virtual void setKnownVersion(const QString& version) = 0;
};

} // namespace miacode::update
