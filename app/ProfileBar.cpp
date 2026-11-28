// The instrument profile in the main window (see ProfileBar.h).
#include "ProfileBar.h"

ProfileBar::ProfileBar()
{
    box.setWantsKeyboardFocus (false);
    menuButton.setButtonText (juce::String::fromUTF8 ("\xe2\x80\xa6"));
    menuButton.setWantsKeyboardFocus (false);
    addAndMakeVisible (box);
    addAndMakeVisible (menuButton);
}

void ProfileBar::show (const vm::ProfileList&, bool, const juce::String&) {}

void ProfileBar::resized()
{
    auto r = getLocalBounds();
    menuButton.setBounds (r.removeFromRight (kMenuWidth));
    r.removeFromRight (kGap);
    box.setBounds (r);
}
