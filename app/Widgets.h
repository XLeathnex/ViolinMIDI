// The live displays: note and tuner, the pitch history and the input meter.
// Each repaints only itself and only when something changed.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "AudioHost.h"

#include <array>

/** One display frame, taken from the audio thread's meters. */
struct LiveFrame
{
    float pitch = 0.0f;        ///< detected, fractional MIDI note
    float outputPitch = 0.0f;  ///< what the instrument is asked to play
    float levelDb = -120.0f;
    int   note = -1;           ///< sounding MIDI note, -1 if none
    bool  heard = false;       ///< a clear pitch above the gate
    bool  active = false;      ///< a note is being sent
};

juce::String noteName (int midiNote);

//==============================================================================
class NoteDisplay final : public juce::Component
{
public:
    NoteDisplay();
    void update (const LiveFrame&, float a4Hz);
    void paint (juce::Graphics&) override;

private:
    LiveFrame frame;
    float a4 = 440.0f;
    float shownCents = 0.0f;
};

//==============================================================================
class PitchHistory final : public juce::Component
{
public:
    PitchHistory();
    void push (const LiveFrame&);
    void clear();
    void paint (juce::Graphics&) override;

    static constexpr int kFrames = 180;   // 6 s at 30 frames per second

private:
    std::array<LiveFrame, kFrames> frames {};
    int head = 0;
    float centre = 67.0f;    // note in the middle of the view, follows the playing
    int idleFrames = kFrames;
};

//==============================================================================
class LevelMeter final : public juce::Component,
                         public juce::SettableTooltipClient
{
public:
    void update (float levelDb, float gateDb, bool clipped);
    void paint (juce::Graphics&) override;

private:
    float shown = -120.0f, gate = -45.0f;
    bool clip = false;
};
