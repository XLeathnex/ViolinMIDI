// ViolinMIDI core - shared types.
// Plain C++17, no dependencies, so the same code runs in the app and in the tests.
#pragma once

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

struct Settings
{
    BendMode     bendMode       = BendMode::Continuous;
    DynamicsMode dynamics       = DynamicsMode::Continuous;
    Response     response       = Response::Normal;
    int          bendRangeSemis = 2;      ///< must match the instrument's pitch-bend range
    float        inputGainDb    = 0.0f;
    float        gateDb         = -45.0f; ///< note-on threshold, RMS dBFS after gain
    int          lowestNote     = 34;     ///< B-flat 1, lowest string of a 7-string violin
    int          highestNote    = 105;    ///< A7
    float        a4Hz           = 440.0f;
    int          fixedVelocity  = 100;
    int          midiChannel    = 1;      ///< 1..16
};

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
