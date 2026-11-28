// The bow hand (2.1, "Bogenhand-Erkennung"). The camera sees a change of bow
// too late to trigger it (60 - 120 ms of pipeline latency plus a frame or two),
// so it announces one instead: while the bow hand is in the end zone of its
// typical stroke, or slowing down towards a predicted turn, it publishes a time
// window. Inside it the note tracker takes a softer dip in the level as a new
// stroke (NoteTracker rules B1 - B7); the timing stays the audio's.
//
// Plain C++ without JUCE, fixed-size state only. Runs on the camera queue; the
// window reaches the audio thread as one 64-bit word (packCue / cueContains).
#pragma once

#include "CameraLogic.h"

#include <cstdint>

namespace cam
{

class BowTracker
{
public:
    struct Settings
    {
        bool  violinRightHand = false;   ///< = PadTracker::Settings::rightHand; the bow is in the other hand
        float threshold = 0.25f;         ///< shortest stroke that counts, shoulder widths, 0.1 .. 1
        float debounceMs = 180.0f;       ///< shortest time between two changes of bow, 80 .. 600
    };

    /** When a softer dip counts as a new stroke, on the Pose clock (seconds). */
    struct Window
    {
        double from = 0.0, until = 0.0;   ///< empty unless until > from
        bool open() const noexcept   { return until > from; }
    };

    /** One frame (Pose::timeSec = capture, Pose::readySec = result known). Returns the window in
        which a softer level dip counts as a new stroke; empty when none is announced. */
    Window update (const Pose&, const Settings&) noexcept;

    const BowReport& report() const noexcept   { return last; }
    void reset() noexcept;

private:
    BowReport last;
};

/** The window as one word for the audio thread: [from ms : 32 | until ms : 32] on the millisecond
    clock (wraps every 49 days; compared wrap-safe). 0 = no window; longer than 5 s is no window
    either. */
uint64_t packCue (const BowTracker::Window&) noexcept;

/** Real-time safe: integer arithmetic only. `nowSec` on the same clock as the window. */
bool cueContains (uint64_t word, double nowSec) noexcept;

} // namespace cam
