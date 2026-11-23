// The tracking filters of the pitch detection, for a CallOutBox: attack guard,
// octave-jump lock, jump guard for stray neighbour strings, and the bridge
// (hold time) over short detection gaps. Replaces OctavePanel.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "Widgets.h"

#include <functional>

class TrackingPanel final : public juce::Component
{
public:
    struct Values
    {
        bool  attackOn = true;   float attackMs = 50.0f;     // 0 .. 60
        bool  sustainOn = true;  float sustainMs = 11.0f;    // 0 .. 100
        bool  jumpOn = false;    int   jumpInterval = 7;      // 5 .. 24 semitones
        float jumpWindowMs = 80.0f;                           // 10 .. 200
        float jumpLockMs = 50.0f;                             // 20 .. 150
        float bridgeMs = 30.0f;                               // 0 .. 200
    };

    explicit TrackingPanel (const Values&);

    std::function<void (const Values&)> onChange;   // after every change by the user

    const Values& values() const noexcept   { return current; }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    /** Two-segment switch Aus | An. */
    struct Switch
    {
        Switch (int radioGroup, const juce::String& tip);
        void set (bool on);
        bool isOn() const   { return onButton.getToggleState(); }

        juce::TextButton offButton { "Aus" }, onButton { "An" };
        ButtonRow row;
    };

    void setupSlider (juce::Slider&, double low, double high, double step, double defaultValue, const juce::String& tip);
    void changed();

    Values current;
    Switch attackSwitch, sustainSwitch, jumpSwitch;
    juce::Slider attackSlider, sustainSlider, intervalSlider, windowSlider, lockSlider, bridgeSlider;

    // A CallOutBox is a window of its own, so it brings its own tooltips.
    juce::TooltipWindow tooltips { this, 700 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackingPanel)
};
