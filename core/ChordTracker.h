// ViolinMIDI core - the chord keyboard (2.1, "Externe Chords").
//
// Follows a MIDI keyboard on every channel: which keys are held (down, or
// kept by the sustain pedal CC 64 >= 64, which counts for every channel), the
// split ("Akkordbereich"), when a strike is complete (a short gather window,
// so a spread chord is one chord and no wrong chord blips out in between),
// the auto-latch, and the key context the chords are heard in. Its output is
// one chord::Context per analysis hop, for OutputStage rule R9.
//
// One per Engine, on the audio thread: fixed arrays only, no allocation.
// Recognition runs only at a commit, never per hop.
#pragma once

#include "Chord.h"
#include "Types.h"

#include <cstdint>

namespace vm
{

class ChordTracker
{
public:
    struct Config
    {
        int  split = 60;             ///< keys below this note belong to the chord
        bool wholeKeyboard = false;  ///< every key belongs to the chord
        bool latch = true;           ///< the last chord stays after all keys and the pedal are up
        int  homeRoot = 0, homeMode = 0;   ///< Settings::keyRoot / scaleMode: where the key context starts
    };

    /** Split, latch and the start key from the engine settings. */
    static Config configFor (const Settings&) noexcept;

    static constexpr double kQuietSec     = 0.012;  ///< a strike is complete when no key came for this long ...
    static constexpr double kMaxGatherSec = 0.035;  ///< ... or this long after its first key
    static constexpr double kShrinkSec    = 0.040;  ///< released keys are looked at again after this

    /** Timing only: chord, key context and keys stay. */
    void prepare (double hopsPerSecond) noexcept;

    /** Keys, chord and key context: the start key again ("Akkord zuruecksetzen"). */
    void reset() noexcept;

    /** Every key and the pedal up (the input went away, the queue overflowed); a latched chord stays. */
    void keysUp() noexcept;

    /** One message from the chord keyboard, applied now: note on/off, CC 64, 120, 121, 123;
        a one-byte 0xFF means keysUp(). Everything else is ignored. */
    void midi (const MidiEvent&) noexcept;

    /** Once per analysis hop, also while the detection is switched off. */
    void advance (const Config&) noexcept;

    const chord::Context& context() const noexcept   { return ctx; }

    /** For the display: bit n of the 128 = key n is held (down or by the pedal). */
    void heldKeys (uint32_t (&words)[4]) const noexcept;

private:
    chord::Context ctx;
};

} // namespace vm
