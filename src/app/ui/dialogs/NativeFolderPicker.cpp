#include "app/ui/dialogs/NativeFolderPicker.h"

namespace miacode::ui {

void installNativeFolderPicker(UiRequestService& requests)
{
    requests.setNativeFolderPicker(showNativeFolders);
}

} // namespace miacode::ui
