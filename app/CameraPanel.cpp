#include "CameraPanel.h"

#include "Brand.h"
#include "Theme.h"

#include <cmath>

namespace
{
    constexpr int kMargin = 20;
    constexpr int kContentW = CameraPanel::kWidth - 2 * kMargin;               // 400

    // Two columns of 192 with a gap of 16, as everywhere under the preview.
    constexpr int kColW = 192, kColGap = 16;
    constexpr int kCol1X = kMargin;                                            // 20
    constexpr int kCol2X = kMargin + kColW + kColGap;                          // 228

    // Top row: switch, then the camera list.
    constexpr int kHeaderY = 12, kHeaderH = 26;
    constexpr int kSwitchW = 96, kSwitchGap = 12;
    constexpr int kCameraX = kMargin + kSwitchW + kSwitchGap;                  // 128
    constexpr int kCameraW = CameraPanel::kWidth - kMargin - kCameraX;         // 292

    // The picture (4:3) and the two lines under it.
    constexpr int kPreviewY = 48, kPreviewH = kContentW * 3 / 4;               // 400 x 300
    constexpr int kLineH = 15;
    constexpr int kStatusY = kPreviewY + kPreviewH + 6;                        // 354
    constexpr int kGestureLineY = kStatusY + kLineH;                           // 369
    constexpr int kSep1Y = kGestureLineY + kLineH + 10;                        // 394

    constexpr int kHeadingH = 20, kNameH = 18, kRowH = 26, kControlH = 24, kSliderH = 20;

    // Scroll pad: heading; zero point and violin hand; per axis (one column
    // each) the name with the live value, the target and the response curve;
    // dead zone and tempo.
    constexpr int kPadHeadingY = kSep1Y + 10;                                  // 404
    constexpr int kPadRowY = kPadHeadingY + kHeadingH + 6;                     // 430
    constexpr int kAxisNameY = kPadRowY + kRowH + 10;                          // 466
    constexpr int kAxisTargetY = kAxisNameY + kNameH + 2;                      // 486
    constexpr int kAxisCurveY = kAxisTargetY + kControlH + 6;                  // 516
    constexpr int kPadSliderNameY = kAxisCurveY + kControlH + 10;              // 550
    constexpr int kPadSliderY = kPadSliderNameY + kNameH + 2;                  // 570
    constexpr int kSep2Y = kPadSliderY + kSliderH + 14;                        // 604

    // Head gestures.
    constexpr int kGestureHeadingY = kSep2Y + 10;                              // 614
    constexpr int kGestureNameY = kGestureHeadingY + kHeadingH + 6;            // 640
    constexpr int kGestureBoxY = kGestureNameY + kNameH + 2;                   // 660
    constexpr int kGestureSliderNameY = kGestureBoxY + kControlH + 10;         // 694
    constexpr int kGestureSliderY = kGestureSliderNameY + kNameH + 2;          // 714
    static_assert (kGestureSliderY + kSliderH + 14 == CameraPanel::kHeight, "the layout fills the panel");

    // "Geigenhand" with its switch in the second column.
    constexpr int kHandLabelW = 80;
    constexpr int kHandRowX = kCol2X + kHandLabelW;                            // 308
    constexpr int kHandRowW = kColW - kHandLabelW;                             // 112

    // The HUD in a top corner of the preview, smaller than in the main window.
    constexpr int kHudW = 99, kHudH = 118, kHudInset = 8;                         // a pad square of 83
    constexpr float kHudSwitchLeft = 0.55f, kHudSwitchRight = 0.45f;          // hysteresis for the side it sits on

    // The picture is dimmed a little so the drawing on it reads.
    constexpr float kVeil = 0.18f;
    constexpr float kMinConfidence = 0.3f;
    constexpr int   kMissingFrames = 8;                                        // about half a second without the hand

    // The emblem in place of the picture: dim while off, breathing while the camera starts.
    constexpr float kEmblemOff = 0.10f, kEmblemLow = 0.12f, kEmblemHigh = 0.34f;
    constexpr double kBreathSeconds = 2.4;

    constexpr juce::uint32 kGestureShownMs = 1200, kNoticeShownMs = 2500;
    constexpr int kPulseIntervalMs = 33, kExpiryIntervalMs = 100;

    juce::String U (const char* utf8) { return juce::String::fromUTF8 (utf8); }

    //==========================================================================
    const char* const kPowerTip =
        "Schaltet die Kamera-Steuerung ein und aus: gr\xc3\xbcn an, rot aus. Beim Einschalten wird die Haltung, in der "
        "die Geige gerade ist, zum Nullpunkt des Schnecken-Pads. Das Bild wird nur auf diesem Mac ausgewertet und "
        "nicht gespeichert.";

    const char* const kCameraTip =
        "Welche Kamera das Bild liefert. \xe2\x80\x9e" "Erste Kamera\xe2\x80\x9c nimmt die erste, die macOS meldet. "
        "Am besten so aufstellen, dass Kopf, Schultern und beide H\xc3\xa4nde im Bild sind.";

    const char* const kPreviewTip =
        "Das Kamerabild, seitenverkehrt wie in einem Spiegel. Silbern: der Nullpunkt (die Ruhehaltung der Geige) mit dem "
        "Totzonen-Oval; solange die Geigenhand darin bleibt, stehen die Werte still. Rot: die Geigenhand, sie steht "
        "f\xc3\xbcr die Schnecke. Verl\xc3\xa4sst sie das Oval, wandern die Werte, je weiter drau\xc3\x9f" "en, desto "
        "schneller, und das Fadenkreuz in der Ecke zeigt sie an. Das Bild bleibt auf diesem Mac.";

