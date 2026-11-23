#include "CameraPanel.h"

#include "Brand.h"

namespace
{
    constexpr int kWidth = 420, kHeight = 600;
    constexpr int kMargin = 24;
    constexpr int kContentW = kWidth - 2 * kMargin;     // 372

    // Top row: switch, then the camera list.
    constexpr int kHeaderY = 12, kHeaderH = 26;
    constexpr int kSwitchW = 96, kSwitchGap = 14;
    constexpr int kCameraX = kMargin + kSwitchW + kSwitchGap;      // 134
    constexpr int kCameraW = kWidth - kMargin - kCameraX;          // 262

    // The picture (4:3) and the two lines under it.
    constexpr int kPreviewY = 44, kPreviewH = 279;                 // 372 x 279 = 4:3
    constexpr int kBarW = 6;
    constexpr int kStatusY = 327, kLineH = 15;
    constexpr int kGestureY = kStatusY + kLineH;                   // 342
    constexpr int kSep1Y = 364;

    // Height group.
    constexpr int kNameY = 371, kNameH = 16;
    constexpr int kCalibrateY = 390, kControlH = 24, kCalibrateGap = 8;
    constexpr int kCalibrateW = (kContentW - kCalibrateGap) / 2;   // 182
    constexpr int kTargetY = 420;
    constexpr int kSliderNameY = 450, kSliderY = kSliderNameY + kNameH, kSliderH = 20;   // 466
    constexpr int kSep2Y = 494;

    // Gesture group.
    constexpr int kGestureNameY = 501;
    constexpr int kGestureBoxY = kGestureNameY + kNameH;           // 517
    constexpr int kGestureSliderNameY = kGestureBoxY + kControlH + 6;   // 547
    constexpr int kGestureSliderY = kGestureSliderNameY + kNameH;  // 563

    // Two columns of 178 with a gap of 16.
    constexpr int kColW = 178, kColGap = 16;
    constexpr int kCol1X = kMargin;                                // 24
    constexpr int kCol2X = kMargin + kColW + kColGap;              // 218

    // "Ziel" and "Geigenhand" in one row: inline labels.
    constexpr int kTargetLabelW = 34, kLabelGap = 0;
    constexpr int kTargetBoxX = kCol1X + kTargetLabelW + kLabelGap;   // 58
    constexpr int kTargetBoxW = kColW - kTargetLabelW - kLabelGap;    // 144
    constexpr int kHandLabelW = 70;
    constexpr int kHandRowX = kCol2X + kHandLabelW;                   // 288
    constexpr int kHandRowW = kColW - kHandLabelW;                    // 108

    constexpr float kMinConfidence = 0.3f;
    constexpr juce::uint32 kGestureShownMs = 1000, kNoticeShownMs = 2000;

    juce::String U (const char* utf8) { return juce::String::fromUTF8 (utf8); }

    const char* const kHeightTip =
        "\xc3\x9c" "bersetzt das Heben und Senken des Geigenhalses in kontinuierliche MIDI-Steuerdaten "
        "(z. B. Expression, Vibrato-Tiefe oder Harmonie-Mix). Geige in die tiefste bzw. h\xc3\xb6" "chste Haltung bringen "
        "und den passenden Knopf dr\xc3\xbc" "cken.";

    const char* const kGestureTip =
        "Erkennt Nicken oder Kopfsch\xc3\xbc" "tteln zum freih\xc3\xa4ndigen Umschalten von Harmonien, "
        "Halte-Funktionen oder Presets w\xc3\xa4hrend des Spiels.";

    // True once `deadline` (millisecond counter, 0 = none) has passed, also across the counter's wrap.
    bool passed (juce::uint32 now, juce::uint32 deadline) noexcept
    {
        return (juce::int32) (now - deadline) >= 0;
    }

    juce::String oneDecimal (double v)
    {
        return juce::String (v, 1).replaceCharacter ('.', ',');
    }
}

