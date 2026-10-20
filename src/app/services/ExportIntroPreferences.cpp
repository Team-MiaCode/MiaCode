#include "app/services/ExportIntroPreferences.h"

#include "app/services/ProjectPreferences.h"

#include <QJsonObject>
#include <QtGlobal>

#include <cmath>

namespace miacode::export_intro_preferences {
namespace {
constexpr auto kPvPreviewStartKey = "export_intro_pv_start_seconds";
}

double pvPreviewStartSeconds(const QString& chartFilePath)
{
    const double stored = miacode::project_preferences::load(chartFilePath)
                              .value(QLatin1String(kPvPreviewStartKey))
                              .toDouble(0.0);
    return std::isfinite(stored) ? qMax(0.0, stored) : 0.0;
}

bool savePvPreviewStartSeconds(const QString& chartFilePath, double seconds)
{
    if (chartFilePath.isEmpty() || !std::isfinite(seconds)) {
        return false;
    }
    QJsonObject preferences = miacode::project_preferences::load(chartFilePath);
    preferences.insert(QLatin1String(kPvPreviewStartKey), qMax(0.0, seconds));
    return miacode::project_preferences::save(chartFilePath, preferences);
}

}  // namespace miacode::export_intro_preferences
