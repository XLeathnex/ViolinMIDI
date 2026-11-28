// The window of the automatic calibration (see CalibrationWizard.h).
#include "CalibrationWizard.h"

CalibrationWizard::CalibrationWizard (CalibrationSession& s) : session (s)
{
    setSize (kWidth, kHeight);
}

CalibrationWizard::~CalibrationWizard()
{
    session.onChange = nullptr;
}

void CalibrationWizard::paint (juce::Graphics&) {}

void CalibrationWizard::resized() {}

bool CalibrationWizard::keyPressed (const juce::KeyPress&)
{
    return false;
}
