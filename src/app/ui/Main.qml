import QtQuick
import QtQuick.Controls
import MiaCode.UI

AppWindow {
    id: window

    platform: applicationContext.platform
    windowChrome: applicationContext.windowChrome
    readonly property var inputContext: mainView.inputContext
    readonly property bool workbenchCommandsAllowed: inputContext && inputContext.workbenchCommandsAllowed
        && !window.applicationContext.shortcuts.captureActive
    readonly property bool windowCommandsEnabled: !mainView.beginnerGuideActive
        && inputContext && inputContext.editingCommandsAllowed
        && !window.applicationContext.shortcuts.captureActive
    property bool sourceEditorFocused: false
    property bool sourceEditorOverlayHeld: false
    readonly property bool playbackCommandsEnabled: mainView.chartEditorActive
        || (mainView.pages.exportSession
            && mainView.pages.exportSession.pageSessionActive
            && mainView.pages.exportSession.selectedDifficultyId > 0)
    readonly property real tooltipTopMargin: 6 + (
        Qt.platform.os === "osx" && window.visibility !== Window.FullScreen && window.windowChrome
        ? window.windowChrome.titleBarHeight / Theme.uiScale : 0)

    required property var applicationContext
    readonly property var shellLifecycle: applicationContext.shell
    readonly property var chartDropBridge: applicationContext.chartDropBridge

    property NativeMenuBar nativeSystemMenu: NativeMenuBar {
        commands: mainView.mainMenuCommands
        shortcuts: window.applicationContext.shortcuts
        documentSession: mainView.documentSession
        pet: window.applicationContext.pet
        commandsEnabled: window.platform.nativeMenuBar
            && mainView.mainMenuCommands.commandsEnabled
        saveEnabled: mainView.editorActive
        wholeDocumentSaveEnabled: mainView.documentSession.hasDocument
        documentAvailable: mainView.documentSession.hasDocument
        editorCommandsEnabled: mainView.editMenuEnabled
        chartCommandsEnabled: mainView.chartEditorActive && window.workbenchCommandsAllowed
        playbackCommandsEnabled: window.playbackCommandsEnabled && window.workbenchCommandsAllowed
        toolCommandsEnabled: mainView.documentSession.hasDocument
    }

    menuBar: window.platform && window.platform.nativeMenuBar ? window.nativeSystemMenu : null

    width: 1280
    height: 800
    minimumWidth: Math.min(Screen.desktopAvailableWidth, Math.ceil(mainView.minimumWidth * Theme.uiScale))
    minimumHeight: Math.min(Screen.desktopAvailableHeight,
        Math.ceil(Math.max(480, mainView.minimumHeight) * Theme.uiScale))
    // C++ explicitly shows this only after MainWindow has registered the root
    // and installed the shared chart-audio drop route.
    visible: false
    title: mainView.windowTitle.replace(/ — /g, " - ")

    Binding {
        target: Theme
        property: "preferences"
        value: window.applicationContext.preferences
    }

    PetOverlay {
        controller: window.applicationContext.pet
        hostWindow: window
    }

    Binding {
        target: Theme
        property: "appBackground"
        value: window.applicationContext.appBackground
    }

    Binding {
        target: Theme
        property: "windowChrome"
        value: window.applicationContext.windowChrome
    }

    // Two-phase close. The unsaved-changes prompt is a QML dialog, so the first
    // close attempt cannot be answered here — it is refused, the question is
    // asked, and the window closes itself once the answer comes back. Telling
    // bootstrap on the way out is what lets preparePreviewForShutdown run
    // before teardown.
    property bool closeApproved: false

    onClosing: function(close) {
        if (window.closeApproved) {
            window.shellLifecycle.notifyRootCloseAccepted("qml_ui_root_closing")
            return
        }
        close.accepted = false
        window.shellLifecycle.requestClose()
    }

    Connections {
        target: window.shellLifecycle
        function onCloseDecided(accepted) {
            if (!accepted)
                return
            window.closeApproved = true
            // A clean document can answer synchronously while onClosing is
            // still on the stack. Close on the next event-loop turn so Qt
            // receives a fresh request instead of dropping a reentrant one.
            Qt.callLater(function() { window.close() })
        }
    }

    // Popups live in ApplicationWindow's overlay, outside the sampled scene.
    MainView {
        id: mainView
        anchors.fill: parent
        backgroundSource: window.backgroundLayer
        hostWindow: window
        applicationContext: window.applicationContext
    }

    Item {
        id: chartDropHint
        anchors.fill: parent
        visible: window.chartDropBridge !== null && window.chartDropBridge.dragActive
        enabled: false

        Rectangle {
            anchors.fill: parent
            color: Theme.colors.state.selected
            opacity: 0.08
            border.color: Theme.colors.text.active
            border.width: 2
        }

        Rectangle {
            anchors.centerIn: parent
            width: Math.min(parent.width - 48, 420)
            height: 72
            radius: Theme.controlRadius
            color: Theme.overlayColor(Theme.colors.background.elevated, Theme.popupOpacity)
            border.color: Theme.colors.text.active
            border.width: 1

            Text {
                anchors.centerIn: parent
                color: Theme.colors.text.primary
                text: (window.chartDropBridge && window.chartDropBridge.chartDocumentDrag)
                      ? qsTrId("drop_chart.preview.open_title")
                      : qsTrId("drop_chart.preview.title")
                font.family: Theme.uiFont
                font.pixelSize: 16
            }
        }
    }

    // Mounted on the root window rather than inside MainView: these bindings
    // must survive whichever page or overlay is on screen.
    ShortcutBindings {
        shortcuts: window.applicationContext.shortcuts
        commands: window.applicationContext.commands
        previewSession: window.applicationContext.preview
        timelineStateBridge: window.applicationContext.timeline.stateBridge
        inputContext: window.inputContext
        commandsEnabled: window.windowCommandsEnabled && window.workbenchCommandsAllowed
        chartCommandsEnabled: mainView.chartEditorActive && window.workbenchCommandsAllowed
        chartNormalizationEnabled: mainView.chartNormalizationAvailable
        playbackCommandsEnabled: window.playbackCommandsEnabled
        menuOwnsChartTransformShortcuts: window.platform.nativeMenuBar
        menuOwnsPreviewRateShortcuts: window.platform.nativeMenuBar
        onChartTransformRequested: opId => mainView.applyChartTransform(opId)
        onChartNormalizationRequested: mainView.normalizeChartWithPreferences()
    }

    UiScaleShortcuts {
        shortcuts: window.applicationContext.shortcuts
        preferencesModel: window.applicationContext.preferencesModel
        commandsEnabled: window.windowCommandsEnabled
    }
}
