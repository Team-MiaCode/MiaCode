#pragma once

#include "app/services/UiRequestService.h"

namespace miacode::ui {

void installNativeFolderPicker(UiRequestService& requests);

// Platform implementation. The native dialog closes when lifetime is destroyed;
// the continuation may destroy lifetime, so invoke it only after native cleanup.
void showNativeFolders(QObject* lifetime, QObject* parentWindow, const QString& title,
                       const QUrl& startFolder, UiRequestService::NativeFolderResult onResolved);

} // namespace miacode::ui
