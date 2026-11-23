#include "AudioPanel.h"

#include "Brand.h"

namespace
{
    constexpr int kWidth = 400;
    constexpr int kMargin = 16;
    constexpr int kContentW = kWidth - 2 * kMargin;     // 368

    constexpr int kHeadingY = 14, kHeadingH = 20;
    constexpr int kInfoY = 36, kInfoH = 16;
    constexpr int kSwitchY = 62, kSwitchW = 96, kSwitchH = 26, kSwitchGap = 14;   // row to 88
    constexpr int kSep1Y = 98;

    // Six blocks, one column: name line, slider below.
    constexpr int kBlock1Y = 108, kBlockPitch = 46;      // 108, 154, 200, 246, 292, 338
    constexpr int kNameH = 18;
    constexpr int kSliderOffset = 20, kSliderH = 24;     // block ends 44 below its top (338 + 44 = 382)
    constexpr int kSep2Y = 388;

    constexpr int kDryY = 400;                           // row to 426
    constexpr int kHeight = kDryY + kSwitchH + kHeadingY;   // 440

    juce::String U (const char* utf8) { return juce::String::fromUTF8 (utf8); }

    // The real minus sign and the infinity sign, as strings so digits after them are no hex digits.
    const juce::String& minus()
    {
        static const juce::String m = U ("\xe2\x88\x92");
        return m;
    }

    const juce::String& minusInfinity()
    {
        static const juce::String m = U ("\xe2\x88\x92\xe2\x88\x9e");
        return m;
    }

    // Decimal comma: 1.5 -> "1,5".
    juce::String oneDecimal (double v)
    {
        return juce::String (v, 1).replaceCharacter ('.', ',');
    }

    const char* const kLevelTip =
        "Regelt die Gesamtlautst\xc3\xa4rke der Audio-Harmonien. Dry-Kill schaltet das Originalsignal stumm "
        "(ideal f\xc3\xbc" "r parallele Effektschleifen).";
}

//==============================================================================
AudioPanel::Switch::Switch (int radioGroup, const juce::String& tip)
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
}

void AudioPanel::Switch::set (bool on)
{
    onButton.setToggleState (on, juce::dontSendNotification);
    offButton.setToggleState (! on, juce::dontSendNotification);
}

//==============================================================================
void AudioPanel::setupBlock (Block& b, const juce::String& label, Kind kind, double low, double high, double step,
                             double defaultValue, const juce::String& tip)
{
    b.label = label;
    b.kind = kind;

    auto& s = b.slider;
    s.setSliderStyle (juce::Slider::LinearHorizontal);
    s.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    s.setRange (low, high, step);
    s.setDoubleClickReturnValue (true, defaultValue);
    s.setWantsKeyboardFocus (false);
    s.setTooltip (tip);
    s.onValueChange = [this] { changed(); };
    addAndMakeVisible (s);
}

AudioPanel::AudioPanel (const Values& v, const juce::String& info)
    : current (v),
      outputInfo (info),
      onSwitch (1, U ("Spielt die Harmoniestimmen zus\xc3\xa4tzlich mit dem eigenen Klang der Geige: Das Eingangssignal wird in der "
                      "Tonh\xc3\xb6he verschoben und \xc3\xbc" "ber den Audio-Ausgang ausgegeben. Es gelten Tonart, Voicing und "
                      "Harmonie-Modus der Harmonie; die Harmonie muss daf\xc3\xbc" "r eingeschaltet sein. Die Stimmen kommen etwa "
                      "eine halbe Schwingung des gespielten Tons sp\xc3\xa4ter: unter 2 ms in hohen Lagen, bis 12 ms auf der tiefsten Saite.")),
      dryKillSwitch (2, U (kLevelTip))
{
    setupBlock (blocks[0], U ("Korpus-Erhalt (Formant)"), percent, 0.0, 100.0, 1.0, 100.0,
                U ("Bewahrt die nat\xc3\xbcrliche Resonanz des Geigenkorpus beim Pitch-Shifting. "
                   "Niedrige Werte lassen tiefe Oktaven wie ein Cello klingen."));
    setupBlock (blocks[1], U ("Bogen-Variation (Humanize)"), percent, 0.0, 100.0, 1.0, 20.0,
                U ("F\xc3\xbcgt den Harmoniestimmen minimale Tonh\xc3\xb6hen- und Zeitabweichungen hinzu, um Kammfilter-Effekte "
                   "zu vermeiden und ein echtes Ensemble zu simulieren."));
    setupBlock (blocks[2], U ("Vibrato-Kopplung"), percent, 0.0, 100.0, 1.0, 100.0,
                U ("Bestimmt, wie stark dein Vibrato auf die Harmoniestimmen \xc3\xbc" "bertragen wird. "
                   "Niedrigere Werte sorgen f\xc3\xbcr einen ruhigeren, edleren Streicherteppich."));
    setupBlock (blocks[3], U ("Stereo-Panorama (Spread)"), percent, 0.0, 100.0, 1.0, 50.0,
                U ("Verteilt die erzeugten Harmoniestimmen links und rechts im Stereobild, "
                   "w\xc3\xa4hrend die Solo-Geige in der Mitte bleibt."));
    setupBlock (blocks[4], U ("Piezo-Gl\xc3\xa4ttung (Hi-Cut)"), kilohertz, 4.0, 16.0, 0.5, 10.0,
                U ("Sanfter H\xc3\xb6" "henfilter nur f\xc3\xbcr die Harmoniestimmen, um scharfes Bogenrauschen und Piezo-Spitzen "
                   "weich hinter der Solo-Geige einzubetten."));
    setupBlock (blocks[5], U ("Harmonie-Pegel"), decibel, -60.0, 6.0, 0.5, 0.0, U (kLevelTip));

    blocks[0].slider.setValue (current.bodyKeep * 100.0, juce::dontSendNotification);
    blocks[1].slider.setValue (current.humanize * 100.0, juce::dontSendNotification);
    blocks[2].slider.setValue (current.vibratoCoupling * 100.0, juce::dontSendNotification);
    blocks[3].slider.setValue (current.stereoSpread * 100.0, juce::dontSendNotification);
    blocks[4].slider.setValue (current.hiCutHz / 1000.0, juce::dontSendNotification);
    blocks[5].slider.setValue (current.harmonyGainDb, juce::dontSendNotification);

    onSwitch.set (current.on);
    dryKillSwitch.set (current.dryKill);
    for (auto* s : { &onSwitch, &dryKillSwitch })
    {
        addAndMakeVisible (s->row);
        s->offButton.onClick = [this] { changed(); };
        s->onButton.onClick = [this] { changed(); };
    }
    for (auto& b : blocks)
        b.slider.setEnabled (current.on);

    setSize (kWidth, kHeight);
}

