#include "app/ui/chrome/WindowChrome.h"
#include "app/ui/theme/UiTheme.h"

#include <QGuiApplication>
#include <QVariantMap>
#include <QWindow>
#include <QQuickWindow>
#include <cfloat>

#import <AppKit/AppKit.h>
#import <QuartzCore/QuartzCore.h>

// AppKit's native Fill action, also used by Chromium's custom draggable regions.
@interface NSWindow (MiaCodeWindowFill)
- (void)_zoomFill:(id)sender;
@end

@interface MiaCodeWindowBindingObserver : NSObject
@property(nonatomic, strong) NSView* view;
@property(nonatomic, copy) void (^windowChanged)(NSWindow*);
@property(nonatomic, copy) void (^layerChanged)(void);
- (instancetype)initWithView:(NSView*)view callback:(void (^)(NSWindow*))callback
               layerCallback:(void (^)(void))layerCallback;
@end

@implementation MiaCodeWindowBindingObserver
- (instancetype)initWithView:(NSView*)view callback:(void (^)(NSWindow*))callback
               layerCallback:(void (^)(void))layerCallback
{
    self = [super init];
    if (self) {
        self.view = view;
        self.windowChanged = callback;
        self.layerChanged = layerCallback;
        [view addObserver:self forKeyPath:@"window"
                  options:NSKeyValueObservingOptionInitial | NSKeyValueObservingOptionNew
                  context:nullptr];
        [view addObserver:self forKeyPath:@"layer"
                  options:NSKeyValueObservingOptionNew context:nullptr];
    }
    return self;
}

- (void)observeValueForKeyPath:(NSString*)keyPath ofObject:(id)object
                       change:(NSDictionary*)change context:(void*)context
{
    if ([keyPath isEqualToString:@"window"]) {
        NSView* observed = self.view;
        self.windowChanged(observed ? observed.window : nil);
    } else if ([keyPath isEqualToString:@"layer"]) {
        self.layerChanged();
    } else {
        [super observeValueForKeyPath:keyPath ofObject:object change:change context:context];
    }
}

- (void)detach
{
    NSView* observed = self.view;
    if (observed != nil) {
        [observed removeObserver:self forKeyPath:@"window"];
        [observed removeObserver:self forKeyPath:@"layer"];
        self.view = nil;
    }
}

- (void)dealloc
{
    [self detach];
}
@end

@interface MiaCodeMaterialHostView : NSView
@property(nonatomic, strong) NSMutableArray<NSVisualEffectView*>* effects;
@property(nonatomic, strong) NSMutableArray<NSArray<NSLayoutConstraint*>*>* placements;
@property(nonatomic, strong) NSMutableArray<NSString*>* edgeKinds;
@end

@implementation MiaCodeMaterialHostView
- (NSView*)hitTest:(NSPoint)point
{
    Q_UNUSED(point);
    return nil;
}

- (instancetype)initWithFrame:(NSRect)frame
{
    self = [super initWithFrame:frame];
    if (self) {
        self.effects = [NSMutableArray array];
        self.placements = [NSMutableArray array];
        self.edgeKinds = [NSMutableArray array];
    }
    return self;
}
@end

