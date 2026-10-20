#include "app/services/EditorCompletionPreferences.h"

#include <QJsonArray>

namespace miacode {
namespace {
constexpr auto kDurations = "editor_completion_duration_candidates";
constexpr auto kDivisions = "editor_completion_division_candidates";

QStringList readValues(const QJsonValue& json, editor::CompletionCandidateGroup group)
{
    QStringList values;
    for (const QJsonValue& value : json.toArray())
        if (value.isString()) values.append(value.toString());
    return editor::normalizeCompletionValues(group, values);
}
} // namespace

editor::SimaiCompletionConfig readEditorCompletionCandidates(const QJsonObject& ui)
{
    return {readValues(ui.value(QLatin1String(kDurations)), editor::CompletionCandidateGroup::Duration),
            readValues(ui.value(QLatin1String(kDivisions)), editor::CompletionCandidateGroup::Division)};
}

void writeEditorCompletionCandidates(QJsonObject& ui, const editor::SimaiCompletionConfig& config)
{
    ui.insert(QLatin1String(kDurations), QJsonArray::fromStringList(config.durations));
    ui.insert(QLatin1String(kDivisions), QJsonArray::fromStringList(config.divisions));
}
} // namespace miacode
