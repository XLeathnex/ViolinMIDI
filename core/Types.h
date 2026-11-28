// ViolinMIDI core - shared types.
// Plain C++17, no dependencies, so the same code runs in the app and in the tests.
#pragma once

#include "Curve.h"
#include "Scale.h"

#include <cstdint>
#include <vector>

namespace vm
{

/** How pitch movement inside a note is sent to the instrument. */
enum class BendMode : int
{
    Continuous = 0, ///< pitch bend follows the string exactly (slides and vibrato)
    Smoothed   = 1, ///< slides are sent, vibrato is filtered out
    Quantized  = 2  ///< no pitch bend, every semitone reached becomes a new note
};

/** How bow loudness is sent to the instrument. */
enum class DynamicsMode : int
{
    Continuous   = 0, ///< velocity plus running expression (CC11)
    VelocityOnly = 1, ///< velocity from the level at the start of the note
    Fixed        = 2  ///< every note with the same velocity
};

/** Trade-off between response time and certainty. */
enum class Response : int
{
    Fast   = 0,
    Normal = 1,
    Safe   = 2
};

/** How the harmony voices are derived from the played note (see Scale.h). */
enum class HarmonyMode : int
{
    Diatonic  = 0, ///< counted in steps of the key: major, minor or diminished as the degree demands
    Chromatic = 1  ///< always the same semitone distances
};

/** Which controller carries the bow loudness (DynamicsMode::Continuous). */
enum class DynamicsTarget : int
{
    Expression       = 0, ///< CC 11
    ModWheel         = 1, ///< CC 1
    Breath           = 2, ///< CC 2
    ModAndExpression = 3  ///< CC 1 and CC 11 with the same value
};

/** Velocity from the way a note begins (plucked or bowed). */
enum class Articulation : int
{
    Off    = 0, ///< velocity from the level only, as before
    Gentle = 1, ///< half way between Off and Strong
    Strong = 2  ///< steep start 95..127, soft start 45..65
};

struct Settings
{
    BendMode     bendMode       = BendMode::Continuous;
    DynamicsMode dynamics       = DynamicsMode::Continuous;
    Response     response       = Response::Normal;
    float        bendRangeSemis = 2.0f;   ///< must match the instrument's pitch-bend range, 0.1 .. 96
    float        vibratoSmoothing = 0.5f; ///< Smoothed mode: 0 = none .. 1 = strongest (0.5 = 180 ms window)
    float        inputGainDb    = 0.0f;
    float        gateDb         = -45.0f; ///< "in": a note may start above this level, RMS dBFS after gain
    float        gateOffDb      = -51.0f; ///< "out": a note ends below this level; never above gateDb
    int          lowestNote     = 34;     ///< B-flat 1, lowest string of a 7-string violin
    int          highestNote    = 105;    ///< A7
    float        a4Hz           = 440.0f;
    int          fixedVelocity  = 100;
    int          midiChannel    = 1;      ///< 1..16; not used while mpe is on
    bool         enabled        = true;   ///< master switch: off ends every note and sends nothing

    // --- protection against octave errors (NoteTracker)
    bool         octaveAttackGuard  = true;  ///< at the start of a note: wait if the octave below is possible
    float        octaveAttackMs     = 50.0f; ///< how long to wait at most, 0 .. 60
    bool         octaveSustainGuard = true;  ///< inside a note: a jump of an octave must last before it is believed
    float        octaveSustainMs    = 11.0f; ///< how long it must last, 0 .. 100

    // --- playing aids (NoteTracker)
    bool         jumpGuard          = false; ///< a wide leap right after a note has started must last before it is believed
    int          jumpIntervalSemis  = 7;     ///< "wide" starts here, 5 .. 24 semitones
    float        jumpWindowMs       = 80.0f; ///< how long after the start of a note the guard watches, 10 .. 200
    float        jumpLockMs         = 50.0f; ///< how long the leap must last, 20 .. 150
    float        bridgeMs           = 30.0f; ///< a note survives this long without a detectable pitch while the level stays above gateOffDb, 0 .. 200

    // --- ringing on (OutputStage for MIDI, Harmonizer for audio)
    float        overlapMs          = 0.0f;  ///< at a change of note the previous one stays this long, 0 .. 1500
    float        releaseMs          = 0.0f;  ///< after the end of playing the note fades out over this time, 0 .. 3000

    // --- loudness
    Articulation   articulation       = Articulation::Off;
    DynamicsTarget dynamicsTarget     = DynamicsTarget::Expression;
    float          dynamicsSmoothingMs = 0.0f; ///< extra smoothing of the controller, 0 .. 150

