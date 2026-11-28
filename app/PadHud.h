// A small translucent XY display for the camera's scroll pad: a crosshair with
// a crimson cursor where the two values stand, and their percentages below.
//
// It is only there while it means something: it fades in as soon as the
// scroll leaves the dead zone and starts moving the values, stays while it
// does, and fades out a moment after the violin is back at rest. On stage the
// player glances at it while sweeping and never has to look past it otherwise.
// Used over the preview in the camera window and over the live display in the
// main window.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class PadHud final : public juce::Component,
                     public juce::SettableTooltipClient,
                     private juce::Timer
{
public:
    PadHud();

    /** The two pad values 0..1 (x to the right, y upwards) and whether the hand
        is outside the dead zone right now. Called with every camera frame. */
    void setState (float x, float y, bool active);

    /** What the two axes control, e.g. "CC 1", "Harmonie-Mix", "aus". */
    void setAxisNames (const juce::String& x, const juce::String& y);

    /** The size the HUD is made for. With less room it shrinks to fit; with
        more it stays this size in the top right corner of its bounds. */
    static constexpr int kWidth = 120, kHeight = 140;

    void paint (juce::Graphics&) override;

    /** Only the box takes the mouse (for its tooltip), and only while it can be seen. */
    bool hitTest (int x, int y) override;

private:
    void timerCallback() override;

    /** Runs the timer only while there is something to do: one call at the end
        of the pause before the fade-out, and steady ticks while it fades. */
    void scheduleFade (double now);

    /** The rounded box and the pad square inside it, in local coordinates. */
    juce::Rectangle<float> boxArea() const;
    static juce::Rectangle<float> padArea (juce::Rectangle<float> box);

    void repaintBox();

    float valueX = 0.5f, valueY = 0.5f;
    bool  isActive = false;
    float level = 0.0f;           // 0 hidden .. 1 fully shown, linear in time; drawn eased
    double releasedAt = 0.0;      // ms (hi-res counter) when the hand came back to rest
    double lastTick = 0.0;        // ms (hi-res counter) of the last fade step
    bool  intercepting = false;
    juce::String nameX { "aus" }, nameY { "aus" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PadHud)
};