    const char* const kStatusTip =
        "Was die Kamera gerade tut, oder was nicht stimmt (gelb). Darunter erscheint kurz jede erkannte Kopf-Geste "
        "mit ihrer Aktion.";

    const char* const kPadTip =
        "Schnecken-Pad: Die Geigenschnecke steuert zwei Werte wie ein XY-Pad, seitlich (X) und auf und ab (Y). "
        "Relativ wie ein Joystick: In der Ruhehaltung und im Oval darum bleiben die Werte stehen; erst au\xc3\x9f" "erhalb "
        "wandern sie, und sie bleiben, wo sie sind, sobald die Geige wieder ruht.";

    const char* const kRecalibrateTip =
        "Macht die jetzige Haltung der Geige zum Nullpunkt: Um ihn liegt das silberne Totzonen-Oval, darin bewegt "
        "sich nichts. In normaler Spielhaltung dr\xc3\xbc" "cken. Geschieht auch bei jedem Einschalten.";

    const char* const kHandTip =
        "Welche Hand den Geigenhals h\xc3\xa4lt; sie steht im Bild f\xc3\xbcr die Schnecke. \xc3\x9c" "blich ist die "
        "linke, \xe2\x80\x9eRechts\xe2\x80\x9c f\xc3\xbcr links gespielte Geigen.";

    const char* const kTargetXTip =
        "Wohin die seitliche Bewegung der Schnecke geht: ein beliebiger MIDI-Controller (CC 0 bis 127) oder der "
        "Harmonie-Mix, der die Harmoniestimmen zwischen leise (0 %) und der eingestellten Lautst\xc3\xa4rke (100 %) "
        "blendet. Rechts au\xc3\x9f" "erhalb des Ovals w\xc3\xa4" "chst der Wert, links davon sinkt er. Die Prozentzahl "
        "dar\xc3\xbc" "ber zeigt, wo er gerade steht.";

    const char* const kTargetYTip =
        "Wohin das Heben und Senken der Schnecke geht: ein beliebiger MIDI-Controller (CC 0 bis 127) oder der "
        "Harmonie-Mix, der die Harmoniestimmen zwischen leise (0 %) und der eingestellten Lautst\xc3\xa4rke (100 %) "
        "blendet. Oberhalb des Ovals w\xc3\xa4" "chst der Wert, unterhalb sinkt er. Die Prozentzahl dar\xc3\xbc" "ber "
        "zeigt, wo er gerade steht.";

    const char* const kLinearTip =
        "Linear: Je weiter die Schnecke aus dem Oval kommt, desto schneller wandert der Wert, gleichm\xc3\xa4\xc3\x9fig "
        "\xc3\xbc" "ber den ganzen Weg.";

    const char* const kFineTip =
        "Fein (exponentiell): Knapp au\xc3\x9f" "erhalb des Ovals wandert der Wert sehr langsam, f\xc3\xbcr feine "
        "Fahrten; erst weit drau\xc3\x9f" "en wird er schnell.";

    const char* const kFastTip =
        "Schnell (logarithmisch): Schon knapp au\xc3\x9f" "erhalb des Ovals wandert der Wert z\xc3\xbcgig, f\xc3\xbcr "
        "kr\xc3\xa4" "ftige Sweeps.";

    const char* const kDeadZoneTip =
        "Gr\xc3\xb6\xc3\x9f" "e des silbernen Ovals um den Nullpunkt, in Prozent des Bildes. Solange die Schnecke darin "
        "bleibt, stehen die Werte still, so bewegt das normale Spielen nichts. Gr\xc3\xb6\xc3\x9f" "er ist ruhiger, "
        "braucht aber mehr Weg, bis sich etwas tut.";

    const char* const kTempoTip =
        "Wie lange ein Wert bei voller Auslenkung f\xc3\xbcr seinen ganzen Bereich braucht, von 0 bis 100 %. Kurze "
        "Zeiten f\xc3\xbcr schnelle Sweeps, lange f\xc3\xbcr langsame Fahrten. Voll ausgelenkt ist die Schnecke etwa ein "
        "F\xc3\xbcnftel des Bildes au\xc3\x9f" "erhalb des Ovals; n\xc3\xa4her daran wandert der Wert langsamer.";

    const char* const kGestureTip =
        "Kopf-Gesten: Nicken und Kopfsch\xc3\xbctteln schalten freih\xc3\xa4ndig Harmonie, Halten, Presets oder die "
        "Erkennung, w\xc3\xa4hrend beide H\xc3\xa4nde an der Geige sind.";

    const char* const kNodTip =
        "Was ein Nicken (Kopf kurz nach unten und wieder hoch) ausl\xc3\xb6st. Eine Kopfbewegung, die zur Musik "
        "geh\xc3\xb6rt, kann wie ein Nicken aussehen: Was auf der B\xc3\xbchne nicht versehentlich passieren darf, "
        "besser auf einen Fu\xc3\x9fschalter legen.";

    const char* const kShakeTip =
        "Was ein Kopfsch\xc3\xbctteln (kurz zur einen und zur anderen Seite) ausl\xc3\xb6st. Was auf der B\xc3\xbchne "
        "nicht versehentlich passieren darf, besser auf einen Fu\xc3\x9fschalter legen.";

    const char* const kSensitivityTip =
        "Wie deutlich eine Kopfbewegung sein muss. H\xc3\xb6her erkennt auch kleine Bewegungen, l\xc3\xb6st aber "
        "leichter versehentlich aus.";

    const char* const kCooldownTip =
        "Nach einer erkannten Geste wird f\xc3\xbcr diese Zeit keine weitere ausgel\xc3\xb6st. Verhindert "
        "Doppelausl\xc3\xb6sungen.";

