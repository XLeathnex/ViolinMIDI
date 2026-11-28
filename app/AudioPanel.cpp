#include "AudioPanel.h"

#include "Brand.h"
#include "Theme.h"

namespace
{
    constexpr int kMargin = 16;
    constexpr int kContentW = AudioPanel::kWidth - 2 * kMargin;     // 728

    // Header: heading and output line on the left, the main switch on the right.
    constexpr int kHeadingY = 14, kHeadingH = 20;
    constexpr int kInfoY = 36, kInfoH = 16;
    constexpr int kSwitchW = 96, kSwitchH = 26;
    constexpr int kMainSwitchY = 20;                                   // centred on the two lines
    constexpr int kTopSepY = 64;

    // Two columns: the curves on the left, everything else on the right.
    constexpr int kColumnsY = 76, kColumnsBottom = AudioPanel::kHeight - kMargin;   // 76 .. 524
    constexpr int kLeftW = 440, kColumnGap = 24;
    constexpr int kRightX = kMargin + kLeftW + kColumnGap;             // 480
    constexpr int kRightW = AudioPanel::kWidth - kMargin - kRightX;   // 264
    constexpr int kEditorGap = 16;
    constexpr int kEditorH = (kColumnsBottom - kColumnsY - kEditorGap) / 2;   // 216

    // Right column: name lines with the control below.
    constexpr int kNameH = 18;
    constexpr int kRowOffset = 22;                       // a switch row below its name line
    constexpr int kSliderOffset = 20, kSliderH = 24;     // a slider below its name line
    constexpr int kDropY = kColumnsY + 2;                // centred on the curves' titles; switch row 100 .. 126
    constexpr int kSep1Y = 147;
    constexpr int kBlockY[] = { 168, 240, 312, 400 };    // ensemble, coupling, spread | level (slider to 444)
    constexpr int kSep2Y = 378;
    constexpr int kDryY = 472;                           // switch row 494 .. 520

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

    juce::String formatPercent (float share)
    {
        return juce::String (juce::roundToInt (share * 100.0f)) + " %";
    }

    // 2000 -> "2 kHz", 3500 -> "3,5 kHz"; from 10 kHz in whole kilohertz.
    juce::String formatKilohertz (float hz)
    {
        const int tenths = juce::roundToInt (hz / 100.0f);
        if (tenths >= 100 || tenths % 10 == 0)
            return juce::String (juce::roundToInt (hz / 1000.0f)) + " kHz";
        return juce::String (tenths / 10) + "," + juce::String (tenths % 10) + " kHz";
    }

    //==========================================================================
    const char* const kMainTip =
        "Spielt die Harmoniestimmen zus\xc3\xa4tzlich mit dem eigenen Klang der Geige: Das Eingangssignal wird in der "
        "Tonh\xc3\xb6he verschoben und \xc3\xbc" "ber den Audio-Ausgang ausgegeben, die Geige selbst bleibt in der Mitte. "
        "Es gelten Tonart, Voicing und Harmonie-Modus der Harmonie; die Harmonie muss daf\xc3\xbc" "r eingeschaltet sein. "
        "Die Stimmen kommen etwa eine halbe Schwingung des gespielten Tons sp\xc3\xa4ter (unter 2 ms in hohen Lagen, bis "
        "12 ms auf der tiefsten Saite), dazu die Ensemble-Entkopplung.";

    const char* const kFormantTip =
        "Formantkorrektur \xc3\xbc" "ber der gespielten Tonh\xc3\xb6he: wie viel Korpusklang die Harmoniestimmen "
        "behalten. 100 %: Der Klangk\xc3\xb6rper bleibt, wo er ist, hohe Stimmen klingen warm statt nach "
        "\xe2\x80\x9eMickymaus\xe2\x80\x9c. 0 %: Er wandert mit der Tonh\xc3\xb6he. Standard: G- und D-Saite 5 bis 20 % "
        "(nat\xc3\xbc" "rlicher Holzklang und Bogenstrich), ab A4 steigend, E-Saite 75 bis 95 %. Klingen hohe Stimmen "
        "schrill: die Kurve rechts anheben.";

    const char* const kFormantStandardTip =
        "Setzt die Formantkurve auf den Standard zur\xc3\xbc" "ck: G- und D-Saite 5 bis 20 %, ab A4 steigend, "
        "E-Saite 75 bis 95 %.";