    // --- key (see Scale.h and OutputStage.h)
    bool         keyFilter      = false;  ///< only notes of the key are started
    int          keyRoot        = 0;      ///< 0 = C .. 11 = B
    int          scaleMode      = 0;      ///< index into scale::kScales, 0 .. scale::kScaleCount - 1
    bool         pentatonic     = false;  ///< with keyFilter: only the five pentatonic notes of the scale's family
    bool         pentaSustain   = false;  ///< with keyFilter and a scale of fewer than seven notes: hold the last valid note through foreign ones

    // --- harmony voices on a second channel (the names date from when it was a triad only)
    bool         triad          = false;  ///< harmony on
    int          triadChannel   = 2;      ///< 1..16
    int          voicing        = 0;      ///< index into scale::kVoicings, 0 .. scale::kVoicingCount - 1 (kFreeVoicing: "Total Free Voicing")
    HarmonyMode  harmonyMode    = HarmonyMode::Diatonic;
    bool         voicingEdited  = false;  ///< voices were added to or taken away from the preset voicing: voicingVoices holds them ("FV")
    uint64_t     voicingVoices  = scale::presetMask (0); ///< with voicingEdited: the voices of the preset voicing as played
    uint64_t     freeVoicing    = scale::maskBit (0) | scale::maskBit (4) | scale::maskBit (7); ///< Total Free Voicing; both masks: bit (semitones + 24) for -24 .. +24, at most 10 set
    float        harmonyOffset  = 0.0f;   ///< harmony softer than the played note: -0.5 .. 0 (velocity and dynamics controllers are scaled by 1 + this)

    // --- the harmony as sound: the violin's own tone, shifted (Harmonizer)
    bool         audioHarmony   = false;
    Curve        formantCurve   = curve::defaultFormant(); ///< over the played pitch: share of the body resonance the voices keep (1: it stays where it is, 0: it moves with the pitch)
    Curve        hiCutCurve     = curve::defaultHiCut();   ///< over the played pitch: cut-off in Hz of the low-pass (12 dB/oct) on the voices only
    bool         dropVoicing    = true;     ///< chords of three notes or more, played from A4 up: the highest voice an octave lower
    float        ensemble       = 1.0f;     ///< ensemble decoupling, 0 .. 1: micro-delay, slow detune and its own place for every voice
    float        vibratoCoupling = 1.0f;    ///< how much of the player's vibrato the voices follow, 0 .. 1
    float        stereoSpread   = 0.35f;    ///< voices spread left and right, 0 .. 1 (0.35: +-35 %)
    float        harmonyGainDb  = 0.0f;     ///< level of the voices, -60 (treated as off) .. +6
    bool         dryKill        = false;    ///< the violin's own sound is left out of the output

    // --- MPE: every note on its own channel 2..16, bend range 48 (OutputStage)
    bool         mpe            = false;

    // --- freeze: what sounds on that track stays, nothing new is sent there (OutputStage). Not stored.
    bool         freezeSolo     = false;
    bool         freezeHarmony  = false;
};

/** The voices of the harmony as a template (see scale::voices): the Total Free
    Voicing's own, those of a preset voicing that were edited, or else the
    preset's own. */
inline uint64_t voicingMask (const Settings& s) noexcept
{
    const int v = scale::clampVoicing (s.voicing);
    if (v == scale::kFreeVoicing)
        return s.freeVoicing;
    return s.voicingEdited ? s.voicingVoices : scale::presetMask (v);
}

/** A preset voicing whose voices differ from its own ("Dreiklang FV"). */
inline bool voicingModified (const Settings& s) noexcept
{
    const int v = scale::clampVoicing (s.voicing);
    return v != scale::kFreeVoicing && s.voicingEdited && scale::cleanFreeMask (s.voicingVoices) != scale::presetMask (v);
}

/** Limits used when sizing buffers; settings are clamped to these. */
constexpr int   kAbsLowestNote  = 28;   // E1, 41.2 Hz
constexpr int   kAbsHighestNote = 108;  // C8
constexpr float kMinA4Hz        = 400.0f;
constexpr float kMaxA4Hz        = 480.0f;

struct MidiEvent
{
    int     sampleOffset = 0; ///< position inside the processed block
    uint8_t size         = 3; ///< number of valid bytes
    uint8_t bytes[3]     = { 0, 0, 0 };
};

using MidiEventList = std::vector<MidiEvent>;

} // namespace vm
