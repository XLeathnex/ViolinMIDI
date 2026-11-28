// The callout of "Externe Chords" (see ChordPanel.h).
#include "ChordPanel.h"

ChordPanel::ChordPanel (const Values& v) : current (v)
{
    setSize (kWidth, kHeight);
}

void ChordPanel::setValues (const Values& v)
{
    current = v;
}

void ChordPanel::setLive (const Live&) {}

void ChordPanel::setLearning (bool) {}

void ChordPanel::paint (juce::Graphics&) {}

void ChordPanel::resized() {}
