#include "TrackingPanel.h"

#include "Brand.h"

namespace
{
    constexpr int kWidth = 400;
    constexpr int kMargin = 16;
    constexpr int kContentW = kWidth - 2 * kMargin;     // 368

    constexpr int kHeadingY = 14, kHeadingH = 20;
    constexpr int kNameH = 18;
    constexpr int kRowOffset = 22, kRowH = 26;           // row below a name line
    constexpr int kSwitchW = 96, kSwitchGap = 14;

    // Group tops (name line) and the hairlines between the groups.
    constexpr int kGroup1Y = 46;                         // row to 94
    constexpr int kSep1Y = 106;
    constexpr int kGroup2Y = 118;                        // row to 166
    constexpr int kSep2Y = 178;
    constexpr int kGroup3Y = 190;                        // name line to 208
    constexpr int kJumpSwitchY = kGroup3Y + kRowOffset;  // 212 .. 238
    constexpr int kSubRow1Y = 248, kSubRowPitch = 28, kSubRowH = 24;   // 248, 276, 304 (to 328)
    constexpr int kSep3Y = 340;
    constexpr int kGroup4Y = 352;                        // row to 400
    constexpr int kHeight = kGroup4Y + kRowOffset + kRowH + kHeadingY;   // 414

    // Sub rows: name left (110), slider, value right (76).
    constexpr int kSubNameW = 110, kSubValueW = 76, kSubGap = 6;
    constexpr int kSubSliderX = kMargin + kSubNameW + kSubGap;                      // 132
    constexpr int kSubSliderW = kContentW - kSubNameW - kSubValueW - 2 * kSubGap;    // 170
    constexpr int kSubValueX = kMargin + kContentW - kSubValueW;                    // 308

    juce::String U (const char* utf8) { return juce::String::fromUTF8 (utf8); }
}

//==============================================================================
TrackingPanel::Switch::Switch (int radioGroup, const juce::String& tip)
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

void TrackingPanel::Switch::set (bool on)
{
    onButton.setToggleState (on, juce::dontSendNotification);
    offButton.setToggleState (! on, juce::dontSendNotification);
}

//==============================================================================
void TrackingPanel::setupSlider (juce::Slider& s, double low, double high, double step, double defaultValue, const juce::String& tip)
{
    s.setSliderStyle (juce::Slider::LinearHorizontal);
    s.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    s.setRange (low, high, step);
    s.setDoubleClickReturnValue (true, defaultValue);
    s.setWantsKeyboardFocus (false);
    s.setTooltip (tip);
    s.onValueChange = [this] { changed(); };
    addAndMakeVisible (s);
}

