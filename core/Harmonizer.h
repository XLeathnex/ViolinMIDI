// ViolinMIDI core - the harmony as sound.
//
// The harmony voices that go out as MIDI can also be played with the violin's
// own tone: this class shifts the pitch of the incoming sound, one copy per
// voice, and mixes the copies (and, unless told otherwise, the violin itself)
// to a stereo output.
//
// The violin plays one note at a time and the engine already knows its period,
// so the shifting works on single periods of the tone rather than on a
// spectrum of the whole signal. That keeps the delay to about two periods and
// lets the resonance of the body stay where it is while the pitch moves.
//
// Plain C++17 without dependencies, like the rest of core/. Real-time safe
// after prepare(): process() and setFrame() do not allocate, lock or log.
#pragma once

#include "Scale.h"
#include "Types.h"

#include <memory>

namespace vm
{

class Harmonizer
{
public:
    static constexpr int kMaxVoices = scale::kMaxVoices;

    /** What is known at one analysis hop (about every 1.3 ms). */
    struct Frame
    {
        bool  sounding = false;       ///< the violin plays a note whose pitch is known
        float periodSamples = 0.0f;   ///< its period in input samples, valid while sounding
        int   voiceCount = 0;
        float semitones[kMaxVoices] = {};  ///< each voice's distance from the tone being played right now, fractional
        bool  frozen = false;         ///< "harmony freeze": keep playing the voices from the tone as it was when
                                      ///< this became true, whatever comes in; `semitones` then refer to that tone
    };

    Harmonizer();
    ~Harmonizer();

    /** Allocates everything. `sampleRate` is the rate of the audio interface. */
    void prepare (double sampleRate);

    /** Silence, nothing remembered. */
    void reset() noexcept;

    /** The state from now on. Called between process() calls, on the same thread. */
    void setFrame (const Frame&) noexcept;

    /** `input` is the violin (mono). `left`/`right` receive the output and must
        not overlap the input. Any number of samples.

        Uses from Settings: bodyKeep, humanize, stereoSpread, hiCutHz,
        harmonyGainDb, dryKill, overlapMs, releaseMs. (audioHarmony and
        vibratoCoupling are the caller's business: it decides whether to call
        this at all and where the voices lie.)

        Output = the violin unchanged and undelayed in the centre (left out
        with dryKill) + the voices. */
    void process (const float* input, float* left, float* right, int numSamples, const Settings&) noexcept;

    /** How far the voices lag behind the input at the given period, in samples. */
    int latencySamples (float periodSamples) const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace vm
