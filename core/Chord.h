// ViolinMIDI core - chords from a keyboard (2.1, "Externe Chords").
//
// Pure functions and tables, no state: which chord a set of held keys is,
// which scale that chord gets in the context of the recent chords, the chord's
// pentatonic, the harmony voices of a voicing over it, and names for the user
// interface. ChordTracker keeps the state (keys, pedal, latch, key context) and
// calls these at a commit; OutputStage rule R9 uses the result.
//
// Pitch-class sets are 12-bit masks: bit n = pitch class n (0 = C). Everything
// except the names is real-time safe (fixed arrays, no allocation).
#pragma once

#include "Scale.h"
#include "Types.h"

#include <cstdint>
#include <string>

namespace vm
{
namespace chord
{

/** Chord qualities. The order is the rank: simpler first (recognition prefers it on a tie). */
enum class Quality : uint8_t
{
    Major, Minor, Dom7, Min7, Maj7, Sus4, Sus2, Power, Dim, Aug, HalfDim, Dim7,
    Maj6, Min6, MinMaj7, Dom7Sus4, Aug7, AugMaj7, Count
};

/** The 21 chord scales: the modes of major, harmonic minor and melodic minor. */
enum class ScaleType : uint8_t
{
    Ionian, Dorian, Phrygian, Lydian, Mixolydian, Aeolian, Locrian,                    // major modes
    HarmonicMinor, Locrian6, Ionian5, Dorian4, PhrygianDominant, Lydian2, AlteredDim,  // harmonic minor modes
    MelodicMinor, Dorian2, LydianAug, LydianDominant, Mixolydian13, Locrian2, Altered, // melodic minor modes
    Count
};

constexpr int kQualityCount   = static_cast<int> (Quality::Count);
constexpr int kScaleTypeCount = static_cast<int> (ScaleType::Count);
constexpr int kMaxCandidates  = 6;

/** One quality: its tones on C and how recognition and the key context judge it. */
struct QualityInfo
{
    const char* symbol = "";        ///< UTF-8 suffix after the root: "", "m", "7", "m7b5" (with a real flat sign) ...
    uint16_t tones = 0;             ///< the chord's intervals above the root, as a mask on C
    uint16_t optional = 0;          ///< tones that may be missing at a small cost (the fifth)
    uint16_t tensions = 0;          ///< further notes that still belong to the chord
    bool     weak = false;          ///< the small outside penalty in the key context
    bool     dominantType = false;  ///< Major, Dom7, Dom7Sus4, Aug7, Aug: the secondary-dominant rule
    ScaleType candidates[kMaxCandidates] = {};   ///< chord scales in tie order
    int      candidateCount = 0;
    int      rotations = 1;         ///< roots the candidates are also tried on: Dim7 4 (r, r+3, r+6, r+9), Aug 3, else 1
};

/** One chord scale: seven steps above its root, and its German name. */
struct ScaleInfo
{
    int8_t steps[7] = { 0, 2, 4, 5, 7, 9, 11 };
    const char* name = "";
};

const QualityInfo& qualityInfo (Quality) noexcept;
const ScaleInfo&   scaleInfo (ScaleType) noexcept;

/** The seven notes of that scale on `root` as a mask. */
uint16_t scaleMask (ScaleType, int root) noexcept;

/** Pitch-class masks. */
constexpr uint16_t pcBit (int pitchClass) noexcept   { return static_cast<uint16_t> (1u << (((pitchClass % 12) + 12) % 12)); }
uint16_t transpose (uint16_t mask, int semitones) noexcept;   ///< rotated by that many pitch classes
int      count (uint16_t mask) noexcept;                       ///< pitch classes in it

//==============================================================================
struct Chord
{
    int8_t   root = -1;        ///< pitch class, -1 = no chord
    Quality  quality = Quality::Major;
    int8_t   bass = -1;        ///< pitch class of the lowest key held (inversion / slash)
    uint16_t tones = 0;        ///< the quality's tones on the root (pitch classes)
    uint16_t played = 0;       ///< what was held when it was recognised
    float    score = 0.0f;
    bool valid() const noexcept { return root >= 0; }
};

/** Everything the output stage needs about the harmony, at one hop. Plain data. */
struct Context
{
    bool      active = false;      ///< a chord has been committed since the last reset (else: the start key)
    bool      hasChord = false;    ///< a chord is held or latched (false: key only - latch off, all keys up)
    bool      gathering = false;   ///< keys are being struck: a chord is about to be committed (R9e)
    Chord     chord;               ///< display; its tones are chordMask
    uint16_t  chordMask = 0;       ///< chord tones, 0 without a chord
    int8_t    scaleRoot = 0;       ///< the chord scale: seven notes, scaleSteps above scaleRoot
    ScaleType scaleType = ScaleType::Ionian;
    int8_t    scaleSteps[7] = { 0, 2, 4, 5, 7, 9, 11 };
    uint16_t  scaleMask = 0x0AB5;  ///< the same seven notes as a mask
    uint16_t  pentaMask = 0x0295;  ///< 3 .. 5 notes of the chord scale ("Nur Pentatonik")
    int8_t    keyTonic = 0;        ///< the key context: tonic of its major scale
    uint32_t  serial = 0;          ///< changes whenever chordMask, scale*, pentaMask, hasChord or gathering changes
};

//==============================================================================
// Recognition (4.2)

constexpr float kMinScore     = 4.0f;   ///< a chord of three or more pitch classes needs this score
constexpr float kMinScoreDyad = 5.0f;   ///< two pitch classes need this

/** The chord a set of pitch classes is, with the lowest key's pitch class, in the key context
    (a collection mask, see collection()) and with the chord in force (it is preferred a little,
    so the same keys keep their name). Not valid (root -1) below the thresholds. */
Chord recognise (uint16_t pitchClasses, int bassPitchClass, uint16_t context, const Chord& current) noexcept;

//==============================================================================
// Key context (4.4): which of the 12 diatonic collections the recent chords are in.

/** The pitch classes of the major scale on `tonic`. */
uint16_t collection (int tonic) noexcept;

/** The major tonic whose collection the start key (Settings::keyRoot, scaleMode) belongs to. */
int homeCollection (int keyRoot, int scaleMode) noexcept;

constexpr float kSeedScore    = 3.0f;    ///< the start key's head start
constexpr float kDecay        = 0.8f;    ///< per committed strike
constexpr float kSwitchMargin = 1.25f;   ///< another collection must lead by this much to take over

/** How well a chord fits collection(tonic): the term added at a strike. */
float contextFit (const Chord&, int tonic) noexcept;

/** The scores of the 12 collections and the one in force. */
struct KeyContext
{
    float scores[12] = {};
    int   key = 0;