//==============================================================================
CameraPanel::CameraPanel (const CameraSettings& s, const juce::StringArray& cameraNames)
    : current (s), cameras (cameraNames)
{
    // Switch Aus | An.
    for (auto* b : { &offButton, &onButton })
    {
        b->setClickingTogglesState (true);
        b->setRadioGroupId (1);
        b->setWantsKeyboardFocus (false);
        b->setTooltip (U ("Schaltet die Kamera-Steuerung ein. Das Bild wird nur auf diesem Mac ausgewertet und nicht gespeichert."));
        powerRow.add (*b);
        b->onClick = [this] { readControls(); notify(); };
    }
    offButton.setConnectedEdges (juce::Button::ConnectedOnRight);
    onButton.setConnectedEdges (juce::Button::ConnectedOnLeft);
    onButton.setToggleState (current.enabled, juce::dontSendNotification);
    offButton.setToggleState (! current.enabled, juce::dontSendNotification);
    addAndMakeVisible (powerRow);

    cameraBox.setWantsKeyboardFocus (false);
    cameraBox.onChange = [this] { readControls(); notify(); };
    addAndMakeVisible (cameraBox);
    fillCameraList();

    // Height.
    lowButton.setButtonText ("Tief-Position setzen");
    highButton.setButtonText ("Hoch-Position setzen");
    for (auto* b : { &lowButton, &highButton })
    {
        b->setWantsKeyboardFocus (false);
        b->setTooltip (U (kHeightTip));
        addAndMakeVisible (b);
    }
    lowButton.onClick = [this] { calibrate (false); };
    highButton.onClick = [this] { calibrate (true); };

    targetBox.addItem ("Kein Ziel", CameraSettings::targetNone + 1);
    targetBox.addItem ("CC1 (Modwheel)", CameraSettings::targetModWheel + 1);
    targetBox.addItem ("CC11 (Expression)", CameraSettings::targetExpression + 1);
    targetBox.addItem ("CC74 (Filter)", CameraSettings::targetFilter + 1);
    targetBox.addItem ("Harmonie-Mix", CameraSettings::targetHarmonyLevel + 1);
    targetBox.setSelectedId (current.heightTarget + 1, juce::dontSendNotification);
    targetBox.setTooltip (U (kHeightTip));
    targetBox.setWantsKeyboardFocus (false);
    targetBox.onChange = [this] { readControls(); notify(); };
    addAndMakeVisible (targetBox);

    for (auto* b : { &leftHandButton, &rightHandButton })
    {
        b->setClickingTogglesState (true);
        b->setRadioGroupId (2);
        b->setWantsKeyboardFocus (false);
        b->setTooltip (U ("Welche Hand den Geigenhals h\xc3\xa4lt. \xc3\x9c" "blich ist die linke."));
        handRow.add (*b);
        b->onClick = [this] { readControls(); notify(); };
    }
    leftHandButton.setConnectedEdges (juce::Button::ConnectedOnRight);
    rightHandButton.setConnectedEdges (juce::Button::ConnectedOnLeft);
    rightHandButton.setToggleState (current.height.rightHand, juce::dontSendNotification);
    leftHandButton.setToggleState (! current.height.rightHand, juce::dontSendNotification);
    addAndMakeVisible (handRow);

    const auto setupSlider = [this] (juce::Slider& sl, double low, double high, double step, double defaultValue,
                                     double value, const juce::String& tip)
    {
        sl.setSliderStyle (juce::Slider::LinearHorizontal);
        sl.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        sl.setRange (low, high, step);
        sl.setDoubleClickReturnValue (true, defaultValue);
        sl.setWantsKeyboardFocus (false);
        sl.setTooltip (tip);
        sl.setValue (value, juce::dontSendNotification);
        sl.onValueChange = [this] { readControls(); notify(); repaint(); };
        addAndMakeVisible (sl);
    };
    setupSlider (smoothingSlider, 0.0, 1000.0, 10.0, 150.0, current.height.smoothingMs,
                 U ("Gl\xc3\xa4ttet die Bewegung. H\xc3\xb6here Werte machen den Verlauf ruhiger, aber tr\xc3\xa4ger."));
    setupSlider (deadZoneSlider, 0.0, 20.0, 1.0, 3.0, current.height.deadZone * 100.0,
                 U ("Bewegungen, die kleiner sind als dieser Anteil des kalibrierten Bereichs, werden ignoriert."));

    // Gestures.
    fillActionList (nodBox);
    fillActionList (shakeBox);
    nodBox.setSelectedId (current.nodAction + 1, juce::dontSendNotification);
    shakeBox.setSelectedId (current.shakeAction + 1, juce::dontSendNotification);
    for (auto* b : { &nodBox, &shakeBox })
    {
        b->setTooltip (U (kGestureTip));
        b->setWantsKeyboardFocus (false);
        b->onChange = [this] { readControls(); notify(); };
        addAndMakeVisible (b);
    }

    setupSlider (sensitivitySlider, 0.0, 100.0, 1.0, 50.0, current.gestures.sensitivity * 100.0,
                 U ("Wie deutlich eine Kopfbewegung sein muss. H\xc3\xb6her erkennt auch kleine Bewegungen, "
                    "l\xc3\xb6st aber leichter versehentlich aus."));
    setupSlider (cooldownSlider, 0.3, 5.0, 0.1, 1.2, current.gestures.cooldownMs / 1000.0,
                 U ("Nach einer erkannten Geste wird f\xc3\xbcr diese Zeit keine weitere ausgel\xc3\xb6st. "
                    "Verhindert Doppelausl\xc3\xb6sungen."));

    setSize (kWidth, kHeight);
}

