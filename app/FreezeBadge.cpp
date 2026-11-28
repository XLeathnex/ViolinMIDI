// The "CHORD FREEZE" pill over the live display (see FreezeBadge.h).
#include "FreezeBadge.h"

FreezeBadge::FreezeBadge()
{
    setInterceptsMouseClicks (false, false);
    setVisible (false);
}

void FreezeBadge::setOn (bool shouldBeOn, bool)
{
    on = shouldBeOn;
}

void FreezeBadge::paint (juce::Graphics&) {}

void FreezeBadge::timerCallback()
{
    stopTimer();
}
