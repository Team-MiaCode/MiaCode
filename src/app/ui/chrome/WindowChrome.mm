#include "app/ui/chrome/WindowChrome.h"

#include <QRectF>
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
        self.windowChanged(self.view.window);
    } else if ([keyPath isEqualToString:@"layer"]) {
        self.layerChanged();
    } else {
        [super observeValueForKeyPath:keyPath ofObject:object change:change context:context];
    }
}

- (void)dealloc
{
    [_view removeObserver:self forKeyPath:@"window"];
    [_view removeObserver:self forKeyPath:@"layer"];
#if !__has_feature(objc_arc)
    [_view release];
    [_windowChanged release];
    [_layerChanged release];
    [super dealloc];
#endif
}
@end

@interface MiaCodeMaterialHostView : NSView
@property(nonatomic, strong) NSView* surfaces;
@property(nonatomic, strong) NSMutableArray<NSView*>* effects;
@property(nonatomic, strong) NSMutableArray<NSArray<NSLayoutConstraint*>*>* placements;
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
    }
    return self;
}
#if !__has_feature(objc_arc)
- (void)dealloc
{
    [_surfaces release];
    [_effects release];
    [_placements release];
    [super dealloc];
}
#endif
@end

namespace {
void pinMaterialContent(NSView* content, NSView* owner)
{
    content.translatesAutoresizingMaskIntoConstraints = NO;
    [NSLayoutConstraint activateConstraints:@[
        [content.leadingAnchor constraintEqualToAnchor:owner.leadingAnchor],
        [content.trailingAnchor constraintEqualToAnchor:owner.trailingAnchor],
        [content.topAnchor constraintEqualToAnchor:owner.topAnchor],
        [content.bottomAnchor constraintEqualToAnchor:owner.bottomAnchor]
    ]];
}
}

