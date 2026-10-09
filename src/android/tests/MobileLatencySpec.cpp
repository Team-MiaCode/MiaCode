#include "android/MobileLatency.h"
#include "android/MobileTimeline.h"
#include "app/ui/latency/LatencyModel.h"
#include "app/ui/preferences/PreferenceDocument.h"
#include "common/IntroConfig.h"
#include "common/PreviewSfxAssets.h"
#include "common/ContentDurationConfig.h"
#include "timeline/TimelineQuickModel.h"

#include <QTemporaryDir>
#include <QDataStream>
#include <QFile>
#include <QTimer>
#include <QScopeGuard>
#include <QtTest>
#include <cmath>
#include <limits>

using namespace miacode;
using namespace miacode::android;

class DetectionEngine : public LatencyEngine {
public:
    QString audio;
    int bpmWrites = 0;
    double documentWholeBpm() const override { return 117; }
    double documentOffsetSeconds() const override { return 0; }
    int documentClockCount() const override { return 4; }
    QString trackPath() const override { return audio; }
    void applyDetectorBpm(double) override { ++bpmWrites; }
    void applyDetectorOffset(double) override {}
    void applyDetectorClockCount(int) override {}
    latency::LatencyAudition* sandbox() const override { return nullptr; }
    void exitSandboxIfActive() override {}
};