    const char* const kHiCutTip =
        "High-Cut \xc3\xbc" "ber der gespielten Tonh\xc3\xb6he: ein Tiefpass (12 dB pro Oktave) nur auf den "
        "Harmoniestimmen, die Geige beh\xc3\xa4lt ihre volle Brillanz. Standard: G- und D-Saite 8 bis 6 kHz, ab E5 "
        "3,5 bis 2,5 kHz. Kratzen die Stimmen: senken; klingen sie dumpf: anheben.";

    const char* const kHiCutStandardTip =
        "Setzt die High-Cut-Kurve auf den Standard zur\xc3\xbc" "ck: G- und D-Saite 8 bis 6 kHz, ab E5 3,5 bis 2,5 kHz.";

    const char* const kDropTip =
        "Drop-Voicing: In Akkorden ab drei T\xc3\xb6nen, gespielt ab A4 (A- und E-Saite), klingt die h\xc3\xb6" "chste "
        "Harmoniestimme eine Oktave tiefer, im Register von Bratsche und Cello. Das gibt ein warmes Fundament statt "
        "schriller Cluster \xc3\xbc" "ber der Melodie. Standard: An. Aus, wenn die Stimmen bewusst \xc3\xbc" "ber der "
        "Geige liegen sollen.";

    const char* const kEnsembleTip =
        "Ensemble-Entkopplung (Humanize): Bei 100 % kommt jede Harmoniestimme etwas sp\xc3\xa4ter (Stimme 1: 8 bis "
        "12 ms, Stimme 2: 16 bis 22 ms usw.) und schwebt langsam um 4 bis 6 Cent (0,2 bis 0,5 Hz), f\xc3\xbc" "r jeden "
        "Akkord neu gew\xc3\xa4hlt. So klingen die Stimmen wie Mitspieler statt wie ein Effekt, ohne Kammfilter und "
        "Phasing gegen die Geige. 0 %: Die Stimmen kommen so fr\xc3\xbc" "h wie m\xc3\xb6glich. Standard 100 %. Weniger "
        "f\xc3\xbc" "r einen engeren, pr\xc3\xa4zisen Satz, etwa bei schnellen Passagen. Doppelklick: Standard.";

    const char* const kCouplingTip =
        "Vibrato-Kopplung: wie stark die Harmoniestimmen deinem Vibrato und deiner Intonation folgen. 100 %: Sie "
        "bewegen sich genau mit der Geige. 0 %: Sie bleiben ruhig auf dem temperierten Ton, ein ruhiger "
        "Streicherteppich unter einem breiten Solo-Vibrato. Standard 100 %. Doppelklick: Standard.";

    const char* const kSpreadTip =
        "Stereo-Panorama: verteilt die Harmoniestimmen abwechselnd nach links und rechts, jede an ihren eigenen Platz; "
        "die Geige bleibt in der Mitte. 35 %: Die \xc3\xa4u\xc3\x9f" "eren Stimmen liegen bei 35 % links und rechts. "
        "0 %: alles in der Mitte, etwa f\xc3\xbc" "r eine Mono-Anlage. Standard 35 %. Doppelklick: Standard.";

    const char* const kLevelTip =
        "Harmonie-Pegel: die Lautst\xc3\xa4rke der Harmoniestimmen gegen\xc3\xbc" "ber der Geige, von aus (ganz links) "
        "bis +6 dB. Standard 0 dB: so laut wie die Geige. Leiser, wenn die Stimmen die Melodie zudecken. "
        "Doppelklick: Standard.";