//==============================================================================
void CameraPanel::fillActionList (juce::ComboBox& box)
{
    box.addItem ("Keine Aktion", CameraSettings::actionNone + 1);
    box.addItem ("Harmonie an/aus", CameraSettings::actionHarmony + 1);
    box.addItem ("Solo halten", CameraSettings::actionFreezeSolo + 1);
    box.addItem ("Harmonie halten", CameraSettings::actionFreezeHarmony + 1);
    box.addItem (U ("N\xc3\xa4" "chstes Preset"), CameraSettings::actionPresetNext + 1);
    box.addItem ("Vorheriges Preset", CameraSettings::actionPresetPrevious + 1);
    box.addItem ("Erkennung an/aus", CameraSettings::actionDetection + 1);
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
    cameraBox.setTooltip (juce::String());
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
    current.enabled = onButton.getToggleState();

    const int camera = cameraBox.getSelectedId();
    if (camera == 1)
        current.deviceName = juce::String();
    else if (camera >= 2 && camera - 2 < cameras.size())
        current.deviceName = cameras[camera - 2];
    // else: the stored camera that is not connected stays as it is

    current.heightTarget = juce::jlimit (0, (int) CameraSettings::numTargets - 1, targetBox.getSelectedId() - 1);
    current.height.rightHand = rightHandButton.getToggleState();
    current.height.smoothingMs = (float) smoothingSlider.getValue();
    current.height.deadZone = (float) (deadZoneSlider.getValue() / 100.0);

    current.nodAction = juce::jlimit (0, (int) CameraSettings::numActions - 1, nodBox.getSelectedId() - 1);
    current.shakeAction = juce::jlimit (0, (int) CameraSettings::numActions - 1, shakeBox.getSelectedId() - 1);
    current.gestures.sensitivity = (float) (sensitivitySlider.getValue() / 100.0);
    current.gestures.cooldownMs = (float) (juce::roundToInt (cooldownSlider.getValue() * 10.0) * 100);
}

void CameraPanel::notify()
{
    if (onChange)
        onChange (current);
}

void CameraPanel::calibrate (bool high)
{
    float raw = 0.0f;
    if (! lastPose.valid || ! cam::HeightTracker::measure (lastPose, current.height.rightHand, raw))
    {
        noticeText = U ("Die Geigenhand ist gerade nicht zu sehen.");
        noticeUntil = juce::Time::getMillisecondCounter() + kNoticeShownMs;
        startTimerHz (10);
        repaint();
        return;
    }

    (high ? current.height.high : current.height.low) = raw;
    notify();
}

//==============================================================================
void CameraPanel::setStatus (const juce::String& text, bool isProblem)
{
    statusText = text;
    statusIsProblem = isProblem;
    repaint (kMargin, kStatusY, kContentW, kLineH);
}

void CameraPanel::showFrame (const juce::Image& preview, const cam::Pose& pose, int value, cam::Gesture gesture)
{
    picture = preview;
    lastPose = pose;
    controllerValue = value;

    if (gesture != cam::Gesture::None)
    {
        gestureText = gesture == cam::Gesture::Nod ? juce::String ("Nicken erkannt")
                                                   : U ("Kopfsch\xc3\xbc" "tteln erkannt");
        gestureUntil = juce::Time::getMillisecondCounter() + kGestureShownMs;
        startTimerHz (10);
        repaint (kMargin, kGestureY, kContentW, kLineH);
    }

    repaint (kMargin, kPreviewY, kContentW, kPreviewH);
    repaint (kMargin, kNameY, kContentW, kNameH);
}

