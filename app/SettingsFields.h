// The engine settings in the settings file and in the presets. The table of
// every setting with its name on disk is in core/SettingsText.cpp (shared with
// the text form of the calibration report and the tools); this file reads and
// writes it with JUCE, with the special cases of older versions.
//
// Adding a setting: one line in the table in core/SettingsText.cpp.
#pragma once

#include <juce_data_structures/juce_data_structures.h>

#include "SettingsText.h"
#include "Types.h"

#include <initializer_list>

namespace fields
{
    /** Which settings toVar() writes. */
    enum class Part
    {
        all,          ///< everything that belongs in a preset file
        presetImage   ///< without the instrument profile's and the wiring's keys (what a preset load changes, 2.1)
    };

    /** A JSON object with every setting that belongs in a preset
        (everything except the master switch and the freeze buttons). */
    juce::var toVar (const vm::Settings&, Part = Part::all);

    /** Takes over every known key of a JSON object written by toVar(). Keys
        that are missing or unknown are skipped, so `into` keeps its value for
        them; values are clamped to their valid range. */
    void fromVar (const juce::var& object, vm::Settings& into);

    /** The same for the settings file shared by all versions. */
    void load (juce::PropertiesFile&, vm::Settings& into);
    void save (juce::PropertiesFile&, const vm::Settings&);

    /** The keys of the instrument profile's 13 settings, in table order. */
    juce::StringArray profileKeys();

    /** Some keys of toVar(), in the presets' own format (for PresetStore::patch). Unknown keys are left out. */
    juce::NamedValueSet subset (const vm::Settings&, std::initializer_list<const char*> keys);
}
