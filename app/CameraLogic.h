// What the camera's picture means: how high the violin is held, and whether
// the player nods or shakes the head.
//
// The camera part proper (CameraTracker) finds a few points of the body in each
// frame. Everything after that is here, in plain C++ without JUCE or Apple
// frameworks, so it runs in the tests: calibration, dead zone and smoothing for
// the height, and the recognition of the two gestures.
#pragma once

namespace cam
{

/** A point in the picture: x to the right, y DOWNWARDS, both 0..1 of the frame.
    `confidence` 0..1; below about 0.3 the point is a guess. */
struct Point
{
    float x = 0.0f, y = 0.0f, confidence = 0.0f;
};

/** The body points of one frame. Left and right are the player's own. */
struct Pose
{
    bool   valid = false;       ///< a person was found
    double timeSec = 0.0;       ///< when the frame was taken, any steady clock
    Point  nose, leftEye, rightEye, leftEar, rightEar, neck;
    Point  leftShoulder, rightShoulder, leftElbow, rightElbow, leftWrist, rightWrist;
};

//==============================================================================
/** How high the violin is held, as a controller value.

    The scroll itself is not something a camera model knows, but the hand that
    holds the neck is right below it: its height against the shoulders, measured
    in shoulder widths so that the distance to the camera does not matter. */
class HeightTracker
{
public:
    struct Settings
    {
        bool  rightHand = false;   ///< the violin is held with the right hand (left-handed instrument)
        float low = -0.2f;         ///< the raw measure at "low position" (calibration)
        float high = 0.6f;         ///< ...and at "high position"
        float smoothingMs = 150.0f; ///< 0 .. 1000
        float deadZone = 0.03f;    ///< 0 .. 0.2 of the calibrated range: smaller movements are ignored
    };

    /** The raw measure for a pose: positive = hand above the shoulder line.
        False if the hand or the shoulders are not visible well enough. */
    static bool measure (const Pose&, bool rightHand, float& raw) noexcept;

    /** Feeds one frame. Returns true when the controller value changed;
        `value` is 0..127. While the hand is not visible the value stays. */
    bool update (const Pose&, const Settings&, int& value) noexcept;

    /** The last raw measure, for the calibration buttons; false before the first. */
    bool lastRaw (float& raw) const noexcept;

    void reset() noexcept;

private:
    bool   haveRaw = false, haveValue = false;
    float  raw = 0.0f;          // last measure
    float  smoothed = 0.0f;     // position 0..1 after smoothing
    float  held = 0.0f;         // position 0..1 after the dead zone
    double lastTime = 0.0;
    int    lastValue = -1;
};

//==============================================================================
enum class Gesture { None, Nod, Shake };

/** Nodding (down and up again) and shaking the head (to one side and back or
    across), told apart from the movement of playing: a gesture is a quick,
    clear there-and-back of the head against the shoulders within about a
    second, not a slow drift and not the small steady motion of bowing. */
class GestureDetector
{
public:
    struct Settings
    {
        float sensitivity = 0.5f;   ///< 0 (only big movements) .. 1 (small ones count)
        float cooldownMs = 1200.0f; ///< after a gesture nothing is recognised for this long, 300 .. 5000
    };

    /** Feeds one frame; returns the gesture that was completed with it, if any. */
    Gesture update (const Pose&, const Settings&) noexcept;

    void reset() noexcept;

private:
    static constexpr int kHistory = 48;   // about 3 s at 15 frames per second

    struct Sample { double time = 0.0; float turn = 0.0f, tilt = 0.0f; };

    Sample history[kHistory];
    int    count = 0, head = 0;
    double blockedUntil = 0.0;
};

} // namespace cam