    // The actions, in the order of CameraSettings::Action.
    const char* const kActionNames[] = { "Keine Aktion", "Harmonie an/aus", "Solo halten", "Harmonie halten",
                                         "N\xc3\xa4" "chstes Preset", "Vorheriges Preset", "Erkennung an/aus" };
    static_assert (sizeof (kActionNames) / sizeof (kActionNames[0]) == CameraSettings::numActions, "one name per action");

    const char* const kCurveNames[] = { "Linear", "Fein", "Schnell" };
    const char* const kCurveTips[] = { kLinearTip, kFineTip, kFastTip };
    static_assert (cam::PadTracker::numResponses == 3, "one segment per response curve");

    //==========================================================================
    // The target lists: 1 "Kein Ziel", 2 "Harmonie-Mix", then CC 0 .. 127 as 3 .. 130.
    int idForTarget (int target) noexcept
    {
        if (target == CameraSettings::targetHarmonyMix)
            return 2;
        if (target >= 0 && target <= 127)
            return target + 3;
        return 1;
    }

    int targetForId (int id) noexcept
    {
        if (id == 2)
            return CameraSettings::targetHarmonyMix;
        if (id >= 3 && id <= 130)
            return id - 3;
        return CameraSettings::targetNone;
    }

    /** The usual meaning of the common controllers, as instruments and Logic name them. */
    const char* controllerName (int cc) noexcept
    {
        switch (cc)
        {
            case 1:  return "Modwheel";
            case 2:  return "Breath";
            case 7:  return "Volume";
            case 10: return "Pan";
            case 11: return "Expression";
            case 71: return "Resonanz";
            case 74: return "Filter/Brightness";
            case 91: return "Reverb";
            case 93: return "Chorus";
            default: return nullptr;
        }
    }

    /** Short, for the HUD: the same names the main window gives it. */
    juce::String hudName (int target)
    {
        if (target == CameraSettings::targetHarmonyMix)
            return "Harmonie-Mix";
        if (target >= 0 && target <= 127)
            return "CC " + juce::String (target);
        return "aus";
    }

    // True once `deadline` (millisecond counter, 0 = none) has passed, also across the counter's wrap.
    bool passed (juce::uint32 now, juce::uint32 deadline) noexcept
    {
        return (juce::int32) (now - deadline) >= 0;
    }

    juce::uint32 deadlineIn (juce::uint32 ms) noexcept
    {
        const auto d = juce::Time::getMillisecondCounter() + ms;
        return d == 0 ? 1 : d;   // 0 means "not shown"
    }

    juce::String oneDecimal (double v)
    {
        return juce::String (v, 1).replaceCharacter ('.', ',');
    }

    juce::String percent (float share)
    {
        return juce::String (juce::roundToInt (share * 100.0f)) + " %";
    }

    /** How long full deflection takes from 0 to 100 %: "0,25 s", "1,25 s", "2,5 s", "10 s". */
    juce::String sweepTime (double speed)
    {
        const double seconds = 1.0 / juce::jmax (0.1, speed);
        const int decimals = seconds < 2.0 ? 2 : (seconds < 9.95 ? 1 : 0);

        auto number = decimals == 0 ? juce::String (juce::roundToInt (seconds)) : juce::String (seconds, decimals);
        if (number.containsChar ('.'))
            number = number.trimCharactersAtEnd ("0").trimCharactersAtEnd (".");

        return number.replaceCharacter ('.', ',') + U (" s f\xc3\xbcr 0\xe2\x80\x93" "100 %");
    }
}

//==============================================================================
CameraPanel::CurveSwitch::CurveSwitch (int radioGroup)
{
    constexpr int n = cam::PadTracker::numResponses;
    for (int i = 0; i < n; ++i)
    {
        auto& b = buttons[i];
        b.setButtonText (kCurveNames[i]);
        b.setClickingTogglesState (true);
        b.setRadioGroupId (radioGroup);
        b.setWantsKeyboardFocus (false);
        b.setTooltip (U (kCurveTips[i]));
        b.setConnectedEdges ((i > 0 ? juce::Button::ConnectedOnLeft : 0) | (i < n - 1 ? juce::Button::ConnectedOnRight : 0));
        row.add (b);
    }
}

void CameraPanel::CurveSwitch::set (int response)
{
    const int r = juce::jlimit (0, (int) cam::PadTracker::numResponses - 1, response);
    for (int i = 0; i < cam::PadTracker::numResponses; ++i)
        buttons[i].setToggleState (i == r, juce::dontSendNotification);
}

int CameraPanel::CurveSwitch::get() const
{
    for (int i = 0; i < cam::PadTracker::numResponses; ++i)
        if (buttons[i].getToggleState())
            return i;
    return cam::PadTracker::linear;
}

