// The main window's content (see WindowRoot.h).
#include "WindowRoot.h"

WindowRoot::WindowRoot (const MainComponent::Options& options) : view (options)
{
    addAndMakeVisible (view);
    setSize (layout::kWidth, layout::kHeight);
}

bool WindowRoot::isFullScreenNow() const
{
    return forced;
}

void WindowRoot::setForcedFullscreen (bool on)
{
    forced = on;
}

void WindowRoot::resized()
{
    view.setBounds (getLocalBounds());
}

void WindowRoot::paint (juce::Graphics&) {}
