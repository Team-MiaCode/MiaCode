#pragma once

#include <QObject>

namespace miacode {

class ShellNotifications final : public QObject
{
    Q_OBJECT

public:
    explicit ShellNotifications(QObject* parent = nullptr);

signals:
    // Discrete shell state (tab visibility, playback flags, geometry) changed
    // and every projection should re-read. Replaced the v1 polling timer.
    void presentationChanged();
    // The preview playhead advanced. Separate from presentationChanged because
    // it fires per frame during playback and nothing else should re-read that
    // often.
    void previewPlayheadChanged();
    void previewSkinDirectoryChanged();
    // A preview render setting (brightness, scale, flow speed, HUD toggles) was
    // written. Preview Settings and the export page both write that one live
    // state, so each re-reads it on this rather than keeping the copy it read
    // when it opened.
    void previewRenderSettingsChanged();

    // The document was replaced by a path outside the QML façade — startup,
    // root chart drop, native File/Open, crash recovery.
    void documentReplaced();

    void editorPreferencesChanged();
    void muriPromptPreferenceChanged();
    void videoExportWorkerRunningChanged(bool running);
    void videoExportCompleted();

    // Menu / shortcut entry points that land on the window and have to reach
    // whichever QML surface owns the action.
    void normalizeWholeChartRequested();
    void mediaToolsRequested();
    void preferencesRequested();
    void coverExportRequested(int difficultyId);
    // The active editor's selection was exported to a range and the export
    // page should now become the visible surface. ExportSession already
    // holds the pending range by the time this fires.
    void selectionRangeExportPageRequested();
};

}  // namespace miacode
