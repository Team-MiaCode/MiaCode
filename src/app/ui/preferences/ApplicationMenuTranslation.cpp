#include "app/ui/preferences/ApplicationMenuTranslation.h"

#include <QtGlobal>

#if !defined(Q_OS_MACOS)

namespace miacode {

void retranslateNativeApplicationMenu()
{
    // Other platforms rebuild their menubar entries from the QML actions,
    // which already follow the installed catalog.
}

}  // namespace miacode

#endif
