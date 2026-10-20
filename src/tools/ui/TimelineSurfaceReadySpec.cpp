#include "app/services/ShellNotifications.h"
#include "app/services/TimelineSurface.h"
#include "app/ui/timeline/TimelineModel.h"

#include <QCoreApplication>
#include <QString>
#include <QStringList>
#include <QTextStream>

namespace {

bool expect(bool condition, const QString& message, QTextStream& out, int* failed)
{
    out << (condition ? "[PASS] " : "[FAIL] ") << message << '\n';
    out.flush();
    if (!condition) ++*failed;
    return condition;
}

// A stand-in surface. Its only job is to prove the report is deliverable
// through the contract; the production one is PlaybackCoordinator.
class FakeTimelineSurface final : public miacode::TimelineSurface
{
public:
    QObject* timelineStateBridge() const override { return nullptr; }
    void noteTimelineSurfaceReady() override { ++readyReports; }

    void navigateToSecond(double) override {}
    void centerOnSecond(double) override {}
    void wheelNavigateToSecond(double) override {}
    void timelineDragStarted() override {}
    void timelineDragFinished(double) override {}
    void timelineUserInteractionStarted() override {}
    void setFollowPreviewEnabled(bool) override {}
    QString bottomTabsCurrentTabId() const override { return QString(); }
    void setBottomTabsCurrentTabId(const QString&) override {}
    bool bottomTabsVisible() const override { return true; }
    bool timelineTabVisible() const override { return true; }
    bool muriTabVisible() const override { return false; }
    bool validationTabVisible() const override { return false; }
    bool ignoreMuriIssuePrompts() const override { return false; }

    int readyReports = 0;
};

// The report has to survive the trip. A question would compile and run and
// change nothing, which is exactly how this broke.
bool verifyTheReportReachesTheSurface(QTextStream& out, int* failed)
{
    miacode::ShellNotifications notifications;
    FakeTimelineSurface surface;
    miacode::TimelineSurface* installed = &surface;
    miacode::ui::TimelineModel model(notifications, installed);

    bool ok = expect(surface.readyReports == 0,
                     QStringLiteral("a fresh model has reported nothing"), out, failed);

    model.surfaceReady();
    ok &= expect(surface.readyReports == 1,
                 QStringLiteral("the QML item's ready report reaches the surface"), out, failed);

    // The item can go through readiness again — a reparent, a resize back from
    // zero. De-duplicating is the host's job (noteTimelineSurfaceReady
    // returns early once the flag is set); the model must not swallow repeats,
    // or a genuine re-arm after the flag was cleared would be lost.
    model.surfaceReady();
    ok &= expect(surface.readyReports == 2,
                 QStringLiteral("a repeated report is forwarded, not swallowed"), out, failed);
    return ok;
}

// Withdrawing the surface must be visible at once: the model binds the slot,
// not a copy, so a teardown cannot leave it calling into a dead window.
bool verifyAWithdrawnSurfaceIsInert(QTextStream& out, int* failed)
{
    miacode::ShellNotifications notifications;
    FakeTimelineSurface surface;
    miacode::TimelineSurface* installed = &surface;
    miacode::ui::TimelineModel model(notifications, installed);

    installed = nullptr;
    model.surfaceReady();
    return expect(surface.readyReports == 0,
                  QStringLiteral("no surface installed means the report goes nowhere, not "
                                 "into a stale pointer"), out, failed);
}

}  // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);
    int failed = 0;

    verifyTheReportReachesTheSurface(out, &failed);
    verifyAWithdrawnSurfaceIsInert(out, &failed);

    if (failed == 0) {
        out << "timeline_surface_ready_spec: OK\n";
    } else {
        out << "timeline_surface_ready_spec: " << failed << " failure(s)\n";
    }
    out.flush();
    return failed == 0 ? 0 : 1;
}
