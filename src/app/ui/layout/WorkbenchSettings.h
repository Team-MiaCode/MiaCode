#pragma once

#include <QFont>
#include <QObject>
#include <QString>
#include <QVariantMap>
#include "editor/SimaiCompletionCatalog.h"

// 桌面工作台的持久化边界。QML 在拖动期间维护临时几何，只在用户完成
// 操作后写入这里，从而避免分隔线移动时连续刷新配置文件。
// 布局项写入 PreferenceDocument 的 preferences_v2.json（与主题、编辑器同一文件）。
// 默认 QSettings NativeFormat 在未设置 organizationName 时，Windows 注册表
// 不会留下这些键，第二次启动仍是默认布局。
// QML preference models own interaction; MainWindow only persists and applies
// their backend-neutral values for preview/runtime consumers.
namespace miacode::ui {

class WorkbenchSettings final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool setupCompleted READ setupCompleted NOTIFY setupCompletedChanged)
    Q_PROPERTY(QString preferenceMigrationStatus READ preferenceMigrationStatus CONSTANT)
    Q_PROPERTY(bool efficientSidebarEnabled READ efficientSidebarEnabled WRITE setEfficientSidebarEnabled NOTIFY efficientSidebarEnabledChanged)
    Q_PROPERTY(bool sidebarVisible READ sidebarVisible WRITE setSidebarVisible NOTIFY sidebarVisibleChanged)
    Q_PROPERTY(int sidebarWidth READ sidebarWidth WRITE setSidebarWidth NOTIFY sidebarWidthChanged)
    Q_PROPERTY(int sidebarDefaultWidth READ sidebarDefaultWidth CONSTANT)
    Q_PROPERTY(int sidebarMinimumContentWidth READ sidebarMinimumContentWidth CONSTANT)
    Q_PROPERTY(int sidebarMaximumContentWidth READ sidebarMaximumContentWidth CONSTANT)
    Q_PROPERTY(bool bottomPanelVisible READ bottomPanelVisible WRITE setBottomPanelVisible NOTIFY bottomPanelVisibleChanged)
    Q_PROPERTY(double bottomPanelHeightRatio READ bottomPanelHeightRatio WRITE setBottomPanelHeightRatio NOTIFY bottomPanelHeightRatioChanged)
    Q_PROPERTY(double bottomPanelDefaultHeightRatio READ bottomPanelDefaultHeightRatio CONSTANT)
    Q_PROPERTY(double bottomPanelMinimumHeightRatio READ bottomPanelMinimumHeightRatio CONSTANT)
    Q_PROPERTY(double bottomPanelMaximumHeightRatio READ bottomPanelMaximumHeightRatio CONSTANT)
    Q_PROPERTY(double previewWidthRatio READ previewWidthRatio WRITE setPreviewWidthRatio NOTIFY previewWidthRatioChanged)
    Q_PROPERTY(double previewDefaultWidthRatio READ previewDefaultWidthRatio CONSTANT)
    Q_PROPERTY(bool previewDetached READ previewDetached WRITE setPreviewDetached NOTIFY previewDetachedChanged)
    Q_PROPERTY(double previewMinimumWidthRatio READ previewMinimumWidthRatio CONSTANT)
    Q_PROPERTY(double previewMaximumWidthRatio READ previewMaximumWidthRatio CONSTANT)
    Q_PROPERTY(bool previewCanvasFreeAspect READ previewCanvasFreeAspect WRITE setPreviewCanvasFreeAspect NOTIFY previewCanvasFreeAspectChanged)
    Q_PROPERTY(bool previewHidePv READ previewHidePv WRITE setPreviewHidePv NOTIFY previewHidePvChanged)
    Q_PROPERTY(QString uiFontFamily READ uiFontFamily CONSTANT)
    Q_PROPERTY(int uiScalePercent READ uiScalePercent WRITE setUiScalePercent NOTIFY uiScalePercentChanged)
    Q_PROPERTY(QString themeModeToken READ themeModeToken NOTIFY themeChanged)
    Q_PROPERTY(QString lightThemeToken READ lightThemeToken NOTIFY themeChanged)
    Q_PROPERTY(QString darkThemeToken READ darkThemeToken NOTIFY themeChanged)
    Q_PROPERTY(QString activeThemeToken READ activeThemeToken NOTIFY themeChanged)
    Q_PROPERTY(bool darkTheme READ darkTheme NOTIFY themeChanged)
    Q_PROPERTY(bool blurMaterialsEnabled READ blurMaterialsEnabled WRITE setBlurMaterialsEnabled NOTIFY blurMaterialsEnabledChanged)
    Q_PROPERTY(QFont codeFont READ codeFont NOTIFY editorSettingsChanged)
    Q_PROPERTY(int editorBlockSpacing READ editorBlockSpacing NOTIFY editorSettingsChanged)
    Q_PROPERTY(int fontSize READ fontSize WRITE setFontSize NOTIFY fontSizeChanged)
    Q_PROPERTY(bool editorHalfWidthInputEnabled READ editorHalfWidthInputEnabled NOTIFY editorSettingsChanged)
    Q_PROPERTY(bool editorOverwriteModeEnabled READ editorOverwriteModeEnabled NOTIFY editorSettingsChanged)
    Q_PROPERTY(bool editorAutoCompletionEnabled READ editorAutoCompletionEnabled NOTIFY editorSettingsChanged)
    Q_PROPERTY(bool editorImeInputDisabled READ editorImeInputDisabled NOTIFY editorSettingsChanged)
    Q_PROPERTY(bool editorAutoWrap READ editorAutoWrap WRITE setEditorAutoWrap NOTIFY editorSettingsChanged)
    Q_PROPERTY(bool editorScrollPastEnd READ editorScrollPastEnd WRITE setEditorScrollPastEnd NOTIFY editorSettingsChanged)
    Q_PROPERTY(bool editorSelectionBeatDisplay READ editorSelectionBeatDisplay WRITE setEditorSelectionBeatDisplay NOTIFY editorSettingsChanged)
    Q_PROPERTY(bool editorOverviewRulerEnabled READ editorOverviewRulerEnabled WRITE setEditorOverviewRulerEnabled NOTIFY editorSettingsChanged)

