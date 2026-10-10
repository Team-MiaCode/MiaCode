pragma Singleton

import QtQuick

QtObject {
    property var preferences: null
    property var appBackground: null
    property var windowChrome: null
    readonly property bool blurMaterialsEnabled: preferences ? preferences.blurMaterialsEnabled : true
    readonly property bool darkTheme: preferences ? preferences.darkTheme : true
    readonly property bool backgroundActive: appBackground
        && appBackground.enabled && appBackground.imageReadable
    readonly property bool nativeMaterialActive: windowChrome
        && blurMaterialsEnabled && windowChrome.nativeMaterialAvailable && !backgroundActive

    readonly property var darkColors: ({
        background: {
            surface: "#121314",
            panel: "#191A1B",
            editorTabStrip: "#191A1B",
            elevated: "#202122",
            control: "#121314",
            controlDisabled: "#202122",
            titleBar: "#121314",
            activityBar: "#121314",
            statusBar: "#121314"
        },
        border: {
            normal: "#2A2B2C",
            control: "#333536",
            status: "#2A2B2C"
        },
        text: {
            // Navigation: selected ≈ 1.0, idle ≈ 0.75 of active.
            active: "#E8E8E8",
            // Settings-page section captions: active brightness; weight, not
            // size, sets them apart from body text.
            section: "#E8E8E8",
            primary: "#BFBFBF",
            secondary: "#AEAEAE",
            disabled: "#6E6E6E",
            // Ordinary notes: same full brightness as text.active, not a muted gray.
            editor: "#E8E8E8",
            lineNumber: "#858889",
            heading: "#FFFFFF",
            onAccent: "#FFFFFF",
            onDanger: "#FFFFFF",
            status: "#AEAEAE",
            chrome: "#AEAEAE"
        },
        previewHud: {
            text: "#FFFFFF",
            shadow: Qt.rgba(0, 0, 0, 190 / 255)
        },
        accent: {
            primary: "#297AA0",
            badge: "#307E9F",
            soft: "#A5D6FF",
            focus: Qt.rgba(0x39 / 255, 0x94 / 255, 0xBC / 255, 0xB3 / 255)
        },
        scroll: {
            handle: "#5E6062",
            handleHover: "#7A7C7E"
        },
        // Boolean controls stay neutral while off; the on state carries a
        // muted accent so a column of options never outweighs the solid
        // primary action. AppSwitch and AppCheckBox share these.
        toggle: {
            track: "#333536",
            knob: "#808284",
            border: "#5C5D5E",
            knobBorder: Qt.rgba(0, 0, 0, 0.35),
            checkedTrack: "#374B5A",
            checkedKnob: "#D6DCE2"
        },
        // AppSlider: a neutral fill, so the handle is what draws the eye.
        slider: {
            rail: "#333536",
            fill: "#6C6F72",
            handle: "#ECECEC"
        },
        state: {
            // Base UI state colors; HoverChrome applies the shared overlay alpha.
            hover: "#1E2021",
            pressed: "#282A2B",
            selected: "#333536",

            // Editor text selection (not row chrome).
            menuSelection: Qt.rgba(0x39 / 255, 0x94 / 255, 0xBC / 255, 0x26 / 255),
            textSelection: Qt.rgba(0x27 / 255, 0x67 / 255, 0x82 / 255, 0xDD / 255),
            followHighlight: "#52B0D8",
            lineHighlight: "#2A2B2C",
            focusLine: "#232425",
            selectionHighlight: "#3E4042"
        },
        listState: {
            hover: "#1E2021",
            pressed: "#282A2B",
            selected: "#333536"
        },
        activityState: {
            hover: "#1E2021",
            pressed: "#282A2B",
            selected: "#333536"
        },
        activityIcon: {
            active: "#E8E8E8",
            hover: "#E8E8E8",
            idle: "#AEAEAE"
        },
        popupState: {
            hover: "#2C2D2E",
            pressed: "#3A3B3C",
            selected: "#333536"
        },
        buttonState: {
            hover: "#333536",
            pressed: "#282A2B",
            selected: "#333536"
        },
        accentState: {
            hover: "#328EB8",
            pressed: "#236888",
            selected: "#307E9F"
        },
        danger: {
            primary: "#C4453C"
        },
        dangerState: {
            hover: "#D4544B",
            pressed: "#A83A33",
            selected: "#C4453C"
        },
        syntax: {
            keyword: "#F5AE9C",
            comment: "#71B77A",
            duration: "#A0B6FF",
            error: "#C62828",
            warning: "#B07B00"
        },
        // 时间轴(QSG)外壳颜色。时间轴是原生绘制，由 TimelineThemeBridge
        // 把这里的分组读入 C++ 快照。与外壳同值的条目
        // （window/base/border/label/textSecondary）须与上方角色保持一致。
        timeline: {
            window: "#191A1B",
            header: "#191A1B",
            sidebar: "#191A1B",
            base: "#121314",
            border: "#2A2B2C",
            axis: "#6E6E6E",
            gridMajor: "#6E6E6E",
            gridSubdivision: Qt.rgba(42 / 255, 43 / 255, 44 / 255, 140 / 255),
            gridMinor: Qt.rgba(42 / 255, 43 / 255, 44 / 255, 70 / 255),
            laneEven: Qt.rgba(1, 1, 1, 11 / 255),
            laneOdd: Qt.rgba(1, 1, 1, 5 / 255),
            label: "#AEAEAE",
            textSecondary: "#AEAEAE",
            waveStroke: Qt.rgba(57 / 255, 148 / 255, 188 / 255, 144 / 255)
        }
    })

    // Adapted from VS Code's bundled Quiet Light theme. Product-specific
    // surfaces keep MiaCode's existing semantic roles while using the source
    // theme's editor, sidebar, selection, accent and syntax colours.
    readonly property var lightColors: ({
        background: {
            surface: "#F4F6F8",
            panel: "#EBEFF3",
            editorTabStrip: "#E3E3E3",
            elevated: "#FFFFFF",
            control: "#FFFFFF",
            controlDisabled: "#E5EAF0",
            titleBar: "#D9E7F8",
            activityBar: "#D9E7F8",
            statusBar: "#526F98"
        },
        border: {
            normal: "#C8D2DE",
            control: "#AAB8C8",
            status: "#405B80"
        },
        text: {
            active: "#2F3B4A",
            // Settings-page section captions: active brightness.
            section: "#2F3B4A",
            primary: "#3D4856",
            secondary: "#5D6B7C",
            disabled: "#9AA6B4",
            editor: "#344050",
            lineNumber: "#6D7A89",
            heading: "#2F3B4A",
            onAccent: "#FFFFFF",
            onDanger: "#FFFFFF",
            status: "#FFFFFF",
            chrome: "#3D4856"
        },
        previewHud: {
            text: "#2F3B4A",
            shadow: Qt.rgba(1, 1, 1, 210 / 255)
        },
        accent: {
            primary: "#526F98",
            badge: "#526F98",
            soft: "#B9CBE3",
            focus: Qt.rgba(0x52 / 255, 0x6F / 255, 0x98 / 255, 0xB3 / 255)
        },
        scroll: {
            handle: "#A3AFBD",
            handleHover: "#7E8DA0"
        },
        toggle: {
            track: "#CDD5DF",
            knob: "#FFFFFF",
            border: "#A3AFBD",
            knobBorder: Qt.rgba(31 / 255, 45 / 255, 61 / 255, 0.22),
            checkedTrack: "#7088AA",
            checkedKnob: "#FFFFFF"
        },
        slider: {
            rail: "#CDD5DF",
            fill: "#7E8DA0",
            handle: "#FFFFFF"
        },
        state: {
            hover: "#E5E9E1",
            pressed: "#C8D1C2",
            selected: "#D3DBCD",
            menuSelection: "#D3DBCD",
            textSelection: "#C9D8EA",
            followHighlight: "#78C5CF",
            lineHighlight: "#DDE5D8",
            focusLine: "#D8E1D2",
            selectionHighlight: "#D3DEEB"
        },
        listState: {
            hover: "#E5E9E1",
            pressed: "#C8D1C2",
            selected: "#D3DBCD"
        },
        activityState: {
            hover: "#C4D8F3",
            pressed: "#B8CEE9",
            selected: "#F4F7FB"
        },
        activityIcon: {
            active: "#405B80",
            hover: "#526F98",
            idle: "#718197"
        },
        popupState: {
            hover: "#E5E9E1",
            pressed: "#C8D1C2",
            selected: "#D3DBCD"
        },
        buttonState: {
            hover: "#E5E9E1",
            pressed: "#C8D1C2",
            selected: "#D3DBCD"
        },
        accentState: {
            hover: "#627FA8",
            pressed: "#405B80",
            selected: "#526F98"
        },
        danger: {
            primary: "#C4473F"
        },
        dangerState: {
            hover: "#D4564D",
            pressed: "#A83C36",
            selected: "#C4473F"
        },
        syntax: {
            keyword: "#4B69C6",
            comment: "#448C27",
            duration: "#9C5D27",
            error: "#CD3131",
            warning: "#9C5D27"
        },
        timeline: {
            window: "#EBEFF3",
            header: "#EBEFF3",
            sidebar: "#EBEFF3",
            base: "#F4F6F8",
            border: "#C8D2DE",
            axis: "#7E8DA0",
            gridMajor: "#A3AFBD",
            gridSubdivision: Qt.rgba(0xA3 / 255, 0xAF / 255, 0xBD / 255, 140 / 255),
            gridMinor: Qt.rgba(0xA3 / 255, 0xAF / 255, 0xBD / 255, 70 / 255),
            laneEven: Qt.rgba(0, 0, 0, 11 / 255),
            laneOdd: Qt.rgba(0, 0, 0, 5 / 255),
            label: "#5D6B7C",
            textSecondary: "#5D6B7C",
            waveStroke: Qt.rgba(0x52 / 255, 0x6F / 255, 0x98 / 255, 166 / 255)
        }
    })

    // MiaCode 1.x light palette mapped onto the v2 semantic roles. Surface
    // opacity remains a runtime concern, so palette authors only provide
    // opaque surface tones and translucent content overlays.
    readonly property var legacyLightColors: ({
        background: {
            surface: "#F8FAFD",
            panel: "#F5F7FA",
            editorTabStrip: "#F5F7FA",
            elevated: "#FFFFFF",
            control: "#FFFFFF",
            controlDisabled: "#F2F5F9",
            titleBar: "#F7F9FC",
            activityBar: "#F7F9FC",
            statusBar: "#F7F9FC"
        },
        border: {
            normal: "#D5E0EC",
            control: "#B8C7DA",
            status: "#D5E0EC"
        },
        text: {
            active: "#203040",
            // Settings-page section captions: active brightness.
            section: "#203040",
            primary: "#203040",
            secondary: "#5F6B7A",
            disabled: "#728196",
            editor: "#203040",
            lineNumber: "#697685",
            heading: "#203040",
            onAccent: "#FFFFFF",
            onDanger: "#FFFFFF",
            status: "#5F6B7A",
            chrome: "#5F6B7A"
        },
        previewHud: {
            text: "#203040",
            shadow: Qt.rgba(1, 1, 1, 210 / 255)
        },
        accent: {
            primary: "#2E77D0",
            badge: "#2E77D0",
            soft: "#B8CCE5",
            focus: Qt.rgba(0x2E / 255, 0x77 / 255, 0xD0 / 255, 0xB3 / 255)
        },
        scroll: {
            handle: "#9CB5CE",
            handleHover: "#81A2C3"
        },
        toggle: {
            track: "#D5E0EC",
            knob: "#FFFFFF",
            border: "#A9B6C6",
            knobBorder: Qt.rgba(32 / 255, 48 / 255, 64 / 255, 0.22),
            checkedTrack: "#6097DA",
            checkedKnob: "#FFFFFF"
        },
        slider: {
            rail: "#D5E0EC",
            fill: "#8A9BB0",
            handle: "#FFFFFF"
        },
        state: {
            hover: "#DBEAFF",
            pressed: "#C4DAF3",
            selected: "#CEE2F8",
            menuSelection: "#CEE2F8",
            textSelection: "#B8CCE5",
            followHighlight: "#5A86D8",
            lineHighlight: "#EDF2F8",
            focusLine: "#DBEAFF",
            selectionHighlight: "#CEE2F8"
        },
        listState: {
            hover: "#DBEAFF",
            pressed: "#C4DAF3",
            selected: "#CEE2F8"
        },
        activityState: {
            hover: "#DBE9F9",
            pressed: "#C3DAF4",
            selected: "#E3E3E3"
        },
        activityIcon: {
            active: "#2B3C4E",
            hover: "#2E77D0",
            idle: "#5D6E83"
        },
        popupState: {
            hover: "#DBEAFF",
            pressed: "#C4DAF3",
            selected: "#CEE2F8"
        },
        buttonState: {
            hover: "#DBEAFF",
            pressed: "#C4DAF3",
            selected: "#CEE2F8"
        },
        accentState: {
            hover: "#3A86E8",
            pressed: "#2668B9",
            selected: "#2E77D0"
        },
        danger: {
            primary: "#C4473F"
        },
        dangerState: {
            hover: "#D4564D",
            pressed: "#A83C36",
            selected: "#C4473F"
        },
        syntax: {
            keyword: "#4B69C6",
            comment: "#448C27",
            duration: "#9C5D27",
            error: "#CD3131",
            warning: "#9C5D27"
        },
        timeline: {
            window: "#F5F5F5",
            header: "#F3F5F8",
            sidebar: "#E8E8E8",
            base: "#F7F8FA",
            border: "#C7D2DF",
            axis: "#9AA7B6",
            gridMajor: "#5A86D8",
            gridSubdivision: Qt.rgba(122 / 255, 154 / 255, 204 / 255, 165 / 255),
            gridMinor: Qt.rgba(174 / 255, 188 / 255, 204 / 255, 110 / 255),
            laneEven: Qt.rgba(1, 1, 1, 30 / 255),
            laneOdd: Qt.rgba(208 / 255, 212 / 255, 216 / 255, 30 / 255),
            label: "#4D5C6D",
            textSecondary: "#5F6B7A",
            waveStroke: Qt.rgba(44 / 255, 156 / 255, 130 / 255, 135 / 255)
        }
    })

    // MiaCode 1.x dark palette mapped onto the same semantic roles.
    readonly property var legacyDarkColors: ({
        background: {
            surface: "#151A20",
            panel: "#1B2129",
            editorTabStrip: "#1B2129",
            elevated: "#232B35",
            control: "#171D24",
            controlDisabled: "#202833",
            titleBar: "#171C23",
            activityBar: "#171C23",
            statusBar: "#171C23"
        },
        border: {
            normal: "#384656",
            control: "#546679",
            status: "#384656"
        },
        text: {
            active: "#E6EEF8",
            // Settings-page section captions: active brightness.
            section: "#E6EEF8",
            primary: "#E6EEF8",
            secondary: "#A9B6C6",
            disabled: "#7B8798",
            editor: "#E6EEF8",
            lineNumber: "#8091A5",
            heading: "#E6EEF8",
            onAccent: "#F7FBFF",
            onDanger: "#F7FBFF",
            status: "#A9B6C6",
            chrome: "#A9B6C6"
        },
        previewHud: {
            text: "#F7FBFF",
            shadow: Qt.rgba(0, 0, 0, 190 / 255)
        },
        accent: {
            primary: "#4F8FEC",
            badge: "#4F8FEC",
            soft: "#A8C8F8",
            focus: Qt.rgba(0x4F / 255, 0x8F / 255, 0xEC / 255, 0xB3 / 255)
        },
        scroll: {
            handle: "#5A6A7B",
            handleHover: "#70849A"
        },
        toggle: {
            track: "#384656",
            knob: "#8494A6",
            border: "#5F6F82",
            knobBorder: Qt.rgba(0, 0, 0, 0.35),
            checkedTrack: "#546D8C",
            checkedKnob: "#D8E0EA"
        },
        slider: {
            rail: "#384656",
            fill: "#7A8BA0",
            handle: "#E6EEF8"
        },
        state: {
            hover: "#303D4D",
            pressed: "#3D4D62",
            selected: "#364962",
            menuSelection: "#364962",
            textSelection: Qt.rgba(0x31 / 255, 0x5D / 255, 0x9E / 255, 0xDD / 255),
            followHighlight: "#67A1F1",
            lineHighlight: "#1F2630",
            focusLine: "#232B35",
            selectionHighlight: "#364962"
        },
        listState: {
            hover: "#303D4D",
            pressed: "#3D4D62",
            selected: "#364962"
        },
        activityState: {
            hover: "#303D4D",
            pressed: "#3D4D62",
            selected: "#3C4A5C"
        },
        activityIcon: {
            active: "#D8E2EE",
            hover: "#E6EEF8",
            idle: "#95A4B7"
        },
        popupState: {
            hover: "#303D4D",
            pressed: "#384656",
            selected: "#364962"
        },
        buttonState: {
            hover: "#303D4D",
            pressed: "#3D4D62",
            selected: "#364962"
        },
        accentState: {
            hover: "#67A1F1",
            pressed: "#3E79D0",
            selected: "#4F8FEC"
        },
        danger: {
            primary: "#D15A50"
        },
        dangerState: {
            hover: "#E26A60",
            pressed: "#B84C44",
            selected: "#D15A50"
        },
        syntax: {
            keyword: "#F29A83",
            comment: "#71B77A",
            duration: "#88A4FF",
            error: "#E35C50",
            warning: "#FF9B4A"
        },
        timeline: {
            window: "#1A2027",
            header: "#1D232B",
            sidebar: "#171D24",
            base: "#202833",
            border: "#4A5C70",
            axis: "#8091A5",
            gridMajor: "#7FA5D8",
            gridSubdivision: Qt.rgba(120 / 255, 152 / 255, 198 / 255, 140 / 255),
            gridMinor: Qt.rgba(95 / 255, 120 / 255, 155 / 255, 80 / 255),
            // Lane bands are content overlays. Keeping them neutral and light
            // preserves the selected surface (including wallpaper) underneath.
            laneEven: Qt.rgba(113 / 255, 130 / 255, 148 / 255, 30 / 255),
            laneOdd: Qt.rgba(79 / 255, 101 / 255, 124 / 255, 30 / 255),
            label: "#C8D5E5",
            textSecondary: "#A9B6C6",
            waveStroke: Qt.rgba(95 / 255, 214 / 255, 180 / 255, 160 / 255)
        }
    })

    // Every entry is an independent theme. Appearance mode only decides which
    // user-selected slot supplies activeThemeToken; it never constrains the
    // theme stored in either slot.
    readonly property var themeCatalog: ({
        "light": { id: "light", dark: false, colors: lightColors },
        "dark": { id: "dark", dark: true, colors: darkColors },
        "legacy": { id: "legacy", dark: true, colors: legacyDarkColors },
        "legacy_light": { id: "legacy_light", dark: false, colors: legacyLightColors }
    })
    readonly property string activeThemeToken: preferences ? preferences.activeThemeToken : "dark"
    readonly property var activeTheme: themeCatalog[activeThemeToken] || themeCatalog.dark
    // Compensate wallpaper highlights per semantic role, preserving palette hue.
    readonly property var colors: {
        const palette = activeTheme.colors
        if (!backgroundActive)
            return palette
        const result = Object.assign({}, palette)
        for (const role of ["state", "listState", "activityState", "popupState", "buttonState"])
            result[role] = compensatedStateColorsFor(palette[role])
        result.state.menuSelection = compensatedHighlightColor(palette.state.menuSelection, 0.16)
        result.state.focusLine = compensatedHighlightColor(palette.state.focusLine, 0.08)
        result.state.lineHighlight = compensatedHighlightColor(palette.state.lineHighlight, 0.08)
        result.state.selectionHighlight = compensatedHighlightColor(palette.state.selectionHighlight, 0.16)
        result.state.textSelection = compensatedHighlightColor(palette.state.textSelection, 0.22)
        return result
    }

    readonly property string uiFont: preferences ? preferences.uiFontFamily : ""
    readonly property font codeFont: preferences ? preferences.codeFont : Qt.font({})
    // 行距, in the pixels of bottom margin each text block carries.
    readonly property int codeBlockSpacing: preferences ? preferences.editorBlockSpacing : 0
    readonly property int uiFontSize: preferences ? preferences.fontSize : 13
    readonly property int headingFontSize: uiFontSize + 2
    // Section captions inside a settings page: body size, set apart from body
    // text by DemiBold and text.section; the page heading stays two steps up.
    readonly property int sectionTitleFontSize: uiFontSize
    readonly property int sectionTitleFontWeight: Font.DemiBold
    readonly property int secondaryFontSize: uiFontSize - 1
    readonly property int captionFontSize: uiFontSize - 3

    readonly property real surfaceOpacity: backgroundActive
        ? appBackground.panelAlpha / 255.0
        : 1.0

    readonly property real nativeMaterialTintOpacity: Qt.platform.os === "windows"
        ? (darkTheme ? 0.83 : 0.72)
        : (darkTheme ? 0.65 : 0.50)
    readonly property color separatorColor: Qt.tint(colors.background.panel, darkTheme
        ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(0, 0, 0, 0.16))
    readonly property color chromeHighlightBaseColor: {
        const c = Qt.color(activeTheme.colors.background.activityBar)
        return darkTheme
            ? Qt.rgba(Math.min(1, c.r + 14 / 255),
                      Math.min(1, c.g + 15 / 255),
                      Math.min(1, c.b + 16 / 255), c.a)
            : c
    }
    readonly property var chromeStateColors: chromeStateColorsFor(nativeMaterialActive)

    function compensatedHighlightColor(baseColor, amount) {
        const source = Qt.color(baseColor)
        const target = chromeHighlightColor(amount)
        const lightness = darkTheme
            ? Math.max(source.hslLightness, target.hslLightness)
            : Math.min(source.hslLightness, target.hslLightness)
        const opacity = backgroundActive ? Math.min(source.a, surfaceOpacity) : source.a
        return Qt.hsla(Math.max(0, source.hslHue), source.hslSaturation, lightness, opacity)
    }

    function compensatedStateColorsFor(stateColors) {
        return Object.assign({}, stateColors, {
            hover: compensatedHighlightColor(stateColors.hover, 0.12),
            pressed: compensatedHighlightColor(stateColors.pressed, 0.20),
            selected: compensatedHighlightColor(stateColors.selected, 0.16)
        })
    }

    function chromeStateColorsFor(materialActive) {
        return materialActive || backgroundActive
            ? compensatedStateColorsFor(activeTheme.colors.activityState)
            : colors.activityState
    }
    readonly property real popupOpacity: 0.96
    readonly property int popupEnterDuration: 120
    readonly property int popupExitDuration: 90
    readonly property color floatingBorderColor: darkTheme
        ? Qt.rgba(1, 1, 1, 0.06) : Qt.rgba(0, 0, 0, 0.08)
    // Frosted menu material is independent of wallpaper visibility.
    readonly property real popupTintOpacity: 0.85
    readonly property int popupBlurRadius: 96
    readonly property real dialogTintOpacity: 0.94
    readonly property int dialogBlurRadius: 128
    readonly property color modalScrimColor: Qt.rgba(0, 0, 0, 0.5)
    readonly property real popupBlurScale: 0.5
    readonly property real popupShadowOpacity: darkTheme ? 0.28 : 0.14
    readonly property real dialogShadowOpacity: darkTheme ? 0.34 : 0.18
    readonly property real followHighlightOpacity: darkTheme ? 0.5 : 0.65
    readonly property color popupTintColor: {
        const c = Qt.color(colors.background.elevated)
        return Qt.rgba(c.r, c.g, c.b, popupTintOpacity)
    }
    readonly property color dialogTintColor: {
        const c = Qt.color(colors.background.panel)
        return Qt.rgba(c.r, c.g, c.b, dialogTintOpacity)
    }

    function overlayColor(baseColor, opacity = surfaceOpacity) {
        if (!backgroundActive)
            return baseColor
        const c = Qt.color(baseColor)
        // Semantic highlights already carry their fill alpha. Cap it here so
        // controls and direct color bindings compose the same wallpaper layer.
        return Qt.rgba(c.r, c.g, c.b, Math.min(c.a, opacity))
    }

    // Content decoration may tint a surface, while surface ownership stays
    // with the surrounding panel. The cap makes that contract hold even when
    // a future palette accidentally supplies an opaque lane/stripe color.
    function contentOverlayColor(baseColor, maxOpacity = 0.12) {
        const c = Qt.color(baseColor)
        return Qt.rgba(c.r, c.g, c.b, Math.min(c.a, maxOpacity))
    }

    function surfaceColor(baseColor) {
        if (!backgroundActive)
            return baseColor
        const c = Qt.color(baseColor)
        if (!darkTheme || surfaceOpacity === 0)
            return Qt.rgba(c.r, c.g, c.b, c.a * surfaceOpacity)
        const panel = Qt.color(activeTheme.colors.background.panel)
        const shade = Math.min(1, (c.r + c.g + c.b) / (panel.r + panel.g + panel.b))
        // Darker surfaces also shade the wallpaper beneath them, retaining
        // their depth relative to the panel. Fade this shading with the fill
        // below 50%, keeping the fully transparent setting clear.
        const alpha = surfaceOpacity + (1 - surfaceOpacity) * (1 - shade)
            * Math.min(1, 2 * surfaceOpacity)
        const scale = surfaceOpacity / alpha
        return Qt.rgba(c.r * scale, c.g * scale, c.b * scale, c.a * alpha)
    }

    function nativeMaterialColor(baseColor) {
        const c = Qt.color(activeThemeToken === "dark" ? "#0C0D0E" : baseColor)
        return Qt.rgba(c.r, c.g, c.b, c.a * nativeMaterialTintOpacity)
    }

    function chromeSurfaceColor(baseColor, materialActive = nativeMaterialActive) {
        if (!materialActive)
            return surfaceColor(baseColor)
        return nativeMaterialColor(baseColor)
    }

    function chromeHighlightColor(amount) {
        const c = Qt.color(activeTheme.colors.text.active)
        return Qt.tint(chromeHighlightBaseColor, Qt.rgba(c.r, c.g, c.b, amount))
    }

    // Shared UI geometry.
    readonly property int controlRadius: 6
    readonly property int compactControlRadius: 5
    readonly property int smallControlRadius: 3
    readonly property int popupRadius: 10
    readonly property int editorTabRadius: 8
    readonly property int workspaceRadius: 10
    readonly property int itemRadius: controlRadius
    readonly property int controlMinHeight: 30
    readonly property int controlHighlightHeight: 26
    readonly property int compactControlHeight: 24
    readonly property int compactFontSize: uiFontSize - 2
    readonly property int settingsRowSpacing: 10
    readonly property int settingsLabelSpacing: 4
    readonly property int panelPadding: 8
    readonly property int compactTabContentPadding: 8
    readonly property int dialogPadding: 16
    readonly property int dialogMargin: 24
    readonly property int dialogHeight: 560
    readonly property int dialogCompactHeight: 280
    readonly property int controlBorderWidth: 1
    readonly property int menuPadding: 7
    readonly property int menuRowHeight: 28
    readonly property int menuParameterRowHeight: 64
    // Default inset so adjacent HoverChrome pills do not touch.
    readonly property int chromeInsetX: 3
    readonly property int chromeInsetY: 2
    readonly property int chromePadding: 4
    readonly property int chromeHighlightOutset: 1
    readonly property int chromeMinSize: 24
    // Content inset for a chromed row. HoverChrome insets itself from the
    // control but never the content, so ChromeRow spends this on padding to
    // keep text off the highlight edge.
    readonly property int rowPaddingX: 10
    readonly property int titleBarBrandIconSize: 17
    readonly property int windowChromeRowHeight: 30
    readonly property int workspaceHeaderHeight: 34
    // Visual baseline shared by the editor tabs and workspace side headings.
    readonly property int workspaceHeaderContentOffsetY: 2
    readonly property real workspaceHeaderContentCenterY:
        workspaceHeaderHeight / 2 + workspaceHeaderContentOffsetY
    readonly property real workspaceSectionTopMargin:
        2 * workspaceHeaderContentOffsetY + (activityButtonSize - controlMinHeight) / 2
    // SplitView handle: 1px layout (same as non-interactive dividers),
    // wider invisible hit, thicker stroke only while hovered/pressed.
    readonly property int splitDividerThickness: 1
    readonly property int splitHandleActiveThickness: 3
    readonly property int splitHandleHitExtent: 9
    readonly property int activityButtonSize: 48
    readonly property int activityIconSize: 22
    readonly property int activityIconTop: Math.round((activityButtonSize - activityIconSize) * 0.5)

    // Per-difficulty swatch, matching v1 difficultyColor() in MainWindowShared.
    readonly property var difficultyColors: [
        "#69A6FF", "#78C85A", "#DCC548", "#E35C50", "#7A4FD1", "#D548B6", "#E29A46"
    ]
    readonly property color difficultyColorFallback: "#8A8F98"
    readonly property int difficultySwatchSize: 10
    readonly property int difficultySwatchRadius: 4
    function difficultyColor(difficultyId) {
        return difficultyId >= 1 && difficultyId <= difficultyColors.length
            ? difficultyColors[difficultyId - 1]
            : difficultyColorFallback
    }
}
