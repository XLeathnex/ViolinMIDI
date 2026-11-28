// The main window's content (2.1). In the window it is exactly the
// MainComponent (1260 x 724). In fullscreen it fills the screen and shows the
// MainComponent scaled (an AffineTransform), laid out at the logical size that
// layout::fullscreen() chose, with the camera panel docked at the right when it
// is wanted. It is never transformed itself (ResizableWindow requires that of
// its content).
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "MainComponent.h"

class WindowRoot final : public juce::Component
{
public:
    explicit WindowRoot (const MainComponent::Options&);

    MainComponent& main() noexcept   { return view; }

    /** The window's fullscreen state, or the forced one. */
    bool isFullScreenNow() const;
    /** Pretends fullscreen at the current size (--fullscreen-size on Linux, the layout check). */
    void setForcedFullscreen (bool);

    /** Computes the plan and hands it to the view. */
    void resized() override;
    /** The window colour behind the remainder of less than one logical pixel. */
    void paint (juce::Graphics&) override;

private:
    MainComponent view;
    bool forced = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WindowRoot)
};
