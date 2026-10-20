#include "app/ui/dialogs/NativeFolderPicker.h"

#include <QPointer>
#include <QWindow>

#import <AppKit/AppKit.h>

namespace miacode::ui {

void showNativeFolders(QObject* lifetime, QObject* parentWindow, const QString& title,
                       const QUrl& startFolder, UiRequestService::NativeFolderResult onResolved)
{
    NSOpenPanel* panel = [[NSOpenPanel alloc] init];
    panel.title = title.toNSString();
    panel.canChooseFiles = NO;
    panel.canChooseDirectories = YES;
    panel.allowsMultipleSelection = YES;
    panel.canCreateDirectories = NO;
    if (startFolder.isLocalFile())
        panel.directoryURL = [NSURL fileURLWithPath:startFolder.toLocalFile().toNSString() isDirectory:YES];

    const QPointer<QObject> guard(lifetime);
    const auto cancellation = QObject::connect(lifetime, &QObject::destroyed, [panel] { [panel cancel:nil]; });
    const auto finished = ^(NSModalResponse response) {
        QObject::disconnect(cancellation);
        QList<QUrl> urls;
        if (response == NSModalResponseOK) {
            for (NSURL* url in panel.URLs)
                if (url.isFileURL) urls.append(QUrl::fromLocalFile(QString::fromNSString(url.path)));
        }
        [panel release];
        if (guard) onResolved(urls, {});
    };
    QWindow* window = qobject_cast<QWindow*>(parentWindow);
    NSWindow* owner = window ? [reinterpret_cast<NSView*>(window->winId()) window] : nil;
    if (owner) [panel beginSheetModalForWindow:owner completionHandler:finished];
    else [panel beginWithCompletionHandler:finished];
}

} // namespace miacode::ui
