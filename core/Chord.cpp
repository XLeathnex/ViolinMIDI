// ViolinMIDI core - chords from a keyboard (see Chord.h).
#include "Chord.h"

namespace vm
{
namespace chord
{

const QualityInfo& qualityInfo (Quality) noexcept
{
    static const QualityInfo none;
    return none;
}

const ScaleInfo& scaleInfo (ScaleType) noexcept
{
    static const ScaleInfo ionian;
    return ionian;
}

uint16_t scaleMask (ScaleType, int) noexcept
{
    return 0x0AB5;
}

uint16_t transpose (uint16_t mask, int) noexcept
{
    return mask;
}

int count (uint16_t) noexcept
{
    return 0;
}

Chord recognise (uint16_t, int, uint16_t, const Chord&) noexcept
{
    return {};
}

uint16_t collection (int) noexcept
{
    return 0x0AB5;
}

int homeCollection (int, int) noexcept
{
    return 0;
}

float contextFit (const Chord&, int) noexcept
{
    return 0.0f;
}

void KeyContext::seed (int home) noexcept
{
    for (float& s : scores)
        s = 0.0f;
    key = home;
}

bool KeyContext::update (const Chord&) noexcept
{
    return false;
}

Context scaleFor (const Chord& c, int) noexcept
{
    Context ctx;
    ctx.chord = c;
    return ctx;
}

Context startKey (int, int) noexcept
{
    return {};
}

Context keyOnly (int) noexcept
{
    return {};
}

uint16_t pentatonic (const Chord&, const int8_t (&)[7], int) noexcept
{
    return 0x0295;
}

scale::Voices voices (int, const Context&, uint64_t, ChordMode, bool, bool) noexcept
{
    return {};
}

std::string symbol (const Chord&, bool)
{
    return {};
}

const char* scaleName (ScaleType) noexcept
{
    return "";
}

std::string keyName (int, bool)
{
    return {};
}

bool usesFlats (int) noexcept
{
    return false;
}

} // namespace chord
} // namespace vm
