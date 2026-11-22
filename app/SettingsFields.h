// Every engine setting with its name on disk, in one table: the settings file
// and the presets are both written from it, so they cannot drift apart.
//
// Adding a setting: one line in the table in SettingsFields.cpp.
#pragma once

#include <juce_data_structures/juce_data_structures.h>

#include "Types.h"

namespace fields
{
    /** A JSON object with every setting that belongs in a preset
        (everything except the master switch and the freeze buttons). */
    juce::var toVar (const vm::Settings&);

    /** Takes over every known key of a JSON object written by toVar(). Keys
        that are missing or unknown are skipped, so `into` keeps its value for
        them; values are clamped to their valid range. */
    void fromVar (const juce::var& object, vm::Settings& into);

    /** The same for the settings file shared by all versions. */
    void load (juce::PropertiesFile&, vm::Settings& into);
    void save (juce::PropertiesFile&, const vm::Settings&);
}
