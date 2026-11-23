// The audio harmonizer (the harmony voices made from the violin's own sound),
// for a CallOutBox: the on/off switch, six sliders for the sound of the voices
// and the dry-kill switch. A line under the heading names the audio output.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "Widgets.h"

#include <array>
#include <functional>

class AudioPanel final : public juce::Component
{
public:
    struct Values
    {
        bool  on = false;
        float bodyKeep = 1.0f;         // 0 .. 1, shown as 0 .. 100 %
        float humanize = 0.2f;         // 0 .. 1
        float vibratoCoupling = 1.0f;  // 0 .. 1
        float stereoSpread = 0.5f;     // 0 .. 1
        float hiCutHz = 10000.0f;      // 4000 .. 16000
        float harmonyGainDb = 0.0f;    // -60 .. +6; -60 is shown as minus infinity
        bool  dryKill = false;
    };

    AudioPanel (const Values&, const juce::String& outputInfo);

    std::function<void (const Values&)> onChange;   // after every change by the user

    const Values& values() const noexcept   { return current; }

    /** One line under the heading, e.g. "Ausgang: MacBook Pro-Lautsprecher", or a problem text. */
    void setOutputInfo (const juce::String& text, bool isProblem);

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

    enum Kind { percent, kilohertz, decibel };

    /** Name line (value on the right), slider below. */
    struct Block
    {
        juce::String label;
        Kind kind = percent;
        juce::Slider slider;
    };

    void setupBlock (Block&, const juce::String& label, Kind, double low, double high, double step,
                     double defaultValue, const juce::String& tip);
    juce::String valueText (const Block&) const;
    void changed();

    Values current;
    juce::String outputInfo;
    bool outputIsProblem = false;

    Switch onSwitch, dryKillSwitch;
    std::array<Block, 6> blocks;   // body, humanize, vibrato, spread, hi-cut, level

    // A CallOutBox is a window of its own, so it brings its own tooltips.
    juce::TooltipWindow tooltips { this, 700 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPanel)
};
