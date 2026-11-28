// Name, colours, type and the logo. Everything that makes up the look lives
// here, so a rename or a colour change happens in one place.
//
// Design rules (2.0, VioLogic):
//  - A calm, dark studio anthracite carries the interface. The colours come
//    from the logo: crimson and brushed silver / cool white.
//  - Crimson is the accent: live data (the note being sent, the pitch line's
//    glow), filled slider tracks, the focus of a menu, the watermark.
//  - Silver-white for every measured value (Hz, ms, %, dB, CC values), the
//    slider knobs, hairlines and the labels of the note grid.
//  - Module and main switches say ON in emerald and OFF in crimson, as a
//    flat, quiet plate - nothing bulky. Problems in amber.
//  - No cards, gradients (except the brushed-metal knob) or all-caps labels.
//    Groups are made by alignment and spacing; separators are hairlines.
//  - Type and control shapes follow macOS: the system typeface, 6 px corners.
//  - One dominant element (the live display); everything else is quiet.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace brand
{

/** The product name shown to the user, and the name of the MIDI port. */
inline constexpr const char* kName = "VioLogic";

/** The first name of the app (until 1.1; 1.2 to 1.5 it was called Archet). The
    app bundle on disk, its executable and identifier, the settings file and
    the update folder keep it, so updates from every earlier version and the
    microphone permission go on working. */
inline constexpr const char* kLegacyName = "ViolinMIDI";

namespace colour
{
    inline const juce::Colour window     { 0xff1b1c1f };   // studio anthracite, the control area
    inline const juce::Colour display    { 0xff131417 };   // the live display
    inline const juce::Colour control    { 0xff28292d };   // buttons, menus, tracks
    inline const juce::Colour controlHi  { 0xff313237 };   // hover
    inline const juce::Colour selected   { 0xff4a4c53 };   // chosen segment of a neutral choice
    inline const juce::Colour separator  { 0xff2c2e33 };   // outlines of fields, lists and tooltips
    inline const juce::Colour hairline   { 0xff47494e };   // separating lines: the brushed silver at about a quarter, as a fine line
    inline const juce::Colour grid       { 0xff1f2125 };
    inline const juce::Colour text       { 0xffe9edf2 };   // cool white
    inline const juce::Colour silver     { 0xffc8cdd4 };   // brushed silver: measured values, knobs, grid labels
    inline const juce::Colour secondary  { 0xff989ea7 };   // names of the controls
    inline const juce::Colour tertiary   { 0xff626770 };
    inline const juce::Colour crimson    { 0xffd2224a };   // the logo's red, a touch brighter for screens
    inline const juce::Colour crimsonDim { 0xff7d1a2f };   // the OFF plate of a switch
    inline const juce::Colour emerald    { 0xff22b07c };   // ON: status dots, text
    inline const juce::Colour emeraldDim { 0xff17694c };   // the ON plate of a switch
    inline const juce::Colour accent     = crimson;        // live data
    inline const juce::Colour problem    { 0xffe8a23a };   // amber: something needs attention
}

/** Corner radius of buttons, menus and segmented controls, as macOS draws them. */
inline constexpr float kControlRadius = 6.0f;

/** Text in the system's own interface typeface: San Francisco on a Mac.
    `height` is the nominal size the layouts were made for (13 = ordinary
    control text); `bold` is the semibold cut used for headings. */
juce::Font font (float height, bool bold = false);

/** The system typeface, for controls that JUCE draws with its default font. */
juce::Typeface::Ptr systemTypeface();

/** Which faces font() resolved to, for the self-test's log. */
juce::String fontDescription();

/** The VioLogic emblem (violin scroll and audio wave in a ring), without the
    word mark, on a transparent background; from assets/Emblem.png. */
juce::Image emblem();

/** Draws the emblem centred in `area`, as large as fits, at the given opacity. */
void drawEmblem (juce::Graphics&, juce::Rectangle<float> area, float opacity = 1.0f);

/** The frames of the animated logo for the start screen (assets/splash), in
    order; empty if the build has none. Decoded on each call: ask once. */
std::vector<juce::Image> splashFrames();

/** How fast the start animation plays: its 30 frames in a little under two seconds. */
inline constexpr double kSplashFramesPerSecond = 16.0;

/** The full app icon: rounded square in the macOS proportions with the emblem. */
void drawIcon (juce::Graphics&, juce::Rectangle<float> area);

/** Writes the icon as a square PNG. Used to produce assets/AppIcon.png. */
bool renderIconPng (const juce::File& file, int size);

} // namespace brand
