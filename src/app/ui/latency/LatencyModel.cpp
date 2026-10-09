#include "latency/LatencyModel.h"

#include "app/services/LatencyAudition.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFileInfo>

namespace miacode::ui {

namespace {

constexpr int kDecimalsBpm = 3;
constexpr int kDecimalsOffset = 3;

}  // namespace

LatencyModel::LatencyModel(miacode::LatencyEngine*& engineSlot,
                                 QObject* parent)
    : QObject(parent)
    , engineSlot_(&engineSlot)
{
    if (miacode::latency::LatencyAudition* controller = sandbox()) {
        connect(controller, &miacode::latency::LatencyAudition::auditionStateChanged, this,
                [this](bool) { emit auditionChanged(); });
        connect(controller, &miacode::latency::LatencyAudition::playheadAdvanced, this,
                [this](double seconds) {
                    playheadSeconds_ = seconds;
                    emit playheadChanged();
                });
        connect(controller, &miacode::latency::LatencyAudition::parametersChanged, this,
                [this]() { emit valuesChanged(); });
    }
}

miacode::latency::LatencyAudition* LatencyModel::sandbox() const
{
    return engine() != nullptr ? engine()->sandbox() : nullptr;
}

void LatencyModel::enter()
{
    ++contextEpoch_;
    if (miacode::latency::LatencyAudition* controller = sandbox()) {
        controller->setOnPage(true);
    }
    refreshFromDocument();
}

void LatencyModel::leave()
{
    ++contextEpoch_;
    if (miacode::latency::LatencyAudition* controller = sandbox()) {
        controller->setOnPage(false);
    }
}

void LatencyModel::refreshFromDocument()
{
    ++contextEpoch_;
    if (engine() == nullptr) {
        return;
    }
    const double documentBpm = engine()->documentWholeBpm();
    bpm_ = documentBpm > 0.0 ? documentBpm : 120.0;
    offsetSeconds_ = engine()->documentOffsetSeconds();
    clockCount_ = qMax(1, engine()->documentClockCount());
    if (miacode::latency::LatencyAudition* controller = sandbox()) {
        controller->setBpm(bpm_);
        controller->setOffsetSeconds(offsetSeconds_);
    }
    emit valuesChanged();
}

void LatencyModel::setBpm(double value)
{
    if (engine() == nullptr || !qIsFinite(value) || !(value > 0.0) || qFuzzyCompare(value, bpm_)) {
        return;
    }
    bpm_ = value;
    engine()->applyDetectorBpm(value);
    if (miacode::latency::LatencyAudition* controller = sandbox()) {
        controller->setBpm(value);
    }
    emit valuesChanged();
}

void LatencyModel::setOffsetSeconds(double value)
{
    if (engine() == nullptr || !qIsFinite(value) || qFuzzyCompare(value, offsetSeconds_)) {
        return;
    }
    offsetSeconds_ = value;
    engine()->applyDetectorOffset(value);
    if (miacode::latency::LatencyAudition* controller = sandbox()) {
        controller->setOffsetSeconds(value);
    }
    emit valuesChanged();
}

void LatencyModel::setClockCount(int value)
{
    if (engine() == nullptr || value <= 0 || value == clockCount_) {
        return;
    }
    clockCount_ = value;
    engine()->applyDetectorClockCount(value);
    emit valuesChanged();
}

int LatencyModel::subdivision() const
{
    miacode::latency::LatencyAudition* controller = sandbox();
    return controller != nullptr ? controller->subdivision() : 4;
}

void LatencyModel::setSubdivision(int value)
{
    if (miacode::latency::LatencyAudition* controller = sandbox()) {
        controller->setSubdivision(value);
        emit valuesChanged();
    }
}

int LatencyModel::sfxVolumePercent() const
{
    miacode::latency::LatencyAudition* controller = sandbox();
    return controller != nullptr ? controller->sfxVolumePercent() : 80;
}

void LatencyModel::setSfxVolumePercent(int value)
{
    if (miacode::latency::LatencyAudition* controller = sandbox()) {
        controller->setSfxVolumePercent(value);
        emit valuesChanged();
    }
}

bool LatencyModel::auditionRunning() const
{
    miacode::latency::LatencyAudition* controller = sandbox();
    return controller != nullptr && controller->isAuditionRunning();
}

void LatencyModel::toggleAudition()
{
    if (miacode::latency::LatencyAudition* controller = sandbox()) {
        controller->toggleAudition();
    }
}

QString LatencyModel::positionText() const
{
    const qint64 totalMs = qMax<qint64>(0, qRound64(playheadSeconds_ * 1000.0));
    return QStringLiteral("%1:%2.%3")
        .arg(totalMs / 60000, 2, 10, QLatin1Char('0'))
        .arg((totalMs / 1000) % 60, 2, 10, QLatin1Char('0'))
        .arg(totalMs % 1000, 3, 10, QLatin1Char('0'));
}

bool LatencyModel::trackAvailable() const
{
    return engine() != nullptr && !engine()->trackPath().isEmpty();
}

bool LatencyModel::ensureAudioEnvelopeReady()
{
    const QString trackPath = engine() != nullptr ? engine()->trackPath() : QString();
    if (trackPath.isEmpty()) {
        clearAudioEnvelopeCache();
        return false;
    }
    const auto fileIdentity = [](const QString& path) {
        const QFileInfo file(path);
        return file.absoluteFilePath() + QLatin1Char(':') + QString::number(file.size())
            + QLatin1Char(':') + QString::number(file.lastModified().toMSecsSinceEpoch());
    };
    const QString identity = fileIdentity(trackPath);
    if (identity == cachedAudioIdentity_ && !cachedOnsetEnvelope_.isEmpty()
        && !cachedTransientEnvelope_.isEmpty()) {
        return true;
    }
    const quint64 epoch = contextEpoch_;
    const auto decoded = miacode::latency_analysis::decodeMonoTrack(
        trackPath, miacode::latency_analysis::kAnalysisSampleRate);
    // The mobile decoder processes events. Page/document changes during
    // decoding must not publish a result into the next calibration context.
    if (epoch != contextEpoch_ || engine() == nullptr || engine()->trackPath() != trackPath
        || identity != fileIdentity(trackPath)) return false;
    if (decoded.samples.isEmpty() || decoded.sampleRate <= 0) {
        clearAudioEnvelopeCache();
        return false;
    }
    cachedAudioIdentity_ = identity;
    cachedAudioDurationSeconds_ = decoded.durationSeconds;
    cachedOnsetEnvelope_ =
        miacode::latency_analysis::buildOnsetEnvelope(decoded.samples, decoded.sampleRate);
    cachedTransientEnvelope_ =
        miacode::latency_analysis::buildTransientEnvelope(decoded.samples, decoded.sampleRate);
    return !cachedOnsetEnvelope_.isEmpty() && !cachedTransientEnvelope_.isEmpty();
}

void LatencyModel::clearAudioEnvelopeCache()
{
    cachedAudioIdentity_.clear();
    cachedOnsetEnvelope_ = miacode::latency_analysis::Envelope();
    cachedTransientEnvelope_ = miacode::latency_analysis::Envelope();
    cachedAudioDurationSeconds_ = 0.0;
}

void LatencyModel::detectBpm()
{
    if (engine() == nullptr) {
        return;
    }
    if (!trackAvailable()) {
        bpmDetectResult_ = qtTrId("latency.track_audio_missing");
        emit detectionChanged();
        return;
    }
    const quint64 epoch = contextEpoch_;
    if (!ensureAudioEnvelopeReady()) {
        if (epoch != contextEpoch_) return;
        bpmDetectResult_ = qtTrId("latency.audio_decode_failed");
        emit detectionChanged();
        return;
    }
    const auto result = miacode::latency_analysis::detectBpm(cachedOnsetEnvelope_);
    if (!(result.bpm > 0.0)) {
        bpmDetectResult_ = qtTrId("latency.bpm_not_detected");
        emit detectionChanged();
        return;
    }
    lastDetectedMeterId_ = result.meterId;
    lastDetectedMeterPhase_ = result.meterPhaseSeconds;
    hasLastDetectedMeterPhase_ = result.meterPhaseValid;
    setBpm(result.bpm);
    bpmDetectResult_ =
        qtTrId("latency.detected_1").arg(result.bpm, 0, 'f', kDecimalsBpm);
    emit detectionChanged();
}

void LatencyModel::detectOffset()
{
    if (engine() == nullptr) {
        return;
    }
    if (!trackAvailable()) {
        offsetDetectResult_ = qtTrId("latency.track_audio_missing");
        emit detectionChanged();
        return;
    }
    if (!(bpm_ > 0.0)) {
        offsetDetectResult_ = qtTrId("latency.set_or_detect_bpm_first");
        emit detectionChanged();
        return;
    }
    const quint64 epoch = contextEpoch_;
    if (!ensureAudioEnvelopeReady()) {
        if (epoch != contextEpoch_) return;
        offsetDetectResult_ = qtTrId("latency.audio_decode_failed");
        emit detectionChanged();
        return;
    }
    miacode::latency_analysis::OffsetDetectionInputs inputs;
    inputs.bpm = bpm_;
    inputs.offsetAnchorSeconds = offsetSeconds_;
    inputs.trackDurationSeconds = cachedAudioDurationSeconds_;
    inputs.meterId = QStringLiteral("auto");
    inputs.snapMode = QStringLiteral("bar");
    inputs.lastDetectedMeterPhase = lastDetectedMeterPhase_;
    inputs.hasLastDetectedMeterPhase = hasLastDetectedMeterPhase_;
    inputs.lastDetectedMeterId = lastDetectedMeterId_;
    const double offset = miacode::latency_analysis::detectOffset(
        cachedOnsetEnvelope_, cachedTransientEnvelope_, inputs);
    setOffsetSeconds(offset);
    offsetDetectResult_ =
        qtTrId("latency.detected_1_s").arg(offset, 0, 'f', kDecimalsOffset);
    emit detectionChanged();
}

}  // namespace miacode::ui