namespace {
NSView* cocoaView(QWindow* window)
{
    if (window == nullptr || QGuiApplication::platformName() != QStringLiteral("cocoa")) {
        return nil;
    }
    return (__bridge NSView*)reinterpret_cast<void*>(window->winId());
}

void configureNativeTitleBar(NSWindow* window, bool fullscreen = false)
{
    if (window == nil) {
        return;
    }
    if (window.titleVisibility != NSWindowTitleHidden) {
        window.titleVisibility = NSWindowTitleHidden;
    }

    static NSString* const toolbarIdentifier = @"MiaCode.WindowTitleBar";
    if (window.toolbar == nil || ![window.toolbar.identifier isEqualToString:toolbarIdentifier]) {
        NSToolbar* toolbar = [[NSToolbar alloc] initWithIdentifier:toolbarIdentifier];
        toolbar.displayMode = NSToolbarDisplayModeIconOnly;
        toolbar.sizeMode = NSToolbarSizeModeSmall;
        toolbar.allowsUserCustomization = NO;
        toolbar.autosavesConfiguration = NO;
        toolbar.showsBaselineSeparator = NO;
        window.toolbar = toolbar;
    }
    const BOOL toolbarVisible = fullscreen ? NO : YES;
    if (window.toolbar.visible != toolbarVisible) {
        window.toolbar.visible = toolbarVisible;
    }

    if (@available(macOS 11.0, *)) {
        if (window.toolbarStyle != NSWindowToolbarStyleUnifiedCompact) {
            window.toolbarStyle = NSWindowToolbarStyleUnifiedCompact;
        }
        if (window.titlebarSeparatorStyle != NSTitlebarSeparatorStyleNone) {
            window.titlebarSeparatorStyle = NSTitlebarSeparatorStyleNone;
        }
    }
}
}  // namespace

