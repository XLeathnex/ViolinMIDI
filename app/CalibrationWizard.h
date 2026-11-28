// The window of the automatic calibration (2.1, "Einmessen", 760 x 560): the
// intro (mode, strings), one page per step with the strings, the instruction,
// the level and the progress of the take, the analysis, and the result with
// before and after, live A/B, "Zuruecksetzen", "Speichern" and "Abbrechen".
//
// It only shows the session and passes the clicks on; the session decides.
// Its own window, so it brings its own tooltips.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "CalibrationSession.h"

class CalibrationWizard final : public juce::Component
{
public:
    static constexpr int kWidth = 760, kHeight = 560;

    explicit CalibrationWizard (CalibrationSession&);
    ~CalibrationWizard() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    /** Enter = "Weiter" on a step page. */
    bool keyPressed (const juce::KeyPress&) override;

private:
    CalibrationSession& session;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CalibrationWizard)
};
