// The instrument profile in the main window (2.1, "Eingang" item 1 "Instrument"):
// a box with the profiles and a "..." menu - new, rename, delete, calibrate, and
// adopting the values another version changed. The three dialogs keep their
// edits until their confirming button ("Abbrechen" changes nothing).
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "Profiles.h"

#include <functional>
#include <utility>
#include <vector>

class ProfileBar final : public juce::Component
{
public:
    static constexpr int kMenuWidth = 30, kGap = 6;

    ProfileBar();

    /** After every change of the list or the active profile. `canAdopt`: the menu offers
        "Werte aus Version <adoptVersion> uebernehmen". */
    void show (const vm::ProfileList&, bool canAdopt, const juce::String& adoptVersion);

    std::function<void (int id)> onSelect;
    std::function<void (const juce::String& name, bool factoryValues)> onCreate;
    std::function<void (int id, const juce::String& name)> onRename;
    std::function<void (int id)> onRemove;
    std::function<void()> onCalibrate, onAdopt;

    void resized() override;

private:
    juce::ComboBox box;
    juce::TextButton menuButton;
};