public:
    // What 关于 MiaCode shows: version, platform triple and build type. Read
    // from the build's own macros so the page cannot drift from the binary.
    Q_INVOKABLE QVariantMap aboutInfo() const;
    explicit WorkbenchSettings(QObject* parent = nullptr);
    bool setupCompleted() const;
    QString preferenceMigrationStatus() const;
    Q_INVOKABLE bool completeSetup();

    bool efficientSidebarEnabled() const;
    bool sidebarVisible() const;
    int sidebarWidth() const;
    int sidebarDefaultWidth() const;
    int sidebarMinimumContentWidth() const;
    int sidebarMaximumContentWidth() const;
    bool bottomPanelVisible() const;
    double bottomPanelHeightRatio() const;
    double bottomPanelDefaultHeightRatio() const;
    double bottomPanelMinimumHeightRatio() const;
    double bottomPanelMaximumHeightRatio() const;
    double previewWidthRatio() const;
    double previewDefaultWidthRatio() const;
    bool previewDetached() const;
    double previewMinimumWidthRatio() const;
    double previewMaximumWidthRatio() const;
    bool previewCanvasFreeAspect() const;
    bool previewHidePv() const;
    QString uiFontFamily() const;
    int uiScalePercent() const;
    void setUiScalePercent(int percent);
    QString themeModeToken() const;
    QString lightThemeToken() const;
    QString darkThemeToken() const;
    QString activeThemeToken() const;
    bool darkTheme() const;
    bool blurMaterialsEnabled() const { return blurMaterialsEnabled_; }
    void setBlurMaterialsEnabled(bool enabled);
    QFont codeFont() const;
    int editorBlockSpacing() const;
    int fontSize() const;
    bool editorHalfWidthInputEnabled() const;
    bool editorOverwriteModeEnabled() const;
    bool editorAutoCompletionEnabled() const;
    const miacode::editor::SimaiCompletionConfig& editorCompletionConfig() const;
    bool editorImeInputDisabled() const;
    bool editorAutoWrap() const;
    void setEditorAutoWrap(bool enabled);
    bool editorScrollPastEnd() const;
    bool editorSelectionBeatDisplay() const;
    bool editorOverviewRulerEnabled() const;

    void setEfficientSidebarEnabled(bool value);
    void setSidebarVisible(bool value);
    void setSidebarWidth(int value);
    void setBottomPanelVisible(bool value);
    void setBottomPanelHeightRatio(double value);
    void setPreviewWidthRatio(double value);
    void setPreviewDetached(bool value);
    void setPreviewCanvasFreeAspect(bool value);
    void setPreviewHidePv(bool value);
    void setFontSize(int value);
    void reloadEditorSettings();
    void reloadTheme();
    void setThemeModeToken(const QString& token);
    void setLightThemeToken(const QString& token);
    void setDarkThemeToken(const QString& token);
    void setEditorAppearance(int pointSize, double lineSpacingFactor);
    void setEditorScrollPastEnd(bool enabled);
    void setEditorSelectionBeatDisplay(bool enabled);
    void setEditorOverviewRulerEnabled(bool enabled);

