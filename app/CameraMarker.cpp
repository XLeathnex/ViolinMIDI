// The colour marker on the violin scroll (see CameraMarker.h).
#include "CameraMarker.h"

namespace cam
{

Hsv toHsv (int, int, int) noexcept
{
    return {};
}

bool MarkerColour::matches (const Hsv&, float) const noexcept
{
    return false;
}

Pick pickColour (const PixelView&, float, float) noexcept
{
    return {};
}

void MarkerFinder::prepare (int) {}

Point MarkerFinder::find (const PixelView&, int, const MarkerColour&, float, double) noexcept
{
    return {};
}

void MarkerFinder::reset() noexcept {}

} // namespace cam
