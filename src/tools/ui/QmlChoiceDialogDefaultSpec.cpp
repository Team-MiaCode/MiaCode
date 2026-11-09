#include "app/services/UiRequestService.h"

#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTextStream>

#include <memory>

namespace {

QQuickItem* findVisualItem(QQuickItem* parent, const QString& name)
{
    if (parent->objectName() == name) {
        return parent;
    }
    for (auto* child : parent->childItems()) {
        if (auto* match = findVisualItem(child, name)) {
            return match;
        }
    }
    return nullptr;
}

}  // namespace

int main(int argc, char** argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    QGuiApplication app(argc, argv);
    QTextStream err(stderr);

    QTemporaryDir temporary;
    if (!temporary.isValid()) {
        return 1;
    }

    miacode::UiRequestService requests;
    QSignalSpy fileRequests(&requests, &miacode::UiRequestService::fileRequested);
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(MIACODE_QML_SPEC_IMPORT_ROOT));
    engine.rootContext()->setContextProperty(QStringLiteral("choiceRequests"), &requests);
    QQmlComponent component(&engine);
    component.setData(R"QML(
import QtQuick
import QtQuick.Controls
import MiaCode.UI

ApplicationWindow {
    width: 640
    height: 480
    visible: true

    UiRequestHost {
        requests: choiceRequests

        function openRequest(requestId, request) {
            requests.cancelFileRequest(requestId)
        }
    }
}
)QML", QUrl(QStringLiteral("qrc:/ChoiceDialogSpec.qml")));
    const std::unique_ptr<QObject> root(component.create());
    auto* window = qobject_cast<QQuickWindow*>(root.get());
    if (!window) {
        err << component.errorString();
        return 1;
    }
    window->requestActivate();

    const QString continueId = QStringLiteral("continue");
    const QString cancelId = QStringLiteral("reject");
    QString answer;
    const auto requestConfirmation = [&](bool focusCancel) {
        answer.clear();
        QVariantMap cancelChoice{{"id", cancelId}, {"label", QStringLiteral("Cancel")}};
        if (focusCancel) {
            cancelChoice.insert(QStringLiteral("default"), true);
        }
        const QVariantList choices{
            QVariantMap{{"id", continueId}, {"label", QStringLiteral("Continue")}},
            cancelChoice,
        };
        requests.requestChoice(miacode::localizedText("export.export_as_zip"),
                               miacode::localizedText("export.empty_difficulty_warning"),
                               choices, cancelId, [&](const QString& selected) {
            answer = selected;
            if (selected == continueId) {
                miacode::FileRequest file;
                file.startPath = temporary.filePath(QStringLiteral("fixture.zip"));
                file.saveMode = true;
                requests.requestFile(file, [](const QString&) {});
            }
        });
    };

    QObject* dialog = root->findChild<QObject*>(QStringLiteral("uiRequestChoiceDialog"));
    if (!dialog) {
        err << "UiRequestHost must create the real choice dialog\n";
        return 1;
    }
    const auto isOpen = [&] { return dialog->property("opened").toBool(); };
    const auto cancelFocused = [&] {
        const auto* active = window->activeFocusItem();
        return active && active->objectName() == QStringLiteral("dialogFooterCancel");
    };
    const auto waitForClosed = [&] {
        return QTest::qWaitFor([&] { return !dialog->property("visible").toBool(); });
    };
    const auto canceledSafely = [&] {
        return QTest::qWaitFor([&] { return answer == cancelId; })
            && fileRequests.isEmpty() && waitForClosed();
    };

    requestConfirmation(false);
    if (!QTest::qWaitFor(isOpen) || cancelFocused()) {
        err << "Existing dialogs without an explicit default must retain their focus behavior\n";
        return 1;
    }
    QTest::keyClick(window, Qt::Key_Escape);
    if (!canceledSafely()) {
        err << "Escape must retain the existing dismissal behavior\n";
        return 1;
    }

    requestConfirmation(true);
    if (!QTest::qWaitFor(cancelFocused)) {
        err << "An explicit default cancel action must take keyboard focus\n";
        return 1;
    }
    QTest::keyClick(window, Qt::Key_Return);
    if (!canceledSafely()) {
        err << "Return on the default cancel action must never request a file\n";
        return 1;
    }

    requestConfirmation(true);
    if (!QTest::qWaitFor(cancelFocused)) {
        err << "The reopened choice dialog must focus Cancel again\n";
        return 1;
    }
    QTest::keyClick(window, Qt::Key_Escape);
    if (!canceledSafely()) {
        err << "Escape must never request a file\n";
        return 1;
    }

    requestConfirmation(true);
    if (!QTest::qWaitFor(cancelFocused)) {
        return 1;
    }
    const auto* continueButton = findVisualItem(window->contentItem(), QStringLiteral("choiceDialogButton_continue"));
    const auto* cancelButton = window->activeFocusItem();
    if (!continueButton || !cancelButton) {
        err << "Both ZIP confirmation actions must be visible in the choice dialog\n";
        return 1;
    }
    const auto continueCenter = continueButton->mapToScene(continueButton->boundingRect().center());
    const auto cancelCenter = cancelButton->mapToScene(cancelButton->boundingRect().center());
    if (continueCenter.x() >= cancelCenter.x()) {
        err << "Continue must be to the left of Cancel\n";
        return 1;
    }
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, continueCenter.toPoint());
    if (!QTest::qWaitFor([&] { return answer == continueId && fileRequests.count() == 1; })
        || !fileRequests.first().at(1).toMap().value(QStringLiteral("saveMode")).toBool()) {
        err << "Only Continue must open the ZIP save picker\n";
        return 1;
    }
    QTextStream(stdout) << "qml_choice_dialog_default_spec ok\n";
    return 0;
}
