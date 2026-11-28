// ViolinMIDI core - the chord keyboard (see ChordTracker.h).
#include "ChordTracker.h"

namespace vm
{

ChordTracker::Config ChordTracker::configFor (const Settings& s) noexcept
{
    Config c;
    c.split = s.chordSplit;
    c.wholeKeyboard = s.chordWholeKeyboard;
    c.latch = s.chordLatch;
    c.homeRoot = s.keyRoot;
    c.homeMode = s.scaleMode;
    return c;
}

void ChordTracker::prepare (double) noexcept {}

void ChordTracker::reset() noexcept
{
    ctx = chord::Context();
}

void ChordTracker::keysUp() noexcept {}

void ChordTracker::midi (const MidiEvent&) noexcept {}

void ChordTracker::advance (const Config&) noexcept {}

void ChordTracker::heldKeys (uint32_t (&words)[4]) const noexcept
{
    for (auto& w : words)
        w = 0;
}

} // namespace vm