class MobileLatencySpec : public QObject {
    Q_OBJECT
private slots:
    void contentDurationFollowsActualV2TimelineTail() {
        QTemporaryDir storage;
        QVERIFY(storage.isValid());
        PreferenceDocument::setPreferencesFilePath(storage.path() + "/preferences.json");
        AndroidDocumentSession document(storage.path());
        document.setMetadataFirst(QStringLiteral("0.25"));
        document.setChartText(QStringLiteral("(120){4}1,2,,,E"));
        MobilePreview preview(&document);
        TimelineQuickModel v2Timeline;
        const auto expectedDuration = [&] {
            v2Timeline.rebuildFromText(document.chartText(), preview.previewChartOffset(),
                simai::buildTimingMetadata(document.workspace().document()));
            return content_duration::totalContentDurationSeconds(v2Timeline.snapshot().durationSeconds, 0);
        };
        QCOMPARE(preview.durationSeconds(), expectedDuration());
        document.setChartText(QStringLiteral("(120){4}1h[1:2],E"));
        QCOMPARE(preview.durationSeconds(), expectedDuration());
        document.setChartText(QStringLiteral("(120){4}1,E"));
        QCOMPARE(preview.durationSeconds(), expectedDuration());
        document.setMetadataFirst(QStringLiteral("-0.3"));
        QCOMPARE(preview.durationSeconds(), expectedDuration());
        document.setMetadataFirst(QStringLiteral("0"));
        document.setChartText(QStringLiteral("(120){4}E"));
        QCOMPARE(preview.durationSeconds(), expectedDuration());
    }
    void replacingTrackRetiresItsDuration() {
        QTemporaryDir storage;
        QVERIFY(storage.isValid());
        PreferenceDocument::setPreferencesFilePath(storage.path() + "/preferences.json");
        const auto fixture = [&](const QString& name, int seconds) {
            const QString directory = storage.path() + "/" + name;
            if (!QDir().mkpath(directory)) return QString();
            QFile chart(directory + "/maidata.txt");
            if (!chart.open(QIODevice::WriteOnly)) return QString();
            chart.write("&first=0\n&lv_5=13\n&inote_5=(120){4}1,E\n");
            chart.close();
            QFile track(directory + "/track.wav");
            if (!track.open(QIODevice::WriteOnly)) return QString();
            constexpr int sampleRate = 24000;
            const int bytes = seconds * sampleRate * 2;
            QDataStream out(&track);
            out.setByteOrder(QDataStream::LittleEndian);
            out.writeRawData("RIFF", 4); out << quint32(36 + bytes);
            out.writeRawData("WAVEfmt ", 8); out << quint32(16) << quint16(1) << quint16(1)
                << quint32(sampleRate) << quint32(sampleRate * 2) << quint16(2) << quint16(16);
            out.writeRawData("data", 4); out << quint32(bytes);
            track.write(QByteArray(bytes, '\0'));
            return directory + "/maidata.txt";
        };
        const QString longTrack = fixture("long", 11), shortTrack = fixture("short", 2);
        QVERIFY(!longTrack.isEmpty() && !shortTrack.isEmpty());
        AndroidDocumentSession document(storage.path() + "/session");
        QVERIFY(document.loadHostFixture(longTrack));
        MobilePreview preview(&document);
        QTRY_COMPARE_WITH_TIMEOUT(preview.trackDurationSeconds(), 11.0, 10000);
        QCOMPARE(preview.durationSeconds(), 11.0);
        const QString retiredPath = document.previewAssetPath("audio");
        preview.setDecodedTrackDuration(retiredPath, 11.125);
        QCOMPARE(preview.trackDurationSeconds(), 11.125);
        QCOMPARE(preview.durationSeconds(), 11.125);
        preview.setDecodedTrackDuration(retiredPath, std::numeric_limits<double>::quiet_NaN());
        QCOMPARE(preview.trackDurationSeconds(), 11.125);
        QVERIFY(document.loadHostFixture(shortTrack));
        QTRY_COMPARE_WITH_TIMEOUT(preview.trackDurationSeconds(), 2.0, 10000);
        preview.setDecodedTrackDuration(retiredPath, 11.125);
        QCOMPARE(preview.trackDurationSeconds(), 2.0);
        EditorSyncController editor;
        AnalysisService analysis(document.workspace());
        MobileTimeline timeline(document, preview, editor, analysis);
        auto* bridge = qobject_cast<TimelineQuickStateBridge*>(timeline.stateBridge());
        QVERIFY(bridge);
        QTRY_VERIFY_WITH_TIMEOUT(bridge->waveformData() != nullptr, 10000);
        QCOMPARE(preview.trackDurationSeconds(), bridge->waveformData()->durationSeconds);
        TimelineQuickModel v2Timeline;
        v2Timeline.rebuildFromText(document.chartText(), 0);
        const double expected = content_duration::totalContentDurationSeconds(v2Timeline.snapshot().durationSeconds, 2);
        QCOMPARE(preview.durationSeconds(), expected);
        QVERIFY(preview.durationSeconds() < 11);
    }
    void replacingIntroDecodeRejectsRetiredSource() {
        QTemporaryDir storage;
        QVERIFY(storage.isValid());
        const QString previousDirectory = preview_sfx::musicDirectoryOverrideStorage();
        const QString previousSelected = preview_sfx::selectedIntroSoundFileName();
        const auto restore = qScopeGuard([&] {
            preview_sfx::setMusicDirectoryOverride(previousDirectory);
            preview_sfx::setSelectedIntroSoundFileName(previousSelected);
        });
        preview_sfx::setMusicDirectoryOverride(storage.path());
        preview_sfx::setSelectedIntroSoundFileName(QString());
        QFile bad(storage.path() + "/retired.wav");
        QVERIFY(bad.open(QIODevice::WriteOnly)); bad.write("retired invalid source"); bad.close();
        const QString currentPath = storage.path() + "/current.wav";
        QFile wave(currentPath);
        QVERIFY(wave.open(QIODevice::WriteOnly));
        QDataStream out(&wave); out.setByteOrder(QDataStream::LittleEndian);
        constexpr int frames = 4800;
        out.writeRawData("RIFF", 4); out << quint32(36 + frames * 4);
        out.writeRawData("WAVEfmt ", 8); out << quint32(16) << quint16(1) << quint16(2)
            << quint32(48000) << quint32(48000 * 4) << quint16(4) << quint16(16);
        out.writeRawData("data", 4); out << quint32(frames * 4);
        for (int i = 0; i < frames; ++i) out << qint16(5000) << qint16(-5000);
        wave.close();
        PreferenceDocument::setPreferencesFilePath(storage.path() + "/preferences.json");
        AndroidDocumentSession document(storage.path() + "/document");
        MobilePreview preview(&document);
        QSignalSpy failures(&preview, &MobilePreview::mediaError);
        preview.configureExportAudition(true, true, 0, 120);
        preview.setIntroSoundFile("retired.wav");
        // An invalid input may fail synchronously while it is still selected.
        // Only errors delivered after the replacement belong to retired work.
        failures.clear();
        preview.setIntroSoundFile("current.wav");
        QTRY_VERIFY_WITH_TIMEOUT(preview.sfxDiagnostics().value("introReady").toBool(), 10000);
        QTRY_VERIFY_WITH_TIMEOUT(preview.sfxDiagnostics().value("ready").toBool(), 10000);
        QCOMPARE(QFileInfo(preview.sfxDiagnostics().value("introSource").toString()).canonicalFilePath(),
            QFileInfo(currentPath).canonicalFilePath());
        QCOMPARE(failures.count(), 0);
    }
    void exportIntroRangeSeekAndTeardownUseActualTransport() {
        QTemporaryDir storage;
        QVERIFY(storage.isValid());
        PreferenceDocument::setPreferencesFilePath(storage.path() + "/preferences.json");
        AndroidDocumentSession document(storage.path());
        document.setChartText(QStringLiteral("(120){4}") + QStringLiteral("1,").repeated(160) + "E");
        document.setMetadataFirst(QStringLiteral("0"));
        MobilePreview preview(&document);
        preview.configureExportAudition(true, true, 4, 120);
        QCOMPARE(preview.lowerBoundSeconds(), -intro::kDurationSeconds);
        preview.setPositionSeconds(-intro::kDurationSeconds);
        QCOMPARE(preview.positionSeconds(), -intro::kDurationSeconds);
        QCOMPARE(preview.sceneRuntime().frameState().playheadSeconds, 0.0);
        preview.setPlaying(true);
        QVERIFY(preview.playing());
        preview.setPositionSeconds(-2);
        QVERIFY(!preview.playing());
        QCOMPARE(preview.positionSeconds(), -2.0);
        preview.configureExportAudition(false, false, 0, 0);
        QCOMPARE(preview.lowerBoundSeconds(), 0.0);
        QCOMPARE(preview.positionSeconds(), 0.0);
        QVERIFY(!preview.playing());
        document.setMetadataFirst(QStringLiteral("-0.3"));
        preview.setPositionSeconds(-0.3);
        preview.configureExportAudition(true, false, 4, 120);
        QCOMPARE(preview.lowerBoundSeconds(), 0.0);
        QCOMPARE(preview.positionSeconds(), 0.0);
        preview.configureExportAudition(false, false, 0, 0);
        QCOMPARE(preview.lowerBoundSeconds(), -0.3);
    }
    void stoppingReturnsToPlaybackEntryAndScrubStaysPaused() {
        QTemporaryDir storage;
        QVERIFY(storage.isValid());
        PreferenceDocument::setPreferencesFilePath(storage.path() + "/preferences.json");
        AndroidDocumentSession document(storage.path());
        document.setChartText(QStringLiteral("(120){4}") + QStringLiteral("1,").repeated(160) + "E");
        document.setMetadataFirst(QStringLiteral("0"));
        MobilePreview preview(&document);
        EditorSyncController editor;
        AnalysisService analysis(document.workspace());
        MobileTimeline timeline(document, preview, editor, analysis);
        auto* bridge = qobject_cast<TimelineQuickStateBridge*>(timeline.stateBridge());
        QVERIFY(bridge);
        bridge->setQuickViewportSize(QSize(800, 220));
        preview.setPositionSeconds(12);
        bridge->setPlayheadSeconds(12, true);
        const double entryScroll = bridge->horizontalScrollValue();
        preview.setPlaying(true);
        QVERIFY(preview.playing());
        QCOMPARE(bridge->playbackEntrySeconds(), 12.0);
        preview.setPositionSeconds(45);
        bridge->setHorizontalScrollValue(0);
        preview.setPlaying(false);
        QCOMPARE(preview.positionSeconds(), 45.0);
        QVERIFY(bridge->horizontalScrollValue() > entryScroll);
        bridge->setFollowProgressEnabled(false);
        preview.stop();
        QCOMPARE(preview.positionSeconds(), 12.0);
        QCOMPARE(bridge->playheadSeconds(), 12.0);
        QCOMPARE(bridge->playbackEntrySeconds(), 12.0);
        QCOMPARE(bridge->horizontalScrollValue(), entryScroll);
        preview.setPlaying(true);
        preview.beginScrub();
        preview.updateScrub(34);
        preview.endScrub();
        QVERIFY(!preview.playing());
        QCOMPARE(preview.positionSeconds(), 34.0);
        QCOMPARE(bridge->playbackEntrySeconds(), 12.0);
        preview.stop();
        QCOMPARE(preview.positionSeconds(), 12.0);
        QCOMPARE(bridge->horizontalScrollValue(), entryScroll);
    }
    void leavingDuringDecodeRejectsOldDetection() {
        QTemporaryDir storage;
        QVERIFY(storage.isValid());
        const QString path = storage.path() + "/pulses.wav";
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        constexpr int sampleRate = 24000, count = sampleRate * 16;
        QDataStream out(&file);
        out.setByteOrder(QDataStream::LittleEndian);
        out.writeRawData("RIFF", 4); out << quint32(36 + count * 2);
        out.writeRawData("WAVEfmt ", 8); out << quint32(16) << quint16(1) << quint16(1)
            << quint32(sampleRate) << quint32(sampleRate * 2) << quint16(2) << quint16(16);
        out.writeRawData("data", 4); out << quint32(count * 2);
        for (int i = 0; i < count; ++i) {
            const double t = double(i % (sampleRate / 2)) / sampleRate;
            out << qint16(20000 * std::exp(-t / 0.006) * std::sin(2 * 3.141592653589793 * 800 * t));
        }
        file.close();
        const auto decoded = latency_analysis::decodeMonoTrack(path);
        QVERIFY(!decoded.samples.isEmpty());
        QVERIFY(latency_analysis::detectBpm(latency_analysis::buildOnsetEnvelope(
            decoded.samples, decoded.sampleRate)).bpm > 0);
        DetectionEngine engine;
        engine.audio = path;
        LatencyEngine* slot = &engine;
        ui::LatencyModel model(slot);
        model.enter();
        bool leftDuringDecode = false;
        QTimer::singleShot(0, &model, [&] { leftDuringDecode = true; model.leave(); });
        model.detectBpm();
        QVERIFY(leftDuringDecode);
        QCOMPARE(engine.bpmWrites, 0);
        QCOMPARE(model.bpm(), 117.0);
        QVERIFY(model.bpmDetectResult().isEmpty());
    }
    void sharedModelAndSandboxRestoreDocument() {
        QTemporaryDir storage;
        QVERIFY(storage.isValid());
        PreferenceDocument::setPreferencesFilePath(storage.path() + "/preferences.json");
        AndroidDocumentSession document(storage.path());
        document.setChartText(QStringLiteral("(117){4}1,2,3,4,E"));
        document.setMetadataFirst(QStringLiteral("-0.250"));
        MobilePreview preview(&document);
        EditorSyncController editor;
        AnalysisService analysis(document.workspace());
        MobileTimeline timeline(document, preview, editor, analysis);
        auto* bridge = qobject_cast<TimelineQuickStateBridge*>(timeline.stateBridge());
        QVERIFY(bridge);
        MobileLatency latency(document, preview);
        LatencyEngine* slot = &latency;
        ui::LatencyModel model(slot);
        const QString chart = document.chartText();
        const auto before = document.workspace().snapshot();
        const auto initialMarkers = preview.sceneRuntime().frameState().noteMarkers.size();
        const auto countNotes = [&] {
            qsizetype count = 0;
            for (const auto& line : bridge->renderSnapshot().lines) count += line.notes.size();
            return count;
        };
        const auto initialNotes = countNotes();
        QVERIFY(initialNotes > 0);
        model.enter();
        QCOMPARE(model.bpm(), 117.0);
        QCOMPARE(model.offsetSeconds(), -0.25);
        QCOMPARE(model.clockCount(), 4);
        QVERIFY(latency.isOnPage());
        QVERIFY(preview.latencyChartActive());
        QVERIFY(countNotes() > initialNotes);
        const auto quarterNotes = countNotes();
        model.setSubdivision(8);
        QVERIFY(countNotes() > quarterNotes);
        QCOMPARE(document.chartText(), chart);
        QCOMPARE(document.workspace().snapshot().sourceText, before.sourceText);
        QCOMPARE(document.documentRevision(), before.revision);

        model.setBpm(150);
        model.setOffsetSeconds(0.125);
        model.setClockCount(6);
        QCOMPARE(latency.documentWholeBpm(), 150.0);
        QCOMPARE(document.metadataFirst(), QStringLiteral("0.125"));
        QCOMPARE(document.metadataClockCount(), QStringLiteral("6"));
        QCOMPARE(document.chartText(), chart);
        QCOMPARE(preview.previewChartOffset(), 0.125);
        model.setBpm(std::numeric_limits<double>::infinity());
        model.setOffsetSeconds(std::numeric_limits<double>::quiet_NaN());
        QCOMPARE(model.bpm(), 150.0);
        QCOMPARE(model.offsetSeconds(), 0.125);
        model.setSfxVolumePercent(23);
        QCOMPARE(latency.sfxVolumePercent(), 23);
        model.leave();
        QVERIFY(!latency.isOnPage());
        QVERIFY(!preview.latencyChartActive());
        QVERIFY(!preview.playing());
        QCOMPARE(preview.sceneRuntime().frameState().noteMarkers.size(), initialMarkers);
        QCOMPARE(countNotes(), initialNotes);
        QCOMPARE(preview.previewChartText(), chart);
        QCOMPARE(preview.previewChartOffset(), 0.125);

        model.enter();
        document.newProject();
        QVERIFY(!latency.isOnPage());
        QVERIFY(!preview.latencyChartActive());
        QVERIFY(!preview.playing());
    }
};

QTEST_MAIN(MobileLatencySpec)
#include "MobileLatencySpec.moc"
