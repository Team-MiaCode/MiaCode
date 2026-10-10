#include "app/ui/export/CoverExportWindow.h"

#include "app/platform/PlatformDiagnostics.h"
#include "app/ui/layout/WorkbenchSettings.h"
#include "app/ui/preferences/LocaleService.h"
#include "common/DebugLog.h"
#include "export/cover_export/CoverCompositeRenderer.h"

#include <QCoreApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QScreen>


namespace miacode::ui {
CoverExportWindow::CoverExportWindow(miacode::ExportEngine& exportEngine,
                                           miacode::PlaybackControl*& playbackControlSlot,
                                           WorkbenchSettings& preferences,
                                           const QIcon& icon,
                                           QObject* parent)
    : QObject(parent)
    , preferences_(preferences)
    , icon_(icon)
    , platform_(this)
    , windowChrome_(this)
    , requests_(this)
    , session_(exportEngine, requests_, playbackControlSlot, this)
{
    connect(&miacode::LocaleService::instance(), &miacode::LocaleService::languageChanged,
            &requests_, &miacode::UiRequestService::retranslate);
    // Export pumps events while capturing. Finish the active operation before
    // deleting its session, including when the application is closing.
    connect(&session_, &CoverExportSession::busyChanged, this, [this] {
        if (session_.busy()) return;
        if (closePending_) {
            close();
        } else if (refreshPending_) {
            refreshPending_ = false;
            session_.refreshDocument(pendingDifficultyId_);
        }
    }, Qt::QueuedConnection);
    windowChrome_.setBlurMaterialsEnabled(preferences_.blurMaterialsEnabled());
    connect(&preferences_, &WorkbenchSettings::themeChanged,
            &windowChrome_, &WindowChrome::refreshNativeTheme);
    connect(&preferences_, &WorkbenchSettings::blurMaterialsEnabledChanged, this, [this] {
        windowChrome_.setBlurMaterialsEnabled(preferences_.blurMaterialsEnabled());
    });
}

CoverExportWindow::~CoverExportWindow()
{
    session_.leave();
    // Destroy live scene nodes and the image provider before their borrowed
    // frame state and layout. Session destruction then releases the capture
    // window, skin repository, chart data and all layer images.
    engine_.reset();
}

bool CoverExportWindow::show(QQuickWindow* referenceWindow, int difficultyId)
{
    engine_ = std::make_unique<QQmlApplicationEngine>();
    miacode::LocaleService::instance().setQmlEngine(engine_.get());
    engine_->addImportPath(QCoreApplication::applicationDirPath() + QStringLiteral("/qml"));
    miacode::cover_export::registerCoverChartImageProvider(engine_.get(), session_.coverLayout());
    connect(engine_.get(), &QQmlApplicationEngine::warnings, this,
            [](const QList<QQmlError>& warnings) {
        for (const auto& warning : warnings) {
            miacode::debug_log::appendLine(miacode::debug_log::Channel::Runtime,
                QStringLiteral("cover_window"), warning.toString());
        }
    });
    engine_->setInitialProperties({
        {QStringLiteral("controller"), QVariant::fromValue(static_cast<QObject*>(this))},
        {QStringLiteral("coverSession"), QVariant::fromValue(static_cast<QObject*>(&session_))},
        {QStringLiteral("preferences"), QVariant::fromValue(static_cast<QObject*>(&preferences_))},
        {QStringLiteral("platform"), QVariant::fromValue(static_cast<QObject*>(&platform_))},
        {QStringLiteral("windowChrome"), QVariant::fromValue(static_cast<QObject*>(&windowChrome_))},
    });
    engine_->loadFromModule(QStringLiteral("MiaCode.UI"), QStringLiteral("CoverExportWindow"));
    if (engine_->rootObjects().isEmpty()) {
        return false;
    }
    window_ = qobject_cast<QQuickWindow*>(engine_->rootObjects().constFirst());
    if (!window_) {
        return false;
    }
    window_->setIcon(icon_);
    if (referenceWindow != nullptr) {
        window_->setScreen(referenceWindow->screen());
    }
    const QRect available = window_->screen()->availableGeometry();
    window_->resize(window_->size().boundedTo(available.size()));
    window_->setPosition(available.center() - QPoint(window_->width() / 2, window_->height() / 2));
    miacode::app::entry::bindHighPerformanceQuickGraphicsDevice(
        window_, QStringLiteral("cover_window"), /*preferVideoShareDevice=*/false);
    windowChrome_.attach(window_, QString());
    session_.enter(difficultyId);
    window_->show();
    window_->requestActivate();
    return true;
}

void CoverExportWindow::raise()
{
    if (window_ && !closePending_) {
        if (window_->windowState() == Qt::WindowMinimized) {
            window_->showNormal();
        }
        window_->raise();
        window_->requestActivate();
    }
}

void CoverExportWindow::refreshDocument(int difficultyId)
{
    if (closePending_) return;
    pendingDifficultyId_ = difficultyId;
    refreshPending_ = session_.busy();
    if (!refreshPending_) session_.refreshDocument(difficultyId);
}

void CoverExportWindow::close()
{
    closePending_ = true;
    if (session_.busy()) {
        return;
    }
    if (window_) {
        window_->hide();
    }
    session_.leave();
    deleteLater();
}

} // namespace miacode::ui
