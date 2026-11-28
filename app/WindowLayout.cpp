// The geometry of the main window (see WindowLayout.h).
#include "WindowLayout.h"

namespace layout
{

Plan windowed() noexcept
{
    return {};
}

Plan fullscreen (int, int, bool, int) noexcept
{
    Plan p;
    p.fullscreen = true;
    return p;
}

GraphSplit graphSplit (int) noexcept
{
    return {};
}

LevelWindow levelWindow (float, float, float) noexcept
{
    return {};
}

int levelMarkStep (float, int) noexcept
{
    return 24;
}

} // namespace layout
