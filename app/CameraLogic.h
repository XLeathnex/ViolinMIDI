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

/** 2.1: what the camera queue's bow tracker (CameraBow.h) saw in a frame, for drawing. */
struct BowReport
{
    bool  tracked = false;      ///< the bow hand is seen and moves along a stroke
    int   direction = 0;        ///< +1 down-bow (away from the violin), -1 up-bow, 0 unknown
    float x = 0.0f, y = 0.0f;   ///< the bow hand, picture coordinates
    float axisX = 0.0f, axisY = 0.0f;   ///< unit direction of a down-bow in the picture
    bool  armed = false;        ///< a change of bow is announced now
    bool  changed = false;      ///< a change of bow was confirmed with this frame
    int   changes = 0;          ///< confirmed changes since BowTracker::reset()
};

/** The body points of one frame. Left and right are the player's own. */
struct Pose
{
    bool   valid = false;       ///< a person was found
    double timeSec = 0.0;       ///< when the frame was taken, any steady clock (2.1: the capture time)
    Point  nose, leftEye, rightEye, leftEar, rightEar, neck;
    Point  leftShoulder, rightShoulder, leftElbow, rightElbow, leftWrist, rightWrist;

    double readySec = 0.0;      ///< 2.1: when its points were known, same clock as timeSec (0: = timeSec)
    Point  marker;              ///< 2.1: the colour marker on the scroll; confidence 0 = not looked for / not found
    BowReport bow;              ///< 2.1: the camera queue's bow hand (drawing only)
};

/** 2.1: how old a frame is when its result is known: host clock now minus the frame's
    presentation time, both in seconds; outside 0 .. 0.5 s (an unexpected clock) kFallbackAgeSec. */
constexpr double kFallbackAgeSec = 0.05;
double frameAge (double hostNowSec, double presentationSec) noexcept;

//==============================================================================
/** 2.1: a fast vertical movement of the scroll ("Schnecken-Geste"): up switches Chord Freeze on,
    down switches it off. */
enum class Flick { none, up, down };

/** Finds flicks in the scroll's deflection: fast enough, far enough, short and mostly vertical,
    confirmed against one-frame glitches, then locked for the cooldown. Owned by PadTracker,
    public for the tests. */
class FlickDetector
{
public:
    struct Params
    {
        float speed = 1.5f;          ///< shoulder widths per second, 0.6 .. 4
        float cooldownMs = 800.0f;   ///< lock time after a flick, 300 .. 3000
    };

    /** One frame. `x`, `y` (up positive) in shoulder widths; `width` = the shoulder width as a share
        of the picture; `continuous` false after a source switch or a gap (the history is cleared). */
    Flick update (double t, float x, float y, float width, bool continuous, const Params&) noexcept;
    /** A movement that may become a flick is under way. */
    bool fastMovement() const noexcept;
    void reset() noexcept;
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
    using Flick = cam::Flick;

    enum Response { linear = 0, fine = 1, fast = 2, numResponses };   ///< fine: exponential, gentle near the zone; fast: logarithmic

    /** 2.1: where the zero point comes from. posture: "Nullpunkt setzen", the current posture
        against the shoulders (2.0); centre: "Totzone mittig setzen", the middle of the picture. */
    enum Anchor { posture = 0, centre = 1, numAnchors };

    struct Settings
    {
        bool  rightHand = false;   ///< the violin is held with the right hand (left-handed instrument)
        float deadZone = 0.06f;    ///< radius of the dead zone, share of the picture, 0.03 .. 0.15 (2.1: .. 0.25)
        int   responseX = linear, responseY = linear;
        float speed = 0.8f;        ///< full deflection sweeps a value through its whole range in 1 / speed s, 0.1 .. 4

        // 2.1
        int   anchor = posture;          ///< Anchor
        bool  useMarker = false;         ///< the colour marker stands for the scroll when it is seen
        bool  flick = false;             ///< "Schnecken-Geste (Chord Freeze)": flicks are taken out of the pad
        float flickSpeed = 1.5f;         ///< FlickDetector::Params::speed
        float flickCooldownMs = 800.0f;  ///< FlickDetector::Params::cooldownMs
    };

    /** 2.1: the dead zone's range (the profile, the slider and the oval use it). */
    static constexpr float kMinDeadZone = 0.03f, kMaxDeadZone = 0.25f;

    /** 2.1: what stood for the scroll in the last frame. */
    enum class Source { none, body, marker };
    static constexpr float  kMarkerConfidence = 0.4f;
    static constexpr double kMarkerGraceSec = 0.4;   ///< marker gone this long: the violin hand takes over

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

    /** Puts the values where they are elsewhere already (0..1 each), so an axis
        takes over from there instead of jumping. */
    void setValues (float x, float y) noexcept;

    // --- 2.1
    /** Centre anchor: the scroll has not been inside the dead zone since recalibrate() or the
        switch-on, so the values wait. */
    bool waitingForZone() const noexcept;
    Source source() const noexcept;
    bool markerOffsetKnown() const noexcept;
    /** The scroll point against its reference in the last frame (posture: against the shoulders,
        centre: the picture position), in the pad's units. False if unknown. For the calibration. */
    bool lastOffset (float& dx, float& dy) const noexcept;
    /** A fast movement is being checked, or a flick episode holds the values. */
    bool flickHeld() const noexcept;
    /** The flick recognised by the last update(), once. */
    Flick takeFlick() noexcept;

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