void CameraPanel::timerCallback()
{
    const auto now = juce::Time::getMillisecondCounter();

    if (gestureUntil != 0 && passed (now, gestureUntil))
    {
        gestureUntil = 0;
        gestureText = juce::String();
        repaint (kMargin, kGestureY, kContentW, kLineH);
    }
    if (noticeUntil != 0 && passed (now, noticeUntil))
    {
        noticeUntil = 0;
        noticeText = juce::String();
        repaint (kMargin, kStatusY, kContentW, kLineH);
    }
    if (gestureUntil == 0 && noticeUntil == 0)
        stopTimer();
}

//==============================================================================
void CameraPanel::resized()
{
    powerRow.setBounds (kMargin, kHeaderY, kSwitchW, kHeaderH);
    cameraBox.setBounds (kCameraX, kHeaderY, kCameraW, kHeaderH);

    lowButton.setBounds (kMargin, kCalibrateY, kCalibrateW, kControlH);
    highButton.setBounds (kMargin + kCalibrateW + kCalibrateGap, kCalibrateY, kCalibrateW, kControlH);

    targetBox.setBounds (kTargetBoxX, kTargetY, kTargetBoxW, kControlH);
    handRow.setBounds (kHandRowX, kTargetY, kHandRowW, kControlH);

    smoothingSlider.setBounds (kCol1X, kSliderY, kColW, kSliderH);
    deadZoneSlider.setBounds (kCol2X, kSliderY, kColW, kSliderH);

    nodBox.setBounds (kCol1X, kGestureBoxY, kColW, kControlH);
    shakeBox.setBounds (kCol2X, kGestureBoxY, kColW, kControlH);

    sensitivitySlider.setBounds (kCol1X, kGestureSliderY, kColW, kSliderH);
    cooldownSlider.setBounds (kCol2X, kGestureSliderY, kColW, kSliderH);
}

void CameraPanel::paintOverlay (juce::Graphics& g, juce::Rectangle<float> area)
{
    using namespace brand;

    if (! lastPose.valid)
        return;

    const auto at = [&area] (const cam::Point& p)
    {
        return juce::Point<float> (area.getX() + p.x * area.getWidth(), area.getY() + p.y * area.getHeight());
    };
    const auto seen = [] (const cam::Point& p) { return p.confidence >= kMinConfidence; };

    g.setColour (colour::accent);

    const auto line = [&] (const cam::Point& a, const cam::Point& b)
    {
        if (seen (a) && seen (b))
            g.drawLine (juce::Line<float> (at (a), at (b)), 1.5f);
    };
    line (lastPose.leftShoulder, lastPose.rightShoulder);
    line (lastPose.leftShoulder, lastPose.leftElbow);
    line (lastPose.leftElbow, lastPose.leftWrist);
    line (lastPose.rightShoulder, lastPose.rightElbow);
    line (lastPose.rightElbow, lastPose.rightWrist);

    const auto dot = [&] (const cam::Point& p)
    {
        if (seen (p))
            g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre (at (p)));
    };
    for (const auto* p : { &lastPose.nose, &lastPose.leftEye, &lastPose.rightEye, &lastPose.leftEar, &lastPose.rightEar,
                           &lastPose.leftShoulder, &lastPose.rightShoulder, &lastPose.leftElbow, &lastPose.rightElbow,
                           &lastPose.leftWrist, &lastPose.rightWrist })
        dot (*p);

    // The violin hand gets a ring.
    const auto& hand = current.height.rightHand ? lastPose.rightWrist : lastPose.leftWrist;
    if (seen (hand))
        g.drawEllipse (juce::Rectangle<float> (16.0f, 16.0f).withCentre (at (hand)), 1.5f);
}

