// The audio harmonizer (the harmony voices made from the violin's own sound),
// the content of its own window "Audio-Harmonizer".
//
// Left: the two curves over the played pitch - how much of the body resonance
// the voices keep (formant correction) and where their low-pass sits (high-cut
// on the wet signal only). Right: drop voicing, the ensemble controls, the
// level of the voices and dry-kill. A line under the heading names the audio
// output; the main switch sits at the top right. While the harmonizer is off
// everything but the main switch is dimmed.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "Curve.h"
#include "CurveEditor.h"
#include "Widgets.h"

#include <array>
#include <functional>

class AudioPanel final : public juce::Component
{
public:
    struct Values
    {
        bool      on = false;
        vm::Curve formant = vm::curve::defaultFormant();   // share of the body kept, 0 .. 1, over the played pitch
        vm::Curve hiCut = vm::curve::defaultHiCut();       // cut-off of the voices' low-pass in Hz, over the played pitch
        bool      dropVoicing = true;
        float     ensemble = 1.0f;          // 0 .. 1, shown as 0 .. 100 %
        float     vibratoCoupling = 1.0f;   // 0 .. 1
        float     stereoSpread = 0.35f;     // 0 .. 1
        float     harmonyGainDb = 0.0f;     // -60 .. +6; -60 is shown as minus infinity
        bool      dryKill = false;
    };

    static constexpr int kWidth = 760, kHeight = 540;

    AudioPanel (const Values&, const juce::String& outputInfo);

    std::function<void (const Values&)> onChange;   // after every change by the user

    const Values& values() const noexcept   { return current; }

    /** One line under the heading, e.g. "Ausgang: MacBook Pro-Lautsprecher", or a problem text (amber). */
    void setOutputInfo (const juce::String& text, bool isProblem);

    /** The pitch being played (fractional MIDI note) for the live line in both
        curves; negative while nothing is played. Repaints only when it moves. */
    void setPlayhead (float midiNote);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    /** Two-segment switch Aus | An, drawn as a status switch. */
    struct Switch
    {
        Switch (int radioGroup, const juce::String& tip);
        void set (bool on);
        bool isOn() const   { return onButton.getToggleState(); }

        juce::TextButton offButton { "Aus" }, onButton { "An" };
        ButtonRow row;
    };

    enum Kind { percent, decibel };

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
    void showEnabledState();
    void changed();

    Values current;
    juce::String outputInfo;
    bool outputIsProblem = false;

    Switch onSwitch, dropSwitch, dryKillSwitch;
    CurveEditor formantEditor, hiCutEditor;
    std::array<Block, 4> blocks;   // ensemble, vibrato coupling, stereo spread, level

    // The panel lives in a window of its own, so it brings its own tooltips.
    juce::TooltipWindow tooltips { this, 700 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPanel)
};
