// What the camera's picture means: where the violin scroll is moved to (two
// axes, like an XY pad), and whether the player nods or shakes the head.
//
// The camera part proper (CameraTracker) finds a few points of the body in each
// frame. Everything after that is here, in plain C++ without JUCE or Apple
// frameworks, so it runs in the tests: zero point, dead zone and response
// curves of the pad, and the recognition of the two gestures.
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
/** The violin scroll as a two-axis pad, like an XY morph pad played with the
    violin. The scroll itself is not something a camera model knows, but the
    hand that holds the neck is right below it and stands in for it: its
    position against the middle of the shoulders, so the player may sway.

    Relative, like a joystick. The position at rest is the zero point: it is
    taken from the first frame after recalibrate() (switching the control on
    or pressing the button). Inside a dead zone around it (a circle of
    `deadZone` of the picture, an oval on the 4:3 screen) both values stay
    where they are. Outside it they move continuously - the further out, the
    faster, shaped by a response curve per axis - and they stay where they got
    to once the hand is back inside. Right and up make a value grow. */
class PadTracker
{
public:
    enum Response { linear = 0, fine = 1, fast = 2, numResponses };   ///< fine: exponential, gentle near the zone; fast: logarithmic

    struct Settings
    {
        bool  rightHand = false;   ///< the violin is held with the right hand (left-handed instrument)
        float deadZone = 0.06f;    ///< radius of the dead zone, share of the picture, 0.03 .. 0.15
        int   responseX = linear, responseY = linear;
        float speed = 0.8f;        ///< full deflection sweeps a value through its whole range in 1 / speed s, 0.1 .. 4
    };

    /** Beyond the dead zone, this far (share of the picture) is full deflection. */
    static constexpr float kReach = 0.2f;

    /** The hand against the middle of the shoulders, in the picture (x right,
        y down). False if the hand or the shoulders are not visible well enough. */
    static bool measure (const Pose&, bool rightHand, float& dx, float& dy) noexcept;

    /** The response curve: deflection -1..1 in, speed -1..1 out. */
    static float shape (float deflection, int response) noexcept;

    /** The next frame that shows the hand becomes the zero point. */
    void recalibrate() noexcept   { needZero = true; }

    /** Feeds one frame. True when valueX() or valueY() changed. */
    bool update (const Pose&, const Settings&) noexcept;

    float x() const noexcept      { return px; }   ///< 0 .. 1
    float y() const noexcept      { return py; }
    int   valueX() const noexcept { return toValue (px); }   ///< 0 .. 127
    int   valueY() const noexcept { return toValue (py); }

    /** The hand is outside the dead zone and moving the values. */
    bool active() const noexcept  { return moving; }
    bool hasZero() const noexcept { return ! needZero; }

    /** For drawing, in picture coordinates (0..1, y down): the zero point as it
        is now (it moves with the shoulders) and the hand. False if unknown. */
    bool zeroPoint (float& x, float& y) const noexcept;
    bool handPoint (float& x, float& y) const noexcept;

    /** Both values back to the middle, and a new zero point with the next frame. */
    void reset() noexcept;

private:
    static int toValue (float v) noexcept;

    bool   needZero = true, moving = false, haveTime = false, haveBody = false;
    float  zeroX = 0.0f, zeroY = 0.0f;     // the hand against the shoulders at rest
    float  px = 0.5f, py = 0.5f;           // the two values
    float  midX = 0.0f, midY = 0.0f;       // the shoulders, last frame
    float  handX = 0.0f, handY = 0.0f;     // the hand, last frame
    double lastTime = 0.0;
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
