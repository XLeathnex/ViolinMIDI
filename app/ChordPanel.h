// The callout of "Externe Chords" (2.1, the "..." next to "Harmonie-Quelle"):
// the MIDI input of the chord keyboard, the chord range (split note with
// "Lernen", or the whole keyboard), auto-latch, Hybrid B's lock time, what is
// recognised right now with the held keys on a small keyboard, and
// "Akkord zuruecksetzen". It changes nothing itself: every change goes out
// through onChange, live data comes in through setLive.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <cstdint>
#include <functional>

class ChordPanel final : public juce::Component
{
public:
    static constexpr int kWidth = 380, kHeight = 500;

    /** The settings it shows and changes. */
    struct Values
    {
        juce::String input;              ///< chosen MIDI input, "" = none
        juce::StringArray inputs;        ///< what can be chosen
        bool  inputOpen = false;
        bool  sameAsRemote = false;      ///< the same device is chosen for the MIDI remote control (a warning)
        int   split = 60;                ///< 24 .. 96
        bool  wholeKeyboard = false;
        bool  latch = true;
        float hybridLockMs = 330.0f;     ///< 150 .. 1000
        bool  hybridB = false;           ///< the lock time applies (chord mode Hybrid B)
    };

    /** What the engine recognises right now (AudioHost::Meters, 30 times a second). */
    struct Live
    {
        int  root = -1, quality = 0, bass = -1, scaleRoot = 0, scaleType = 0, keyTonic = 0;
        bool active = false;
        uint32_t keys[4] = {};           ///< held keys, bit n = key n
        unsigned int messages = 0;       ///< ChordInput::messagesReceived
    };

    explicit ChordPanel (const Values&);

    /** Values that changed elsewhere (a preset, a foot switch, the device list), without onChange. */
    void setValues (const Values&);
    /** Repaints only what changed. */
    void setLive (const Live&);
    /** "Lernen" is waiting for a key: the button says so ("Taste druecken ..."). */
    void setLearning (bool);

    std::function<void (const Values&)> onChange;
    /** "Lernen" was clicked (again: cancel, onLearn with the panel no longer learning). */
    std::function<void()> onLearn;
    /** "Akkord zuruecksetzen". */
    std::function<void()> onReset;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    Values current;
};
