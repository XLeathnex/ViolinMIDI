// The camera control in a window of its own ("Kamera").
//
// Top: the switch and the camera. Then the mirrored preview: the tracked body,
// the zero point with its silver dead-zone oval, the violin hand as a crimson
// cursor (with a line to the zero point while it moves the values) and the
// small XY HUD while it does. While the camera is off the dimmed emblem stands
// in for the picture; while it is starting, the emblem breathes. Under the
// preview: what the camera is doing, a recognised gesture, and the settings of
// the scroll pad (zero point, violin hand, target and response curve of each
// axis, dead zone, tempo) and of the head gestures.
//
// The panel tracks nothing itself. The main window owns the PadTracker and
// hands it over with every frame; the panel only shows what it reports.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "CameraLogic.h"
#include "CameraSettings.h"
#include "PadHud.h"
#include "Widgets.h"

#include <functional>
#include <utility>
#include <vector>

class CameraPanel final : public juce::Component,
                          public juce::TooltipClient,
                          private juce::Timer
{
public:
    static constexpr int kWidth = 440, kHeight = 748;

    CameraPanel (const CameraSettings&, const juce::StringArray& cameraNames);

    /** Any change, including the on/off switch (CameraSettings::enabled). */
    std::function<void (const CameraSettings&)> onChange;

    /** "Nullpunkt setzen": the next frame that shows the violin hand becomes the zero point. */
    std::function<void()> onRecalibrate;

    const CameraSettings& settings() const noexcept   { return current; }

    void setCameras (const juce::StringArray& names);

    /** A new frame: the mirrored preview picture (may be invalid: the last one
        stays), the body points in its coordinates (0..1, y down), the pad as it
        is after this frame, and a gesture that was just recognised (shown for
        about a second). The pad is only read, not kept. */
    void showFrame (const juce::Image& preview, const cam::Pose& pose, const cam::PadTracker& pad, cam::Gesture gesture);

    /** One line under the picture: what the camera is doing, or what is wrong (amber). */
    void setStatus (const juce::String& text, bool isProblem);

    /** The camera is on but no picture has come yet: the emblem in the preview
        breathes until the first one arrives. Also drops a picture of a camera
        that was switched away from. */
    void setWaiting (bool waitingForPicture);

    /** Shows the switch on or off without reporting it, for when the camera
        could not be started. Optional. */
    void setPowerState (bool on);

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Explains the part under the mouse: the preview, or the name of a control. */
    juce::String getTooltip() override;

private:
    /** Linear | Fein | Schnell, one axis's response curve. */
    struct CurveSwitch
    {
        explicit CurveSwitch (int radioGroup);
        void set (int response);
        int  get() const;

        juce::TextButton buttons[cam::PadTracker::numResponses];
        ButtonRow row;
    };

    /** What the panel took from the pad with the last frame. */
    struct PadView
    {
        bool  known = false;                 // a frame has come since the picture was dropped
        bool  zeroTaken = false;             // the pad has a zero point (it may still not be drawable)
        bool  hasZero = false, hasHand = false, active = false;   // what can be drawn now
        float zeroX = 0.0f, zeroY = 0.0f;    // picture coordinates, 0..1, y down
        float handX = 0.0f, handY = 0.0f;
        float x = 0.5f, y = 0.5f;            // the two values, 0..1
    };

    void timerCallback() override;
    void updateTimer();

    void fillCameraList();
    static void fillActionList (juce::ComboBox&);
    static void fillTargetList (juce::ComboBox&);
    void readControls();
    void notify();
    void dropPicture();
    void placeHud();
    void showNotice (const juce::String& text, bool isProblem);
    void updateHudNames();

    juce::Rectangle<int> previewArea() const;
    juce::Rectangle<float> pictureArea() const;
    void paintPreview (juce::Graphics&);
    void paintPlaceholder (juce::Graphics&, juce::Rectangle<float> area);
    void paintBody (juce::Graphics&, juce::Rectangle<float> pictureBounds) const;
    void paintPad (juce::Graphics&, juce::Rectangle<float> pictureBounds) const;
    void paintCaption (juce::Graphics&, juce::Rectangle<float> pictureBounds) const;
    void repaintLiveParts();

    CameraSettings current;
    juce::StringArray cameras;

    // The last frame.
    juce::Image picture;
    cam::Pose lastPose;
    PadView padView;
    int  framesWithoutHand = 0;
    bool hudOnLeft = true;
    bool waiting = false;

    juce::String statusText, gestureText, noticeText;
    bool statusIsProblem = false, noticeIsProblem = false;
    juce::uint32 gestureUntil = 0, noticeUntil = 0;   // millisecond counter; 0 = not shown

    // Names of controls that explain themselves like the control below them.
    std::vector<std::pair<juce::Rectangle<int>, juce::String>> nameTips;

    juce::TextButton offButton { "Aus" }, onButton { "An" };
    ButtonRow powerRow;
    juce::ComboBox cameraBox;

    juce::TextButton recalibrateButton { "Nullpunkt setzen" };
    juce::TextButton leftHandButton { "Links" }, rightHandButton { "Rechts" };
    ButtonRow handRow;
    juce::ComboBox targetX, targetY;
    CurveSwitch curveX { 3 }, curveY { 4 };
    juce::Slider deadZoneSlider, tempoSlider;

    juce::ComboBox nodBox, shakeBox;
    juce::Slider sensitivitySlider, cooldownSlider;

    PadHud hud;   // over the preview, in the top corner away from the violin

    // The panel lives in a window of its own, so it brings its own tooltips.
    juce::TooltipWindow tooltips { this, 700 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CameraPanel)
};
