// Where the harmonizer's voices go (see OutputRouting.h).
#include "OutputRouting.h"

namespace routing
{

int pairCount (int) noexcept
{
    return 0;
}

std::string pairLabel (int, int)
{
    return {};
}

int clampPair (int, int) noexcept
{
    return 0;
}

Channels channelsFor (int, int) noexcept
{
    return {};
}

bool clocksDiffer (const std::string&, const std::string&)
{
    return false;
}

bool keepSavedOutput (const std::string&, const std::vector<std::string>&)
{
    return true;
}

Decision reconcile (const Wish&, const InUse&, const Present&)
{
    return {};
}

} // namespace routing
