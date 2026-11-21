// ViolinMIDI core - audio in, MIDI out.
//
// Engine::process() is real-time safe after prepare(): no allocation, no locks,
// provided the output list has enough capacity reserved (see kMaxEventsPerHop).
#pragma once

#include "Biquad.h"
#include "NoteTracker.h"
#include "PitchDetector.h"
#include "Types.h"

#include <vector>

namespace vm
{

class Engine
{
public:
    /** What the user interface shows. */
    struct Meter
    {
        float levelDb     = -120.0f;
        float clarity     = 0.0f;
        float pitch       = 0.0f;   ///< detected pitch, fractional MIDI note
        float outputPitch = 0.0f;   ///< pitch the instrument is asked to play
        int   note        = -1;     ///< sounding MIDI note or -1
        bool  voiced      = false;
        bool  active      = false;
    };

    /** One row per analysis hop, for tests and offline analysis only. */
    struct TraceRow
    {
        double timeSec;
        bool   voiced;
        float  pitch, clarity, levelDb;
        bool   active;
        int    note;
    };

    /** The tracker emits at most this many events per analysis hop. */
    static constexpr int kMaxEventsPerHop = 8;

    void prepare (double sampleRate);

    /** Clears the audio history without sending MIDI. */
    void reset();

    /** Processes one block of mono input. Events are appended to `out`. */
    void process (const float* input, int numSamples, const Settings& settings, MidiEventList& out);

    /** Ends the sounding note and returns the controllers to neutral. */
    void shutdown (MidiEventList& out);

    const Meter& meter() const noexcept       { return currentMeter; }
    double internalSampleRate() const noexcept { return fs; }
    int    hopSize() const noexcept            { return hop; }
    int    decimation() const noexcept         { return decim; }

    /** Upper bound for the number of events one process() call can produce. */
    int maxEventsFor (int numSamples) const noexcept;

    void setTrace (std::vector<TraceRow>* destination) noexcept { trace = destination; }

private:
    void configure (const Settings& s);
    void analyseHop (const Settings& s, int offset, MidiEventList& out);

    double fsIn = 48000.0, fs = 48000.0;
    int decim = 1, decimPhase = 0;
    int hop = 64, hopFill = 0;
    long long hopCount = 0;

    Biquad lowpassA, lowpassB, highpass;
    std::vector<float> hopBuffer;

    PitchDetector detector;
    NoteTracker tracker;
    Meter currentMeter;
    std::vector<TraceRow>* trace = nullptr;

    // configuration cache
    bool   configured = false;
    int    cfgLowest = -1, cfgHighest = -1;
    float  cfgA4 = 0.0f, cfgGainDb = 1.0e9f;
    Response cfgResponse = Response::Normal;
    double gain = 1.0;
};

} // namespace vm
