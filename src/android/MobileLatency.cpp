#include "MobileLatency.h"

#include "common/ChartClockCount.h"
#include "timeline/TimelineMarkerOffset.h"
#include "tools/latency/LatencyTestChartBuilder.h"

namespace miacode::android {

MobileLatency::MobileLatency(AndroidDocumentSession& document, MobilePreview& preview, QObject* parent)
    : LatencyAudition(parent), document_(document), preview_(preview)
{
    connect(&preview_, &MobilePreview::positionChanged, this, [this] {
        if (onPage_) emit playheadAdvanced(qMax(0.0, preview_.positionSeconds()));
    });
    connect(&preview_, &MobilePreview::playingChanged, this, [this] {
        if (onPage_) emit auditionStateChanged(preview_.playing());
    });
    connect(&preview_, &MobilePreview::transportChanged, this, [this] {
        if (onPage_ && !qFuzzyCompare(lastDuration_, preview_.trackDurationSeconds())) regenerate();
    });
    connect(&document_, &AndroidDocumentSession::documentReplaced, this, [this] { setOnPage(false); });
    connect(&document_, &AndroidDocumentSession::mediaAssetsChanged, this, [this] {
        if (onPage_) { exitSandboxIfActive(); regenerate(); }
    });
}

MobileLatency::~MobileLatency() { setOnPage(false); }
double MobileLatency::documentWholeBpm() const
{
    const auto& document = document_.workspace().document();
    const double whole = chart_clock::wholeBpmFromFields(document.extraFields);
    if (whole > 0) return whole;
    for (const int id : document.difficultyIds()) {
        const double bpm = chart_clock::firstBpmFromChart(document.difficulty(id)->chart);
        if (bpm > 0) return bpm;
    }
    return 0;
}
double MobileLatency::documentOffsetSeconds() const
{ return timeline::offset::parsedFirstSeconds(document_.workspace().document().first); }
int MobileLatency::documentClockCount() const
{
    const int count = chart_clock::clockCountFromDocument(document_.workspace().document());
    return count > 0 ? count : 4;
}
QString MobileLatency::trackPath() const { return document_.previewAssetPath(QStringLiteral("audio")); }
void MobileLatency::applyDetectorBpm(double value)
{
    if (qIsFinite(value) && value > 0)
        document_.workspace().upsertExtraField(QStringLiteral("wholebpm"), QString::number(value, 'f', 3));
}
void MobileLatency::applyDetectorOffset(double value)
{
    if (qIsFinite(value)) document_.setMetadataFirst(QString::number(value, 'f', 3));
}
void MobileLatency::applyDetectorClockCount(int value)
{ document_.setMetadataClockCount(QString::number(qMax(1, value))); }

void MobileLatency::setOnPage(bool enabled)
{
    if (enabled == onPage_) return;
    preview_.setPlaying(false);
    onPage_ = enabled;
    if (enabled) {
        bpm_ = documentWholeBpm();
        if (!(bpm_ > 0)) bpm_ = 120;
        offset_ = documentOffsetSeconds();
        preview_.setLatencyAuditionVolume(sfxVolumePercent_);
        regenerate();
    } else {
        preview_.clearLatencyChart();
        preview_.setLatencyAuditionVolume(-1);
        preview_.stop();
    }
    emit auditionStateChanged(false);
    emit playheadAdvanced(qMax(0.0, preview_.positionSeconds()));
    emit parametersChanged();
}
void MobileLatency::regenerate()
{
    if (!onPage_) return;
    lastDuration_ = preview_.trackDurationSeconds();
    const double duration = lastDuration_ > 1 ? lastDuration_ : 180;
    preview_.setLatencyChart(latency::buildTestChartText(bpm_, subdivision_, duration), offset_);
}
void MobileLatency::setBpm(double value)
{
    if (!qIsFinite(value) || value <= 0 || qFuzzyCompare(value, bpm_)) return;
    bpm_ = value; regenerate(); emit parametersChanged();
}
void MobileLatency::setOffsetSeconds(double value)
{
    if (!qIsFinite(value) || qFuzzyCompare(value, offset_)) return;
    offset_ = value; regenerate(); emit parametersChanged();
}
void MobileLatency::setSubdivision(int value)
{
    value = value == 8 ? 8 : 4;
    if (value == subdivision_) return;
    subdivision_ = value; regenerate(); emit parametersChanged();
}
void MobileLatency::setSfxVolumePercent(int value)
{
    value = qBound(0, value, 100);
    if (value == sfxVolumePercent_) return;
    sfxVolumePercent_ = value;
    if (onPage_) preview_.setLatencyAuditionVolume(value);
    emit parametersChanged();
}
void MobileLatency::toggleAudition() { if (onPage_) preview_.togglePlayback(); }
void MobileLatency::exitSandboxIfActive() { if (onPage_) preview_.stop(); }

}
