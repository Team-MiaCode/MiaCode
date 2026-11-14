#include "app/services/EditorCompletionPreferences.h"

#include <QJsonArray>

namespace miacode {
namespace {
constexpr auto kDurations = "editor_completion_duration_candidates";
constexpr auto kDivisions = "editor_completion_division_candidates";
constexpr auto kDurationSuffixes = "editor_completion_duration_suffix_candidates";

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
    // Duration continuation is built in; older suffix presets use the current default.
    return {readValues(ui.value(QLatin1String(kDurations)), editor::CompletionCandidateGroup::Duration),
            readValues(ui.value(QLatin1String(kDivisions)), editor::CompletionCandidateGroup::Division),
            editor::defaultCompletionValues(editor::CompletionCandidateGroup::DurationSuffix)};
}

void writeEditorCompletionCandidates(QJsonObject& ui, const editor::SimaiCompletionConfig& config)
{
    ui.insert(QLatin1String(kDurations), QJsonArray::fromStringList(config.durations));
    ui.insert(QLatin1String(kDivisions), QJsonArray::fromStringList(config.divisions));
    ui.insert(QLatin1String(kDurationSuffixes), QJsonArray::fromStringList(
        editor::defaultCompletionValues(editor::CompletionCandidateGroup::DurationSuffix)));
}
} // namespace miacode