namespace miacode::ui {

struct WindowChrome::MacState {
    MiaCodeWindowBindingObserver* windowObserver = nil;
    MiaCodeMaterialHostView* materialView = nil;
    NSMutableArray<id>* fullScreenObservers = nil;
    qreal windowedTitleBarLeadingInset = 0;
    qreal windowedTitleBarHeight = 0;
};

void WindowChrome::MacStateDeleter::operator()(MacState* state) const
{
    delete state;
}

void WindowChrome::applyMacOsAppearance(QWindow* window)
{
    if (QGuiApplication::platformName() != QStringLiteral("cocoa")) {
        return;
    }
    NSView* view = cocoaView(window);
    NSWindow* nativeWindow = (view != nil) ? view.window : nil;
    if (nativeWindow == nil) {
        return;
    }
    nativeWindow.appearance = [NSAppearance appearanceNamed:
        UiTheme::isDarkTheme() ? NSAppearanceNameDarkAqua : NSAppearanceNameAqua];
}

void WindowChrome::refreshMacOsMaterial(QWindow* window)
{
    if (window == nullptr) {
        return;
    }
    NSView* view = cocoaView(window);
    NSWindow* nativeWindow = view.window;
    if (nativeWindow == nil) {
        return;
    }
    const bool materialWanted = blurMaterialsEnabled_ && !materialRegions_.isEmpty();
    if (nativeWindow.opaque) {
        nativeWindow.opaque = NO;
    }
    if (![nativeWindow.backgroundColor isEqual:NSColor.clearColor]) {
        nativeWindow.backgroundColor = NSColor.clearColor;
    }
    if (!materialWanted) {
        releaseMacOsMaterial();
        setNativeMaterialAvailable(false);
        return;
    }

    // Qt's native effect integration requires its backing container layer.
    // Wait for scene-graph initialization before adding the backdrop.
    auto* quickWindow = qobject_cast<QQuickWindow*>(window);
    if (quickWindow != nullptr && !quickWindow->isSceneGraphInitialized()) {
        return;
    }
    if (![view.layer respondsToSelector:NSSelectorFromString(@"contentLayer")]) {
        return;
    }

    if (!mac_) {
        mac_.reset(new MacState);
    }

    // Keep Qt's content view identity and place the native backdrop beneath
    // its Metal layer. Material geometry belongs to AppKit's layout system.
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    MiaCodeMaterialHostView* material = mac_->materialView;
    bool layoutChanged = false;
    if (material == nil) {
        layoutChanged = true;
        material = [[MiaCodeMaterialHostView alloc] initWithFrame:view.bounds];
        material.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        material.wantsLayer = YES;
        material.layer.zPosition = -FLT_MAX;
        mac_->materialView = material;
    }
    if (material.superview != view || material.layer.superlayer != view.layer) {
        layoutChanged = true;
        [material removeFromSuperview];
        [view addSubview:material];
    }
    if (!NSEqualRects(material.frame, view.bounds)) {
        layoutChanged = true;
        material.frame = view.bounds;
    }

    const NSUInteger regionCount = static_cast<NSUInteger>(materialRegions_.size());
    while (material.effects.count > regionCount) {
        layoutChanged = true;
        [NSLayoutConstraint deactivateConstraints:material.placements.lastObject];
        [material.placements removeLastObject];
        [material.edgeKinds removeLastObject];
        [material.effects.lastObject removeFromSuperview];
        [material.effects removeLastObject];
    }

    // Pin each chrome strip to window edges with AutoLayout so live window
    // resizing stretches strips in AppKit without rebuilding views or masks,
    // and leaves the central workspace outside behind-window blur calculation.
    for (NSUInteger i = 0; i < regionCount; ++i) {
        const QVariantMap region = materialRegions_.at(i).toMap();
        NSString* edgeKind = region.value(QStringLiteral("edge")).toString().toNSString();

        NSVisualEffectView* visualEffect = i < material.effects.count ? material.effects[i] : nil;
        if (visualEffect == nil) {
            layoutChanged = true;
            visualEffect = [[NSVisualEffectView alloc] initWithFrame:NSZeroRect];
            visualEffect.material = NSVisualEffectMaterialSidebar;
            visualEffect.blendingMode = NSVisualEffectBlendingModeBehindWindow;
            visualEffect.state = NSVisualEffectStateFollowsWindowActiveState;
            visualEffect.translatesAutoresizingMaskIntoConstraints = NO;
            [material addSubview:visualEffect];
            [material.effects addObject:visualEffect];
            [material.placements addObject:@[]];
            [material.edgeKinds addObject:@""];
        }

        if (![material.edgeKinds[i] isEqualToString:edgeKind]) {
            layoutChanged = true;
            [NSLayoutConstraint deactivateConstraints:material.placements[i]];
            NSArray<NSLayoutConstraint*>* placement = @[];
            if ([edgeKind isEqualToString:@"top"]) {
                placement = @[
                    [visualEffect.leadingAnchor constraintEqualToAnchor:material.leadingAnchor],
                    [visualEffect.trailingAnchor constraintEqualToAnchor:material.trailingAnchor],
                    [visualEffect.topAnchor constraintEqualToAnchor:material.topAnchor],
                    [visualEffect.heightAnchor constraintEqualToConstant:0]
                ];
            } else if ([edgeKind isEqualToString:@"left"]) {
                placement = @[
                    [visualEffect.leadingAnchor constraintEqualToAnchor:material.leadingAnchor],
                    [visualEffect.topAnchor constraintEqualToAnchor:material.topAnchor],
                    [visualEffect.bottomAnchor constraintEqualToAnchor:material.bottomAnchor],
                    [visualEffect.widthAnchor constraintEqualToConstant:0]
                ];
            }
            material.placements[i] = placement;
            material.edgeKinds[i] = edgeKind;
            [NSLayoutConstraint activateConstraints:placement];
        }

        NSArray<NSLayoutConstraint*>* placement = material.placements[i];
        CGFloat constants[4] = {0, 0, 0, 0};
        if ([edgeKind isEqualToString:@"top"]) {
            constants[0] = region.value(QStringLiteral("leftInset")).toDouble();
            constants[1] = -region.value(QStringLiteral("rightInset")).toDouble();
            constants[2] = region.value(QStringLiteral("topInset")).toDouble();
            constants[3] = region.value(QStringLiteral("thickness")).toDouble();
        } else if ([edgeKind isEqualToString:@"left"]) {
            constants[0] = region.value(QStringLiteral("leftInset")).toDouble();
            constants[1] = region.value(QStringLiteral("topInset")).toDouble();
            constants[2] = -region.value(QStringLiteral("bottomInset")).toDouble();
            constants[3] = region.value(QStringLiteral("thickness")).toDouble();
        }
        for (NSUInteger j = 0; j < placement.count; ++j) {
            if (placement[j].constant != constants[j]) {
                placement[j].constant = constants[j];
                layoutChanged = true;
            }
        }
    }
    if (layoutChanged) {
        [material layoutSubtreeIfNeeded];
    }
    [CATransaction commit];
    setNativeMaterialAvailable(true);
}

void WindowChrome::handleTitleBarDoubleClick()
{
    if (window_.isNull()) {
        return;
    }

    NSView* view = cocoaView(window_.data());
    NSWindow* nativeWindow = view.window;
    if (nativeWindow == nil) {
        return;
    }

    NSString* action = [NSUserDefaults.standardUserDefaults
        stringForKey:@"AppleActionOnDoubleClick"];
    if ([action isEqualToString:@"Fill"]
            && [nativeWindow respondsToSelector:@selector(_zoomFill:)]) {
        [nativeWindow _zoomFill:nil];
    } else if (action == nil || [action isEqualToString:@"Maximize"]) {
        [nativeWindow performZoom:nil];
    } else if ([action isEqualToString:@"Minimize"]) {
        [nativeWindow performMiniaturize:nil];
    }
}

void WindowChrome::observeMacOsWindow(QWindow* window)
{
    if (!mac_) {
        mac_.reset(new MacState);
    }
    if (mac_->windowObserver != nil) {
        return;
    }
    NSView* view = cocoaView(window);
    if (view == nil) {
        return;
    }
    QPointer<WindowChrome> guard(this);
    mac_->windowObserver = [[MiaCodeWindowBindingObserver alloc]
        initWithView:view callback:^(NSWindow* nativeWindow) {
            if (!guard) {
                return;
            }
            if (nativeWindow == nil || window_.isNull()) {
                stopObservingMacOsFullScreen();
                return;
            }
            refreshNativeTheme();
            configureNativeTitleBar(nativeWindow,
                window_->windowState() == Qt::WindowFullScreen);
            observeMacOsFullScreen(window_.data());
            updateMacOsTitleBarMetrics(window_.data());
            materialUpdateTimer_.start();
        } layerCallback:^{
            if (!guard) {
                return;
            }
            refreshMaterialAfterPresentation();
        }];
}

void WindowChrome::stopObservingMacOsWindow()
{
    if (!mac_ || mac_->windowObserver == nil) {
        return;
    }
    [mac_->windowObserver detach];
    mac_->windowObserver = nil;
}

void WindowChrome::updateMacOsTitleBarMetrics(QWindow* window)
{
    NSView* view = cocoaView(window);
    NSWindow* nativeWindow = view.window;
    NSView* contentView = nativeWindow.contentView;
    if (contentView == nil) {
        return;
    }
    if (!mac_) {
        mac_.reset(new MacState);
    }

    [contentView.superview layoutSubtreeIfNeeded];

    const qreal systemTitleBarHeight = static_cast<qreal>(
        NSHeight(contentView.bounds) - NSHeight(nativeWindow.contentLayoutRect));
    if (systemTitleBarHeight > 0) {
        mac_->windowedTitleBarHeight = systemTitleBarHeight;
        setTitleBarHeight(mac_->windowedTitleBarHeight);
    }

    if (window->windowState() == Qt::WindowFullScreen) {
        setTitleBarLeadingInset(0);
        return;
    }

    NSButton* buttons[] = {
        [nativeWindow standardWindowButton:NSWindowCloseButton],
        [nativeWindow standardWindowButton:NSWindowMiniaturizeButton],
        [nativeWindow standardWindowButton:NSWindowZoomButton]
    };

    NSRect group = NSZeroRect;
    NSRect previous = NSZeroRect;
    bool hasButton = false;
    qreal buttonGap = 0;
    for (NSButton* button : buttons) {
        if (button == nil || button.superview == nil) {
            continue;
        }
        const NSRect frame = [contentView convertRect:button.frame fromView:button.superview];
        group = hasButton ? NSUnionRect(group, frame) : frame;
        if (hasButton) {
            const qreal gap = NSMinX(frame) - NSMaxX(previous);
            if (gap > 0 && (buttonGap == 0 || gap < buttonGap)) {
                buttonGap = gap;
            }
        }
        previous = frame;
        hasButton = true;
    }

    if (!hasButton) {
        setTitleBarLeadingInset(0);
        return;
    }

    mac_->windowedTitleBarLeadingInset = static_cast<qreal>(NSMaxX(group)) + buttonGap;
    setTitleBarLeadingInset(mac_->windowedTitleBarLeadingInset);
}

void WindowChrome::observeMacOsFullScreen(QWindow* window)
{
    stopObservingMacOsFullScreen();

    NSView* view = cocoaView(window);
    NSWindow* nativeWindow = (view != nil) ? view.window : nil;
    if (nativeWindow == nil) {
        return;
    }
    if (!mac_) {
        mac_.reset(new MacState);
    }

    NSNotificationCenter* center = NSNotificationCenter.defaultCenter;
    QPointer<WindowChrome> guard(this);
    __weak NSWindow* weakNativeWindow = nativeWindow;
    auto observe = [&](NSNotificationName name, void (^handler)(NSNotification*)) {
        return [center addObserverForName:name
                                   object:nativeWindow
                                    queue:NSOperationQueue.mainQueue
                               usingBlock:handler];
    };
    mac_->fullScreenObservers = [NSMutableArray arrayWithObjects:
        observe(NSWindowWillEnterFullScreenNotification, ^(__unused NSNotification*) {
            NSWindow* win = weakNativeWindow;
            if (!guard || !mac_ || win == nil) {
                return;
            }
            win.toolbar.visible = NO;
            setTitleBarLeadingInset(0);
            setTitleBarHeight(mac_->windowedTitleBarHeight);
        }),
        observe(NSWindowDidEnterFullScreenNotification, ^(__unused NSNotification*) {
            NSWindow* win = weakNativeWindow;
            if (!guard || !mac_ || win == nil) {
                return;
            }
            win.toolbar.visible = NO;
            setTitleBarLeadingInset(0);
            setTitleBarHeight(mac_->windowedTitleBarHeight);
            materialUpdateTimer_.start();
        }),
        observe(NSWindowWillExitFullScreenNotification, ^(__unused NSNotification*) {
            NSWindow* win = weakNativeWindow;
            if (!guard || !mac_ || win == nil) {
                return;
            }
            configureNativeTitleBar(win);
            setTitleBarLeadingInset(mac_->windowedTitleBarLeadingInset);
            setTitleBarHeight(mac_->windowedTitleBarHeight);
        }),
        observe(NSWindowDidExitFullScreenNotification, ^(__unused NSNotification*) {
            NSWindow* win = weakNativeWindow;
            if (!guard || win == nil) {
                return;
            }
            configureNativeTitleBar(win);
            updateMacOsTitleBarMetrics(window_.data());
            materialUpdateTimer_.start();
        }),
        nil];
}

void WindowChrome::stopObservingMacOsFullScreen()
{
    if (!mac_ || mac_->fullScreenObservers == nil) {
        return;
    }
    NSNotificationCenter* center = NSNotificationCenter.defaultCenter;
    for (id observer in mac_->fullScreenObservers) {
        [center removeObserver:observer];
    }
    mac_->fullScreenObservers = nil;
}

void WindowChrome::releaseMacOsMaterial()
{
    if (!mac_ || mac_->materialView == nil) {
        return;
    }
    [mac_->materialView removeFromSuperview];
    mac_->materialView = nil;
}

} // namespace miacode::ui