void CameraPanel::paintPreview (juce::Graphics& g, juce::Rectangle<int> area)
{
    using namespace brand;

    g.setColour (colour::display);
    g.fillRect (area);

    auto pictureArea = area.toFloat();
    if (picture.isValid())
    {
        pictureArea = juce::RectanglePlacement (juce::RectanglePlacement::centred)
                          .appliedTo (picture.getBounds().toFloat(), area.toFloat());
        g.drawImage (picture, pictureArea);
    }
    else
    {
        g.setColour (colour::tertiary);
        g.setFont (font (12.5f));
        g.drawText (current.enabled ? U ("Warte auf das Bild \xe2\x80\xa6") : juce::String ("Kamera aus"), area,
                    juce::Justification::centred, false);
    }

    g.saveState();
    g.reduceClipRegion (area);
    paintOverlay (g, pictureArea);

    // The position between low and high, as a bar on the right edge.
    if (controllerValue >= 0)
    {
        const float fraction = juce::jlimit (0.0f, 1.0f, (float) controllerValue / 127.0f);
        const float h = fraction * (float) area.getHeight();
        g.setColour (colour::accent);
        g.fillRect ((float) (area.getRight() - kBarW), (float) area.getBottom() - h, (float) kBarW, h);
    }
    g.restoreState();
}

void CameraPanel::paint (juce::Graphics& g)
{
    using namespace brand;

    g.fillAll (colour::window);

    paintPreview (g, juce::Rectangle<int> (kMargin, kPreviewY, kContentW, kPreviewH));

    // Status line, or the notice that replaces it for two seconds; the gesture line below.
    g.setFont (font (11.5f));
    const bool notice = noticeText.isNotEmpty();
    g.setColour ((notice || statusIsProblem) ? colour::problem : colour::secondary);
    g.drawText (notice ? noticeText : statusText, juce::Rectangle<int> (kMargin, kStatusY, kContentW, kLineH),
                juce::Justification::centredLeft, true);

    g.setColour (colour::secondary);
    g.drawText (gestureText, juce::Rectangle<int> (kMargin, kGestureY, kContentW, kLineH),
                juce::Justification::centredLeft, true);

    g.setColour (colour::separator);
    g.fillRect (kMargin, kSep1Y, kContentW, 1);
    g.fillRect (kMargin, kSep2Y, kContentW, 1);

    g.setFont (font (12.5f));

    const auto label = [&g] (const juce::String& text, juce::Rectangle<int> r)
    {
        g.setColour (colour::secondary);
        g.drawText (text, r, juce::Justification::centredLeft, false);
    };
    const auto value = [&g] (const juce::String& text, juce::Rectangle<int> r)
    {
        g.setColour (colour::text);
        g.drawText (text, r, juce::Justification::centredRight, false);
    };

    // Height group.
    const juce::Rectangle<int> heightLine (kMargin, kNameY, kContentW, kNameH);
    label (U ("Schnecken-H\xc3\xb6he"), heightLine);
    value (controllerValue >= 0 ? juce::String (controllerValue) : U ("\xe2\x80\x93"), heightLine);

    label ("Ziel", juce::Rectangle<int> (kCol1X, kTargetY, kTargetLabelW, kControlH));
    label ("Geigenhand", juce::Rectangle<int> (kCol2X, kTargetY, kHandLabelW, kControlH));

    const juce::Rectangle<int> smoothingLine (kCol1X, kSliderNameY, kColW, kNameH), deadZoneLine (kCol2X, kSliderNameY, kColW, kNameH);
    label (U ("Gl\xc3\xa4ttung"), smoothingLine);
    value (juce::String (juce::roundToInt (smoothingSlider.getValue())) + " ms", smoothingLine);
    label ("Totzone", deadZoneLine);
    value (juce::String (juce::roundToInt (deadZoneSlider.getValue())) + " %", deadZoneLine);

    // Gesture group.
    label ("Nicken", juce::Rectangle<int> (kCol1X, kGestureNameY, kColW, kNameH));
    label (U ("Kopfsch\xc3\xbc" "tteln"), juce::Rectangle<int> (kCol2X, kGestureNameY, kColW, kNameH));

    const juce::Rectangle<int> sensitivityLine (kCol1X, kGestureSliderNameY, kColW, kNameH),
                               cooldownLine (kCol2X, kGestureSliderNameY, kColW, kNameH);
    label ("Empfindlichkeit", sensitivityLine);
    value (juce::String (juce::roundToInt (sensitivitySlider.getValue())) + " %", sensitivityLine);
    label ("Sperrzeit", cooldownLine);
    value (oneDecimal (cooldownSlider.getValue()) + " s", cooldownLine);
}
