#pragma once

#include <functional>

namespace miacode {

class EditorPageRouter
{
public:
    virtual ~EditorPageRouter() = default;

    // The selected chart may remain the preview source on the metadata page.
    // Page activation is a separate condition from having that chart loaded.
    virtual bool hasActiveDifficulty() const = 0;
    virtual int activeDifficultyId() const = 0;
    virtual bool difficultyPageActive() const = 0;

    // Each of these returns false when the domain switch cannot be committed.
    // The page host only calls them after its QML leave decision succeeds.
    virtual bool enterDifficultyPage(int difficultyId) = 0;
    virtual bool enterMetadataPage() = 0;
    virtual bool enterLatencyPage() = 0;
    virtual bool enterExportPage() = 0;

    // Closing the final editor view leaves the document loaded while removing
    // its runtime presentation: transport, timeline, SFX and preview scene.
    virtual bool clearEditorPresentation() { return false; }

    // 打包为 ZIP. Routed here rather than through a command bus because it is
    // reached from the page host's own menu entry.
    virtual void packChartAsZip() = 0;

    virtual void openPreferences() = 0;

    // "May the window close?" — the same unsaved-changes question the page
    // switches ask, at window scope. The continuation runs once; declining and
    // dismissing are the same answer.
    virtual void requestShellClose(std::function<void(bool)> onDecided) = 0;

protected:
    EditorPageRouter() = default;
    EditorPageRouter(const EditorPageRouter&) = default;
    EditorPageRouter& operator=(const EditorPageRouter&) = default;
};

}  // namespace miacode
