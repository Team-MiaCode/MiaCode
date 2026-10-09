#pragma once

#include <QObject>

namespace miacode::latency {

// The shared calibration model drives each platform's existing transport.
class LatencyAudition : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual void setOnPage(bool enabled) = 0;
    virtual bool isAuditionRunning() const = 0;
    virtual int subdivision() const = 0;
    virtual int sfxVolumePercent() const = 0;
    virtual void setBpm(double value) = 0;
    virtual void setOffsetSeconds(double value) = 0;
    virtual void setSubdivision(int value) = 0;
    virtual void setSfxVolumePercent(int value) = 0;
    virtual void toggleAudition() = 0;
signals:
    void auditionStateChanged(bool running);
    void parametersChanged();
    void playheadAdvanced(double seconds);
};

}
