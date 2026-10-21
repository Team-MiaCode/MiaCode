#include "app/ui/preferences/ApplicationMenuTranslation.h"

#import <AppKit/AppKit.h>

namespace miacode {

void retranslateNativeApplicationMenu()
{
    // QCocoaMenuLoader owns the standard application-menu items (About,
    // Preferences, Services, Hide, Quit) and only retranslates them inside a
    // menu bar sync. The platform class is private to the Cocoa plugin, so
    // both the class and the refresh method are probed before use.
    Class loaderClass = NSClassFromString(@"QCocoaMenuLoader");
    if (loaderClass == nil || ![loaderClass respondsToSelector:@selector(sharedMenuLoader)]) {
        return;
    }
    id loader = [loaderClass performSelector:@selector(sharedMenuLoader)];
    if ([loader respondsToSelector:@selector(qtTranslateApplicationMenu)]) {
        [loader performSelector:@selector(qtTranslateApplicationMenu)];
    }
}

}  // namespace miacode
