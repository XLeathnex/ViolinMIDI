// ViolinMIDI core - automatic calibration (2.1, "Automatisches Einmessen").
//
// The 7-step protocol (one recording, a "take", per string and step), the
// progress of a take while it is played, and the analysis: every take is run
// offline through the same vm::Engine the app uses, the detection values, the
// camera dead zone and the harmonizer's curve points and level are derived
// from what comes out, and the result says before and after, with findings
// for the Profimodus report. Nothing here touches the audio thread; the app
// (app/CalibrationSession) captures the input through SpscRings.
//
// The implementation is split: CalibrationProtocol.cpp (instrument, protocol,
// goals, LiveCounter), CalibrationAnalysis.cpp (passes, reference, metrics,
// detection values, dead zone, orchestration), CalibrationSound.cpp (the
// harmonizer part), CalibrationText.cpp (German texts; English developer
// notes). CalibrationReport.h writes and reads the Profimodus folder.
//
// Plain C++17, deterministic: the same inputs give the same result for any
// number of threads. Compiles without warnings under the app's strict flags.
#pragma once

#include "Curve.h"
#include "Engine.h"
#include "Profiles.h"
#include "SpscRing.h"
#include "Types.h"

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace vm
{
namespace calib
{

//==============================================================================
// Instrument and protocol (1.1, 1.2)

constexpr int kMaxStrings = 7;
constexpr int kOpenNotes[kMaxStrings] = { 34, 41, 48, 55, 62, 69, 76 };   ///< B-flat 1, F2, C3, G3, D4, A4, E5

/** The voices together this far under the violin (4.4.6). */
constexpr float kMixTargetDb = -3.0f;

struct Instrument
{
    int strings = 4;
    int open[kMaxStrings] = {};                  ///< ascending, open[0] = lowest, open[strings - 1] = 76

    /** 4 + (lowest <= 49) + (lowest <= 42) + (lowest <= 35) strings in fifths, E on top. */
    static Instrument fromLowestNote (int lowestNote) noexcept;
    /** 4 .. 7 strings. */
    static Instrument withStrings (int count) noexcept;
    int lowest() const noexcept   { return open[0]; }
};

enum class Step : int { silence = 0, openLong = 1, openShort, openStaccato, runLegato, runDetache, runStaccato, eHigh };
constexpr int kSteps = 7;   ///< the protocol's steps; the quiet take comes before step 1

/** One take. `string` indexes Instrument::open (-1 for the quiet take); `variant` is step 7's
    0/1/2 = legato / detache / staccato. */
struct TakeId
{
    Step step = Step::silence;
    int  string = -1;
    int  variant = 0;
};

/** The quiet take, then step 1 for every string (lowest first), step 2 ..., step 7 with three
    variants on the E string: 1 + 6 * strings + 3 takes. */
std::vector<TakeId> protocol (const Instrument&);

/** When a take is complete. */
struct Goal
{
    float lowNote = 0, highNote = 0;   ///< expected pitch range
    int   notes = 0;                   ///< strokes or notes needed
    float sustainSec = 0;              ///< steady tone needed (step 1, the quiet take)
    float minNoteSec = 0, maxNoteSec = 0;
    bool  separated = false;
    float maxTakeSec = 90;
};
Goal goalFor (const TakeId&, const Instrument&) noexcept;

/** File stem: "s0-ruhe", "s1-g", "s7-e-legato". */
std::string takeName (const TakeId&, const Instrument&);

/** BlockStamp lives in SpscRing.h (AudioHost pushes it): vm::calib::BlockStamp. */

/** One recording. */
struct Take
{
    TakeId id;
    double sampleRate = 48000;
    std::vector<float> audio;          ///< raw input before the gain; the first `preroll` samples are warm-up
    int preroll = 0;
    long long firstSample = 0, lostSamples = 0;
    bool skipped = false, interrupted = false;
    std::vector<Engine::TraceRow> liveHops;   ///< Profimodus
    std::vector<BlockStamp> blocks;           ///< Profimodus
};

/** One camera frame while a take runs (from PadTracker::lastOffset). */
struct CameraSample
{
    double timeSec = 0;
    int    take = -1;
    bool   valid = false;
    float  dx = 0, dy = 0;
    bool   flick = false;
};

//==============================================================================
// Progress of a take while it is played (2.5)

struct Progress
{
    float fraction = 0;       ///< 0..1 of the goal
    int   notes = 0, needed = 0;
    float seconds = 0, sustainSec = 0;
    float levelDb0 = -120;    ///< gain 0, as the analysis measures
    float heardPitch = -1;    ///< the note being heard, fractional; for "Gehoert: G3 -4 ct"
    int   wrongNotes = 0;     ///< notes outside the expected range
    bool  tooQuiet = false;   ///< voiced but less than 20 dB above N0
    bool  clipped = false;    ///< raw |x| >= 0.98
    bool  playedInSilence = false;
    bool  done = false;
    float quietSec = 0;       ///< time since the level fell to within 10 dB of N0
};

/** Counts a take against its goal with an Engine of its own on fixed probe settings. The capture
    thread pushes the raw input; the message thread reads a copy of the progress (the caller
    guards it). */
class LiveCounter
{
public:
    LiveCounter();
    ~LiveCounter();

    /** A new take. `noiseFloorDb0` is N0 from the quiet take (-120 until it is measured). */
    void begin (const TakeId&, const Instrument&, double sampleRate, float noiseFloorDb0);
    /** Raw input before the gain. Capture thread. */
    void push (const float* samples, int count);
    const Progress& progress() const noexcept   { return state; }

    /** The probe settings: gain 0, In N0 + 15 (-60 before N0), Out In - 6, default guards,
        jump guard off, bridge 30 ms, lowest note one below the instrument's, triad, key filter
        and harmonizer off. */
    static Settings probeSettings (const Instrument&, float noiseFloorDb0) noexcept;

private:
    Progress state;
};

//==============================================================================
// The values the calibration sets

/** The 13 detection values and the dead zone: the instrument profile's values (profiles A.2). */
using DetectionValues = ProfileValues;

struct SoundValues
{
    Curve formant = curve::defaultFormant(), hiCut = curve::defaultHiCut();
    float harmonyGainDb = 0;
    static SoundValues of (const Settings&) noexcept;
    void applyTo (Settings&) const noexcept;
};

/** The 16 engine fields the calibration sets (13 detection, 3 sound), from `from` into `into`. */
void copyCalibratedFields (const Settings& from, Settings& into) noexcept;

//==============================================================================
// The result

enum class Limit   { none, low, high };
enum class Verdict { ok, values, setup, parameter, program };

struct Change
{
    std::string key, label, unit;      ///< key: the settings-file key ("formantCurve@62", "padDeadZone")
    double before = 0, after = 0, wanted = 0;
    Limit limit = Limit::none;
    bool measured = false, changed = false;
    std::string reason;
};

struct Metrics
{
    int notes = 0, missed = 0, octaveErrors = 0, wrongNotes = 0, glissBlips = 0, dropouts = 0, spurious = 0,
        missedRetriggers = 0;
    float latencyMedianMs = -1, latencyP90Ms = -1, hangMedianMs = -1, hangP90Ms = -1;
};

struct SoundZone
{
    int anchor = 55; float lo = 0, hi = 0; bool measured = false, changed = false;
    float shrillDb = 0, thinDb = 0, muddyDb = 0, dullDb = 0;      ///< at the values after
    float shrillBefore = 0, thinBefore = 0, muddyBefore = 0;
    float kBefore = 0, kAfter = 0, cutBefore = 0, cutAfter = 0; Limit kLimit = Limit::none, cutLimit = Limit::none;
    float coverage = 0;                                            ///< share of voice weight with a dry reference
    float clickShare = -1, clickP90 = -1, voicePitchP90Cents = -1, voiceLatencyMs = -1;   ///< Profimodus
};

struct StringReport
{
    int open = 0; bool played = false;
    float loudDb0 = 0, softDb0 = 0, tuningCents = 0, settleMs = 0, octaveSqueakMs = 0, releaseMs = 0,
          bridgeGapP95Ms = 0, wetMinusDryDb = 0;
    Metrics before, after;
};

struct Finding
{
    std::string id, area;              ///< area: detection, camera, sound, setup
    Verdict verdict = Verdict::ok;
    int severity = 1;                  ///< 1 info, 2 warning, 3 problem
    std::string text, dev, evidenceJson;   ///< German for the player, English for the developer
};

struct CameraResult { int samples = 0; float zeroX = 0, zeroY = 0, p50 = 0, p95 = 0; };

struct Result
{
    bool ok = false, cancelled = false; std::string error;
    DetectionValues detectionBefore, detectionAfter; bool deadZoneMeasured = false;
    SoundValues soundBefore, soundAfter; bool soundMeasured = false; int soundVoices = 0; int soundVoicing = -1;
    float noiseFloorDb0 = -120;
    std::vector<Change> changes;
    Metrics before, after;
    std::vector<StringReport> strings;
    std::vector<SoundZone> zones;
    std::vector<Finding> findings;
    CameraResult camera;
    float detacheMedianMs = -1, detacheP90Ms = -1;
};

struct Inputs
{
    Instrument instrument;
    std::vector<Take> takes;
    std::vector<CameraSample> camera;
    Settings before;                  ///< engine settings when the wizard started (= what is saved)
    float deadZoneBefore = 0.06f;
    bool  cameraRan = false, measureSound = true, professional = false;
};

/** Probe, candidate and render settings derived from `before`: everything the calibration does not
    measure is neutral - key filter, pentaFilter, harmonySource FixedKey, both freezes off
    (critique 2.9). */
Settings neutralised (const Settings& before) noexcept;

class Analyzer
{
public:
    struct Control
    {
        std::atomic<float> progress { 0 };   ///< 0..1
        std::atomic<int>   phase { 0 };      ///< 0 detection, 1 harmonizer, 2 report
        std::atomic<bool>  cancel { false };
    };

    /** Deterministic: the same inputs give the same result for any thread count (1..6). */
    static Result run (const Inputs&, Control&, int threads);
};

//==============================================================================
// Pieces of the analysis, public for the tests

struct RefNote { int note = 0; double tOn = 0, tOff = 0; float sustainDb0 = 0; bool separated = false, folded = false; int take = -1; };
struct MeasureRow { double t = 0; float levelDb0 = 0, pitch = 0, clarity = 0, hintClarity = 0; bool voiced = false, doubt = false; };

std::vector<MeasureRow> measurePass (const Take&, const Settings& before);
std::vector<RefNote> referenceNotes (const std::vector<MeasureRow>&, const Take&, const Instrument&, float noiseDb0);
Metrics evaluate (const Take&, const std::vector<RefNote>&, const Settings& candidate, double* cpuSec = nullptr);

/** Ascending search: the smallest candidate with the fewest errors, then `marginSteps` further
    up. Stops after two error-free candidates in a row. `limit` = high when errors remain at the
    last candidate. */
float pickSmallestMinimal (const std::vector<float>& candidates, const std::function<int (float)>& errorsFor,
                           int marginSteps, Limit& limit, int* evaluated = nullptr);

/** The camera dead zone from the samples of the takes (4.3.9): 0.03 .. 0.25, rounded up to 0.01. */
float deadZoneFrom (const std::vector<CameraSample>&, const std::vector<Take>&, int& samples, float& p50, float& p95, Limit&);

//==============================================================================
// Texts (CalibrationText.cpp): German for the wizard and the report, UTF-8

std::string stringName (int openNote);               ///< "G-Saite", "B-Saite" (= B-flat)
std::string noteText (float pitch, bool withCents);   ///< "G3", "G3 -4 ct" (with a real minus sign)
std::string stepTitle (Step);                         ///< "Leersaiten lang"
std::string stepTarget (Step);                        ///< the spec's "Messziel", word for word
std::string instruction (const TakeId&, const Instrument&);
/** The hint under the progress during a take, first match wins (2.5); empty if none. */
std::string hintText (const Progress&, const TakeId&, const Instrument&);
std::string verdictName (Verdict);                    ///< "ok", "values", "setup", "parameter", "program"

} // namespace calib
} // namespace vm
