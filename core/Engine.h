// ViolinMIDI core - audio in, MIDI out.
//
// Engine::process() is real-time safe after prepare(): no allocation, no locks,
// provided the output list has enough capacity reserved (see kMaxEventsPerHop).
#pragma once

#include "Biquad.h"
#include "ChordTracker.h"
#include "Harmonizer.h"
#include "NoteTracker.h"
#include "OutputStage.h"
#include "PitchDetector.h"
#include "SpscRing.h"
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

        // 2.1: the chord in force (external chords), also while the detection is off
        int   chordRoot = -1, chordQuality = 0, chordBass = -1;   ///< chord::Chord; root -1 = none
        int   chordScaleRoot = 0, chordScaleType = 0, keyTonic = 0;
        bool  chordActive = false;  ///< a chord was committed since the last reset
        uint32_t chordKeys[4] = {}; ///< held keys of the chord keyboard, bit n = key n (display)
        // 2.1: new strokes the camera's bow cue helped with (NoteTracker B3/B4); counts up
        unsigned bowStrokes = 0;
    };

    /** One row per analysis hop, for tests, offline analysis and the calibration's telemetry
        (2.1). Plain data: it travels through an SpscRing. */
    struct TraceRow
    {
        double timeSec = 0.0;
        bool   voiced = false;
        float  pitch = 0.0f, clarity = 0.0f, levelDb = -120.0f;
        bool   active = false;
        int    note = -1;
        // 2.1
        long long hop = 0;           ///< hops since prepare()
        float  hintClarity = 0.0f;   ///< NoteTracker::Input::hintClarity
        bool   octaveDoubt = false;  ///< NoteTracker::Input::octaveDoubt
        int    stageNote = -1;       ///< the main note after the output stage, -1 = none
        float  outputPitch = 0.0f;   ///< the pitch the instrument is asked to play
    };

    /** The tracker emits at most this many events per analysis hop. */
    static constexpr int kMaxEventsPerHop = 10;

    /** A harmony freeze armed before the note starts looping after this much live tone (or at the note's end). */
    static constexpr double kFreezeLeadInSec = 0.25;

    /** After the output stage (key filter, harmony, freeze, MPE). */
    static constexpr int kMaxOutputEventsPerHop = OutputStage::maxEventsFor (kMaxEventsPerHop) + 20;

    /** "All notes off" (controller 123) on every channel these settings send
        notes on. For the panic button and the master switch. */
    static void appendAllNotesOff (const Settings& settings, int sampleOffset, MidiEventList& out);

    /** Drop voicing (Settings::dropVoicing), for the audio harmonizer: in a
        chord of three notes or more (the violin and at least two voices) whose
        root is A4 or higher, the highest voice above the violin sounds an
        octave lower - a viola or cello register under the melody instead of a
        cluster high above it. Not if it would land on the violin itself or on
        another voice. */
    static void applyDropVoicing (Harmonizer::Frame& frame, int root, const Settings& settings) noexcept;

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
    /** What the audio harmonizer was told at the last hop (for tests). */
    const Harmonizer::Frame& harmonizerFrame() const noexcept { return frame; }
    int    decimation() const noexcept         { return decim; }

    /** Upper bound for the number of events one process() call can produce. */
    int maxEventsFor (int numSamples) const noexcept;

    void setTrace (std::vector<TraceRow>* destination) noexcept { trace = destination; }

    //==========================================================================
    // 2.1, all on the thread that calls process(), before it

    /** MIDI from the chord keyboard (any channel), applied now (ChordTracker::midi). */
    void chordInput (const MidiEvent* events, int count) noexcept;
    /** Every key and the pedal up (the input went away); a latched chord stays. */
    void chordKeysUp() noexcept        { chords.keysUp(); }
    /** Forget keys, chord and key context; the start key applies again. */
    void chordReset() noexcept         { chords.reset(); }
    const ChordTracker& chordTracker() const noexcept   { return chords; }

    /** The calibration's live telemetry: one TraceRow per hop into this ring, or none (null).
        Wait-free; a row that does not fit is counted in telemetryDrops(). */
    void setTelemetry (SpscRing<TraceRow>* ring) noexcept   { telemetry = ring; }
    unsigned telemetryDrops() const noexcept                { return telemetryDropped; }
    /** Analysis hops since prepare(). */
    long long hops() const noexcept                          { return hopCount; }

    /** The camera announces a change of bow now (NoteTracker::Input::bowCue, with
        Settings::bowAssist); it stays until changed. */
    void setBowCue (bool announced) noexcept                 { bowCue = announced; }

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
    int   liveHops = 0;              // hops the harmonizer has been sent live voices in a row
    MidiEventList hopEvents;      // what the tracker produced in one hop
    Settings lastSettings;
    bool wasEnabled = true;
    Meter currentMeter;
    std::vector<TraceRow>* trace = nullptr;

    // 2.1
    ChordTracker chords;
    SpscRing<TraceRow>* telemetry = nullptr;
    unsigned telemetryDropped = 0;
    bool bowCue = false;

    // configuration cache
    bool   configured = false;
    int    cfgLowest = -1, cfgHighest = -1;
    float  cfgA4 = 0.0f, cfgGainDb = 1.0e9f;
    Response cfgResponse = Response::Normal;
    double gain = 1.0;
};

} // namespace vm