    /** All 0, the home collection kSeedScore and in force. */
    void seed (int home) noexcept;
    /** One committed strike with a valid chord: decay, add the fit, switch with hysteresis.
        True if `key` changed. */
    bool update (const Chord&) noexcept;
};

//==============================================================================
// Chord scales (4.3), pentatonic (4.5)

/** The chord's scale in the key context: fills chord, chordMask, scaleRoot, scaleType, scaleSteps,
    scaleMask, pentaMask and keyTonic. `active`, `hasChord`, `gathering` and `serial` are the
    tracker's and are left at their defaults. */
Context scaleFor (const Chord&, int keyTonic) noexcept;

/** Before the first chord (and after a reset): the start key without chord tones. */
Context startKey (int keyRoot, int scaleMode) noexcept;

/** Latch off and nothing held: Ionian on the key tonic, its major pentatonic, no chord tones. */
Context keyOnly (int keyTonic) noexcept;

/** 3 .. 5 notes of the chord scale: the chord's root, third (or sus note) and fifth, filled up
    from the scale away from semitone clashes. */
uint16_t pentatonic (const Chord&, const int8_t (&steps)[7], int scaleRoot) noexcept;

//==============================================================================
// Voices (4.6)

/** The harmony voices of a voicing template over the chord context, sorted ascending (the
    audio harmonizer pairs old and new voices by index). Pentatonic: on pentaMask. Diatonic, or a
    Hybrid without `chordTargets`, or no chord: counted in the chord scale. Hybrid with
    `chordTargets`: snapped to the chord tones. Octave voices never move. */
scale::Voices voices (int note, const Context&, uint64_t voicingMask, ChordMode, bool chordTargets,
                      bool withoutPlayedNote) noexcept;

//==============================================================================
// Names (user interface only; not real-time)

/** The chord symbol in UTF-8 with real sharp, flat and degree signs, e.g. C#m7b5/G#, Bb, F(dim)7
    (written here in ASCII); letters as in the app's root menu (B = H). Empty for an invalid chord. */
std::string symbol (const Chord&, bool flats);

/** "Ionisch", "Phrygisch Dominant" ... */
const char* scaleName (ScaleType) noexcept;

/** "C-Dur / a-Moll". */
std::string keyName (int tonic, bool flats);

/** The key context is written with flats: its tonic is F, B-flat, E-flat, A-flat, D-flat or G-flat. */
bool usesFlats (int keyTonic) noexcept;

} // namespace chord
} // namespace vm
