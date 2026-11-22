// Two protections against octave errors, for a CallOutBox: the attack guard
// (waits briefly at the start of a bow stroke for the lower fundamental) and
// the octave-jump lock (filters short octave outliers during a held note).
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "Widgets.h"

#include <functional>

class OctavePanel final : public juce::Component
{
public:
    struct Values
    {
        bool  attackOn = true;   float attackMs = 50.0f;    // 0 .. 60
        bool  sustainOn = true;  float sustainMs = 11.0f;   // 0 .. 100
    };

    explicit OctavePanel (const Values&);

    std::function<void (const Values&)> onChange;   // after every change by the user

    const Values& values() const noexcept   { return current; }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    /** Name line, then switch and slider. */
    struct Block
    {
        Block (int radioGroup, const juce::String& name, double maxMs, double defaultMs, const juce::String& tip);

        void setValues (bool on, double ms);

        juce::String label;
        juce::TextButton offButton { "Aus" }, onButton { "An" };
        ButtonRow row;
        juce::Slider slider;
    };

    void changed();
    void paintBlock (juce::Graphics&, const Block&, int top);

    Values current;
    Block attack, sustain;

    // A CallOutBox is a window of its own, so it brings its own tooltips.
    juce::TooltipWindow tooltips { this, 700 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OctavePanel)
};