//==============================================================================
CameraPanel::CameraPanel (const CameraSettings& s, const juce::StringArray& cameraNames)
    : current (s), cameras (cameraNames)
{
    // Switch Aus | An: the module's main switch, ON on emerald, OFF on crimson.
    for (auto* b : { &offButton, &onButton })
    {
        b->setClickingTogglesState (true);
        b->setRadioGroupId (1);
        b->setWantsKeyboardFocus (false);
        b->setTooltip (U (kPowerTip));
        powerRow.add (*b);
        b->onClick = [this] { readControls(); notify(); };
    }
    offButton.setConnectedEdges (juce::Button::ConnectedOnRight);
    onButton.setConnectedEdges (juce::Button::ConnectedOnLeft);
    onButton.setToggleState (current.enabled, juce::dontSendNotification);
    offButton.setToggleState (! current.enabled, juce::dontSendNotification);
    setSwitchStyle (powerRow, SwitchStyle::status);
    addAndMakeVisible (powerRow);

    cameraBox.setWantsKeyboardFocus (false);
    cameraBox.setTooltip (U (kCameraTip));
    cameraBox.onChange = [this] { readControls(); notify(); };
    addAndMakeVisible (cameraBox);
    fillCameraList();

    // Scroll pad: zero point and violin hand.
    recalibrateButton.setWantsKeyboardFocus (false);
    recalibrateButton.setTooltip (U (kRecalibrateTip));
    recalibrateButton.setEnabled (current.enabled);
    recalibrateButton.onClick = [this]
    {
        if (onRecalibrate)
            onRecalibrate();

        // The pad takes the zero point from the next frame that shows the hand; say so if that may take a while.
        if (padView.hasHand)
            showNotice (U ("Neuer Nullpunkt: die jetzige Haltung der Geige."), false);
        else
            showNotice (U ("Die Geigenhand ist nicht zu sehen. Der Nullpunkt folgt, sobald sie im Bild ist."), true);
    };
    addAndMakeVisible (recalibrateButton);

    for (auto* b : { &leftHandButton, &rightHandButton })
    {
        b->setClickingTogglesState (true);
        b->setRadioGroupId (2);
        b->setWantsKeyboardFocus (false);
        b->setTooltip (U (kHandTip));
        handRow.add (*b);
        b->onClick = [this] { readControls(); notify(); };
    }
    leftHandButton.setConnectedEdges (juce::Button::ConnectedOnRight);
    rightHandButton.setConnectedEdges (juce::Button::ConnectedOnLeft);
    rightHandButton.setToggleState (current.pad.rightHand, juce::dontSendNotification);
    leftHandButton.setToggleState (! current.pad.rightHand, juce::dontSendNotification);
    addAndMakeVisible (handRow);

    // Per axis: target and response curve.
    for (auto* box : { &targetX, &targetY })
    {
        fillTargetList (*box);
        box->setWantsKeyboardFocus (false);
        box->onChange = [this] { readControls(); notify(); };
        addAndMakeVisible (box);
    }
    targetX.setSelectedId (idForTarget (current.xTarget), juce::dontSendNotification);
    targetY.setSelectedId (idForTarget (current.yTarget), juce::dontSendNotification);
    targetX.setTooltip (U (kTargetXTip));
    targetY.setTooltip (U (kTargetYTip));

    for (auto* curve : { &curveX, &curveY })
    {
        for (auto& b : curve->buttons)
            b.onClick = [this] { readControls(); notify(); };
        addAndMakeVisible (curve->row);
    }
    curveX.set (current.pad.responseX);
    curveY.set (current.pad.responseY);

    const auto setupSlider = [this] (juce::Slider& sl, double low, double high, double step, double defaultValue,
                                     double value, const char* tip)
    {
        sl.setSliderStyle (juce::Slider::LinearHorizontal);
        sl.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        sl.setRange (low, high, step);
        sl.setDoubleClickReturnValue (true, defaultValue);
        sl.setWantsKeyboardFocus (false);
        sl.setTooltip (U (tip));
        sl.setValue (value, juce::dontSendNotification);
        sl.onValueChange = [this] { readControls(); notify(); repaint(); };
        addAndMakeVisible (sl);
    };
    setupSlider (deadZoneSlider, 3.0, 15.0, 1.0, 6.0, current.pad.deadZone * 100.0, kDeadZoneTip);
    setupSlider (tempoSlider, 0.1, 4.0, 0.05, 0.8, current.pad.speed, kTempoTip);
    tempoSlider.setSkewFactorFromMidPoint (0.8);   // the useful range (about 0.3 to 2) gets most of the travel

    // Gestures.
    fillActionList (nodBox);
    fillActionList (shakeBox);
    nodBox.setSelectedId (current.nodAction + 1, juce::dontSendNotification);
    shakeBox.setSelectedId (current.shakeAction + 1, juce::dontSendNotification);
    nodBox.setTooltip (U (kNodTip));
    shakeBox.setTooltip (U (kShakeTip));
    for (auto* b : { &nodBox, &shakeBox })
    {
        b->setWantsKeyboardFocus (false);
        b->onChange = [this] { readControls(); notify(); };
        addAndMakeVisible (b);
    }

    setupSlider (sensitivitySlider, 0.0, 100.0, 1.0, 50.0, current.gestures.sensitivity * 100.0, kSensitivityTip);
    setupSlider (cooldownSlider, 0.3, 5.0, 0.1, 1.2, current.gestures.cooldownMs / 1000.0, kCooldownTip);

    // The names and headings explain themselves like the control they belong to.
    const auto tip = [this] (juce::Rectangle<int> area, const char* text) { nameTips.emplace_back (area, U (text)); };
    tip ({ kMargin, kStatusY, kContentW, 2 * kLineH }, kStatusTip);
    tip ({ kMargin, kPadHeadingY, kContentW, kHeadingH }, kPadTip);
    tip ({ kCol2X, kPadRowY, kHandLabelW, kRowH }, kHandTip);
    tip ({ kCol1X, kAxisNameY, kColW, kNameH }, kTargetXTip);
    tip ({ kCol2X, kAxisNameY, kColW, kNameH }, kTargetYTip);
    tip ({ kCol1X, kPadSliderNameY, kColW, kNameH }, kDeadZoneTip);
    tip ({ kCol2X, kPadSliderNameY, kColW, kNameH }, kTempoTip);
    tip ({ kMargin, kGestureHeadingY, kContentW, kHeadingH }, kGestureTip);
    tip ({ kCol1X, kGestureNameY, kColW, kNameH }, kNodTip);
    tip ({ kCol2X, kGestureNameY, kColW, kNameH }, kShakeTip);
    tip ({ kCol1X, kGestureSliderNameY, kColW, kNameH }, kSensitivityTip);
    tip ({ kCol2X, kGestureSliderNameY, kColW, kNameH }, kCooldownTip);

    updateHudNames();
    addAndMakeVisible (hud);

    setSize (kWidth, kHeight);
    placeHud();
}

