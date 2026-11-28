// The "CHORD FREEZE" pill over the live display (2.1): visible while the
// harmony is frozen, whatever froze it (button, MIDI, nod, the scroll flick),
// and flashing for a moment when a flick switched it.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class FreezeBadge final : public juce::Component,
                          public juce::SettableTooltipClient,
                          private juce::Timer
{
public:
    static constexpr int kWidth = 156, kHeight = 26;

    FreezeBadge();

    /** Shown or not; `flash`: a flick switched it (the border flashes for 400 ms). */
    void setOn (bool on, bool flash);
    bool isOn() const noexcept   { return on; }

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    bool on = false;
};
