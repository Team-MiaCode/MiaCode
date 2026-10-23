#pragma once

#include <QVector>

#include "core/analysis/MuriTypes.h"

namespace miacode::muri {

struct MuriPanelEntry {
    bool isStatic = false;
    MuriKind kind = MuriKind::Overlap;
    MuriAlertLevel alertLevel = MuriAlertLevel::Muri;
    double second = 0.0;
    double occurrenceSecond = 0.0;
    int line = 1;
    int col = 1;
    QString rawDetail;
    MuriDetailKind detailKind = MuriDetailKind::None;
    MuriDetailArgs detailArgs;
};

struct MuriPanelEntrySource {
    bool isStatic = false;
    int index = 0;
    MuriKind kind = MuriKind::Overlap;
    MuriAlertLevel alertLevel = MuriAlertLevel::Muri;
    double second = 0.0;
    int line = 1;
    int col = 1;
};

QVector<MuriPanelEntrySource> selectVisibleMuriPanelEntries(
    const MuriAnalysisReport& analysisReport,
    const QVector<MuriStaticReference>& staticReferences);
MuriPanelEntry resolveMuriPanelEntry(
    const MuriPanelEntrySource& source,
    const MuriAnalysisReport& analysisReport,
    const QVector<MuriStaticReference>& staticReferences);

QVector<MuriPanelEntry> buildVisibleMuriPanelEntries(
    const MuriAnalysisReport& analysisReport,
    const QVector<MuriStaticReference>& staticReferences);

}  // namespace miacode::muri
