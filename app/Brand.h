// Name, colours, type and the logo. Everything that makes up the look lives
// here, so a rename or a colour change happens in one place.
//
// Design rules (see README, "Gestaltung"):
//  - Neutral greys carry the interface. The accent colour is reserved for live
//    data: what is being played and sent. Red only for problems.
//  - No cards, pills, glows, gradients or all-caps labels. Groups are made by
//    alignment and spacing; separators are single hairlines.
//  - One dominant element (the live display); everything else is quiet.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace brand
{

/** The product name shown to the user. */
inline constexpr const char* kName = "Archet";

/** The name used before the rename. The app bundle on disk, the settings file,
    the update folder and the MIDI port of earlier installations keep it, so
    updates and existing MainStage setups go on working. */
inline constexpr const char* kLegacyName = "ViolinMIDI";

namespace colour
{
    inline const juce::Colour window     { 0xff1f1f1f };   // control area
    inline const juce::Colour display    { 0xff151515 };   // the live display
    inline const juce::Colour control    { 0xff2d2d2d };   // buttons, menus, tracks
    inline const juce::Colour controlHi  { 0xff363636 };   // hover
    inline const juce::Colour selected   { 0xff595959 };   // chosen segment
    inline const juce::Colour separator  { 0xff2c2c2c };
    inline const juce::Colour grid       { 0xff222222 };
    inline const juce::Colour text       { 0xffe6e6e6 };
    inline const juce::Colour secondary  { 0xff9b9b9b };
    inline const juce::Colour tertiary   { 0xff676767 };
    inline const juce::Colour accent     { 0xffe9a23b };   // live data only
    inline const juce::Colour problem    { 0xffe5584f };
}

juce::Font font (float height, bool bold = false);

/** The f-hole with rising notes, without background. */
void drawMark (juce::Graphics&, juce::Rectangle<float> area);

/** The full app icon: rounded square in the macOS proportions with the mark. */
void drawIcon (juce::Graphics&, juce::Rectangle<float> area);

/** Writes the icon as a square PNG. Used to produce assets/AppIcon.png. */
bool renderIconPng (const juce::File& file, int size);

} // namespace brand