void AudioPanel::setOutputInfo (const juce::String& text, bool isProblem)
{
    outputInfo = text;
    outputIsProblem = isProblem;
    repaint();
}

juce::String AudioPanel::valueText (const Block& b) const
{
    const double v = b.slider.getValue();

    switch (b.kind)
    {
        case percent:
            return juce::String (juce::roundToInt (v)) + " %";

        case kilohertz:
            return oneDecimal (v) + " kHz";

        case decibel:
            if (v <= b.slider.getMinimum() + 0.01)
                return minusInfinity();
            if (v < 0.0)
                return minus() + oneDecimal (-v) + " dB";
            return (v > 0.0 ? juce::String ("+") : juce::String()) + oneDecimal (v) + " dB";
    }
    return {};
}

void AudioPanel::changed()
{
    current.on = onSwitch.isOn();
    current.bodyKeep = (float) (blocks[0].slider.getValue() / 100.0);
    current.humanize = (float) (blocks[1].slider.getValue() / 100.0);
    current.vibratoCoupling = (float) (blocks[2].slider.getValue() / 100.0);
    current.stereoSpread = (float) (blocks[3].slider.getValue() / 100.0);
    current.hiCutHz = (float) (blocks[4].slider.getValue() * 1000.0);
    current.harmonyGainDb = (float) blocks[5].slider.getValue();
    current.dryKill = dryKillSwitch.isOn();

    for (auto& b : blocks)
        b.slider.setEnabled (current.on);
    repaint();

    if (onChange)
        onChange (current);
}

void AudioPanel::resized()
{
    onSwitch.row.setBounds (kMargin, kSwitchY, kSwitchW, kSwitchH);
    dryKillSwitch.row.setBounds (kMargin, kDryY, kSwitchW, kSwitchH);

    int top = kBlock1Y;
    for (auto& b : blocks)
    {
        b.slider.setBounds (kMargin, top + kSliderOffset, kContentW, kSliderH);
        top += kBlockPitch;
    }
}

void AudioPanel::paint (juce::Graphics& g)
{
    using namespace brand;

    g.setColour (colour::text);
    g.setFont (font (13.0f, true));
    g.drawText ("Audio-Harmonizer", juce::Rectangle<int> (kMargin, kHeadingY, kContentW, kHeadingH),
                juce::Justification::centredLeft, false);

    g.setFont (font (11.5f));
    g.setColour (outputIsProblem ? colour::problem : colour::secondary);
    g.drawText (outputInfo, juce::Rectangle<int> (kMargin, kInfoY, kContentW, kInfoH),
                juce::Justification::centredLeft, true);

    // Labels to the right of the two switches.
    const int labelX = kMargin + kSwitchW + kSwitchGap;
    g.setFont (font (12.5f));
    g.setColour (colour::secondary);
    g.drawText ("Audio-Harmonizer", juce::Rectangle<int> (labelX, kSwitchY, kContentW - kSwitchW - kSwitchGap, kSwitchH),
                juce::Justification::centredLeft, false);
    g.drawText ("Dry-Kill", juce::Rectangle<int> (labelX, kDryY, kContentW - kSwitchW - kSwitchGap, kSwitchH),
                juce::Justification::centredLeft, false);

    g.setColour (colour::separator);
    g.fillRect (kMargin, kSep1Y, kContentW, 1);
    g.fillRect (kMargin, kSep2Y, kContentW, 1);

    const float alpha = current.on ? 1.0f : 0.5f;
    int top = kBlock1Y;
    for (const auto& b : blocks)
    {
        const juce::Rectangle<int> line (kMargin, top, kContentW, kNameH);

        g.setFont (font (12.5f));
        g.setColour (colour::secondary.withMultipliedAlpha (alpha));
        g.drawText (b.label, line, juce::Justification::centredLeft, false);

        g.setColour (colour::text.withMultipliedAlpha (alpha));
        g.drawText (valueText (b), line, juce::Justification::centredRight, false);

        top += kBlockPitch;
    }
}
