#include "timeline/quick/TimelineNativePresentation.h"
#include "timeline/quick/TimelineFramePresentationClock.h"
#include "common/DebugLog.h"

#include <QQuickWindow>
#include <QSGRendererInterface>

#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <objc/runtime.h>

@interface TimelineMetalPresentationState : NSObject {
@public
    IMP nextDrawableImplementation;
    id<CAMetalDrawable> pendingDrawable;
}
@end
@implementation TimelineMetalPresentationState
@end

namespace {
char presentationStateKey;

id nextTimelineDrawable(CAMetalLayer* layer, SEL selector)
{
    auto* state = static_cast<TimelineMetalPresentationState*>(objc_getAssociatedObject(layer, &presentationStateKey));
    const auto original = reinterpret_cast<id (*)(id, SEL)>(state->nextDrawableImplementation);
    id<CAMetalDrawable> drawable = original(layer, selector);
    state->pendingDrawable = drawable;
    return drawable;
}

CAMetalLayer* findMetalLayer(CALayer* root)
{
    if ([root isKindOfClass:CAMetalLayer.class]) return static_cast<CAMetalLayer*>(root);
    for (CALayer* child in root.sublayers) {
        if (CAMetalLayer* layer = findMetalLayer(child)) return layer;
    }
    return nil;
}
}

void observeTimelineNativePresentation(
    QQuickWindow* window, const std::shared_ptr<TimelineFramePresentationClock>& clock)
{
    if (window == nullptr || window->rendererInterface()->graphicsApi() != QSGRendererInterface::Metal) return;
    auto* view = (__bridge NSView*)reinterpret_cast<void*>(window->winId());
    CAMetalLayer* layer = findMetalLayer(view.layer);
    if (layer == nil) return;
    auto* state = static_cast<TimelineMetalPresentationState*>(objc_getAssociatedObject(layer, &presentationStateKey));
    if (state == nil) {
        // Preserve Qt's layer subclass and observe only this window's drawable acquisition.
        @synchronized(CAMetalLayer.class) {
            Class originalClass = object_getClass(layer);
            NSString* name = [@"MiaCodeTimelinePresentation_" stringByAppendingString:NSStringFromClass(originalClass)];
            Class observedClass = NSClassFromString(name);
            if (observedClass == Nil) {
                observedClass = objc_allocateClassPair(originalClass, name.UTF8String, 0);
                class_addMethod(observedClass, @selector(nextDrawable), reinterpret_cast<IMP>(nextTimelineDrawable), "@@:");
                objc_registerClassPair(observedClass);
            }
            state = [TimelineMetalPresentationState new];
            state->nextDrawableImplementation = class_getMethodImplementation(originalClass, @selector(nextDrawable));
            objc_setAssociatedObject(layer, &presentationStateKey, state, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
            object_setClass(layer, observedClass);
        }
        miacode::debug_log::appendLine(miacode::debug_log::Channel::Runtime,
            QStringLiteral("timeline/presentation"),
            QStringLiteral("action=native_drawable_observer backend=metal"));
    }

    // Acquisition can precede scene synchronization. Attach the sample only after
    // synchronization, when its playhead and the drawable belong to the same frame.
    id<CAMetalDrawable> drawable = state->pendingDrawable;
    state->pendingDrawable = nil;
    const auto sample = clock->renderFrameSample();
    if (drawable == nil || sample.sampledAtNs < 0) return;
    clock->activateNativePresentation();
    [drawable addPresentedHandler:^(id<MTLDrawable> presented) {
        const double time = presented.presentedTime;
        if (time <= 0.0) {
            return;
        }
        auto presentationNs = TimelineFramePresentationClock::nowNs();
        presentationNs -= static_cast<long long>((CACurrentMediaTime() - time) * 1000000000.0);
        clock->recordPresentation(sample, presentationNs);
    }];
}
