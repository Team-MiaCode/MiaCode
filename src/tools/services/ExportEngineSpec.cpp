#include "app/services/ApplicationServices.h"
#include "app/services/ExportEngine.h"

#include <QCoreApplication>
#include <QStringList>
#include <QTextStream>

namespace {

bool require(bool condition, const QString& message, QTextStream& err)
{
    if (!condition) {
        err << "FAIL: " << message << Qt::endl;
    }
    return condition;
}

// A stand-in implementation. Its only job is to prove the contract can be
// implemented without a window — the production one is
// miacode::runtime::VideoExportHost (src/app/runtime/export/).
class FakeExportEngine final : public miacode::ExportEngine
{
public:
    VideoExportTask buildSeedTask(int difficultyId) override
    {
        seededDifficultyId = difficultyId;
        VideoExportTask task;
        task.contentDurationSeconds = 12.5;
        return task;
    }

    void applySharedTaskSettings(const VideoExportTask& task) override
    {
        appliedDurationSeconds = task.contentDurationSeconds;
    }

    bool startAudition(int difficultyId, const VideoExportTask&) override
    {
        auditioningDifficultyId = difficultyId;
        return true;
    }

    void stopAudition() override { auditioningDifficultyId = 0; }
    void clearAudition() override { auditioningDifficultyId = 0; }

    bool launchVideoExport(const VideoExportTask&, int difficultyId,
                           QString* errorMessage) override
    {
        if (difficultyId == 0) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("no difficulty");
            }
            return false;
        }
        launchedDifficultyId = difficultyId;
        return true;
    }

    bool launchBatchExport(const VideoExportTask&, const QStringList& chartDirectories,
                           const QList<int>& selectedDifficultyIds, const QString&,
                           BatchResult* result, const BatchCallbacks& callbacks,
                           QString*) override
    {
        if (callbacks.progressChanged) {
            callbacks.progressChanged(50, QStringLiteral("half"));
        }
        if (callbacks.cancellationRequested && callbacks.cancellationRequested()) {
            if (result != nullptr) {
                result->canceled = true;
            }
            return true;
        }
        if (result != nullptr) {
            result->successCount = chartDirectories.size() * selectedDifficultyIds.size();
        }
        return true;
    }

    void cancelVideoExport() override { ++cancelCount; }

    QList<int> difficultyIds() const override { return difficulties; }

    QString difficultyChartText(int difficultyId) const override
    {
        return difficulties.contains(difficultyId) ? QStringLiteral("1,2,3,") : QString();
    }

    int lastOpenedDifficultyId() const override { return lastOpened; }
    MuriRenderOptions muriRenderOptions() const override { return {}; }
    double currentAudioClockSecond() const override { return audioClockSecond; }
    void refreshIntroState() override { ++introRefreshCount; }
    void setIntroAuditionPlaying(bool playing) override { introAuditioning = playing; }
    bool introAuditionPlaying() const override { return introAuditioning; }

    QList<int> difficulties{3, 4};
    int lastOpened = 4;
    bool introAuditioning = false;
    int introRefreshCount = 0;
    double audioClockSecond = 7.5;

    int seededDifficultyId = 0;
    int auditioningDifficultyId = 0;
    int launchedDifficultyId = 0;
    int cancelCount = 0;
    double appliedDurationSeconds = 0.0;
};

// The contract is implementable with no window, no QApplication, no widget.
bool verifyImplementableWithoutAWindow(QTextStream& err)
{
    FakeExportEngine engine;
    miacode::ExportEngine& contract = engine;

    const VideoExportTask seed = contract.buildSeedTask(3);
    bool ok = require(engine.seededDifficultyId == 3 && seed.contentDurationSeconds > 0.0,
                      QStringLiteral("seeding a task reaches the implementation"), err);

    contract.applySharedTaskSettings(seed);
    ok &= require(engine.appliedDurationSeconds == seed.contentDurationSeconds,
                  QStringLiteral("live settings reach the implementation"), err);

    ok &= require(contract.difficultyIds() == QList<int>({3, 4})
                      && !contract.difficultyChartText(3).isEmpty()
                      && contract.difficultyChartText(9).isEmpty()
                      && contract.lastOpenedDifficultyId() == 4,
                  QStringLiteral("the page's document queries reach the implementation"), err);
    ok &= require(contract.currentAudioClockSecond() > 0.0,
                  QStringLiteral("the live playhead reaches the implementation"), err);
    contract.refreshIntroState();
    ok &= require(engine.introRefreshCount == 1,
                  QStringLiteral("the intro-state nudge reaches the implementation"), err);

    ok &= require(contract.startAudition(4, seed) && engine.auditioningDifficultyId == 4,
                  QStringLiteral("audition starts on the requested difficulty"), err);
    contract.stopAudition();
    ok &= require(engine.auditioningDifficultyId == 0,
                  QStringLiteral("stopping the audition clears it"), err);

    contract.cancelVideoExport();
    ok &= require(engine.cancelCount == 1,
                  QStringLiteral("cancel reaches the implementation"), err);
    return ok;
}

