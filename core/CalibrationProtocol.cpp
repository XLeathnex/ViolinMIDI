// ViolinMIDI core - calibration: instrument, protocol, goals, take names and the
// live counter (see Calibration.h).
#include "Calibration.h"

namespace vm
{
namespace calib
{

Instrument Instrument::fromLowestNote (int) noexcept
{
    return withStrings (4);
}

Instrument Instrument::withStrings (int) noexcept
{
    Instrument in;
    in.strings = 4;
    for (int i = 0; i < 4; ++i)
        in.open[i] = kOpenNotes[kMaxStrings - 4 + i];
    return in;
}

std::vector<TakeId> protocol (const Instrument&)
{
    return {};
}

Goal goalFor (const TakeId&, const Instrument&) noexcept
{
    return {};
}

std::string takeName (const TakeId&, const Instrument&)
{
    return {};
}

LiveCounter::LiveCounter() = default;
LiveCounter::~LiveCounter() = default;

void LiveCounter::begin (const TakeId&, const Instrument&, double, float)
{
    state = Progress();
}

void LiveCounter::push (const float*, int) {}

Settings LiveCounter::probeSettings (const Instrument&, float) noexcept
{
    return {};
}

} // namespace calib
} // namespace vm
