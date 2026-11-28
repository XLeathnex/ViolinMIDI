// The live display (note, tuning, pitch and loudness over time) and the input meter.
// Each repaints only itself and only while something changes.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "AudioHost.h"

#include <array>
#include <functional>
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

    // 2.1
    float levelMinDb = -120.0f, levelMaxDb = -120.0f;   ///< lowest / highest level since the previous frame
    char  chord[24] = {};      ///< chord symbol (UTF-8, chord::symbol), empty = none
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
/** The dominant element of the main window. Left: the note, a tuning scale and
    what goes out. Right: the pitch history over the note grid and, in the same
    time grid below it, the loudness with the In and Out thresholds. A small
    zoom strip in the top left corner of the pitch graph changes the time and
    pitch span together. */
class LiveDisplay final : public juce::Component,
                          public juce::SettableTooltipClient
{
public:
    LiveDisplay();

    /** One frame from the meters, 30 times a second. The thresholds are the
        engine's In and Out levels in dB; they move the lines in the loudness curve. */
    void push (const LiveFrame&, float a4Hz, float gateDb, float gateOffDb);

    /** A free area at the top left of the pitch graph, under the zoom strip, in
        this component's coordinates, for a translucent overlay (the camera HUD).
        It lies over the older history and never covers "now" or the zoom strip. */
    juce::Rectangle<int> overlayArea() const;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Explains the part under the mouse: the note, the pitch graph or the loudness curve. */
    juce::String getTooltip() override;

    /** 2.1: the zoom of both axes, steps into layout::kTimeSteps / kHeightSteps (from the settings,
        without onZoom). */
    void setZoom (int timeStep, int heightStep);
    /** 2.1: after a click in the zoom strip, to remember it. */
    std::function<void (int timeStep, int heightStep)> onZoom;

    static constexpr int kFramesPerSecond = 30;
    static constexpr int kFrames = 24 * kFramesPerSecond + 1; // the longest zoom step, 24 s
    static constexpr int kNoteWidth = 250;                     // the left part, with the note

private:
    /** Where everything goes; derived from the size alone. */
    struct Layout
    {
        juce::Rectangle<float> note;      // the left part, inside its padding
        juce::Rectangle<float> pitch;     // the pitch history
        juce::Rectangle<float> level;     // the loudness curve, same width and time grid
        juce::Rectangle<float> labels;    // the column left of both, for note names and dB marks
        juce::Rectangle<float> timeRow;   // the time marks under both
    };

    Layout layout() const;
    const LiveFrame& frameAt (int age) const   { return frames[(size_t) ((head - age + kFrames) % kFrames)]; }
    int spanFrames() const;
    float spanSemitones() const;
    void setZoom (int step);

    void paintNote (juce::Graphics&, juce::Rectangle<float>);
    void paintTimeGrid (juce::Graphics&, const Layout&);
    void paintPitch (juce::Graphics&, const Layout&);
    void paintLevel (juce::Graphics&, const Layout&);
    void paintWatermark (juce::Graphics&, juce::Rectangle<float>);

    std::vector<LiveFrame> frames;
    int head = 0;
    int idleFrames = kFrames + 1;  // frames in a row with nothing to show, at most kFrames + 1
    int outsideFrames = 0;     // frames in a row the pitch was near or past the edge of the view
    float centre = 67.0f;      // note in the middle of the history, follows the playing
    float shownCents = 0.0f;
    float a4 = 440.0f;
    float gateIn = -45.0f, gateOut = -51.0f;
    int zoom = 2;              // index into the zoom steps; 2 is the standard view

    juce::TextButton zoomOutButton, zoomInButton, zoomResetButton;

    // The emblem behind the grid, drawn once per size and screen scale.
    juce::Image watermark;
    juce::Rectangle<float> watermarkArea;
    float watermarkScale = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LiveDisplay)
};

//==============================================================================
/** 2.1: a button that paints an icon (four corner arrows: into fullscreen, or out of it), on the
    plate of a TextButton. */
class IconButton final : public juce::Button
{
public:
    enum class Icon { enterFullscreen, exitFullscreen };

    explicit IconButton (const juce::String& name = {});
    void setIcon (Icon);
    Icon icon() const noexcept   { return shown; }

    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    Icon shown = Icon::enterFullscreen;
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