// A launch that cannot start must say why, so the page can show it rather than
// failing silently.
bool verifyFailedLaunchReportsItsReason(QTextStream& err)
{
    FakeExportEngine engine;
    QString error;
    bool ok = require(!engine.launchVideoExport(VideoExportTask{}, 0, &error)
                          && !error.isEmpty(),
                      QStringLiteral("a refused launch returns false and fills the message"),
                      err);
    ok &= require(engine.launchVideoExport(VideoExportTask{}, 2, &error)
                      && engine.launchedDifficultyId == 2,
                  QStringLiteral("an accepted launch reports the difficulty it started"), err);
    return ok;
}

// Batch runs synchronously, so progress and cancellation travel through the
// caller's callbacks; a cancel is an outcome, not an error.
bool verifyBatchReportsThroughItsCallbacks(QTextStream& err)
{
    FakeExportEngine engine;

    int lastPercent = -1;
    miacode::ExportEngine::BatchCallbacks callbacks;
    callbacks.progressChanged = [&lastPercent](int percent, const QString&) {
        lastPercent = percent;
    };
    callbacks.cancellationRequested = [] { return false; };

    miacode::ExportEngine::BatchResult result;
    QString error;
    bool ok = require(engine.launchBatchExport(VideoExportTask{},
                                               QStringList{QStringLiteral("/a"),
                                                           QStringLiteral("/b")},
                                               QList<int>{3, 4}, QStringLiteral("/out"),
                                               &result, callbacks, &error)
                          && lastPercent == 50 && result.successCount == 4
                          && !result.canceled,
                      QStringLiteral("batch reports progress and its success count"), err);

    miacode::ExportEngine::BatchResult cancelled;
    callbacks.cancellationRequested = [] { return true; };
    ok &= require(engine.launchBatchExport(VideoExportTask{},
                                           QStringList{QStringLiteral("/a")},
                                           QList<int>{3}, QStringLiteral("/out"),
                                           &cancelled, callbacks, &error)
                      && cancelled.canceled,
                  QStringLiteral("a user cancel is an outcome, not a launch failure"), err);
    return ok;
}

// The slot, not a snapshot: whoever holds the engine sees the window's
// withdrawal immediately. This is what stops the export page — a QObject child
// that outlives the section it used to point at — from calling into a
// destroyed engine during teardown.
bool verifyTheSlotIsTheSingleSourceOfTruth(QTextStream& err)
{
    miacode::ApplicationServices services;
    bool ok = require(services.exportEngine() == nullptr,
                      QStringLiteral("no engine is installed until a window installs one"),
                      err);

    miacode::ExportEngine*& slot = services.exportEngineSlot();
    ok &= require(slot == nullptr,
                  QStringLiteral("the slot starts empty too"), err);

    FakeExportEngine engine;
    services.setExportEngine(&engine);
    ok &= require(slot == &engine && services.exportEngine() == &engine,
                  QStringLiteral("installing is visible through a slot bound earlier"), err);

    services.setExportEngine(nullptr);
    ok &= require(slot == nullptr && services.exportEngine() == nullptr,
                  QStringLiteral("withdrawing is visible through that same bound slot"), err);
    return ok;
}

}  // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTextStream err(stderr);
    bool ok = true;

    ok &= verifyImplementableWithoutAWindow(err);
    ok &= verifyFailedLaunchReportsItsReason(err);
    ok &= verifyBatchReportsThroughItsCallbacks(err);
    ok &= verifyTheSlotIsTheSingleSourceOfTruth(err);

    if (ok) {
        QTextStream(stdout) << "export_engine_spec: OK" << Qt::endl;
    }
    return ok ? 0 : 1;
}
