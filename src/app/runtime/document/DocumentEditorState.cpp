#include "app/runtime/document/DocumentSessionHost.h"
#include "app/runtime/Shared.h"

#include "common/CrashRecovery.h"
#include "common/DebugLog.h"
#include "preview/runtime/PreviewRuntime.h"

#include <QtCore>
#include <QtGui>

using namespace miacode::runtime::shared;

namespace {

QString cursorSummary(const QTextCursor& cursor)
{
    return QStringLiteral(
        "anchor=%1 pos=%2 has_selection=%3 sel_start=%4 sel_end=%5 sel_len=%6")
        .arg(cursor.anchor())
        .arg(cursor.position())
        .arg(cursor.hasSelection() ? 1 : 0)
        .arg(cursor.selectionStart())
        .arg(cursor.selectionEnd())
        .arg(qAbs(cursor.position() - cursor.anchor()));
}

void logSelectionRestore(const QString& scope, const QString& payload)
{
    miacode::debug_log::appendLine(
        miacode::debug_log::Channel::Runtime,
        QStringLiteral("selection_restore/%1").arg(scope),
        payload,
        true
    );
}

}  // namespace

void miacode::runtime::DocumentSessionHost::updatePauseButtonAppearance()
{
    // The play/pause presentation changed, and the QML transport's button reads
    // the same condition this one does — playing_ OR the export
    // intro's lead-in. setPreviewPlayingFlag announces the first; nothing
    // announced the second, so the intro played with the transport still
    // showing a play button. The notification is queued for the same reason
    // that writer is: callers reach here from the middle of a transition.
    QMetaObject::invokeMethod(
        &session_, [this]() { emit session_.presentationChanged(); }, Qt::QueuedConnection);
}

void miacode::runtime::DocumentSessionHost::updateDirtyState()
{
    const bool dirty = state_.documentDirty_;
    if (dirty) {
        if (state_.autosaveDirtySinceMs_ < 0) {
            state_.autosaveDirtySinceMs_ = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();
        }
        if (ui_.autosaveTimer_ != nullptr && !ui_.autosaveTimer_->isActive()) {
            ui_.autosaveTimer_->start();
        }
    } else {
        state_.autosaveDirtySinceMs_ = -1;
        if (ui_.autosaveTimer_ != nullptr) {
            ui_.autosaveTimer_->stop();
        }
        if (ui_.autosaveIdleTimer_ != nullptr) {
            ui_.autosaveIdleTimer_->stop();
        }
    }
    session_.updateWindowTitle();
}

quint64 miacode::runtime::DocumentSessionHost::appliedWorkspaceRevision() const
{
    return session_.appliedQmlWorkspaceRevision_;
}

void miacode::runtime::DocumentSessionHost::noteDocumentEditedForAutosave()
{
    if (session_.applicationServices_.workspace().isNetPreview()) return;
    if (ui_.autosaveIdleTimer_ != nullptr) {
        ui_.autosaveIdleTimer_->start();
    }
    // Crash-time autosave — push the current document text into the
    // crash-handler's snapshot mailbox so an abnormal exit (segfault,
    // abort, std::terminate) within the next ~milliseconds still
    // produces a recovery file. Cheap: bounded memcpy + atomic store,
    // no disk I/O. The 2-second debounced .bak write above is for
    // routine autosave; this is the per-edit safety net.
    if (!state_.currentFilePath_.isEmpty()) {
        miacode::crash_recovery::updateSnapshot(
            state_.currentFilePath_,
            currentDocumentTextForAutosave());
    }
}

void miacode::runtime::DocumentSessionHost::clearDeletedDifficultyUndoState()
{
    state_.deletedDifficultyUndoState_ = Session::DeletedDifficultyUndoState{};
}

bool miacode::runtime::DocumentSessionHost::undoDeletedDifficultyField()
{
    if (!state_.deletedDifficultyUndoState_.valid) {
        return false;
    }

    const Session::DeletedDifficultyUndoState deletedState = state_.deletedDifficultyUndoState_;
    if (!SimaiDocument::isDifficultyId(deletedState.difficultyId)) {
        clearDeletedDifficultyUndoState();
        return false;
    }
    if (session_.applicationServices_.workspace().document().difficulty(deletedState.difficultyId) != nullptr) {
        return false;
    }

    session_.stopQtPreviewPlayback(true);
    miacode::ChartWorkspace& workspace = session_.applicationServices_.workspace();
    if (!workspace.addDifficulty(deletedState.difficultyId)) {
        return false;
    }
    workspace.replaceDifficultyChart(
        deletedState.difficultyId, deletedState.difficultyData.chart);
    workspace.updateDifficultyField(
        deletedState.difficultyId,
        miacode::ChartWorkspaceDifficultyField::Level,
        deletedState.difficultyData.level);
    // Under the unified mode the snapshot's designer may be a name the shared
    // one has since moved past. Restoring it verbatim would broadcast that
    // stale name across every difficulty, so the current &des wins.
    const QString designer = workspace.unifiedDesignerEnabled()
        ? workspace.document().designer
        : deletedState.difficultyData.designer;
    workspace.updateDifficultyField(
        deletedState.difficultyId,
        miacode::ChartWorkspaceDifficultyField::Designer,
        designer);
    state_.validationCacheByDifficulty_.remove(deletedState.difficultyId);
    state_.documentDirty_ = true;
    updateDirtyState();

    const bool shouldActivateRestoredDifficulty = deletedState.wasActive || !session_.hasActiveDifficulty();
    if (shouldActivateRestoredDifficulty) {
        state_.activeOutlineKey_ = QStringLiteral("chart");
        if (!switchToDifficultyField(deletedState.difficultyId)) {
            return false;
        }
    }

    clearDeletedDifficultyUndoState();
    if (state_.currentFilePath_.isEmpty()) {
        return true;
    }
    if (!saveToPath(state_.currentFilePath_)) {
        return true;
    }
    return true;
}
