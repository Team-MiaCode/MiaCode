#pragma once

#include "app/services/update/UpdateStateStore.h"

namespace miacode::update {

// 读写偏好 JSON 里的 `updates` 对象，与 PreferenceDocument 现有的主题/语言
// 访问器同构：load -> 改子对象 -> save。
class PreferenceUpdateStateStore final : public UpdateStateStore
{
public:
    bool checkEnabled() const override;
    void setCheckEnabled(bool enabled) override;

    QDateTime lastCheckAt() const override;
    QString lastOutcome() const override;
    void recordCheck(const QDateTime& atUtc, const QString& outcome) override;

    QString skippedVersion() const override;
    void setSkippedVersion(const QString& version) override;

    QString knownVersion() const override;
    void setKnownVersion(const QString& version) override;
};

} // namespace miacode::update
