#pragma once

#include "common/PreferenceProvider.h"

namespace miacode::runtime {

// Production language provider. Persisted settings stay in the app; libraries
// receive render/task values explicitly.
class PreferenceDocumentProvider final : public miacode::preferences::PreferenceProvider
{
public:
    QString resolvedLanguageToken() const override;
};

// Installs the process-wide PreferenceDocumentProvider. The GUI entry and the
// video export entries (CLI export and export worker) call it before any
// library code reads preferences.
void installPreferenceDocumentProvider();

}  // namespace miacode::runtime
