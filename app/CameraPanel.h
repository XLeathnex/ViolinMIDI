// The camera control in a small window of its own: switch and camera choice,
// the mirrored preview with the tracked body points and the current position,
// the calibration of the violin height with its target, and the head gestures.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "CameraLogic.h"
#include "CameraSettings.h"
#include "Widgets.h"

#include <functional>

class CameraPanel final : public juce::Component,
                          private juce::Timer
{
public:
    CameraPanel (const CameraSettings&, const juce::StringArray& cameraNames);

    /** Any change, including the on/off switch (CameraSettings::enabled). */
    std::function<void (const CameraSettings&)> onChange;

    const CameraSettings& settings() const noexcept   { return current; }

    void setCameras (const juce::StringArray& names);

    /** A new frame: the mirrored preview picture (may be invalid), the body points in its coordinates
        (0..1, y down), the controller value 0..127 the height gives right now (-1: unknown), and a
        gesture that was just recognised (shown for about a second). */
    void showFrame (const juce::Image& preview, const cam::Pose& pose, int controllerValue, cam::Gesture gesture);

    /** One line under the picture: what the camera is doing, or what is wrong. */
    void setStatus (const juce::String& text, bool isProblem);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    void fillCameraList();
    void fillActionList (juce::ComboBox&);
    void calibrate (bool high);
    void notify();
    void readControls();
    void paintPreview (juce::Graphics&, juce::Rectangle<int> area);
    void paintOverlay (juce::Graphics&, juce::Rectangle<float> picture);

    CameraSettings current;
    juce::StringArray cameras;

    // The last frame.
    juce::Image picture;
    cam::Pose lastPose;
    int controllerValue = -1;

    juce::String statusText;
    bool statusIsProblem = false;
    juce::String gestureText, noticeText;
    juce::uint32 gestureUntil = 0, noticeUntil = 0;   // millisecond counter; 0 = not shown

    juce::TextButton offButton { "Aus" }, onButton { "An" };
    ButtonRow powerRow;
    juce::ComboBox cameraBox;

    juce::TextButton lowButton, highButton;
    juce::ComboBox targetBox;
    juce::TextButton leftHandButton { "Links" }, rightHandButton { "Rechts" };
    ButtonRow handRow;
    juce::Slider smoothingSlider, deadZoneSlider;

    juce::ComboBox nodBox, shakeBox;
    juce::Slider sensitivitySlider, cooldownSlider;

    // The panel lives in a window of its own, so it brings its own tooltips.
    juce::TooltipWindow tooltips { this, 700 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CameraPanel)
};
