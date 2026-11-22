// The live display (note, tuning, pitch over time) and the input meter.
// Each repaints only itself and only while something changes.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "AudioHost.h"

#include <array>
#include <vector>

/** One display frame, taken from the audio thread's meters. */
struct LiveFrame
{
    float pitch = 0.0f;        ///< detected, fractional MIDI note
    float outputPitch = 0.0f;  ///< what the instrument is asked to play
    float levelDb = -120.0f;
    int   note = -1;           ///< sounding MIDI note, -1 if none
    bool  heard = false;       ///< a clear pitch above the gate
    bool  active = false;      ///< a note is being sent
    bool  held = false;        ///< ...kept by the pentatonic sustain
    bool  enabled = true;      ///< master switch
    int   triadCount = 0;      ///< notes on the harmony channel
    int   triad[vm::scale::kMaxVoices] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
    int   frozenNote = -1;     ///< the note kept by the solo freeze or -1
    bool  harmonyFrozen = false;
};

juce::String noteName (int midiNote);

//==============================================================================
/** Buttons side by side as one segmented control. Each segment is as wide as
    its text needs, the rest is shared evenly. */
class ButtonRow final : public juce::Component,
                        private juce::Button::Listener
{
public:
    ~ButtonRow() override;

    void add (juce::Button& b);
    void resized() override;
    /** The track, the plate under the chosen segment and the hairlines between the others. */
    void paint (juce::Graphics&) override;

private:
    void buttonClicked (juce::Button*) override {}
    void buttonStateChanged (juce::Button*) override   { repaint(); }

    std::vector<juce::Component::SafePointer<juce::Button>> items;
};

//==============================================================================
class LiveDisplay final : public juce::Component
{
public:
    LiveDisplay();
    void push (const LiveFrame&, float a4Hz);
    void paint (juce::Graphics&) override;

    static constexpr int kFrames = 180;   // 6 s at 30 frames per second
    static constexpr int kNoteWidth = 250; // the left part, with the note

private:
    void paintNote (juce::Graphics&, juce::Rectangle<float>);
    void paintHistory (juce::Graphics&, juce::Rectangle<float>);

    std::array<LiveFrame, kFrames> frames {};
    int head = 0;
    int idleFrames = kFrames;
    float centre = 67.0f;      // note in the middle of the history, follows the playing
    float shownCents = 0.0f;
    float a4 = 440.0f;
};

//==============================================================================
class LevelMeter final : public juce::Component,
                         public juce::SettableTooltipClient
{
public:
    void update (float levelDb, float gateDb, float gateOffDb, bool clipped);
    void paint (juce::Graphics&) override;

private:
    float shown = -120.0f, gate = -45.0f, gateOff = -51.0f;
    bool clip = false;
};
