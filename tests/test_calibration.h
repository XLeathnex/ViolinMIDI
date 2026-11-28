// ViolinMIDI - tests for the automatic calibration (2.1, core/Calibration.h,
// core/CalibrationReport.h): the protocol, the live counter, every derived
// value on synthetic sessions with injected problems, the harmonizer part, the
// report folder and the determinism of the analysis. The long end-to-end run
// only with --long.
//
// Included by test_main.cpp inside its anonymous namespace (it uses check());
// Calibration.h and CalibrationReport.h are included at its top.
#pragma once

void calibInstrument()
{
    check (vm::calib::kOpenNotes[vm::calib::kMaxStrings - 1] == 76, "the E string is on top");
}

void calibLiveCounter() {}
void calibLevels() {}
void calibOutThreshold() {}
void calibSearch() {}
void calibOctaveGuards() {}
void calibBridge() {}
void calibJumpGuard() {}
void calibLowestNote() {}
void calibDeadZone() {}
void calibSoundDefault() {}
void calibSoundPiezo() {}
void calibMix() {}
void calibDeterminism() {}
void calibReport() {}
void calibSettingsText() {}
void calibFields() {}

/** About 100 s of a synthetic session through the whole analysis: only with --long. */
void calibEndToEnd()
{
    if (! gLongTests)
        info ("skipped; run with --long");
}
