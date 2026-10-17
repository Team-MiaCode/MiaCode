// Contract regression for application-service ownership and playback slots.
// ApplicationServices owns document, analysis, editor-sync, UI-request,
// job-progress and preview-appearance services independently of QWidget.
// This target links Qt6::Core / Qt6::Gui only.

#include "app/services/ApplicationServices.h"
#include "app/services/PlaybackControl.h"

#include <QCoreApplication>
#include <QPointer>
#include <QSignalSpy>
#include <QStringList>
#include <QTextStream>

#include <algorithm>

namespace {

bool require(bool condition, const QString& message, QTextStream& err)
{
    if (!condition) {
        err << "FAIL: " << message << Qt::endl;
    }
    return condition;
}

// The whole point of the assembly: it stands up with no window, no
// QApplication and no widget parent anywhere in the chain.
bool verifyConstructsWithoutAnyWidgetHost(QTextStream& err)
{
    miacode::ApplicationServices services;
    bool ok = require(services.parent() == nullptr,
                      QStringLiteral("the assembly needs no parent object"), err);
    ok &= require(services.workspace().parent() == &services
                      && services.analysis().parent() == &services
                      && services.editorSync().parent() == &services
                      && services.uiRequests().parent() == &services
                      && services.jobProgress().parent() == &services
                      && services.previewAppearance().parent() == &services,
                  QStringLiteral("every QObject service is parented to the assembly"), err);
    return ok;
}

// One owner means one instance: a second UiRequestService would mean a second
// dialog host, and a second ChartWorkspace would mean a second document truth.
bool verifySingleInstancePerService(QTextStream& err)
{
    miacode::ApplicationServices services;
    bool ok = require(&services.workspace() == &services.workspace()
                          && &services.files() == &services.files()
                          && &services.analysis() == &services.analysis()
                          && &services.editorSync() == &services.editorSync()
                          && &services.uiRequests() == &services.uiRequests()
                          && &services.jobProgress() == &services.jobProgress()
                          && &services.previewAppearance() == &services.previewAppearance(),
                      QStringLiteral("each accessor returns the same instance every call"), err);

    const miacode::ApplicationServices& constView = services;
    ok &= require(&constView.workspace() == &services.workspace()
                      && &constView.uiRequests() == &services.uiRequests(),
                  QStringLiteral("the const accessors name the same instances"), err);
    return ok;
}

// The file service and the analysis service must be wired onto the assembly's
// own workspace, not onto one they built themselves.
bool verifyServicesShareOneWorkspace(QTextStream& err)
{
    miacode::ApplicationServices services;
    const miacode::ChartWorkspaceResult opened = services.workspace().openSource(
        QStringLiteral("&title=spec\n&wholebpm=120\n&inote_3=(120){1}1,\nE\n"));
    bool ok = require(opened.accepted,
                      QStringLiteral("the assembly's workspace accepts a document"), err);
    if (!ok) {
        return false;
    }

    const miacode::ChartWorkspaceSnapshot workspaceState = services.workspace().snapshot();
    const miacode::AnalysisSnapshot analysed =
        miacode::AnalysisService::analyze(services.workspace(),
                                              services.validationLocale());
    ok &= require(analysed.difficultyId == workspaceState.activeDifficultyId
                      && analysed.revision == workspaceState.revision,
                  QStringLiteral("analysis reads the assembly's workspace, "
                                 "stamped with its revision"),
                  err);

    // The file service must hold the same workspace, not one of its own: a
    // second workspace is how "save" and "what is on screen" drift apart.
    ok &= require(&services.files().workspace() == &services.workspace(),
                  QStringLiteral("the file service is wired onto the assembly's workspace"), err);
    return ok;
}

// Destroying the assembly destroys everything it owns, in one step, with no
// dangling service left behind for a late callback to reach.
bool verifyDestructionReleasesEverything(QTextStream& err)
{
    QPointer<QObject> workspace;
    QPointer<QObject> uiRequests;
    QPointer<QObject> jobProgress;
    {
        miacode::ApplicationServices services;
        workspace = &services.workspace();
        uiRequests = &services.uiRequests();
        jobProgress = &services.jobProgress();
    }
    return require(workspace.isNull() && uiRequests.isNull() && jobProgress.isNull(),
                   QStringLiteral("every owned service dies with the assembly"), err);
}

// The services stay usable through the assembly — the accessors hand out live
// objects, not copies or freshly-defaulted ones.
bool verifyServicesAreLiveThroughTheAssembly(QTextStream& err)
{
    miacode::ApplicationServices services;

    QSignalSpy fileRequested(&services.uiRequests(),
                             &miacode::UiRequestService::fileRequested);
    miacode::FileRequest request;
    request.title = QStringLiteral("spec");
    const QString id = services.uiRequests().requestFile(request, [](const QString&) {});
    bool ok = require(!id.isEmpty() && fileRequested.count() == 1
                          && services.uiRequests().pendingFileRequestCount() == 1,
                      QStringLiteral("the assembly's UI request service is the live one"), err);
    services.uiRequests().cancelFileRequest(id);

    const quint64 token = services.jobProgress().begin(
        QStringLiteral("spec job"), QStringLiteral("running"), true);
    ok &= require(token != 0 && services.jobProgress().token() == token
                      && services.jobProgress().active(),
                  QStringLiteral("the assembly's job progress service is the live one"), err);
    services.jobProgress().end();

    return ok;
}

class FakePlaybackControl final : public miacode::PlaybackControl
{
public:
    miacode::PlaybackSnapshot playbackSnapshot() const override { return snapshot; }
    bool acceptsPlaybackCallback(const miacode::PlaybackCallbackStamp& stamp) const override
    {
        return stamp == snapshot.stamp();
    }
    void togglePlayback() override {}
    void stopOrPlay() override {}
    void setPlaybackRangeEnabled(bool, double, double) override {}
    void stop() override {}
    void seek(double) override {}
    void beginScrub() override {}
    void updateScrub(double) override {}
    void endScrub(double) override {}
    void setPlaybackRate(double) override {}
    void nudgePlaybackRate(int) override {}

