// The look of every standard control: buttons, menus, sliders, lists.
// Set once as the default, so new controls look right without extra work.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/** How a button or a two-segment ButtonRow ("Aus | An") shows its state. */
enum class SwitchStyle
{
    plain,    ///< as any other button or segmented control
    status,   ///< a module or main switch: ON on an emerald plate, OFF on a crimson one
    latch     ///< a function that is either engaged or not (freeze): ON emerald, OFF plain
};

/** Gives a TextButton that toggles, or a ButtonRow of two segments (OFF first,
    then ON), one of the switch styles. The Theme and ButtonRow draw it. */
void setSwitchStyle (juce::Component&, SwitchStyle);
SwitchStyle switchStyle (const juce::Component&);

/** A small status dot at the left of a button that opens a module's window
    (Audio-Harmonizer, Kamera): emerald while the module is on, crimson while off. */
void setStatusDot (juce::Button&, bool moduleOn);

class Theme final : public juce::LookAndFeel_V4
{
public:
    Theme();

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool highlighted, bool down) override;

    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    void drawComboBox (juce::Graphics&, int width, int height, bool down,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

    int getSliderThumbRadius (juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;

    void drawCallOutBoxBackground (juce::CallOutBox&, juce::Graphics&, const juce::Path&, juce::Image&) override;

    void drawTooltip (juce::Graphics&, const juce::String& text, int width, int height) override;
    juce::Rectangle<int> getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos,
                                           juce::Rectangle<int> parentArea) override;
};
