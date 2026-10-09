#pragma once

#include "SettingsUiSmoke.h"
#include "app/ui/layout/WorkbenchSettings.h"
#include "app/ui/preferences/PreferenceDocument.h"
#include <QJsonArray>
#include <QMouseEvent>

namespace miacode::android {
inline bool prepareLayoutUiSmoke(const QString& mode)
{
    if (mode == "restart") return QFile::exists(PreferenceDocument::preferencesFilePath());
    if (mode != "seed" || QFile::exists(PreferenceDocument::preferencesFilePath())) return false;
    return PreferenceDocument::savePreferencesObject(QJsonObject{
        {"ui", QJsonObject{{"sidebar_visible", true}, {"sidebar_width", 190},
            {"bottom_panel_visible", true}, {"bottom_panel_height_ratio", 0.35},
            {"preview_width_ratio", 0.5}}},
        {"future_section", QJsonObject{{"layout_sentinel", "preserved"}}}
    });
}

inline void startLayoutUiSmoke(QGuiApplication& app, QQmlApplicationEngine& engine,
    ui::WorkbenchSettings& settings, const QString& storage, const QString& mode)
{
    struct Proof {
        QElapsedTimer elapsed;
        int phase = 0;
        bool resized = false;
        int safeAreaPhase = 0;
        QVariantList originalPadding;
        QSize originalSize;
        int dragStep = -1;
        QPointF dragStart, dragDelta;
        QByteArray beforeDrag, beforeResize;
        double scaleBeforeDrag = 0;
        QStringList checks;
    };
    const auto proof = std::make_shared<Proof>();
    proof->elapsed.start();
    auto* timer = new QTimer(&app);
    timer->setInterval(100);
    QObject::connect(timer, &QTimer::timeout, timer, [&, timer, proof, storage, mode] {
        auto preferencesBytes = [] {
            QFile file(PreferenceDocument::preferencesFilePath());
            return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
        };
        auto finish = [&](bool ok, const QString& reason) {
            timer->stop();
            QSaveFile file(storage + "/layout-" + mode + "-proof.json");
            const auto bytes = QJsonDocument(QJsonObject{{"ok", ok}, {"reason", reason},
                {"checks", QJsonArray::fromStringList(proof->checks)},
                {"sidebarWidth", settings.sidebarWidth()}, {"sidebarVisible", settings.sidebarVisible()},
                {"bottomVisible", settings.bottomPanelVisible()},
                {"bottomRatio", settings.bottomPanelHeightRatio()}, {"previewRatio", settings.previewWidthRatio()},
                {"P5Accepted", false}, {"pairedV2FidelityVerified", false}}).toJson(QJsonDocument::Indented);
            ok = file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit() && ok;
            qInfo() << "Layout UI smoke:" << mode << (ok ? "passed" : "failed") << reason;
            app.exit(ok ? 0 : 61);
        };
        if (proof->elapsed.elapsed() > 25000) { finish(false, "deadline"); return; }
        if (engine.rootObjects().isEmpty()) return;
        auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
        if (!window || !window->isExposed() || proof->elapsed.elapsed() < 1200) return;
        if (!proof->resized) {
            const int index = app.arguments().indexOf("--size");
            const auto size = app.arguments().value(index + 1).split('x');
            if (index < 0 || size.size() != 2 || size[0].toInt() <= 0 || size[1].toInt() <= 0) {
                finish(false, "invalid geometry"); return;
            }
            proof->originalSize = QSize(size[0].toInt(), size[1].toInt());
            window->resize(proof->originalSize);
            proof->resized = true;
            return;
        }
        const auto item = [&](const char* name) { return settingsControlInVisualTree(window->contentItem(), name); };
        if (proof->safeAreaPhase == 0) {
            for (const char* name : {"leftPadding", "topPadding", "rightPadding", "bottomPadding"})
                proof->originalPadding.append(window->property(name));
            window->setProperty("leftPadding", 17);
            window->setProperty("topPadding", 24);
            window->setProperty("rightPadding", 11);
            window->setProperty("bottomPadding", 60);
            proof->safeAreaPhase = 1;
            return;
        }
        if (proof->safeAreaPhase == 1) {
            auto* scene = item("mobileSceneContent");
            auto* workbench = item("mobileWorkbenchRoot");
            auto* status = item("mobileStatusBar");
            auto* overlay = window->findChild<QQuickItem*>("mobileWorkbenchOverlay");
            if (!scene || !workbench || !status || !overlay) { finish(false, "safe area consumers missing"); return; }
            const QRectF bounds = scene->mapRectToScene(QRectF(0, 0, scene->width(), scene->height()));
            const QRectF fitted = workbench->mapRectToScene(QRectF(0, 0, workbench->width(), workbench->height()));
            const QRectF statusBounds = status->mapRectToScene(QRectF(0, 0, status->width(), status->height()));
            const QRectF overlayBounds = overlay->mapRectToScene(QRectF(0, 0, overlay->width(), overlay->height()));
            const QRectF tolerance = bounds.adjusted(-1, -1, 1, 1);
            if (qAbs(bounds.width() - (window->width() - 28)) > 1
                || qAbs(bounds.height() - (window->height() - 84)) > 1
                || !tolerance.contains(fitted) || !tolerance.contains(statusBounds)
                || !tolerance.contains(overlayBounds)
                || qAbs(fitted.width() - bounds.width()) > 1 || qAbs(fitted.height() - bounds.height()) > 1
                || qAbs(overlayBounds.width() - bounds.width()) > 1 || qAbs(overlayBounds.height() - bounds.height()) > 1
                || !window->grabWindow().save(storage + "/layout-safe-area-" + mode + ".png")) {
                finish(false, "workbench, status or popup overlay exceeds safe content area"); return;
            }
            int index = 0;
            for (const char* name : {"leftPadding", "topPadding", "rightPadding", "bottomPadding"})
                window->setProperty(name, proof->originalPadding.at(index++));
            proof->safeAreaPhase = 2;
            proof->checks << "workbench, status and popup overlay fit the same padded content area";
            return;
        }
        auto* host = item("mobileWorkspaceHost");
        auto* side = item("mobileSidebarContent");
        auto* split = item("mobileWorkspaceSplit");
        auto* center = item("mobileCenterSplit");
        auto* editor = item("mobileEditorHost");
        auto* bottom = item("mobileBottomPanel");
        auto* preview = item("v2PreviewPane");
        auto* toolbar = item("mobileMainToolbar");
        auto* sidebarHandle = item("mobileSidebarDivider");
        auto* previewHandle = item("mobilePreviewDivider");
        auto* bottomHandle = item("mobileBottomDivider");
        if (!host || !side || !split || !center || !editor || !bottom || !preview || !toolbar
            || !sidebarHandle || !previewHandle || !bottomHandle) { finish(false, "actual layout item missing"); return; }
        const double scale = window->property("workbenchScale").toDouble();
        const double available = window->property("previewEditorAvailableWidth").toDouble();
        auto capture = [&](const QString& name) { return window->grabWindow().save(storage + "/layout-" + name + ".png"); };
        auto geometryMatches = [&] {
            const double previewMinimum = preview->property("minimumWidth").toDouble();
            const double previewMaximum = qMax(previewMinimum, qMin(available * settings.previewMaximumWidthRatio(),
                available - bottom->property("minimumWidth").toDouble()));
            const double expectedPreview = qBound(previewMinimum,
                available * settings.previewWidthRatio(), previewMaximum);
            const double minBottom = qMax(bottom->property("minimumHeight").toDouble(),
                center->height() * settings.bottomPanelMinimumHeightRatio());
            const double maxBottom = qMax(bottom->property("minimumHeight").toDouble(), qMin(
                center->height() * settings.bottomPanelMaximumHeightRatio(), center->height() - 180 - 1));
            const double expectedBottom = qBound(minBottom, center->height() * settings.bottomPanelHeightRatio(), maxBottom);
            const QPointF previewTop = preview->mapToScene(QPointF());
            const QPointF previewEnd = preview->mapToScene(QPointF(preview->width(), preview->height()));
            return scale > 0 && scale <= 1 && qAbs(side->width() - (settings.sidebarVisible() ? settings.sidebarWidth() : 0)) <= 1
                && qAbs(preview->width() - expectedPreview) <= 2
                && (!bottom->isVisible() || qAbs(bottom->height() - expectedBottom) <= 2)
                && center->width() >= bottom->property("minimumWidth").toDouble() - 1
                && editor->height() >= 180 - 1
                && qAbs(previewEnd.x() - window->width()) <= 2
                && previewTop.x() >= 0 && previewTop.y() >= 0 && previewEnd.y() <= window->height() + 1;
        };
        auto beginDrag = [&](QQuickItem* handle, QPointF delta) {
            proof->dragStart = handle->mapToScene(QPointF(handle->width() / 2, handle->height() / 2));
            proof->dragDelta = delta;
            proof->beforeDrag = preferencesBytes();
            proof->scaleBeforeDrag = scale;
            proof->dragStep = 0;
        };
        if (proof->dragStep >= 0) {
            const int step = proof->dragStep++;
            const QPointF point = proof->dragStart + proof->dragDelta * qMin(step, 4) / 4.0;
            const auto type = step == 0 ? QEvent::MouseButtonPress : step == 5 ? QEvent::MouseButtonRelease : QEvent::MouseMove;
            QMouseEvent event(type, point, window->mapToGlobal(point.toPoint()),
                step == 0 || step == 5 ? Qt::LeftButton : Qt::NoButton,
                step == 5 ? Qt::NoButton : Qt::LeftButton, Qt::NoModifier);
            QCoreApplication::sendEvent(window, &event);
            if (step < 5 && (preferencesBytes() != proof->beforeDrag
                || qAbs(window->property("workbenchScale").toDouble() - proof->scaleBeforeDrag) > 0.000001)) {
                finish(false, "drag changed persistence or pointer transform before release"); return;
            }
            if (step == 5) { proof->dragStep = -1; ++proof->phase; }
            return;
        }
        if (mode == "restart") {
            if (proof->phase == 0) {
                if (settings.sidebarVisible() || settings.bottomPanelVisible() || side->isVisible() || bottom->isVisible()
                    || settings.sidebarWidth() != 230 || !geometryMatches()) {
                    finish(false, "cold restart did not restore collapsed layout"); return;
                }
                if (!capture("restart-hidden")) { finish(false, "restart hidden capture"); return; }
                QMetaObject::invokeMethod(toolbar, "toggleSidebarRequested");
                QMetaObject::invokeMethod(toolbar, "toggleBottomRequested");
                proof->phase = 1;
                return;
            }
            if (!settings.sidebarVisible() || !settings.bottomPanelVisible() || !bottom->isVisible() || !geometryMatches()) {
                finish(false, "reopened panes did not restore saved proportions"); return;
            }
            proof->checks << "cold process restores collapsed panes and reopens their saved geometry";
            if (!capture("restart")) { finish(false, "restart capture"); return; }
            finish(true, "");
            return;
        }
        switch (proof->phase) {
        case 0:
            if (!geometryMatches() || !capture("before")) { finish(false, "initial layout geometry"); return; }
            beginDrag(sidebarHandle, QPointF(40 * scale, 0));
            break;
        case 1:
            if (settings.sidebarWidth() != 230 || !geometryMatches()) { finish(false, "sidebar pointer drag not applied"); return; }
            proof->checks << "sidebar pointer drag preserves transform and saves only on release";
            beginDrag(previewHandle, QPointF((preview->width() - available * 0.4) * scale, 0));
            break;
        case 2:
            if (settings.previewWidthRatio() >= 0.49 || !geometryMatches()) { finish(false, "preview pointer drag not applied"); return; }
            proof->checks << "preview pointer drag persists workspace ratio";
            beginDrag(bottomHandle, QPointF(0, (bottom->height() - center->height() * 0.45) * scale));
            break;
        case 3:
            if (settings.bottomPanelHeightRatio() <= 0.36 || !geometryMatches() || !capture("moved")) {
                finish(false, "bottom pointer drag not applied"); return;
            }
            proof->checks << "bottom pointer drag persists height ratio";
            proof->beforeResize = preferencesBytes();
            window->resize(proof->originalSize.width() / 2, proof->originalSize.height() / 2);
            ++proof->phase;
            break;
        case 4:
            if (preferencesBytes() != proof->beforeResize || !geometryMatches()) { finish(false, "small window changed preferences or clipped layout"); return; }
            window->resize(proof->originalSize);
            ++proof->phase;
            break;
        case 5:
            if (preferencesBytes() != proof->beforeResize || !geometryMatches()) { finish(false, "enlarging window did not restore proportions"); return; }
            proof->checks << "window shrink and enlargement keep saved proportions and visible pane boundaries";
            beginDrag(sidebarHandle, QPointF(-side->width() * scale, 0));
            break;
        case 6:
            if (settings.sidebarVisible() || side->isVisible() || settings.sidebarWidth() != 230) {
                finish(false, "sidebar collapse did not preserve last expanded width"); return;
            }
            QMetaObject::invokeMethod(toolbar, "toggleBottomRequested");
            ++proof->phase;
            break;
        default:
            if (settings.bottomPanelVisible() || bottom->isVisible() || !geometryMatches()
                || PreferenceDocument::loadPreferencesObject().value("future_section").toObject().value("layout_sentinel").toString() != "preserved") {
                finish(false, "collapsed layout or foreign settings preservation"); return;
            }
            proof->checks << "sidebar collapse and toolbar bottom toggle persist visibility without losing expanded sizes";
            if (!capture("seed-hidden")) { finish(false, "hidden capture"); return; }
            finish(true, "");
        }
    });
    timer->start();
}
} // namespace miacode::android
