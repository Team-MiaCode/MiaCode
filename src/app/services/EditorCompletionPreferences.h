#pragma once

#include "editor/SimaiCompletionCatalog.h"
#include <QJsonObject>

namespace miacode {

// The UI section's serialization boundary; editor_core only receives values.
editor::SimaiCompletionConfig readEditorCompletionCandidates(const QJsonObject& ui);
void writeEditorCompletionCandidates(QJsonObject& ui, const editor::SimaiCompletionConfig& config);

} // namespace miacode
