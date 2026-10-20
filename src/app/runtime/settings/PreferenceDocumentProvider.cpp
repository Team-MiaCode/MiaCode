#include "app/runtime/settings/PreferenceDocumentProvider.h"

#include "app/services/PreferenceDocument.h"
#include "app/runtime/settings/EncoderProbeCache.h"

namespace miacode::runtime {

QString PreferenceDocumentProvider::resolvedLanguageToken() const
{
    return PreferenceDocument::resolvedLanguageToken();
}

void installPreferenceDocumentProvider()
{
    static PreferenceDocumentProvider provider;
    miacode::video_export::installEncoderProbeCache(
        std::make_shared<SettingsEncoderProbeCache>());
    miacode::preferences::installPreferenceProvider(&provider);
}

}  // namespace miacode::runtime