    miacode::PlaybackSnapshot snapshot;
};

bool verifyPlaybackControlSlotIsLive(QTextStream& err)
{
    miacode::ApplicationServices services;
    FakePlaybackControl control;
    miacode::PlaybackControl*& slot = services.playbackControlSlot();
    bool ok = require(services.playbackControl() == nullptr,
                      QStringLiteral("the playback slot starts empty"), err);
    services.setPlaybackControl(&control);
    ok &= require(slot == &control && services.playbackControl() == &control,
                  QStringLiteral("the playback control is published through one live slot"), err);
    services.setPlaybackControl(nullptr);
    ok &= require(slot == nullptr && services.playbackControl() == nullptr,
                  QStringLiteral("withdrawing the playback control is visible to slot holders"), err);
    return ok;
}

bool verifyValidationLocaleHasANonWidgetOwner(QTextStream& err)
{
    miacode::ApplicationServices services;
    const SimaiValidationLocale locale = services.validationLocale();
    return require(locale == miacode::uiValidationLocale(),
                   QStringLiteral("the assembly publishes the shared UI validation locale"), err);
}

}  // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTextStream err(stderr);
    bool ok = true;

    ok &= verifyConstructsWithoutAnyWidgetHost(err);
    ok &= verifySingleInstancePerService(err);
    ok &= verifyServicesShareOneWorkspace(err);
    ok &= verifyDestructionReleasesEverything(err);
    ok &= verifyServicesAreLiveThroughTheAssembly(err);
    ok &= verifyPlaybackControlSlotIsLive(err);
    ok &= verifyValidationLocaleHasANonWidgetOwner(err);

    if (ok) {
        QTextStream(stdout) << "application_services_spec: OK" << Qt::endl;
    }
    return ok ? 0 : 1;
}
