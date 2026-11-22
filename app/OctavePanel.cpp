#include "OctavePanel.h"

#include "Brand.h"

namespace
{
    constexpr int kWidth = 380, kHeight = 184;
    constexpr int kMargin = 16;
    constexpr int kContentW = kWidth - 2 * kMargin;     // 348

    constexpr int kHeadingY = 14, kHeadingH = 20;
    constexpr int kBlock1Y = 46, kBlock2Y = 120;
    constexpr int kSeparatorY = 107;
    constexpr int kNameH = 18;
    constexpr int kRowOffset = 22, kRowH = 26;           // row below the name line
    constexpr int kSwitchW = 96, kSwitchGap = 14;

    juce::String U (const char* utf8) { return juce::String::fromUTF8 (utf8); }
}

//==============================================================================
OctavePanel::Block::Block (int radioGroup, const juce::String& name, double maxMs, double defaultMs, const juce::String& tip)
    : label (name)
{
    for (auto* b : { &offButton, &onButton })
    {
        b->setClickingTogglesState (true);
        b->setRadioGroupId (radioGroup);
        b->setWantsKeyboardFocus (false);
        b->setTooltip (tip);
        row.add (*b);
    }
    offButton.setConnectedEdges (juce::Button::ConnectedOnRight);
    onButton.setConnectedEdges (juce::Button::ConnectedOnLeft);

    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    slider.setRange (0.0, maxMs, 1.0);
    slider.setDoubleClickReturnValue (true, defaultMs);
    slider.setTooltip (tip);
}

void OctavePanel::Block::setValues (bool on, double ms)
{
    onButton.setToggleState (on, juce::dontSendNotification);
    offButton.setToggleState (! on, juce::dontSendNotification);
    slider.setValue (ms, juce::dontSendNotification);
    slider.setEnabled (on);
}

//==============================================================================
OctavePanel::OctavePanel (const Values& v)
    : current (v),
      attack (1, U ("Anstreich-Schutz"), 60.0, 50.0,
              U ("Anstreich-Schutz (Attack): Wartet beim Bogenansatz kurz ab und pr\xc3\xbc" "ft auf den tieferen Grundton, "
                 "damit tiefe Saiten beim Anstreichen nicht versehentlich eine Oktave zu hoch starten.")),
      sustain (2, U ("Oktavsprung-Sperre"), 100.0, 11.0,
               U ("Oktavsprung-Sperre (Sustain): Filtert kurze, versehentliche Oktav-Ausrei\xc3\x9f" "er w\xc3\xa4hrend eines "
                  "gehaltenen Tons heraus. H\xc3\xb6here Werte machen den Ton stabiler, reagieren aber etwas tr\xc3\xa4ger "
                  "auf gewollte Oktavspr\xc3\xbcnge."))
{
    attack.setValues (current.attackOn, current.attackMs);
    sustain.setValues (current.sustainOn, current.sustainMs);

    for (auto* b : { &attack, &sustain })
    {
        addAndMakeVisible (b->row);
        addAndMakeVisible (b->slider);

        b->offButton.onClick = [this] { changed(); };
        b->onButton.onClick = [this] { changed(); };
        b->slider.onValueChange = [this] { changed(); };
    }

    setSize (kWidth, kHeight);
}

void OctavePanel::changed()
{
    current.attackOn = attack.onButton.getToggleState();
    current.attackMs = (float) attack.slider.getValue();
    current.sustainOn = sustain.onButton.getToggleState();
    current.sustainMs = (float) sustain.slider.getValue();

    attack.slider.setEnabled (current.attackOn);
    sustain.slider.setEnabled (current.sustainOn);
    repaint();

    if (onChange)
        onChange (current);
}

void OctavePanel::resized()
{
    const auto place = [this] (Block& block, int top)
    {
        const int y = top + kRowOffset;
        block.row.setBounds (kMargin, y, kSwitchW, kRowH);

        const int sliderX = kMargin + kSwitchW + kSwitchGap;
        block.slider.setBounds (sliderX, y, getWidth() - kMargin - sliderX, kRowH);
    };
    place (attack, kBlock1Y);
    place (sustain, kBlock2Y);
}

void OctavePanel::paintBlock (juce::Graphics& g, const Block& block, int top)
{
    using namespace brand;

    const float alpha = block.slider.isEnabled() ? 1.0f : 0.5f;
    const juce::Rectangle<int> line (kMargin, top, kContentW, kNameH);

    g.setFont (font (12.5f));
    g.setColour (colour::secondary.withMultipliedAlpha (alpha));
    g.drawText (block.label, line, juce::Justification::centredLeft, false);

    g.setColour (colour::text.withMultipliedAlpha (alpha));
    g.drawText (juce::String (juce::roundToInt (block.slider.getValue())) + " ms", line,
                juce::Justification::centredRight, false);
}

void OctavePanel::paint (juce::Graphics& g)
{
    using namespace brand;

    g.setColour (colour::text);
    g.setFont (font (13.0f, true));
    g.drawText ("Oktavsprung", juce::Rectangle<int> (kMargin, kHeadingY, kContentW, kHeadingH),
                juce::Justification::centredLeft, false);

    g.setColour (colour::separator);
    g.fillRect (kMargin, kSeparatorY, kContentW, 1);

    paintBlock (g, attack, kBlock1Y);
    paintBlock (g, sustain, kBlock2Y);
}
