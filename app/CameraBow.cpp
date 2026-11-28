// The bow hand (see CameraBow.h).
#include "CameraBow.h"

namespace cam
{

BowTracker::Window BowTracker::update (const Pose&, const Settings&) noexcept
{
    return {};
}

void BowTracker::reset() noexcept
{
    last = BowReport();
}

uint64_t packCue (const BowTracker::Window&) noexcept
{
    return 0;
}

bool cueContains (uint64_t, double) noexcept
{
    return false;
}

} // namespace cam
