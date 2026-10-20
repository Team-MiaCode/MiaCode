#include "audio/bass/BassPreviewAudioBackend.h"

#include "audio/bass/PreviewBassEmergencyPause.h"

#include "audio/bass/BassPreviewDebugLogRouting.h"
#include "audio/bass/BassPreviewRetainedState.h"
#include "core/chart/ChartAssetPaths.h"
#include "common/DebugLog.h"
#include "common/DebugOptions.h"
#include "common/FileContentStamp.h"
#include "common/OperationLog.h"
#include "audio/PreviewAudioMixConfig.h"
#include "core/scene/PreviewSfxAssets.h"
#include "core/scene/PreviewSfxTimeline.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QtMath>

#include <cstdio>   // G1 Commit 8 followup: std::snprintf for startup-beacon lines

#include "bass.h"
#include "bassmix.h"

#include "audio/bass/BassPreviewAudioBackendImpl.h"
#include "audio/bass/BassPreviewAudioBackendSample.h"

using namespace miacode::audio::bass_detail;

BassPreviewAudioBackend::BassPreviewAudioBackend(QObject* parent)
    : QObject(parent)
{
    appendAudioDebugLog("BassPreviewAudioBackend created");
}

BassPreviewAudioBackend::~BassPreviewAudioBackend()
{
    appendAudioDebugLog("BassPreviewAudioBackend destroying");
    shuttingDown_.store(true, std::memory_order_release);
    miacode::preview_audio::PreviewBassEmergencyPause::disarm();
    // PreviewAudioWorker serializes health sampling with shutdown on this backend thread.
    stopPlaybackSession();
    resetAssets();
    unloadOptionalPlugins();
    detachOutputGlitchProbe();
    if (masterMixer_ != 0) {
        BASS_StreamFree(masterMixer_);
        noteBassErr("dtor/master_stream_free");
        masterMixer_ = 0;
        masterMixerOutputBufferSeconds_ = 0.0;
        masterMixerBytesPerSecond_ = 0.0;
    }
    unloadBassFx();
    bassDeviceLease_.release();
    engineInitialized_ = false;
}

QString BassPreviewAudioBackend::backendId() const
{
    return QStringLiteral("bass");
}

int BassPreviewAudioBackend::nativeErrorCode() const noexcept
{
    return lastNativeErrorCode_;
}

void BassPreviewAudioBackend::clearNativeErrorCode() noexcept
{
    lastNativeErrorCode_ = 0;
}

QString BassPreviewAudioBackend::resolveTrackPath(const QString& chartPath) const
{
    // Resolved-path warmup cache removed (2026-06-03). It keyed on the chart-path
    // string only and never re-validated the track file, so a same-named track
    // with new content — or a track that appeared/changed after warmup — was
    // shadowed by a stale (or empty) cached path, silently dropping the BGM.
    // The live resolver is cheap and exists-checked; the warmup worker still
    // byte-prefetches the file into the OS cache, which was warmup's real value.
    return miacode::chart_assets::resolveTrackPath(chartPath);
}

QString BassPreviewAudioBackend::resolveSfxDir() const
{
    return miacode::preview_sfx::resolveSfxDirectory();
}

bool BassPreviewAudioBackend::runtimeLibrariesPresent() const
{
#if defined(Q_OS_WIN)
    return runtimeLibraryExists(QStringLiteral("bass.dll"))
        && runtimeLibraryExists(QStringLiteral("bassmix.dll"))
        && runtimeLibraryExists(QStringLiteral("bass_fx.dll"));
#elif defined(Q_OS_MACOS)
    return runtimeLibraryExists(QStringLiteral("libbass.dylib"))
        && runtimeLibraryExists(QStringLiteral("libbassmix.dylib"))
        && runtimeLibraryExists(QStringLiteral("libbass_fx.dylib"));
#elif defined(Q_OS_LINUX)
    return runtimeLibraryExists(QStringLiteral("libbass.so"))
        && runtimeLibraryExists(QStringLiteral("libbassmix.so"))
        && runtimeLibraryExists(QStringLiteral("libbass_fx.so"));
#else
    return false;
#endif
}

bool BassPreviewAudioBackend::canBePrimary(QString* reason) const
{
    if (!runtimeLibrariesPresent()) {
        if (reason != nullptr) {
            *reason = QStringLiteral("missing bundled BASS runtime libraries");
        }
        return false;
    }
    if (reason != nullptr) {
        *reason = QStringLiteral("bundled BASS runtime libraries are available");
    }
    return true;
}

void BassPreviewAudioBackend::setWarmupResolvedPaths(const QString& chartPath, const QString& trackPath, const QString& sfxDir)
{
    MC_OP("BassPreviewAudioBackend::setWarmupResolvedPaths");
    // No-op: the resolved-path cache was removed (see resolveTrackPath). Path
    // resolution is now always live so a same-named track with new content is
    // picked up. Kept as a no-op to preserve the backend interface; the warmup
    // worker still byte-prefetches the files into the OS cache.
    Q_UNUSED(chartPath);
    Q_UNUSED(trackPath);
    Q_UNUSED(sfxDir);
}

namespace miacode::preview_audio {

PreviewAudioBackendProvider bassPreviewAudioBackendProvider()
{
    PreviewAudioBackendProvider provider;
    provider.factory = []() -> std::unique_ptr<PreviewAudioBackend> {
        return std::make_unique<BassPreviewAudioBackend>();
    };
    provider.pauseActiveOutput = &PreviewBassEmergencyPause::pauseActiveOutput;
    return provider;
}

}  // namespace miacode::preview_audio