//==============================================================================
void CameraPanel::fillActionList (juce::ComboBox& box)
{
    for (int i = 0; i < CameraSettings::numActions; ++i)
        box.addItem (U (kActionNames[i]), i + 1);
}

void CameraPanel::fillTargetList (juce::ComboBox& box)
{
    box.addItem ("Kein Ziel", idForTarget (CameraSettings::targetNone));
    box.addItem ("Harmonie-Mix", idForTarget (CameraSettings::targetHarmonyMix));
    box.addSeparator();

    for (int cc = 0; cc <= 127; ++cc)
    {
        auto text = "CC " + juce::String (cc);
        if (const char* name = controllerName (cc))
            text << " (" << name << ")";
        box.addItem (text, idForTarget (cc));
    }
}

// Item 1 is the first camera (empty name), then the names, then the stored name if it is not among them.
void CameraPanel::fillCameraList()
{
    cameraBox.clear (juce::dontSendNotification);
    cameraBox.addItem ("Erste Kamera", 1);

    for (int i = 0; i < cameras.size(); ++i)
        cameraBox.addItem (cameras[i], i + 2);

    int selected = 1;
    if (current.deviceName.isNotEmpty())
    {
        const int index = cameras.indexOf (current.deviceName);
        if (index >= 0)
            selected = index + 2;
        else
        {
            selected = cameras.size() + 2;
            cameraBox.addItem (current.deviceName + " (nicht angeschlossen)", selected);
        }
    }
    cameraBox.setSelectedId (selected, juce::dontSendNotification);
}

void CameraPanel::setCameras (const juce::StringArray& names)
{
    cameras = names;
    fillCameraList();
}

//==============================================================================
// Takes everything the controls show into `current`.
void CameraPanel::readControls()
{
    const bool wasOn = current.enabled;
    current.enabled = onButton.getToggleState();

    const int camera = cameraBox.getSelectedId();
    if (camera == 1)
        current.deviceName = juce::String();
    else if (camera >= 2 && camera - 2 < cameras.size())
        current.deviceName = cameras[camera - 2];
    // else: the stored camera that is not connected stays as it is

    current.pad.rightHand = rightHandButton.getToggleState();
    current.pad.deadZone = (float) (deadZoneSlider.getValue() / 100.0);
    current.pad.speed = (float) tempoSlider.getValue();
    current.pad.responseX = curveX.get();
    current.pad.responseY = curveY.get();
    current.xTarget = targetForId (targetX.getSelectedId());
    current.yTarget = targetForId (targetY.getSelectedId());

    current.nodAction = juce::jlimit (0, (int) CameraSettings::numActions - 1, nodBox.getSelectedId() - 1);
    current.shakeAction = juce::jlimit (0, (int) CameraSettings::numActions - 1, shakeBox.getSelectedId() - 1);
    current.gestures.sensitivity = (float) (sensitivitySlider.getValue() / 100.0);
    current.gestures.cooldownMs = (float) (juce::roundToInt (cooldownSlider.getValue() * 10.0) * 100);

    // Switched on or off: the old picture means nothing any more.
    if (current.enabled != wasOn)
    {
        recalibrateButton.setEnabled (current.enabled);
        dropPicture();
    }
    updateHudNames();
}

void CameraPanel::notify()
{
    if (onChange)
        onChange (current);
}

void CameraPanel::updateHudNames()
{
    hud.setAxisNames (hudName (current.xTarget), hudName (current.yTarget));
}

void CameraPanel::dropPicture()
{
    hud.setState (padView.x, padView.y, false);   // fades out where it stood

    picture = juce::Image();
    lastPose = cam::Pose();
    padView = PadView();
    framesWithoutHand = 0;

    updateTimer();
    repaintLiveParts();
}

//==============================================================================
void CameraPanel::setStatus (const juce::String& text, bool isProblem)
{
    if (text == statusText && isProblem == statusIsProblem)
        return;

    statusText = text;
    statusIsProblem = isProblem;
    repaint (kMargin, kStatusY, kContentW, kLineH);
}

void CameraPanel::showNotice (const juce::String& text, bool isProblem)
{
    noticeText = text;
    noticeIsProblem = isProblem;
    noticeUntil = deadlineIn (kNoticeShownMs);
    updateTimer();
    repaint (kMargin, kStatusY, kContentW, kLineH);
}

void CameraPanel::setWaiting (bool waitingForPicture)
{
    if (waitingForPicture == waiting)
        return;

    waiting = waitingForPicture;
    if (waiting)
        dropPicture();   // a picture from before (another camera, an earlier run) is not what is coming

    updateTimer();
    repaint (previewArea());
}

//==============================================================================
void CameraPanel::setDocked (bool shouldBeDocked)
{
    docked = shouldBeDocked;
}

void CameraPanel::setDeadZone (float zone)
{
    current.pad.deadZone = zone;
    deadZoneSlider.setValue (zone * 100.0, juce::dontSendNotification);
    repaint();
}

void CameraPanel::setAnchor (int, bool) {}

bool CameraPanel::isPicking() const noexcept
{
    return false;
}

void CameraPanel::cancelPipette() {}

void CameraPanel::showEvent (const juce::String&, bool) {}

void CameraPanel::setFreeze (bool) {}

void CameraPanel::setBowStats (int, unsigned) {}

void CameraPanel::setPowerState (bool on)
{
    onButton.setToggleState (on, juce::dontSendNotification);
    offButton.setToggleState (! on, juce::dontSendNotification);
    powerRow.repaint();

    if (on != current.enabled)
    {
        current.enabled = on;
        recalibrateButton.setEnabled (on);
        dropPicture();
    }
}

