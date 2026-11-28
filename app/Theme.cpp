#include "Theme.h"

#include "Brand.h"
#include "Widgets.h"

using namespace brand;

namespace
{
    constexpr float kRadius = kControlRadius;   // controls; nothing else in the app is rounded more
    const juce::Identifier kSwitchStyleKey ("viologicSwitchStyle");
    const juce::Identifier kStatusDotKey ("viologicStatusDot");
}

void setSwitchStyle (juce::Component& c, SwitchStyle style)
{
    c.getProperties().set (kSwitchStyleKey, (int) style);
    c.repaint();
}

SwitchStyle switchStyle (const juce::Component& c)
{
    const auto v = c.getProperties()[kSwitchStyleKey];
    return v.isInt() ? (SwitchStyle) (int) v : SwitchStyle::plain;
}

void setStatusDot (juce::Button& b, bool moduleOn)
{
    const auto before = b.getProperties()[kStatusDotKey];
    if (before.isBool() && (bool) before == moduleOn)
        return;
    b.getProperties().set (kStatusDotKey, moduleOn);
    b.repaint();
}

Theme::Theme()
{
    setColourScheme ({ colour::window, colour::control, colour::control, colour::separator,
                       colour::text, colour::selected, colour::text, colour::selected, colour::text });

    // Everything JUCE draws with its default font (dialogs, text fields) uses the system typeface too.
    if (auto face = systemTypeface())
        setDefaultSansSerifTypeface (face);

    setColour (juce::ResizableWindow::backgroundColourId, colour::window);
    setColour (juce::DocumentWindow::textColourId, colour::text);
    setColour (juce::Label::textColourId, colour::text);

    setColour (juce::TextButton::buttonColourId, colour::control);
    setColour (juce::TextButton::buttonOnColourId, colour::selected);
    setColour (juce::TextButton::textColourOffId, colour::text);
    setColour (juce::TextButton::textColourOnId, colour::text);

    setColour (juce::ComboBox::backgroundColourId, colour::control);
    setColour (juce::ComboBox::outlineColourId, colour::control);
    setColour (juce::ComboBox::textColourId, colour::text);
    setColour (juce::ComboBox::arrowColourId, colour::secondary);
    setColour (juce::ComboBox::focusedOutlineColourId, colour::crimson);

    setColour (juce::PopupMenu::backgroundColourId, colour::control);
    setColour (juce::PopupMenu::textColourId, colour::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colour::crimsonDim);
    setColour (juce::PopupMenu::highlightedTextColourId, colour::text);

    setColour (juce::Slider::textBoxTextColourId, colour::text);
    setColour (juce::Slider::textBoxBackgroundColourId, colour::control);
    setColour (juce::Slider::textBoxOutlineColourId, colour::control);

    setColour (juce::TextEditor::backgroundColourId, colour::display);
    setColour (juce::TextEditor::textColourId, colour::secondary);
    setColour (juce::TextEditor::outlineColourId, colour::separator);
    setColour (juce::TextEditor::focusedOutlineColourId, colour::crimson);
    setColour (juce::TextEditor::highlightColourId, colour::selected);
    setColour (juce::CaretComponent::caretColourId, colour::text);

    setColour (juce::ListBox::backgroundColourId, colour::display);
    setColour (juce::ListBox::outlineColourId, colour::separator);
    setColour (juce::ListBox::textColourId, colour::text);
    setColour (juce::ToggleButton::textColourId, colour::text);
    setColour (juce::ToggleButton::tickColourId, colour::text);
    setColour (juce::ToggleButton::tickDisabledColourId, colour::tertiary);
    setColour (juce::ScrollBar::thumbColourId, colour::selected);

    setColour (juce::TooltipWindow::backgroundColourId, colour::control);
    setColour (juce::TooltipWindow::textColourId, colour::text);
    setColour (juce::TooltipWindow::outlineColourId, colour::separator);
}

//==============================================================================
juce::Font Theme::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return font (juce::jmin (13.0f, (float) buttonHeight * 0.54f));
}

