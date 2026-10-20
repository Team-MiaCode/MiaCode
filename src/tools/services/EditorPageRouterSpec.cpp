#include "app/services/ApplicationServices.h"
#include "app/services/EditorPageRouter.h"

#include <QCoreApplication>
#include <QString>
#include <QStringList>
#include <QTextStream>

#include <functional>

namespace {

bool require(bool condition, const QString& message, QTextStream& err)
{
    if (!condition) {
        err << "FAIL: " << message << Qt::endl;
    }
    return condition;
}

// A stand-in router. Its only job is to prove the contract can be implemented
// with no window; the production one is MainWindow.
class FakeRouter final : public miacode::EditorPageRouter
{
public:
    bool hasActiveDifficulty() const override { return activeDifficulty > 0; }
    int activeDifficultyId() const override { return activeDifficulty; }
    bool difficultyPageActive() const override { return page == QStringLiteral("difficulty"); }

    bool enterDifficultyPage(int difficultyId) override
    {
        if (refuseSwitches) {
            return false;
        }
        activeDifficulty = difficultyId;
        page = QStringLiteral("difficulty");
        return true;
    }

    bool enterMetadataPage() override { return enterOverlay(QStringLiteral("metadata")); }
    bool enterLatencyPage() override { return enterOverlay(QStringLiteral("latency")); }
    bool enterExportPage() override { return enterOverlay(QStringLiteral("export")); }

    void packChartAsZip() override { ++packCount; }
    void openPreferences() override { ++preferencesCount; }

    void requestShellClose(std::function<void(bool)> onDecided) override
    {
        ++closeRequestCount;
        if (onDecided) {
            onDecided(!refuseSwitches);
        }
    }

    bool refuseSwitches = false;
    int activeDifficulty = 0;
    int packCount = 0;
    int preferencesCount = 0;
    int closeRequestCount = 0;
    QString page;

private:
    bool enterOverlay(const QString& id)
    {
        if (refuseSwitches) {
            return false;
        }
        activeDifficulty = 0;
        page = id;
        return true;
    }
};

bool verifyImplementableWithoutAWindow(QTextStream& err)
{
    FakeRouter router;
    miacode::EditorPageRouter& contract = router;

    bool ok = require(!contract.hasActiveDifficulty() && contract.activeDifficultyId() == 0,
                      QStringLiteral("a fresh router reports no active difficulty"), err);
    ok &= require(contract.enterDifficultyPage(3) && contract.hasActiveDifficulty()
                      && contract.activeDifficultyId() == 3,
                  QStringLiteral("entering a difficulty page makes it the active one"), err);
    ok &= require(contract.enterExportPage() && !contract.hasActiveDifficulty(),
                  QStringLiteral("an overlay page clears the active difficulty, which is what "
                                 "the page host records a resume target for"), err);
    ok &= require(contract.enterLatencyPage() && contract.enterMetadataPage(),
                  QStringLiteral("every page is reachable through the contract"), err);

    contract.packChartAsZip();
    ok &= require(router.packCount == 1,
                  QStringLiteral("packing a zip reaches the implementation"), err);

    contract.openPreferences();
    ok &= require(router.preferencesCount == 1,
                  QStringLiteral("偏好设置 reaches the implementation"), err);

    // Declining and dismissing are the same answer, so the continuation always
    // runs with an explicit verdict rather than being dropped.
    bool closeAnswer = false;
    int closeAnswers = 0;
    contract.requestShellClose([&](bool confirmed) {
        closeAnswer = confirmed;
        ++closeAnswers;
    });
    ok &= require(router.closeRequestCount == 1 && closeAnswers == 1 && closeAnswer,
                  QStringLiteral("the close question answers exactly once"), err);
    return ok;
}

// The return value is not decorative: a caller that shows a page after a
// refused switch leaves the shell and the document disagreeing about where the
// user is. The unsaved-changes guard is the switch that gets declined.
bool verifyRefusedSwitchesAreReported(QTextStream& err)
{
    FakeRouter router;
    router.refuseSwitches = true;
    miacode::EditorPageRouter& contract = router;

    return require(!contract.enterDifficultyPage(3) && !contract.enterMetadataPage()
                       && !contract.enterLatencyPage() && !contract.enterExportPage()
                       && router.page.isEmpty(),
                   QStringLiteral("every refused switch returns false and changes nothing"),
                   err);
}

// Same slot discipline as the export engine: the window installs itself and
// withdraws before teardown, and holders bind to the slot so the withdrawal is
// visible at once rather than leaving each with a dangling copy.
bool verifyTheSlotIsTheSingleSourceOfTruth(QTextStream& err)
{
    miacode::ApplicationServices services;
    bool ok = require(services.editorPageRouter() == nullptr,
                      QStringLiteral("no router is installed until a window installs one"), err);

    miacode::EditorPageRouter*& slot = services.editorPageRouterSlot();
    FakeRouter router;
    services.setEditorPageRouter(&router);
    ok &= require(slot == &router && services.editorPageRouter() == &router,
                  QStringLiteral("installing is visible through a slot bound earlier"), err);

    services.setEditorPageRouter(nullptr);
    ok &= require(slot == nullptr && services.editorPageRouter() == nullptr,
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
    ok &= verifyRefusedSwitchesAreReported(err);
    ok &= verifyTheSlotIsTheSingleSourceOfTruth(err);

    if (ok) {
        QTextStream(stdout) << "editor_page_router_spec: OK" << Qt::endl;
    }
    return ok ? 0 : 1;
}