void CameraPanel::showFrame (const juce::Image& preview, const cam::Pose& pose, const cam::PadTracker& pad, cam::Gesture gesture)
{
    if (preview.isValid())
    {
        const bool first = ! picture.isValid();
        picture = preview;
        if (first)
            updateTimer();   // the emblem stops breathing
    }
    lastPose = pose;

    padView.known = true;
    padView.zeroTaken = pad.hasZero();
    padView.hasZero = pad.zeroPoint (padView.zeroX, padView.zeroY);
    padView.hasHand = pad.handPoint (padView.handX, padView.handY);
    padView.active = pad.active();
    padView.x = pad.x();
    padView.y = pad.y();
    framesWithoutHand = padView.hasHand ? 0 : juce::jmin (framesWithoutHand + 1, kMissingFrames);

    placeHud();
    hud.setState (padView.x, padView.y, padView.active && picture.isValid());

    if (gesture != cam::Gesture::None)
    {
        const bool nod = gesture == cam::Gesture::Nod;
        const int action = nod ? current.nodAction : current.shakeAction;
        gestureText = (nod ? juce::String ("Nicken erkannt") : U ("Kopfsch\xc3\xbctteln erkannt")) + ": "
                    + (action > CameraSettings::actionNone && action < CameraSettings::numActions
                           ? U (kActionNames[action]) : juce::String ("keine Aktion zugewiesen"));
        gestureUntil = deadlineIn (kGestureShownMs);
        updateTimer();
    }

    repaintLiveParts();
}

void CameraPanel::repaintLiveParts()
{
    repaint (previewArea());
    repaint (kMargin, kGestureLineY, kContentW, kLineH);
    repaint (kMargin, kAxisNameY, kContentW, kNameH);
}

// The HUD goes into the top corner away from the zero point, so it never covers the violin. It sits on the
// picture itself: a 16:9 camera is letterboxed in the 4:3 preview, and the HUD should not straddle the band.
void CameraPanel::placeHud()
{
    if (padView.hasZero)
    {
        if (padView.zeroX > kHudSwitchLeft)
            hudOnLeft = true;
        else if (padView.zeroX < kHudSwitchRight)
            hudOnLeft = false;
    }

    const auto area = pictureArea().getSmallestIntegerContainer().getIntersection (previewArea());
    const int x = hudOnLeft ? area.getX() + kHudInset : area.getRight() - kHudInset - kHudW;
    const int h = juce::jlimit (0, kHudH, area.getHeight() - 2 * kHudInset);   // a very flat picture: the HUD shrinks
    const juce::Rectangle<int> bounds (x, area.getY() + kHudInset, kHudW, h);
    if (hud.getBounds() != bounds)
        hud.setBounds (bounds);
}

//==============================================================================
void CameraPanel::updateTimer()
{
    // Fast while the emblem breathes, slow while only a line has to disappear in time.
    const bool pulsing = waiting && ! picture.isValid();
    const bool expiring = gestureUntil != 0 || noticeUntil != 0;
    const int interval = pulsing ? kPulseIntervalMs : (expiring ? kExpiryIntervalMs : 0);

    if (interval == 0)
        stopTimer();
    else if (getTimerInterval() != interval)
        startTimer (interval);
}

void CameraPanel::timerCallback()
{
    const auto now = juce::Time::getMillisecondCounter();

    if (gestureUntil != 0 && passed (now, gestureUntil))
    {
        gestureUntil = 0;
        gestureText = juce::String();
        repaint (kMargin, kGestureLineY, kContentW, kLineH);
    }
    if (noticeUntil != 0 && passed (now, noticeUntil))
    {
        noticeUntil = 0;
        noticeText = juce::String();
        repaint (kMargin, kStatusY, kContentW, kLineH);
    }
    if (waiting && ! picture.isValid())
        repaint (previewArea());

    updateTimer();
}

//==============================================================================
juce::String CameraPanel::getTooltip()
{
    const auto p = getMouseXYRelative();
    if (previewArea().contains (p))
        return U (kPreviewTip);

    for (const auto& [area, text] : nameTips)
        if (area.contains (p))
            return text;

    return {};
}

void CameraPanel::resized()
{
    powerRow.setBounds (kMargin, kHeaderY, kSwitchW, kHeaderH);
    cameraBox.setBounds (kCameraX, kHeaderY, kCameraW, kHeaderH);

    recalibrateButton.setBounds (kCol1X, kPadRowY, kColW, kRowH);
    handRow.setBounds (kHandRowX, kPadRowY, kHandRowW, kRowH);

    targetX.setBounds (kCol1X, kAxisTargetY, kColW, kControlH);
    targetY.setBounds (kCol2X, kAxisTargetY, kColW, kControlH);
    curveX.row.setBounds (kCol1X, kAxisCurveY, kColW, kControlH);
    curveY.row.setBounds (kCol2X, kAxisCurveY, kColW, kControlH);

    deadZoneSlider.setBounds (kCol1X, kPadSliderY, kColW, kSliderH);
    tempoSlider.setBounds (kCol2X, kPadSliderY, kColW, kSliderH);

    nodBox.setBounds (kCol1X, kGestureBoxY, kColW, kControlH);
    shakeBox.setBounds (kCol2X, kGestureBoxY, kColW, kControlH);

    sensitivitySlider.setBounds (kCol1X, kGestureSliderY, kColW, kSliderH);
    cooldownSlider.setBounds (kCol2X, kGestureSliderY, kColW, kSliderH);

    placeHud();
}

//==============================================================================
juce::Rectangle<int> CameraPanel::previewArea() const
{
    return { kMargin, kPreviewY, kContentW, kPreviewH };
}