void Theme::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& background,
                                  bool highlighted, bool down)
{
    // The segments of a segmented control are drawn by the control as one piece.
    if (dynamic_cast<ButtonRow*> (b.getParentComponent()) != nullptr)
        return;

    const auto r = b.getLocalBounds().toFloat();
    const bool on = b.getToggleState();

    auto fill = background;
    switch (switchStyle (b))
    {
        case SwitchStyle::status: fill = on ? colour::emeraldDim : colour::crimsonDim; break;
        case SwitchStyle::latch:  fill = on ? colour::emeraldDim : background; break;
        case SwitchStyle::plain:  break;
    }

    if (! b.isEnabled())
        fill = fill.withMultipliedAlpha (0.5f);
    else if (down)
        fill = fill.darker (0.15f);
    else if (highlighted && ! on)
        fill = fill.brighter (0.06f);

    const bool flatLeft = b.isConnectedOnLeft(), flatRight = b.isConnectedOnRight();
    const bool flatTop = b.isConnectedOnTop(), flatBottom = b.isConnectedOnBottom();

    juce::Path p;
    p.addRoundedRectangle (r.getX(), r.getY(), r.getWidth(), r.getHeight(), kRadius, kRadius,
                           ! (flatLeft || flatTop), ! (flatRight || flatTop),
                           ! (flatLeft || flatBottom), ! (flatRight || flatBottom));
    g.setColour (fill);
    g.fillPath (p);

    // segments are told apart by a hairline, not by gaps
    if (flatRight && ! on)
    {
        g.setColour (colour::window.withAlpha (0.8f));
        g.fillRect (r.getRight() - 1.0f, r.getY() + 5.0f, 1.0f, r.getHeight() - 10.0f);
    }

    // the status dot of a module button
    const auto dot = b.getProperties()[kStatusDotKey];
    if (dot.isBool())
    {
        const float d = 7.0f;
        const auto area = juce::Rectangle<float> (d, d).withCentre ({ r.getX() + 13.0f, r.getCentreY() });
        g.setColour (((bool) dot ? colour::emerald : colour::crimson).withMultipliedAlpha (b.isEnabled() ? 1.0f : 0.5f));
        g.fillEllipse (area);
    }
}

//==============================================================================
juce::Font Theme::getComboBoxFont (juce::ComboBox&)
{
    return font (13.0f);
}

juce::Font Theme::getPopupMenuFont()
{
    return font (13.5f);
}

void Theme::drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<float> ((float) width, (float) height);
    g.setColour (box.isEnabled() && box.isMouseOver (true) ? colour::controlHi : colour::control);
    g.fillRoundedRectangle (r, kRadius);

    // the active field: a fine crimson frame while its list is open or it has the keyboard
    if (box.isEnabled() && (box.isPopupActive() || box.hasKeyboardFocus (true)))
    {
        g.setColour (colour::crimson);
        g.drawRoundedRectangle (r.reduced (0.5f), kRadius, 1.0f);
    }

    // up/down marks, as macOS pop-up buttons have
    const float cx = (float) width - 13.0f, cy = (float) height * 0.5f;
    juce::Path marks;
    marks.startNewSubPath (cx - 3.0f, cy - 2.0f);
    marks.lineTo (cx, cy - 5.0f);
    marks.lineTo (cx + 3.0f, cy - 2.0f);
    marks.startNewSubPath (cx - 3.0f, cy + 2.0f);
    marks.lineTo (cx, cy + 5.0f);
    marks.lineTo (cx + 3.0f, cy + 2.0f);
    g.setColour (colour::secondary.withAlpha (box.isEnabled() ? 1.0f : 0.4f));
    g.strokePath (marks, juce::PathStrokeType (1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void Theme::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (6, 0, box.getWidth() - 26, box.getHeight());
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

    g.setColour (colour::control.brighter (0.12f));
    g.fillRoundedRectangle (track, 2.0f);
    g.setColour (enabled ? colour::crimson : colour::tertiary.withAlpha (0.5f));
    g.fillRoundedRectangle (track.withRight (sliderPos), 2.0f);

    // the round knob of a macOS slider in brushed silver, with a thin dark edge instead of a shadow
    const auto thumb = juce::Rectangle<float> (14.0f, 14.0f).withCentre ({ sliderPos, cy });
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillEllipse (thumb.expanded (1.0f));
    if (enabled)
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff4f6f8), thumb.getX(), thumb.getY(),
                                                 colour::silver.darker (0.18f), thumb.getX(), thumb.getBottom(), false));
    else
        g.setColour (colour::tertiary);
    g.fillEllipse (thumb);
}

//==============================================================================
// Pop-up panels: flat, one hairline, no shadow.
void Theme::drawCallOutBoxBackground (juce::CallOutBox&, juce::Graphics& g, const juce::Path& path, juce::Image&)
{
    g.setColour (brand::colour::window);
    g.fillPath (path);
    g.setColour (brand::colour::separator.brighter (0.35f));
    g.strokePath (path, juce::PathStrokeType (1.0f));
}

void Theme::drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height)
{
    const auto r = juce::Rectangle<float> ((float) width, (float) height);
    g.setColour (colour::control);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (colour::separator.brighter (0.2f));
    g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);

    juce::AttributedString s;
    s.setJustification (juce::Justification::centredLeft);
    s.append (text, font (12.5f), colour::text);
    juce::TextLayout layout;
    layout.createLayoutWithBalancedLineLengths (s, 300.0f);
    layout.draw (g, r.reduced (9.0f, 6.0f));
}

juce::Rectangle<int> Theme::getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos,
                                              juce::Rectangle<int> parentArea)
{
    juce::AttributedString s;
    s.append (tipText, font (12.5f));
    juce::TextLayout layout;
    layout.createLayoutWithBalancedLineLengths (s, 300.0f);

    const int w = (int) std::ceil (layout.getWidth()) + 18;
    const int h = (int) std::ceil (layout.getHeight()) + 12;
    return juce::Rectangle<int> (screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 24,
                                 screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 6) : screenPos.y + 6,
                                 w, h).constrainedWithin (parentArea);
}