namespace miacode::ui {
namespace {
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
#if !__has_feature(objc_arc)
        [toolbar release];
#endif
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
}

void WindowChrome::refreshMacOsMaterial(QWindow* window)
{
    if (window == nullptr) {
        return;
    }
    NSView* view = (__bridge NSView*)reinterpret_cast<void*>(window->winId());
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

    // Keep Qt's content view identity and place the native backdrop beneath
    // its Metal layer. Material geometry belongs to AppKit's layout system.
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    MiaCodeMaterialHostView* material = (__bridge MiaCodeMaterialHostView*)macMaterialView_;
    bool layoutChanged = false;
    if (material == nil) {
        layoutChanged = true;
        material = [[MiaCodeMaterialHostView alloc] initWithFrame:view.bounds];
        material.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        material.wantsLayer = YES;
        material.layer.zPosition = -FLT_MAX;
        NSView* surfaces = [[NSView alloc] initWithFrame:material.bounds];
        [material addSubview:surfaces];
        pinMaterialContent(surfaces, material);
        material.surfaces = surfaces;
#if !__has_feature(objc_arc)
        [surfaces release];
#endif
#if __has_feature(objc_arc)
        macMaterialView_ = (__bridge_retained void*)material;
#else
        macMaterialView_ = material;
#endif
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

    NSView* surfaces = material.surfaces;
    const NSUInteger regionCount = static_cast<NSUInteger>(materialRegions_.size());
    while (material.effects.count > regionCount) {
        layoutChanged = true;
        [NSLayoutConstraint deactivateConstraints:material.placements.lastObject];
        [material.placements removeLastObject];
        [material.effects.lastObject removeFromSuperview];
        [material.effects removeLastObject];
    }
    for (NSUInteger i = 0; i < regionCount; ++i) {
        const QVariantMap region = materialRegions_.at(i).toMap();
        const QRectF rect = region.value(QStringLiteral("rect")).toRectF();
        NSView* effect = i < material.effects.count ? material.effects[i] : nil;
        if (effect == nil) {
            layoutChanged = true;
            NSVisualEffectView* visualEffect = [[NSVisualEffectView alloc] initWithFrame:NSZeroRect];
            visualEffect.material = NSVisualEffectMaterialSidebar;
            visualEffect.blendingMode = NSVisualEffectBlendingModeBehindWindow;
            visualEffect.state = NSVisualEffectStateFollowsWindowActiveState;
            effect = visualEffect;
            effect.translatesAutoresizingMaskIntoConstraints = NO;
            [surfaces addSubview:effect];
            [material.effects addObject:effect];
            NSArray<NSLayoutConstraint*>* placement = @[
                [effect.leadingAnchor constraintEqualToAnchor:surfaces.leadingAnchor],
                [effect.topAnchor constraintEqualToAnchor:surfaces.topAnchor],
                i == 0 ? [effect.trailingAnchor constraintEqualToAnchor:surfaces.trailingAnchor]
                       : [effect.widthAnchor constraintEqualToConstant:0],
                i == 1 ? [effect.bottomAnchor constraintEqualToAnchor:surfaces.bottomAnchor]
                       : [effect.heightAnchor constraintEqualToConstant:0]
            ];
            [material.placements addObject:placement];
            [NSLayoutConstraint activateConstraints:placement];
#if !__has_feature(objc_arc)
            [effect release];
#endif
        }
        NSArray<NSLayoutConstraint*>* placement = material.placements[i];
        const CGFloat constants[] = {
            rect.x(), rect.y(),
            i == 0 ? rect.right() - NSWidth(view.bounds) : rect.width(),
            i == 1 ? rect.bottom() - NSHeight(view.bounds) : rect.height()
        };
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

    NSView* view = (__bridge NSView*)reinterpret_cast<void*>(window_->winId());
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
    if (macViewWindowObserver_ != nullptr) {
        return;
    }
    NSView* view = (__bridge NSView*)reinterpret_cast<void*>(window->winId());
    MiaCodeWindowBindingObserver* observer = [[MiaCodeWindowBindingObserver alloc]
        initWithView:view callback:^(NSWindow* nativeWindow) {
            if (nativeWindow == nil || window_.isNull()) {
                stopObservingMacOsFullScreen();
                return;
            }
            refreshNativeTheme();
            configureNativeTitleBar(nativeWindow,
                window_->windowState() == Qt::WindowFullScreen);
            observeMacOsFullScreen(window_.data());
            materialUpdateTimer_.start();
        } layerCallback:^{
            materialUpdateTimer_.start();
            refreshMaterialAfterPresentation();
        }];
#if __has_feature(objc_arc)
    macViewWindowObserver_ = (__bridge_retained void*)observer;
#else
    macViewWindowObserver_ = observer;
#endif
}

void WindowChrome::stopObservingMacOsWindow()
{
    if (macViewWindowObserver_ == nullptr) {
        return;
    }
#if __has_feature(objc_arc)
    MiaCodeWindowBindingObserver* observer =
        (__bridge_transfer MiaCodeWindowBindingObserver*)macViewWindowObserver_;
    Q_UNUSED(observer);
#else
    [static_cast<MiaCodeWindowBindingObserver*>(macViewWindowObserver_) release];
#endif
    macViewWindowObserver_ = nullptr;
}

void WindowChrome::updateMacOsTitleBarMetrics(QWindow* window)
{
    NSView* view = (__bridge NSView*)reinterpret_cast<void*>(window->winId());
    NSWindow* nativeWindow = view.window;
    NSView* contentView = nativeWindow.contentView;
    if (contentView == nil) {
        return;
    }

    [contentView.superview layoutSubtreeIfNeeded];

    const qreal systemTitleBarHeight = static_cast<qreal>(
        NSHeight(contentView.bounds) - NSHeight(nativeWindow.contentLayoutRect));
    if (systemTitleBarHeight > 0) {
        windowedTitleBarHeight_ = systemTitleBarHeight;
        setTitleBarHeight(windowedTitleBarHeight_);
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

    windowedTitleBarLeadingInset_ = static_cast<qreal>(NSMaxX(group)) + buttonGap;
    setTitleBarLeadingInset(windowedTitleBarLeadingInset_);
}

void WindowChrome::observeMacOsFullScreen(QWindow* window)
{
    stopObservingMacOsFullScreen();

    NSView* view = (__bridge NSView*)reinterpret_cast<void*>(window->winId());
    NSWindow* nativeWindow = (view != nil) ? view.window : nil;
    if (nativeWindow == nil) {
        return;
    }

    NSNotificationCenter* center = NSNotificationCenter.defaultCenter;
    id willEnterObserver = [center
        addObserverForName:NSWindowWillEnterFullScreenNotification
                    object:nativeWindow
                     queue:NSOperationQueue.mainQueue
                usingBlock:^(__unused NSNotification* notification) {
                    nativeWindow.toolbar.visible = NO;
                    setTitleBarLeadingInset(0);
                    setTitleBarHeight(windowedTitleBarHeight_);
                }];
    id didEnterObserver = [center
        addObserverForName:NSWindowDidEnterFullScreenNotification
                    object:nativeWindow
                     queue:NSOperationQueue.mainQueue
                usingBlock:^(__unused NSNotification* notification) {
                    nativeWindow.toolbar.visible = NO;
                    setTitleBarLeadingInset(0);
                    setTitleBarHeight(windowedTitleBarHeight_);
                    materialUpdateTimer_.start();
                }];
    id willExitObserver = [center
        addObserverForName:NSWindowWillExitFullScreenNotification
                    object:nativeWindow
                     queue:NSOperationQueue.mainQueue
                usingBlock:^(__unused NSNotification* notification) {
                    configureNativeTitleBar(nativeWindow);
                    setTitleBarLeadingInset(windowedTitleBarLeadingInset_);
                    setTitleBarHeight(windowedTitleBarHeight_);
                }];
    id didExitObserver = [center
        addObserverForName:NSWindowDidExitFullScreenNotification
                    object:nativeWindow
                     queue:NSOperationQueue.mainQueue
                usingBlock:^(__unused NSNotification* notification) {
                    configureNativeTitleBar(nativeWindow);
                    materialUpdateTimer_.start();
                }];

    macWillEnterFullScreenObserver_ = (__bridge void*)willEnterObserver;
    macDidEnterFullScreenObserver_ = (__bridge void*)didEnterObserver;
    macWillExitFullScreenObserver_ = (__bridge void*)willExitObserver;
    macDidExitFullScreenObserver_ = (__bridge void*)didExitObserver;
}

void WindowChrome::stopObservingMacOsFullScreen()
{
    NSNotificationCenter* center = NSNotificationCenter.defaultCenter;
    if (macWillEnterFullScreenObserver_ != nullptr) {
        id observer = (__bridge id)macWillEnterFullScreenObserver_;
        [center removeObserver:observer];
        macWillEnterFullScreenObserver_ = nullptr;
    }
    if (macDidEnterFullScreenObserver_ != nullptr) {
        id observer = (__bridge id)macDidEnterFullScreenObserver_;
        [center removeObserver:observer];
        macDidEnterFullScreenObserver_ = nullptr;
    }
    if (macWillExitFullScreenObserver_ != nullptr) {
        id observer = (__bridge id)macWillExitFullScreenObserver_;
        [center removeObserver:observer];
        macWillExitFullScreenObserver_ = nullptr;
    }
    if (macDidExitFullScreenObserver_ != nullptr) {
        id observer = (__bridge id)macDidExitFullScreenObserver_;
        [center removeObserver:observer];
        macDidExitFullScreenObserver_ = nullptr;
    }
}

void WindowChrome::releaseMacOsMaterial()
{
    if (macMaterialView_ == nullptr) {
        return;
    }
#if __has_feature(objc_arc)
    NSView* material = (__bridge_transfer NSView*)macMaterialView_;
#else
    NSView* material = static_cast<NSView*>(macMaterialView_);
#endif
    [material removeFromSuperview];
#if !__has_feature(objc_arc)
    [material release];
#endif
    macMaterialView_ = nullptr;
}

} // namespace miacode::ui
