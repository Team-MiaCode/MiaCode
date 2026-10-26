#pragma once

#include <scintillaquick/scintillaquick_item.h>
#include <QPointF>
#include <QtMath>

namespace miacode::ui {

inline void prepareEditorContextMenu(ScintillaQuick_item& editor, const QPointF& position)
{
    editor.forceActiveFocus(Qt::MouseFocusReason);
    // Context commands keep their target even when invoked outside the selection.
    if (!editor.send(SCI_GETSELECTIONEMPTY)) return;
    const auto byte = editor.send(SCI_POSITIONFROMPOINTCLOSE, qRound(position.x()), qRound(position.y()));
    if (byte >= 0) editor.send(SCI_SETEMPTYSELECTION, byte);
}

} // namespace miacode::ui
