#pragma once

#include <QString>

namespace miacode::preferences {

// Narrow language port for library-generated timeline labels. Font/render and
// export settings are explicit values; app owns their persistence. The installed
// provider must outlive its consumers; production entries install a static owner.
class PreferenceProvider
{
public:
    virtual ~PreferenceProvider() = default;

    // Effective UI language token ("en_US", "zh_CN", "ja_JP", ...).
    virtual QString resolvedLanguageToken() const = 0;

protected:
    PreferenceProvider() = default;
    PreferenceProvider(const PreferenceProvider&) = default;
    PreferenceProvider& operator=(const PreferenceProvider&) = default;
};

// Installs the process-wide provider (not owned). Call once from each process
// entry point before library code runs; nullptr uninstalls.
void installPreferenceProvider(PreferenceProvider* provider);
PreferenceProvider* preferenceProvider();

// An unconfigured host has no language override.
QString resolvedLanguageToken();

}  // namespace miacode::preferences
