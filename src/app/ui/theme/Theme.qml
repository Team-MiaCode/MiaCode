pragma Singleton

import QtQuick
import "ThemePalettes.js" as PaletteData

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

    readonly property var themeCatalog: PaletteData.catalog.themes
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
    readonly property real uiScale: (preferences?.uiScalePercent ?? 100) / 100.0

    function iconSourceSize(width, height) {
        // IconImage does not upscale a smaller source within its display box.
        // Retain baseline resolution below 100%; the scene performs the shrink.
        const resolutionScale = Math.max(1, uiScale)
        return Qt.size(Math.ceil(width * resolutionScale), Math.ceil(height * resolutionScale))
    }
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
    readonly property int difficultySwatchSize: 12
    readonly property int difficultySwatchRadius: 2
    function difficultyColor(difficultyId) {
        return difficultyId >= 1 && difficultyId <= difficultyColors.length
            ? difficultyColors[difficultyId - 1]
            : difficultyColorFallback
    }
}
