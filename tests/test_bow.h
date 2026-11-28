// ViolinMIDI - tests for the bow hand on the camera side (2.1, app/CameraBow.h):
// announced windows around the reversals of synthetic strokes at 15 and 30
// frames per second, no window without bowing, threshold, debounce, direction,
// and the one-word cue for the audio thread.
//
// Included by test_main.cpp inside its anonymous namespace (it uses check());
// CameraBow.h is included at its top.
#pragma once

void cameraBowHand()
{
    check (! cam::BowTracker::Window().open(), "an empty window is closed");
}
