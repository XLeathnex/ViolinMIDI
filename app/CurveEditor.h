// An editor for a value that follows the played pitch (vm::Curve, "key
// tracking") over the four strings of the violin, G3 to E6.
//
// It draws the curve exactly as the audio thread computes it (curve::value),
// the string zones with their note names, a node per point and, while a tone
// is played, a crimson line at its pitch. The mouse sets, moves and deletes
// points; notes snap to semitones so a point lands exactly on a string. The
// curve it hands out is always curve::sanitised, so whatever the editor does,
// the harmonizer gets a valid curve.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "Curve.h"

#include <functional>

class CurveEditor final : public juce::Component,
                          public juce::SettableTooltipClient
{
public:
    /** `yLow` .. `yHigh`: the value range. `logarithmic`: the value axis and the
        interpolation are logarithmic (frequencies). `formatValue` turns a value
        into its text with unit ("20 %", "3,5 kHz"), for the readouts and the axis. */
    CurveEditor (const juce::String& title, float yLow, float yHigh, bool logarithmic,
                 std::function<juce::String (float)> formatValue);

    /** Shows a curve (sanitised). Does not call onChange. */
    void setCurve (const vm::Curve&);
    const vm::Curve& getCurve() const noexcept   { return curve; }

    /** Shows a small "Standard" button in the header that restores this curve;
        the context menu offers the same. It is also what an invalid curve
        falls back to, and what the editor shows until setCurve is called. */
    void setDefaultCurve (const vm::Curve&, const juce::String& buttonTooltip);

    /** The pitch being played now (fractional MIDI note), negative for none.
        Draws a crimson line and a dot where the curve is read. Cheap to call
        at the display rate: it repaints only when the pitch moves. */
    void setPlayhead (float midiNote);

    /** After every change by the user, also while a point is dragged. */
    std::function<void (const vm::Curve&)> onChange;

    /** How the mouse works, in German, for a tooltip that says what the curve does first. */
    static juce::String mouseHelp();

    /** The header line (title, live value, "Standard") above the graph. */
    static constexpr int kHeaderHeight = 26;
    static constexpr int kMinHeight = 150;

    void paint (juce::Graphics&) override;
    void resized() override;
    void enablementChanged() override;

    void mouseMove (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> graphArea() const;
    juce::Rectangle<float> plotArea() const;

    float xForNote (float note) const;
    float yForValue (float value) const;
    float noteAtX (float x) const;
    float valueAtY (float y) const;
    juce::Point<float> nodePosition (int index) const;

    /** The node under `pos` (nearest within the hit radius), or -1. */
    int nodeAt (juce::Point<float> pos) const;

    /** A click on free space: a new point there, or, on a note that has one,
        that point moved there. The index of the point to drag on, or -1. */
    int addPoint (juce::Point<float> pos);
    void movePoint (int index, juce::Point<float> pos);
    void removePoint (int index);
    void restoreDefault();
    void showMenu (int node);

    /** Sanitises the curve, repaints and tells onChange. */
    void changed();
    void updateHover (juce::Point<float> pos);

    juce::String readout (float note, float value) const;

    void paintGrid (juce::Graphics&, juce::Rectangle<float> graph, juce::Rectangle<float> plot) const;
    void paintCurve (juce::Graphics&, juce::Rectangle<float> plot) const;
    void paintTag (juce::Graphics&, juce::Rectangle<float> graph, juce::Point<float> at, const juce::String&) const;

    juce::String titleText;
    float low, high;
    bool logScale;
    std::function<juce::String (float)> format;

    vm::Curve curve, fallback, standard;
    bool hasStandard = false;
    bool curveGiven = false;   // setCurve was called
    juce::TextButton standardButton { "Standard" };

    float playhead = -1.0f;

    int hoverNode = -1, dragNode = -1;
    bool hoverInside = false;
    bool pressedOnNode = false;   // the first click of a (double) click hit a point
    juce::Point<float> hoverPos, dragOffset;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CurveEditor)
};
