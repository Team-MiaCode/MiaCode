#pragma once

namespace miacode {

// Retranslate the standard application-menu items owned by the platform.
// Qt only does this while it syncs the menu bar, so a runtime language change
// has to ask for it explicitly.
void retranslateNativeApplicationMenu();

}  // namespace miacode
