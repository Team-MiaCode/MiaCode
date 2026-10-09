#pragma once

#include "app/services/LatencyAudition.h"
#include "app/services/LatencyEngine.h"
#include "AndroidDocumentSession.h"
#include "MobilePreview.h"

namespace miacode::android {

class MobileLatency final : public latency::LatencyAudition, public LatencyEngine {
    Q_OBJECT
public:
    MobileLatency(AndroidDocumentSession& document, MobilePreview& preview, QObject* parent = nullptr);
    ~MobileLatency() override;
    double documentWholeBpm() const override;
    double documentOffsetSeconds() const override;
    int documentClockCount() const override;
    QString trackPath() const override;
    void applyDetectorBpm(double value) override;
    void applyDetectorOffset(double value) override;
    void applyDetectorClockCount(int value) override;
    latency::LatencyAudition* sandbox() const override { return const_cast<MobileLatency*>(this); }
    void exitSandboxIfActive() override;
    void setOnPage(bool enabled) override;
    bool isOnPage() const { return onPage_; }
    bool isAuditionRunning() const override { return onPage_ && preview_.playing(); }
    int subdivision() const override { return subdivision_; }
    int sfxVolumePercent() const override { return sfxVolumePercent_; }
    void setBpm(double value) override;
    void setOffsetSeconds(double value) override;
    void setSubdivision(int value) override;
    void setSfxVolumePercent(int value) override;
    void toggleAudition() override;
private:
    void regenerate();
    AndroidDocumentSession& document_;
    MobilePreview& preview_;
    double bpm_ = 120;
    double offset_ = 0;
    int subdivision_ = 4;
    int sfxVolumePercent_ = 80;
    bool onPage_ = false;
    double lastDuration_ = 0;
};

}