TrackingPanel::TrackingPanel (const Values& v)
    : current (v),
      attackSwitch (1, U ("Anstreich-Schutz (Attack): Wartet beim Bogenansatz kurz ab und pr\xc3\xbc" "ft auf den tieferen Grundton, "
                          "damit tiefe Saiten beim Anstreichen nicht versehentlich eine Oktave zu hoch starten.")),
      sustainSwitch (2, U ("Oktavsprung-Sperre (Sustain): Filtert kurze, versehentliche Oktav-Ausrei\xc3\x9f" "er w\xc3\xa4hrend eines "
                           "gehaltenen Tons heraus. H\xc3\xb6here Werte machen den Ton stabiler, reagieren aber etwas tr\xc3\xa4ger "
                           "auf gewollte Oktavspr\xc3\xbcnge.")),
      jumpSwitch (3, U ("Filtert versehentlich angerissene Nachbarsaiten heraus, indem ungew\xc3\xb6hnlich gro\xc3\x9f" "e "
                        "Intervallspr\xc3\xbcnge kurzzeitig zur\xc3\xbc" "ckgehalten werden."))
{
    setupSlider (attackSlider, 0.0, 60.0, 1.0, 50.0,
                 U ("Anstreich-Schutz (Attack): Wartet beim Bogenansatz kurz ab und pr\xc3\xbc" "ft auf den tieferen Grundton, "
                    "damit tiefe Saiten beim Anstreichen nicht versehentlich eine Oktave zu hoch starten."));
    setupSlider (sustainSlider, 0.0, 100.0, 1.0, 11.0,
                 U ("Oktavsprung-Sperre (Sustain): Filtert kurze, versehentliche Oktav-Ausrei\xc3\x9f" "er w\xc3\xa4hrend eines "
                    "gehaltenen Tons heraus. H\xc3\xb6here Werte machen den Ton stabiler, reagieren aber etwas tr\xc3\xa4ger "
                    "auf gewollte Oktavspr\xc3\xbcnge."));
    setupSlider (intervalSlider, 5.0, 24.0, 1.0, 7.0,
                 U ("Mindestabstand in Halbt\xc3\xb6nen zum aktuellen Ton, ab dem ein pl\xc3\xb6tzlicher Tonh\xc3\xb6hensprung "
                    "als m\xc3\xb6gliche Fehlsaite gepr\xc3\xbc" "ft wird."));
    setupSlider (windowSlider, 10.0, 200.0, 5.0, 80.0,
                 U ("Zeitfenster nach einem Tonansatz oder Tonwechsel, innerhalb dessen gro\xc3\x9f" "e Intervallspr\xc3\xbcnge abgefangen werden."));
    setupSlider (lockSlider, 20.0, 150.0, 5.0, 50.0,
                 U ("Dauer, f\xc3\xbcr die ein erkannter gro\xc3\x9f" "er Intervallsprung unterdr\xc3\xbc" "ckt wird. "
                    "H\xc3\xa4lt der neue Ton l\xc3\xa4nger an, wird er als gewollt gespielt."));
    setupSlider (bridgeSlider, 0.0, 200.0, 5.0, 30.0,
                 U ("H\xc3\xa4lt den aktuellen Ton bei kurzen Erkennungsl\xc3\xbc" "cken (z. B. beim Bogenwechsel) fest, "
                    "solange der Pegel \xc3\xbc" "ber der Out-Schwelle bleibt. Auf den tiefen Saiten wirkt sie erst ab etwa 45 ms, "
                    "weil dort ohnehin so lange gewartet wird."));

    attackSwitch.set (current.attackOn);
    sustainSwitch.set (current.sustainOn);
    jumpSwitch.set (current.jumpOn);
    attackSlider.setValue (current.attackMs, juce::dontSendNotification);
    sustainSlider.setValue (current.sustainMs, juce::dontSendNotification);
    intervalSlider.setValue (current.jumpInterval, juce::dontSendNotification);
    windowSlider.setValue (current.jumpWindowMs, juce::dontSendNotification);
    lockSlider.setValue (current.jumpLockMs, juce::dontSendNotification);
    bridgeSlider.setValue (current.bridgeMs, juce::dontSendNotification);

    for (auto* s : { &attackSwitch, &sustainSwitch, &jumpSwitch })
    {
        addAndMakeVisible (s->row);
        s->offButton.onClick = [this] { changed(); };
        s->onButton.onClick = [this] { changed(); };
    }

    // The enabled state follows the switches.
    attackSlider.setEnabled (current.attackOn);
    sustainSlider.setEnabled (current.sustainOn);
    for (auto* s : { &intervalSlider, &windowSlider, &lockSlider })
        s->setEnabled (current.jumpOn);

    setSize (kWidth, kHeight);
}

void TrackingPanel::changed()
{
    current.attackOn = attackSwitch.isOn();
    current.attackMs = (float) attackSlider.getValue();
    current.sustainOn = sustainSwitch.isOn();
    current.sustainMs = (float) sustainSlider.getValue();
    current.jumpOn = jumpSwitch.isOn();
    current.jumpInterval = juce::roundToInt (intervalSlider.getValue());
    current.jumpWindowMs = (float) windowSlider.getValue();
    current.jumpLockMs = (float) lockSlider.getValue();
    current.bridgeMs = (float) bridgeSlider.getValue();

    attackSlider.setEnabled (current.attackOn);
    sustainSlider.setEnabled (current.sustainOn);
    for (auto* s : { &intervalSlider, &windowSlider, &lockSlider })
        s->setEnabled (current.jumpOn);
    repaint();

    if (onChange)
        onChange (current);
}

