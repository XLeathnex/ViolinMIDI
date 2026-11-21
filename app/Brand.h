// Name, colours and the logo. Everything that makes up the look lives here,
// so a rename or a new colour is a change in one place.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace brand
{

/** The product name shown to the user. */
inline constexpr const char* kName    = "Archet";
inline constexpr const char* kTagline = "Violin to MIDI";

/** The name used before the rename. The app bundle on disk, the settings file,
    the update folder and the MIDI port of earlier installations keep it, so
    updates and existing MainStage setups go on working. */
inline constexpr const char* kLegacyName = "ViolinMIDI";

namespace colour
{
    inline const juce::Colour window     { 0xff0c0f12 };
    inline const juce::Colour panel      { 0xff14191e };
    inline const juce::Colour raised     { 0xff1c2228 };
    inline const juce::Colour hover      { 0xff242b32 };
    inline const juce::Colour line       { 0xff29313a };
    inline const juce::Colour text       { 0xffeaeef1 };
    inline const juce::Colour textDim    { 0xff97a1aa };
    inline const juce::Colour textFaint  { 0xff5c6670 };
    inline const juce::Colour accent     { 0xfff4b24c };   // amber, like rosin
    inline const juce::Colour accentDeep { 0xffe2862e };
    inline const juce::Colour good       { 0xff4fd18b };
    inline const juce::Colour bad        { 0xffff5f5f };
}

juce::Font font (float height, bool bold = false);

/** Small caps-style heading font for card titles. */
juce::Font headingFont();

/** The f-hole with rising notes, without background. */
void drawMark (juce::Graphics&, juce::Rectangle<float> area);

/** The full app icon: rounded square in the macOS proportions with the mark. */
void drawIcon (juce::Graphics&, juce::Rectangle<float> area);

/** Writes the icon as a square PNG. Used to produce assets/AppIcon.png. */
bool renderIconPng (const juce::File& file, int size);

} // namespace brand