signals:
    void setupCompletedChanged();
    void efficientSidebarEnabledChanged();
    void sidebarVisibleChanged();
    void sidebarWidthChanged();
    void bottomPanelVisibleChanged();
    void bottomPanelHeightRatioChanged();
    void previewWidthRatioChanged();
    void previewDetachedChanged();
    void previewCanvasFreeAspectChanged();
    void previewHidePvChanged();
    void fontSizeChanged();
    void editorSettingsChanged();
    void themeChanged();
    void blurMaterialsEnabledChanged();
    void uiScalePercentChanged();

private:
    static constexpr int kSidebarDefaultWidth = 190;
    static constexpr int kUiScaleDefaultPercent = 100;
    static constexpr int kUiScaleMinimumPercent = 75;
    static constexpr int kUiScaleMaximumPercent = 200;
    static constexpr double kBottomPanelDefaultHeightRatio = 0.25;
    static constexpr double kPreviewDefaultWidthRatio = 0.5;
    static constexpr int kSidebarMinimumContentWidth = 120;
    static constexpr int kSidebarMaximumContentWidth = 272;
    static constexpr double kBottomPanelMinimumHeightRatio = 0.15;
    static constexpr double kBottomPanelMaximumHeightRatio = 0.65;
    static constexpr double kPreviewMinimumWidthRatio = 0.3;
    static constexpr double kPreviewMaximumWidthRatio = 0.7;

    bool efficientSidebarEnabled_ = false;
    bool sidebarVisible_ = true;
    bool blurMaterialsEnabled_ = true;
    int sidebarWidth_ = kSidebarDefaultWidth;
    bool bottomPanelVisible_ = true;
    double bottomPanelHeightRatio_ = kBottomPanelDefaultHeightRatio;
    double previewWidthRatio_ = kPreviewDefaultWidthRatio;
    bool previewDetached_ = false;
    bool previewCanvasFreeAspect_ = false;
    bool previewHidePv_ = false;
    QString uiFontFamily_;
    int uiScalePercent_ = kUiScaleDefaultPercent;
    QFont codeFont_;
    int editorBlockSpacing_ = 0;
    int fontSize_ = 13;
    bool editorHalfWidthInputEnabled_ = true;
    bool editorOverwriteModeEnabled_ = false;
    bool editorAutoCompletionEnabled_ = true;
    miacode::editor::SimaiCompletionConfig editorCompletionConfig_ = miacode::editor::defaultCompletionConfig();
    bool editorImeInputDisabled_ = true;
    bool editorAutoWrap_ = true;
    bool editorScrollPastEnd_ = true;
    bool editorSelectionBeatDisplay_ = true;
    bool editorOverviewRulerEnabled_ = false;
    bool darkTheme_ = true;
    QString publishedThemeModeToken_;
    QString publishedLightThemeToken_;
    QString publishedDarkThemeToken_;
};
} // namespace miacode::ui
