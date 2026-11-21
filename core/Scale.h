// ViolinMIDI core - keys, church modes, pentatonic scales and diatonic triads.
// Header only, no state.
#pragma once

namespace vm
{
namespace scale
{

constexpr int kModeCount = 7;

/** Semitones above the root. Order: Ionian, Dorian, Phrygian, Lydian, Mixolydian, Aeolian, Locrian. */
constexpr int kModeSteps[kModeCount][7] = {
    { 0, 2, 4, 5, 7, 9, 11 },
    { 0, 2, 3, 5, 7, 9, 10 },
    { 0, 1, 3, 5, 7, 8, 10 },
    { 0, 2, 4, 6, 7, 9, 11 },
    { 0, 2, 4, 5, 7, 9, 10 },
    { 0, 2, 3, 5, 7, 8, 10 },
    { 0, 1, 3, 5, 6, 8, 10 }
};

constexpr int kMajorPentatonic[5] = { 0, 2, 4, 7, 9 };
constexpr int kMinorPentatonic[5] = { 0, 3, 5, 7, 10 };

/** Ionian, Lydian and Mixolydian use the major pentatonic, the others the minor one. */
constexpr bool usesMajorPentatonic (int mode) noexcept { return mode == 0 || mode == 3 || mode == 4; }

constexpr int pitchClass (int note) noexcept { return ((note % 12) + 12) % 12; }
constexpr int clampMode (int mode) noexcept  { return mode < 0 ? 0 : (mode >= kModeCount ? kModeCount - 1 : mode); }

/** Bit n is set if pitch class n (0 = C) belongs to the scale. */
inline unsigned mask (int root, int mode, bool pentatonic) noexcept
{
    root = pitchClass (root);
    mode = clampMode (mode);
    unsigned m = 0;

    if (pentatonic)
    {
        const int* steps = usesMajorPentatonic (mode) ? kMajorPentatonic : kMinorPentatonic;
        for (int i = 0; i < 5; ++i)
            m |= 1u << ((root + steps[i]) % 12);
    }
    else
    {
        for (int i = 0; i < 7; ++i)
            m |= 1u << ((root + kModeSteps[mode][i]) % 12);
    }
    return m;
}

inline bool contains (unsigned scaleMask, int note) noexcept
{
    return ((scaleMask >> pitchClass (note)) & 1u) != 0;
}

struct Triad
{
    int notes[3] = { -1, -1, -1 };
    int count = 0;
};

/** The triad built on `note` from the 1st, 3rd and 5th step of the mode above it,
    so its quality (major, minor, diminished) follows the degree. A note outside
    the seven notes of the mode gets a major triad. Notes above 127 are left out. */
inline Triad triad (int note, int root, int mode, bool withRoot) noexcept
{
    root = pitchClass (root);
    mode = clampMode (mode);
    const int* steps = kModeSteps[mode];
    const int relative = pitchClass (note - root);

    int third = 4, fifth = 7;
    for (int i = 0; i < 7; ++i)
    {
        if (steps[i] == relative)
        {
            third = (steps[(i + 2) % 7] - relative + 12) % 12;
            fifth = (steps[(i + 4) % 7] - relative + 12) % 12;
            break;
        }
    }

    Triad t;
    const int candidates[3] = { withRoot ? note : -1, note + third, note + fifth };
    for (int n : candidates)
        if (n >= 0 && n <= 127)
            t.notes[t.count++] = n;
    return t;
}

} // namespace scale
} // namespace vm
