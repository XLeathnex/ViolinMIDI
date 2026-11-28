// "ViolinMIDI --layout-check" (2.1, also the first part of --selftest): lays out
// the real main window in every configuration that matters - the window with
// Feste Tonart and with Externe Chords, fullscreen on the common Mac screens
// with and without the docked camera panel, both pages where the columns are
// paged - and checks that nothing lies outside its parent, nothing overlaps,
// every text fits, and the display keeps its 75 / 25 split. Settings and presets
// live in a temporary folder; the user's files are never touched.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace layoutcheck
{
    struct Problem
    {
        juce::String configuration, problem;
    };

    /** Message thread. Empty if every configuration passes. */
    juce::Array<Problem> run();
}
