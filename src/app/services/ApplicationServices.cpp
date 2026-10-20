#include "app/services/ApplicationServices.h"

#include "app/services/PreferenceDocument.h"

namespace miacode {

SimaiValidationLocale uiValidationLocale()
{
    const QString token = PreferenceDocument::resolvedLanguageToken();
    if (token.startsWith(QStringLiteral("zh"))) {
        return SimaiValidationLocale::Chinese;
    }
    if (token.startsWith(QStringLiteral("ja"))) {
        return SimaiValidationLocale::Japanese;
    }
    return SimaiValidationLocale::English;
}

ApplicationServices::ApplicationServices(QObject* parent)
    : QObject(parent)
    , workspace_(this)
    , files_(workspace_)
    , validationLocale_(uiValidationLocale())
    , analysis_(workspace_, validationLocale_, {}, -1.0, this)
    , editorSync_(this)
    , uiRequests_(this)
    , jobProgress_(this)
    , previewAppearance_(this)
    , shellNotifications_(this)
{
}
}  // namespace miacode