juce::Rectangle<float> CameraPanel::pictureArea() const
{
    const auto area = previewArea().toFloat();
    if (! picture.isValid())
        return area;

    return juce::RectanglePlacement (juce::RectanglePlacement::centred).appliedTo (picture.getBounds().toFloat(), area);
}

void CameraPanel::paintPlaceholder (juce::Graphics& g, juce::Rectangle<float> area)
{
    using namespace brand;

    // The emblem a little above the middle, the caption under it.
    const float side = area.getHeight() * 0.44f;
    const auto rest = juce::Rectangle<float> (side, side).withCentre (area.getCentre().translated (0.0f, -12.0f));

    auto emblemArea = rest;
    float opacity = kEmblemOff;
    if (waiting)
    {
        // A slow breath: brighter and a touch larger, then back.
        const double t = juce::Time::getMillisecondCounterHiRes() * 0.001;
        const float phase = (float) (0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * t / kBreathSeconds));
        opacity = kEmblemLow + (kEmblemHigh - kEmblemLow) * phase;
        emblemArea = rest.withSizeKeepingCentre (side * (0.95f + 0.05f * phase), side * (0.95f + 0.05f * phase));
    }
    drawEmblem (g, emblemArea, opacity);

    const auto caption = waiting ? U ("Warte auf das Bild \xe2\x80\xa6")
                                 : (current.enabled ? juce::String ("Kein Bild") : juce::String ("Kamera aus"));
    g.setFont (font (12.5f));
    g.setColour (waiting ? colour::secondary : colour::tertiary);
    g.drawText (caption, juce::Rectangle<float> (area.getX(), rest.getBottom() + 12.0f, area.getWidth(), 18.0f),
                juce::Justification::centred, false);
}

void CameraPanel::paintBody (juce::Graphics& g, juce::Rectangle<float> pictureBounds) const
{
    using namespace brand;

    if (! lastPose.valid)
        return;

    const auto at = [&pictureBounds] (const cam::Point& p)
    {
        return juce::Point<float> (pictureBounds.getX() + p.x * pictureBounds.getWidth(),
                                   pictureBounds.getY() + p.y * pictureBounds.getHeight());
    };
    const auto seen = [] (const cam::Point& p) { return p.confidence >= kMinConfidence; };

    // Quiet, in cool white: the body is the background for the pad, not the message.
    g.setColour (colour::text.withAlpha (0.38f));
    const auto line = [&] (const cam::Point& a, const cam::Point& b)
    {
        if (seen (a) && seen (b))
            g.drawLine (juce::Line<float> (at (a), at (b)), 1.25f);
    };
    line (lastPose.leftShoulder, lastPose.rightShoulder);
    line (lastPose.leftShoulder, lastPose.leftElbow);
    line (lastPose.leftElbow, lastPose.leftWrist);
    line (lastPose.rightShoulder, lastPose.rightElbow);
    line (lastPose.rightElbow, lastPose.rightWrist);

    g.setColour (colour::text.withAlpha (0.6f));
    const auto dot = [&] (const cam::Point& p, float size)
    {
        if (seen (p))
            g.fillEllipse (juce::Rectangle<float> (size, size).withCentre (at (p)));
    };
    for (const auto* p : { &lastPose.nose, &lastPose.leftEye, &lastPose.rightEye, &lastPose.leftEar, &lastPose.rightEar })
        dot (*p, 3.5f);
    for (const auto* p : { &lastPose.leftShoulder, &lastPose.rightShoulder, &lastPose.leftElbow, &lastPose.rightElbow,
                           &lastPose.leftWrist, &lastPose.rightWrist })
        dot (*p, 5.0f);
}

void CameraPanel::paintPad (juce::Graphics& g, juce::Rectangle<float> pictureBounds) const
{
    using namespace brand;

    const auto at = [&pictureBounds] (float x, float y)
    {
        return juce::Point<float> (pictureBounds.getX() + x * pictureBounds.getWidth(),
                                   pictureBounds.getY() + y * pictureBounds.getHeight());
    };

    const auto zero = at (padView.zeroX, padView.zeroY);
    if (padView.hasZero)
    {
        // The dead zone is a circle of `deadZone` of the picture in its own coordinates, so an oval on the
        // 4:3 picture. Brighter while the hand rests in it: that is when the values are held.
        const float zone = juce::jlimit (0.03f, 0.15f, current.pad.deadZone);
        const auto oval = juce::Rectangle<float> (2.0f * zone * pictureBounds.getWidth(), 2.0f * zone * pictureBounds.getHeight())
                              .withCentre (zero);
        const bool resting = padView.hasHand && ! padView.active;

        g.setColour (colour::silver.withAlpha (resting ? 0.10f : 0.05f));
        g.fillEllipse (oval);
        g.setColour (colour::silver.withAlpha (resting ? 0.95f : 0.65f));
        g.drawEllipse (oval, 1.5f);

        // The zero point itself: a small cross.
        g.setColour (colour::silver);
        g.fillRect (juce::Rectangle<float> (9.0f, 1.0f).withCentre (zero));
        g.fillRect (juce::Rectangle<float> (1.0f, 9.0f).withCentre (zero));
    }

    if (! padView.hasHand)
        return;

    constexpr float kRing = 9.0f;   // radius of the hand's ring
    const auto hand = at (padView.handX, padView.handY);

    // While the values move: a thin line from the zero point to the edge of the hand's ring.
    if (padView.active && padView.hasZero)
    {
        const float distance = zero.getDistanceFrom (hand);
        if (distance > kRing)
        {
            const auto end = hand + (zero - hand) * (kRing / distance);
            g.setColour (colour::crimson.withAlpha (0.75f));
            g.drawLine (juce::Line<float> (zero, end), 1.25f);
        }
    }

    g.setColour (colour::crimson.withAlpha (padView.active ? 1.0f : 0.85f));
    g.drawEllipse (juce::Rectangle<float> (2.0f * kRing, 2.0f * kRing).withCentre (hand), 2.0f);
    g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre (hand));
}

