#pragma once

// Per-chart 片头 settings stored in the chart's project preferences
// (<chartDir>/.miacode/preferences.json). The PV-preview start belongs to the
// song, not to a difficulty or to the dialog: every difficulty of the chart and
// every batch item reads the value its own project last stored.

#include <QString>

namespace miacode::export_intro_preferences {

// Chart second the PV-preview segment starts from; 0 when the chart never
// stored one.
double pvPreviewStartSeconds(const QString& chartFilePath);

bool savePvPreviewStartSeconds(const QString& chartFilePath, double seconds);

}  // namespace miacode::export_intro_preferences