    const char* const kDryKillTip =
        "Dry-Kill: L\xc3\xa4sst den Originalklang der Geige aus dem Ausgang weg, es kommen nur die Harmoniestimmen. "
        "F\xc3\xbc" "r parallele Effektwege oder wenn die Geige schon \xc3\xbc" "ber einen eigenen Kanal am Pult liegt. "
        "Standard: Aus.";
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
    setSwitchStyle (row, SwitchStyle::status);
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
      onSwitch (1, U (kMainTip)),
      dropSwitch (2, U (kDropTip)),
      dryKillSwitch (3, U (kDryKillTip)),
      formantEditor (U ("Formantkorrektur (Korpus)"), vm::curve::kFormantLow, vm::curve::kFormantHigh, false, formatPercent),
      hiCutEditor (U ("High-Cut (Wet)"), vm::curve::kHiCutLowHz, vm::curve::kHiCutHighHz, true, formatKilohertz)
{
    formantEditor.setDefaultCurve (vm::curve::defaultFormant(), U (kFormantStandardTip));
    hiCutEditor.setDefaultCurve (vm::curve::defaultHiCut(), U (kHiCutStandardTip));
    formantEditor.setTooltip (U (kFormantTip) + " " + CurveEditor::mouseHelp());
    hiCutEditor.setTooltip (U (kHiCutTip) + " " + CurveEditor::mouseHelp());

    // The editors sanitise what they are given; from here on the panel reports their curves.
    formantEditor.setCurve (current.formant);
    hiCutEditor.setCurve (current.hiCut);
    current.formant = formantEditor.getCurve();
    current.hiCut = hiCutEditor.getCurve();

    for (auto* e : { &formantEditor, &hiCutEditor })
    {
        e->onChange = [this] (const vm::Curve&) { changed(); };
        addAndMakeVisible (*e);
    }

    setupBlock (blocks[0], U ("Ensemble-Entkopplung"), percent, 0.0, 100.0, 1.0, 100.0, U (kEnsembleTip));
    setupBlock (blocks[1], U ("Vibrato-Kopplung"), percent, 0.0, 100.0, 1.0, 100.0, U (kCouplingTip));
    setupBlock (blocks[2], U ("Stereo-Panorama"), percent, 0.0, 100.0, 1.0, 35.0, U (kSpreadTip));
    setupBlock (blocks[3], U ("Harmonie-Pegel"), decibel, -60.0, 6.0, 0.5, 0.0, U (kLevelTip));

    blocks[0].slider.setValue (current.ensemble * 100.0, juce::dontSendNotification);
    blocks[1].slider.setValue (current.vibratoCoupling * 100.0, juce::dontSendNotification);
    blocks[2].slider.setValue (current.stereoSpread * 100.0, juce::dontSendNotification);
    blocks[3].slider.setValue (current.harmonyGainDb, juce::dontSendNotification);

    onSwitch.set (current.on);
    dropSwitch.set (current.dropVoicing);
    dryKillSwitch.set (current.dryKill);
    for (auto* s : { &onSwitch, &dropSwitch, &dryKillSwitch })
    {
        addAndMakeVisible (s->row);
        s->offButton.onClick = [this] { changed(); };
        s->onButton.onClick = [this] { changed(); };
    }

    showEnabledState();
    setOpaque (true);   // it fills its window; the curves' live line repaints often
    setSize (kWidth, kHeight);
}

void AudioPanel::setOutputInfo (const juce::String& text, bool isProblem)
{
    if (text == outputInfo && isProblem == outputIsProblem)
        return;

    outputInfo = text;
    outputIsProblem = isProblem;
    repaint (kMargin, kInfoY, kContentW, kInfoH);
}

void AudioPanel::setPlayhead (float midiNote)
{
    formantEditor.setPlayhead (midiNote);
    hiCutEditor.setPlayhead (midiNote);
}

juce::String AudioPanel::valueText (const Block& b) const
{
    const double v = b.slider.getValue();

    switch (b.kind)
    {
        case percent:
            return juce::String (juce::roundToInt (v)) + " %";

        case decibel:
            if (v <= b.slider.getMinimum() + 0.01)
                return minusInfinity() + " dB";
            if (v < 0.0)
                return minus() + oneDecimal (-v) + " dB";
            return (v > 0.0 ? juce::String ("+") : juce::String()) + oneDecimal (v) + " dB";
    }
    return {};
}

/** Everything but the main switch is dimmed and locked while the harmonizer is off. */
void AudioPanel::setValues (const Values& v)
{
    onSwitch.set (v.on);
    dropSwitch.set (v.dropVoicing);
    dryKillSwitch.set (v.dryKill);

    if (v.formant != formantEditor.getCurve())
        formantEditor.setCurve (v.formant);
    if (v.hiCut != hiCutEditor.getCurve())
        hiCutEditor.setCurve (v.hiCut);

    blocks[0].slider.setValue (v.ensemble * 100.0, juce::dontSendNotification);
    blocks[1].slider.setValue (v.vibratoCoupling * 100.0, juce::dontSendNotification);
    blocks[2].slider.setValue (v.stereoSpread * 100.0, juce::dontSendNotification);
    blocks[3].slider.setValue (v.harmonyGainDb, juce::dontSendNotification);

    current = v;
    current.formant = formantEditor.getCurve();
    current.hiCut = hiCutEditor.getCurve();
    showEnabledState();
    repaint();
}

void AudioPanel::showEnabledState()
{
    const bool on = current.on;
    formantEditor.setEnabled (on);
    hiCutEditor.setEnabled (on);
    dropSwitch.row.setEnabled (on);
    dryKillSwitch.row.setEnabled (on);
    for (auto& b : blocks)
        b.slider.setEnabled (on);
}

void AudioPanel::changed()
{
    current.on = onSwitch.isOn();
    current.formant = formantEditor.getCurve();
    current.hiCut = hiCutEditor.getCurve();
    current.dropVoicing = dropSwitch.isOn();
    current.ensemble = (float) (blocks[0].slider.getValue() / 100.0);
    current.vibratoCoupling = (float) (blocks[1].slider.getValue() / 100.0);
    current.stereoSpread = (float) (blocks[2].slider.getValue() / 100.0);
    current.harmonyGainDb = (float) blocks[3].slider.getValue();
    current.dryKill = dryKillSwitch.isOn();

    showEnabledState();
    repaint();

    if (onChange)
        onChange (current);
}

void AudioPanel::resized()
{
    onSwitch.row.setBounds (kMargin + kContentW - kSwitchW, kMainSwitchY, kSwitchW, kSwitchH);

    formantEditor.setBounds (kMargin, kColumnsY, kLeftW, kEditorH);
    hiCutEditor.setBounds (kMargin, kColumnsY + kEditorH + kEditorGap, kLeftW, kEditorH);

    dropSwitch.row.setBounds (kRightX, kDropY + kRowOffset, kSwitchW, kSwitchH);
    for (size_t i = 0; i < blocks.size(); ++i)
        blocks[i].slider.setBounds (kRightX, kBlockY[i] + kSliderOffset, kRightW, kSliderH);
    dryKillSwitch.row.setBounds (kRightX, kDryY + kRowOffset, kSwitchW, kSwitchH);
}

void AudioPanel::paint (juce::Graphics& g)
{
    using namespace brand;

    g.fillAll (colour::window);

    g.setColour (colour::text);
    g.setFont (font (13.0f, true));
    g.drawText ("Audio-Harmonizer", juce::Rectangle<int> (kMargin, kHeadingY, kContentW - kSwitchW - 16, kHeadingH),
                juce::Justification::centredLeft, false);

    g.setFont (font (11.5f));
    g.setColour (outputIsProblem ? colour::problem : colour::secondary);
    g.drawText (outputInfo, juce::Rectangle<int> (kMargin, kInfoY, kContentW - kSwitchW - 16, kInfoH),
                juce::Justification::centredLeft, true);

    // hairlines: under the header, between the columns, between the groups on the right
    g.setColour (colour::hairline);
    g.fillRect (kMargin, kTopSepY, kContentW, 1);
    g.fillRect (kMargin + kLeftW + kColumnGap / 2, kColumnsY, 1, kColumnsBottom - kColumnsY);
    g.fillRect (kRightX, kSep1Y, kRightW, 1);
    g.fillRect (kRightX, kSep2Y, kRightW, 1);

    // Name lines, value on the right in silver; dimmed while the harmonizer is off.
    const float alpha = current.on ? 1.0f : 0.5f;
    const auto nameLine = [&g, alpha] (const juce::String& name, const juce::String& value, int top)
    {
        const juce::Rectangle<int> line (kRightX, top, kRightW, kNameH);

        g.setFont (font (12.5f));
        g.setColour (colour::secondary.withMultipliedAlpha (alpha));
        g.drawText (name, line, juce::Justification::centredLeft, false);

        if (value.isNotEmpty())
        {
            g.setColour (colour::silver.withMultipliedAlpha (alpha));
            g.drawText (value, line, juce::Justification::centredRight, false);
        }
    };

    nameLine ("Drop-Voicing", {}, kDropY);
    for (size_t i = 0; i < blocks.size(); ++i)
        nameLine (blocks[i].label, valueText (blocks[i]), kBlockY[i]);
    nameLine ("Dry-Kill", {}, kDryY);
}