void CameraPanel::paintCaption (juce::Graphics& g, juce::Rectangle<float> pictureBounds) const
{
    using namespace brand;

    // Only once the hand has been missing for a moment, so a single lost frame does not flash a message.
    if (framesWithoutHand < kMissingFrames)
        return;

    const auto text = padView.zeroTaken ? U ("Die Geigenhand ist nicht zu sehen")
                                        : U ("Nullpunkt folgt, sobald die Geigenhand zu sehen ist");
    const auto f = font (11.5f);
    const float w = juce::GlyphArrangement::getStringWidth (f, text) + 20.0f, h = 20.0f;
    const auto pill = juce::Rectangle<float> (juce::jmin (w, pictureBounds.getWidth() - 16.0f), h)
                          .withCentre ({ pictureBounds.getCentreX(), pictureBounds.getBottom() - 10.0f - h * 0.5f });

    g.setColour (colour::display.withAlpha (0.82f));
    g.fillRoundedRectangle (pill, kControlRadius);
    g.setFont (f);
    g.setColour (colour::problem);
    g.drawText (text, pill, juce::Justification::centred, true);
}

void CameraPanel::paintPreview (juce::Graphics& g)
{
    using namespace brand;

    const auto area = previewArea().toFloat();
    {
        juce::Graphics::ScopedSaveState state (g);
        juce::Path frame;
        frame.addRoundedRectangle (area, kControlRadius);
        g.reduceClipRegion (frame);

        g.setColour (colour::display);
        g.fillRect (area);

        if (picture.isValid())
        {
            const auto pic = pictureArea();
            g.drawImage (picture, pic);
            g.setColour (juce::Colours::black.withAlpha (kVeil));
            g.fillRect (pic);

            paintBody (g, pic);
            paintPad (g, pic);
            paintCaption (g, pic);
        }
        else
        {
            paintPlaceholder (g, area);
        }
    }

    g.setColour (colour::separator);
    g.drawRoundedRectangle (area.reduced (0.5f), kControlRadius, 1.0f);
}

void CameraPanel::paint (juce::Graphics& g)
{
    using namespace brand;

    g.fillAll (colour::window);

    // The preview repaints about 15 times a second on its own; the text below only when it is part of the area.
    if (g.clipRegionIntersects (previewArea()))
        paintPreview (g);

    if (! g.clipRegionIntersects ({ 0, kStatusY, getWidth(), getHeight() - kStatusY }))
        return;

    // Status line, or the notice that replaces it for a moment; the gesture line below.
    g.setFont (font (11.5f));
    const bool notice = noticeText.isNotEmpty();
    g.setColour ((notice ? noticeIsProblem : statusIsProblem) ? colour::problem : colour::secondary);
    g.drawText (notice ? noticeText : statusText, juce::Rectangle<int> (kMargin, kStatusY, kContentW, kLineH),
                juce::Justification::centredLeft, true);

    g.setColour (colour::secondary);
    g.drawText (gestureText, juce::Rectangle<int> (kMargin, kGestureLineY, kContentW, kLineH),
                juce::Justification::centredLeft, true);

    g.setColour (colour::hairline);
    g.fillRect (kMargin, kSep1Y, kContentW, 1);
    g.fillRect (kMargin, kSep2Y, kContentW, 1);

    // Headings.
    g.setColour (colour::text);
    g.setFont (font (13.0f, true));
    g.drawText ("Schnecken-Pad", juce::Rectangle<int> (kMargin, kPadHeadingY, kContentW, kHeadingH),
                juce::Justification::centredLeft, false);
    g.drawText ("Kopf-Gesten", juce::Rectangle<int> (kMargin, kGestureHeadingY, kContentW, kHeadingH),
                juce::Justification::centredLeft, false);

    // Name lines: the name in grey on the left, the value in silver on the right.
    g.setFont (font (12.5f));
    const auto nameLine = [&g] (const juce::String& name, const juce::String& value, juce::Rectangle<int> r)
    {
        g.setColour (colour::secondary);
        g.drawText (name, r, juce::Justification::centredLeft, false);
        if (value.isNotEmpty())
        {
            g.setColour (colour::silver);
            g.drawText (value, r, juce::Justification::centredRight, false);
        }
    };
    const auto column = [] (int x, int y) { return juce::Rectangle<int> (x, y, kColW, kNameH); };

    nameLine ("Geigenhand", {}, juce::Rectangle<int> (kCol2X, kPadRowY, kHandLabelW, kRowH));

    const auto live = [this] (float v) { return padView.known ? percent (v) : U ("\xe2\x80\x93"); };
    nameLine (U ("X \xc2\xb7 seitlich"), live (padView.x), column (kCol1X, kAxisNameY));
    nameLine (U ("Y \xc2\xb7 auf und ab"), live (padView.y), column (kCol2X, kAxisNameY));

    nameLine ("Totzone", juce::String (juce::roundToInt (deadZoneSlider.getValue())) + " %", column (kCol1X, kPadSliderNameY));
    nameLine ("Tempo", sweepTime (tempoSlider.getValue()), column (kCol2X, kPadSliderNameY));

    nameLine ("Nicken", {}, column (kCol1X, kGestureNameY));
    nameLine (U ("Kopfsch\xc3\xbctteln"), {}, column (kCol2X, kGestureNameY));
    nameLine ("Empfindlichkeit", juce::String (juce::roundToInt (sensitivitySlider.getValue())) + " %",
              column (kCol1X, kGestureSliderNameY));
    nameLine ("Sperrzeit", oneDecimal (cooldownSlider.getValue()) + " s", column (kCol2X, kGestureSliderNameY));
}
