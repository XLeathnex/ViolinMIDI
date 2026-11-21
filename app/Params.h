// Every setting that can be changed from outside the window, in one list:
// the MIDI mapping table shows exactly these, in this order.
//
// Adding a parameter: one entry in Params.cpp. It then appears in the mapping
// table and can be learned; nothing else has to change.
#pragma once

#include <juce_core/juce_core.h>

#include "SettingsStore.h"

#include <functional>
#include <vector>

struct Param
{
    enum class Kind
    {
        Toggle,   ///< on / off
        Choice,   ///< one of count options, value 0 .. count - 1
        Range,    ///< a number from low to high
        Trigger   ///< an action, no value ("Alle Noten aus")
    };

    juce::String id;      ///< never changes: used in the settings file
    juce::String name;    ///< shown in the mapping table
    Kind kind = Kind::Toggle;
    double low = 0.0, high = 1.0;   ///< Choice: 0 and count - 1
    std::function<double (const AppSettings&)> get;
    std::function<void (AppSettings&, double)> set;
};

namespace params
{
    const std::vector<Param>& all();

    /** Index into all() or -1. */
    int indexOf (const juce::String& id);

    /** Applies an incoming MIDI controller value (0..127) or a note-on (its
        velocity, 1..127) to one parameter.

        Controller: Toggle on from 64; Choice spreads 0..127 over the options;
                    Range spreads 0..127 over low..high; Trigger fires from 64.
        Note-on:    Toggle flips; Choice moves to the next option, wrapping;
                    Range takes the velocity like a controller value; Trigger fires.

        Returns true if `settings` changed or, for a Trigger, if it fired. */
    bool applyMidi (const Param&, AppSettings& settings, bool isNote, int value);
}