void TrackingPanel::resized()
{
    const auto placeBlock = [this] (Switch& sw, juce::Slider& slider, int top)
    {
        const int y = top + kRowOffset;
        sw.row.setBounds (kMargin, y, kSwitchW, kRowH);

        const int sliderX = kMargin + kSwitchW + kSwitchGap;
        slider.setBounds (sliderX, y, getWidth() - kMargin - sliderX, kRowH);
    };
    placeBlock (attackSwitch, attackSlider, kGroup1Y);
    placeBlock (sustainSwitch, sustainSlider, kGroup2Y);

    jumpSwitch.row.setBounds (kMargin, kJumpSwitchY, kSwitchW, kRowH);
    int y = kSubRow1Y;
    for (auto* s : { &intervalSlider, &windowSlider, &lockSlider })
    {
        s->setBounds (kSubSliderX, y, kSubSliderW, kSubRowH);
        y += kSubRowPitch;
    }

    bridgeSlider.setBounds (kMargin, kGroup4Y + kRowOffset, kContentW, kRowH);
}

void TrackingPanel::paint (juce::Graphics& g)
{
    using namespace brand;

    g.setColour (colour::text);
    g.setFont (font (13.0f, true));
    g.drawText ("Tracking-Filter", juce::Rectangle<int> (kMargin, kHeadingY, kContentW, kHeadingH),
                juce::Justification::centredLeft, false);

    for (const int y : { kSep1Y, kSep2Y, kSep3Y })
    {
        g.setColour (colour::separator);
        g.fillRect (kMargin, y, kContentW, 1);
    }

    // Name line with the value on the right; dimmed while the control is off.
    const auto nameLine = [&g] (const juce::String& name, const juce::String& value, int top, bool enabled)
    {
        const float alpha = enabled ? 1.0f : 0.5f;
        const juce::Rectangle<int> line (kMargin, top, kContentW, kNameH);

        g.setFont (font (12.5f));
        g.setColour (colour::secondary.withMultipliedAlpha (alpha));
        g.drawText (name, line, juce::Justification::centredLeft, false);

        g.setColour (colour::text.withMultipliedAlpha (alpha));
        g.drawText (value, line, juce::Justification::centredRight, false);
    };

    nameLine (U ("Anstreich-Schutz"), juce::String (juce::roundToInt (attackSlider.getValue())) + " ms",
              kGroup1Y, current.attackOn);
    nameLine (U ("Oktavsprung-Sperre"), juce::String (juce::roundToInt (sustainSlider.getValue())) + " ms",
              kGroup2Y, current.sustainOn);
    nameLine (U ("\xc3\x9c" "berbr\xc3\xbc" "ckung (Haltezeit)"), juce::String (juce::roundToInt (bridgeSlider.getValue())) + " ms",
              kGroup4Y, true);

    // Jump guard: heading line, then the three sub rows.
    g.setFont (font (12.5f));
    g.setColour (colour::secondary);
    g.drawText ("Sprungschutz", juce::Rectangle<int> (kMargin, kGroup3Y, kContentW, kNameH),
                juce::Justification::centredLeft, false);

    const float alpha = current.jumpOn ? 1.0f : 0.5f;
    const auto subRow = [&g, alpha] (const juce::String& name, const juce::String& value, int top)
    {
        g.setFont (font (12.5f));
        g.setColour (colour::secondary.withMultipliedAlpha (alpha));
        g.drawText (name, juce::Rectangle<int> (kMargin, top, kSubNameW, kSubRowH), juce::Justification::centredLeft, false);

        g.setColour (colour::text.withMultipliedAlpha (alpha));
        g.drawText (value, juce::Rectangle<int> (kSubValueX, top, kSubValueW, kSubRowH), juce::Justification::centredRight, false);
    };
    subRow (U ("Sprung-Intervall"), juce::String (juce::roundToInt (intervalSlider.getValue())) + U (" Halbt\xc3\xb6ne"),
            kSubRow1Y);
    subRow (U ("Sprung-Fenster"), juce::String (juce::roundToInt (windowSlider.getValue())) + " ms", kSubRow1Y + kSubRowPitch);
    subRow (U ("Sprung-Sperrzeit"), juce::String (juce::roundToInt (lockSlider.getValue())) + " ms", kSubRow1Y + 2 * kSubRowPitch);
}
