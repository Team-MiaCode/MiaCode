#pragma once

#include <QString>

#include "export/video_export/VideoExportController.h"

namespace miacode {

// The export page as the runtime drives it: page entry and exit, the active
// tab and difficulty, the intro lead-in previewed while the page is open, and
// a chart selection staged for export. The QML-facing implementation
// (miacode::ui::ExportSession) is created through the factory the UI installs
// on ApplicationServices before the session is assembled.
class ExportPagePort
{
public:
    virtual ~ExportPagePort() = default;

    virtual void enter(int previousActiveDifficultyId) = 0;
    virtual void leave() = 0;
    virtual void setActiveTab(const QString& tabId) = 0;
    virtual void selectDifficulty(int difficultyId) = 0;
    virtual bool pageSessionActive() const = 0;
    virtual int selectedDifficultyId() const = 0;
    virtual IntroBannerSpec previewIntroSpec() const = 0;
    // PV-preview intro, preview only: the card fades while the PV segment
    // window is being dragged, and the audition repeats while looping is on.
    virtual bool introPvSegmentDragging() const = 0;
    virtual bool introAuditionLoop() const = 0;
    // The audition starts at the segment head after the window moved, and at
    // the picked moment after an explicit seek inside the window.
    virtual bool introAuditionFromHead() const = 0;
    virtual void requestSelectionRangeExport(double startSecond, double endSecond) = 0;

protected:
    ExportPagePort() = default;
    ExportPagePort(const ExportPagePort&) = default;
    ExportPagePort& operator=(const ExportPagePort&) = default;
};

}  // namespace miacode
