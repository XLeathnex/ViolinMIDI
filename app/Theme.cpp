#include "Theme.h"

#include "Brand.h"

using namespace brand;

namespace
{
    constexpr float kRadius = 7.0f;
}

Theme::Theme()
{
    setColourScheme ({ colour::window, colour::raised, colour::raised, colour::line,
                       colour::text, colour::accent, colour::window, colour::accent, colour::text });

    setColour (juce::ResizableWindow::backgroundColourId, colour::window);
    setColour (juce::DocumentWindow::textColourId, colour::text);
    setColour (juce::Label::textColourId, colour::text);

    setColour (juce::TextButton::buttonColourId, colour::raised);
    setColour (juce::TextButton::buttonOnColourId, colour::accent);
    setColour (juce::TextButton::textColourOffId, colour::text);
    setColour (juce::TextButton::textColourOnId, colour::window);

    setColour (juce::ComboBox::backgroundColourId, colour::raised);
    setColour (juce::ComboBox::outlineColourId, colour::line);
    setColour (juce::ComboBox::textColourId, colour::text);
    setColour (juce::ComboBox::arrowColourId, colour::textDim);
    setColour (juce::ComboBox::focusedOutlineColourId, colour::accent);

    setColour (juce::PopupMenu::backgroundColourId, colour::raised);
    setColour (juce::PopupMenu::textColourId, colour::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colour::accent.withAlpha (0.20f));
    setColour (juce::PopupMenu::highlightedTextColourId, colour::text);

    setColour (juce::Slider::trackColourId, colour::accent);
    setColour (juce::Slider::backgroundColourId, colour::line);
    setColour (juce::Slider::thumbColourId, colour::text);
    setColour (juce::Slider::textBoxTextColourId, colour::text);
    setColour (juce::Slider::textBoxBackgroundColourId, colour::raised);
    setColour (juce::Slider::textBoxOutlineColourId, colour::line);
    setColour (juce::Slider::textBoxHighlightColourId, colour::accent.withAlpha (0.35f));

    setColour (juce::TextEditor::backgroundColourId, colour::panel);
    setColour (juce::TextEditor::textColourId, colour::textDim);
    setColour (juce::TextEditor::outlineColourId, colour::line);
    setColour (juce::TextEditor::focusedOutlineColourId, colour::accent);
    setColour (juce::TextEditor::highlightColourId, colour::accent.withAlpha (0.35f));
    setColour (juce::CaretComponent::caretColourId, colour::accent);

    setColour (juce::ListBox::backgroundColourId, colour::raised);
    setColour (juce::ListBox::outlineColourId, colour::line);
    setColour (juce::ListBox::textColourId, colour::text);
    setColour (juce::ToggleButton::textColourId, colour::text);
    setColour (juce::ToggleButton::tickColourId, colour::accent);
    setColour (juce::ToggleButton::tickDisabledColourId, colour::textFaint);
    setColour (juce::ScrollBar::thumbColourId, colour::line.brighter (0.3f));

    setColour (juce::TooltipWindow::backgroundColourId, colour::raised);
    setColour (juce::TooltipWindow::textColourId, colour::text);
    setColour (juce::TooltipWindow::outlineColourId, colour::line);
}

//==============================================================================
juce::Font Theme::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return font (juce::jmin (13.5f, (float) buttonHeight * 0.52f));
}

void Theme::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& background,
                                  bool highlighted, bool down)
{
    const auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();

    auto fill = background;
    if (! b.isEnabled())
        fill = fill.withMultipliedAlpha (0.45f);
    else if (down)
        fill = on ? fill.darker (0.12f) : colour::window;
    else if (highlighted)
        fill = on ? fill.brighter (0.08f) : colour::hover;

    const bool flatLeft = b.isConnectedOnLeft(), flatRight = b.isConnectedOnRight();
    const bool flatTop = b.isConnectedOnTop(), flatBottom = b.isConnectedOnBottom();

    juce::Path p;
    p.addRoundedRectangle (r.getX(), r.getY(), r.getWidth(), r.getHeight(), kRadius, kRadius,
                           ! (flatLeft || flatTop), ! (flatRight || flatTop),
                           ! (flatLeft || flatBottom), ! (flatRight || flatBottom));
    g.setColour (fill);
    g.fillPath (p);

    g.setColour (on ? fill : colour::line);
    g.strokePath (p, juce::PathStrokeType (1.0f));
}

