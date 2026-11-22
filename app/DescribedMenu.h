// Combo-box entries with a short description under the name, for choices that
// need a sentence of explanation.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/** Adds an entry to a combo box whose row in the popup list shows the name
    and, below it in smaller, dimmer type, a short description. The closed
    box shows only the name. */
void addDescribedItem (juce::ComboBox& box, int itemId, const juce::String& name, const juce::String& description);
