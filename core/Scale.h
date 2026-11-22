// ViolinMIDI core - keys, scales and the harmony voices built on a played note.
// Header only, no state.
#pragma once

#include <cstdint>

namespace vm
{
namespace scale
{

//==============================================================================
// Scales. The index is what Settings::scaleMode holds and what is stored on
// disk: new scales go at the end, existing ones keep their place.

struct ScaleDef
{
    int  steps[7];        ///< semitones above the root, `count` of them
    int  count;
    int  parent;          ///< the seven-note scale whose steps are counted for harmony voices
    bool majorPentatonic; ///< Settings::pentatonic picks the major (else the minor) pentatonic
};

constexpr int kScaleCount = 13;

constexpr ScaleDef kScales[kScaleCount] = {
    { { 0, 2, 4, 5, 7, 9, 11 }, 7, 0, true  },   //  0 major (Ionian)
    { { 0, 2, 3, 5, 7, 9, 10 }, 7, 1, false },   //  1 Dorian
    { { 0, 1, 3, 5, 7, 8, 10 }, 7, 2, false },   //  2 Phrygian
    { { 0, 2, 4, 6, 7, 9, 11 }, 7, 3, true  },   //  3 Lydian
    { { 0, 2, 4, 5, 7, 9, 10 }, 7, 4, true  },   //  4 Mixolydian
    { { 0, 2, 3, 5, 7, 8, 10 }, 7, 5, false },   //  5 natural minor (Aeolian)
    { { 0, 1, 3, 5, 6, 8, 10 }, 7, 6, false },   //  6 Locrian
    { { 0, 2, 3, 5, 7, 8, 11 }, 7, 7, false },   //  7 harmonic minor
    { { 0, 2, 3, 5, 7, 9, 11 }, 7, 8, false },   //  8 melodic minor
    { { 0, 1, 4, 5, 7, 8, 10 }, 7, 9, true  },   //  9 Phrygian dominant
    { { 0, 2, 4, 7, 9, 0, 0 },  5, 0, true  },   // 10 major pentatonic, harmonised as major
    { { 0, 3, 5, 7, 10, 0, 0 }, 5, 5, false },   // 11 minor pentatonic, harmonised as natural minor
    { { 0, 3, 5, 6, 7, 10, 0 }, 6, 5, false }    // 12 blues, harmonised as natural minor
};

constexpr int kMajorPentatonic[5] = { 0, 2, 4, 7, 9 };
constexpr int kMinorPentatonic[5] = { 0, 3, 5, 7, 10 };

constexpr int pitchClass (int note) noexcept { return ((note % 12) + 12) % 12; }
constexpr int clampMode (int mode) noexcept  { return mode < 0 ? 0 : (mode >= kScaleCount ? kScaleCount - 1 : mode); }

/** How many notes the scale has: 7, or fewer for the pentatonic and blues scales. */
constexpr int noteCount (int mode) noexcept  { return kScales[clampMode (mode)].count; }

constexpr bool usesMajorPentatonic (int mode) noexcept { return kScales[clampMode (mode)].majorPentatonic; }

/** Bit n is set if pitch class n (0 = C) belongs to the scale. With `pentatonic`
    it is the major or minor pentatonic of the scale's family instead. */
inline unsigned mask (int root, int mode, bool pentatonic) noexcept
{
    root = pitchClass (root);
    const ScaleDef& s = kScales[clampMode (mode)];
    unsigned m = 0;

    if (pentatonic)
    {
        const int* steps = s.majorPentatonic ? kMajorPentatonic : kMinorPentatonic;
        for (int i = 0; i < 5; ++i)
            m |= 1u << ((root + steps[i]) % 12);
    }
    else
    {
        for (int i = 0; i < s.count; ++i)
            m |= 1u << ((root + s.steps[i]) % 12);
    }
    return m;
}

inline bool contains (unsigned scaleMask, int note) noexcept
{
    return ((scaleMask >> pitchClass (note)) & 1u) != 0;
}

//==============================================================================
// Voicings. The index is what Settings::voicing holds and what is stored on disk.

constexpr int kMaxVoices = 10;

struct VoicingDef
{
    bool steps;     ///< true: `v` counts scale steps above the played note (2 = a third); false: semitones
    int  count;
    int  v[4];
};

constexpr int kVoicingCount = 13;
constexpr int kFreeVoicing = 12;   ///< the voices come from Settings::freeVoicing

constexpr VoicingDef kVoicings[kVoicingCount] = {
    { true,  3, { 0, 2, 4, 0 } },      //  0 close triad         1 - 3 - 5
    { true,  4, { 0, 2, 4, 6 } },      //  1 seventh chord       1 - 3 - 5 - 7
    { true,  3, { 0, 4, 9, 0 } },      //  2 open                1 - 5 - 10
    { true,  3, { 0, 4, 7, 0 } },      //  3 power               1 - 5 - 8
    { true,  3, { 0, 3, 6, 0 } },      //  4 fourths             1 - 4 - 7
    { true,  2, { 0, 2, 0, 0 } },      //  5 duet in thirds      1 - 3
    { true,  2, { 0, 5, 0, 0 } },      //  6 duet in sixths      1 - 6
    { false, 1, { -12, 0, 0, 0 } },    //  7 octave below
    { false, 2, { -24, -12, 0, 0 } },  //  8 two octaves below
    { false, 2, { -12, 12, 0, 0 } },   //  9 octave below and above
    { false, 1, { 12, 0, 0, 0 } },     // 10 octave above
    { false, 2, { 12, 24, 0, 0 } },    // 11 two octaves above
    { false, 0, { 0, 0, 0, 0 } }       // 12 free
};

constexpr int clampVoicing (int voicing) noexcept { return voicing < 0 ? 0 : (voicing >= kVoicingCount ? kVoicingCount - 1 : voicing); }

/** Chromatic harmony: the fixed size of each step - major second and third,
    perfect fourth and fifth, major sixth, minor seventh. */
constexpr int kFixedSteps[7] = { 0, 2, 4, 5, 7, 9, 10 };

/** Keeps the bits for -24 .. +24 and of those at most kMaxVoices, the lowest first. */
inline uint64_t cleanFreeMask (uint64_t freeMask) noexcept
{
    uint64_t cleaned = 0;
    int used = 0;
    for (int i = 0; i < 49 && used < kMaxVoices; ++i)
    {
        if ((freeMask >> i) & 1ull)
        {
            cleaned |= 1ull << i;
            ++used;
        }
    }
    return cleaned;
}

/** Semitones from `note` up to the scale step `step` above it (0 = the note
    itself, 2 = a third, 7 = an octave, 9 = a tenth).

    Diatonic: counted in the seven-note scale, so the third above D in C major
    is F. A note that is not in the scale has no steps to count from and gets
    the fixed sizes, as in chromatic harmony. */
inline int stepInterval (int note, int root, int mode, int step, bool diatonic) noexcept
{
    const int octaves = step / 7, within = step % 7;

    if (diatonic)
    {
        const ScaleDef& s = kScales[kScales[clampMode (mode)].parent];
        const int relative = pitchClass (note - root);

        for (int i = 0; i < 7; ++i)
        {
            if (s.steps[i] == relative)
            {
                const int target = i + within;
                return s.steps[target % 7] + 12 * (target / 7) - relative + 12 * octaves;
            }
        }
    }
    return kFixedSteps[within] + 12 * octaves;
}

struct Voices
{
    int notes[kMaxVoices] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
    int count = 0;
};

/** The harmony voices for a played note, lowest definition first. Notes
    outside 0..127 are left out; with `withoutPlayedNote` so is the played note
    itself (when the harmony shares the channel of the melody). */
inline Voices voices (int note, int root, int mode, int voicing, bool diatonic, uint64_t freeMask,
                      bool withoutPlayedNote = false) noexcept
{
    Voices result;
    const auto add = [&] (int n)
    {
        if (n >= 0 && n <= 127 && result.count < kMaxVoices && ! (withoutPlayedNote && n == note))
            result.notes[result.count++] = n;
    };

    voicing = clampVoicing (voicing);

    if (voicing == kFreeVoicing)
    {
        const uint64_t cleaned = cleanFreeMask (freeMask);
        for (int i = 0; i < 49; ++i)
            if ((cleaned >> i) & 1ull)
                add (note + i - 24);
        return result;
    }

    const VoicingDef& def = kVoicings[voicing];
    for (int i = 0; i < def.count; ++i)
        add (note + (def.steps ? stepInterval (note, root, mode, def.v[i], diatonic) : def.v[i]));
    return result;
}

} // namespace scale
} // namespace vm