//==============================================================================
juce::Font Theme::getComboBoxFont (juce::ComboBox&)
{
    return font (13.5f);
}

juce::Font Theme::getPopupMenuFont()
{
    return font (14.0f);
}

void Theme::drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<float> ((float) width, (float) height).reduced (0.5f);
    const bool hover = box.isEnabled() && box.isMouseOver (true);

    g.setColour (hover ? colour::hover : colour::raised);
    g.fillRoundedRectangle (r, kRadius);
    g.setColour (box.hasKeyboardFocus (true) ? colour::accent.withAlpha (0.6f) : colour::line);
    g.drawRoundedRectangle (r, kRadius, 1.0f);

    const float cx = (float) width - 15.0f, cy = (float) height * 0.5f;
    juce::Path chevron;
    chevron.startNewSubPath (cx - 4.0f, cy - 2.0f);
    chevron.lineTo (cx, cy + 2.0f);
    chevron.lineTo (cx + 4.0f, cy - 2.0f);
    g.setColour (colour::textDim.withAlpha (box.isEnabled() ? 1.0f : 0.4f));
    g.strokePath (chevron, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void Theme::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 1, box.getWidth() - 30, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

//==============================================================================
int Theme::getSliderThumbRadius (juce::Slider&)
{
    return 7;
}

void Theme::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                              float sliderPos, float minSliderPos, float maxSliderPos,
                              juce::Slider::SliderStyle style, juce::Slider& slider)
{
    if (style != juce::Slider::LinearHorizontal)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);
        return;
    }

    const bool enabled = slider.isEnabled();
    const float cy = (float) y + (float) height * 0.5f;
    const juce::Rectangle<float> track ((float) x, cy - 2.0f, (float) width, 4.0f);

    g.setColour (colour::line);
    g.fillRoundedRectangle (track, 2.0f);
    g.setColour (colour::accent.withAlpha (enabled ? 0.9f : 0.25f));
    g.fillRoundedRectangle (track.withRight (sliderPos), 2.0f);

    const float r = 7.0f;
    const auto thumb = juce::Rectangle<float> (2.0f * r, 2.0f * r).withCentre ({ sliderPos, cy });
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillEllipse (thumb.translated (0.0f, 1.0f));
    g.setColour (enabled ? (slider.isMouseOverOrDragging() ? juce::Colours::white : colour::text) : colour::textFaint);
    g.fillEllipse (thumb);
}

//==============================================================================
void Theme::drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height)
{
    const auto r = juce::Rectangle<float> ((float) width, (float) height);
    g.setColour (colour::raised);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (colour::line);
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);

    juce::AttributedString s;
    s.setJustification (juce::Justification::centredLeft);
    s.append (text, font (13.0f), colour::text);
    juce::TextLayout layout;
    layout.createLayoutWithBalancedLineLengths (s, 320.0f);
    layout.draw (g, r.reduced (10.0f, 7.0f));
}

juce::Rectangle<int> Theme::getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos,
                                              juce::Rectangle<int> parentArea)
{
    juce::AttributedString s;
    s.append (tipText, font (13.0f));
    juce::TextLayout layout;
    layout.createLayoutWithBalancedLineLengths (s, 320.0f);

    const int w = (int) std::ceil (layout.getWidth()) + 20;
    const int h = (int) std::ceil (layout.getHeight()) + 14;
    return juce::Rectangle<int> (screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 24,
                                 screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 6) : screenPos.y + 6,
                                 w, h).constrainedWithin (parentArea);
}
