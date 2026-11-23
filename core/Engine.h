// ViolinMIDI core - audio in, MIDI out.
//
// Engine::process() is real-time safe after prepare(): no allocation, no locks,
// provided the output list has enough capacity reserved (see kMaxEventsPerHop).
#pragma once

#include "Biquad.h"
#include "Harmonizer.h"
#include "NoteTracker.h"
#include "OutputStage.h"
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
        int   note        = -1;     ///< MIDI note being sent or -1
        bool  voiced      = false;
        bool  active      = false;  ///< a note is being sent
        bool  held        = false;  ///< ...kept by the pentatonic sustain
        int   triadCount  = 0;      ///< notes sounding on the harmony channel
        int   triad[scale::kMaxVoices] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
        int   frozenNote  = -1;     ///< the note kept by the solo freeze or -1
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
    static constexpr int kMaxEventsPerHop = 10;
    /** After the output stage (key filter, harmony, freeze, MPE). */
    static constexpr int kMaxOutputEventsPerHop = OutputStage::maxEventsFor (kMaxEventsPerHop) + 20;

    /** "All notes off" (controller 123) on every channel these settings send
        notes on. For the panic button and the master switch. */
    static void appendAllNotesOff (const Settings& settings, int sampleOffset, MidiEventList& out);

    void prepare (double sampleRate);

    /** Clears the audio history without sending MIDI. */
    void reset();

    /** Processes one block of mono input. Events are appended to `out`.

        `left` and `right` (both or neither) receive the sound output: the
        harmony voices played with the violin's own tone (Settings::audioHarmony),
        silence otherwise. They must not overlap the input. */
    void process (const float* input, int numSamples, const Settings& settings, MidiEventList& out,
                  float* left = nullptr, float* right = nullptr);

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
    static Settings effective (const Settings& s) noexcept;
    void updateHarmonizerFrame (const Settings& s) noexcept;
    void silenceOutput (const Settings& s, int offset, MidiEventList& out);

    double fsIn = 48000.0, fs = 48000.0;
    int decim = 1, decimPhase = 0;
    int hop = 64, hopFill = 0;
    long long hopCount = 0;

    Biquad lowpassA, lowpassB, highpass;
    std::vector<float> hopBuffer;

    PitchDetector detector;
    NoteTracker tracker;
    OutputStage stage;

    // the harmony as sound
    Harmonizer harmonizer;
    Harmonizer::Frame frame;         // what the harmonizer is told at the next hop boundary
    bool  harmonizerOn = false;
    bool  frameFrozen = false;
    float frozenPitch = 60.0f;       // the played pitch when the harmony was frozen
    int   lastRoot = -1;             // the last root the harmony was built on
    MidiEventList hopEvents;      // what the tracker produced in one hop
    Settings lastSettings;
    bool wasEnabled = true;
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
