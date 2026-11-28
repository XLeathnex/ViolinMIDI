#include "CameraLogic.h"

#include <algorithm>
#include <cmath>

namespace cam
{

namespace
{
    constexpr float  kMinConfidence = 0.3f;     // below this a point is a guess
    constexpr float  kMinShoulderWidth = 0.03f; // of the frame: narrower, and nothing can be measured against it
    constexpr double kLongestStep = 0.5;        // s: a longer pause between frames starts afresh

    // Gestures. All sizes are in shoulder widths, so the distance to the camera does not matter.
    constexpr double kGestureWindow = 1.0;      // s: there and back must fit into this
    constexpr int    kMinSamples = 5;
    constexpr float  kLargestThreshold = 0.24f; // sensitivity 0: only a big movement counts
    constexpr float  kSmallestThreshold = 0.12f; // sensitivity 1; below this the jitter of the points alone looks like nodding
    constexpr float  kReturnShare = 0.5f;       // back to within this share of the movement = "back"
    constexpr float  kOtherAxisShare = 0.8f;    // the other direction may move at most this much of it
    constexpr float  kSecondSideShare = 0.4f;   // shaking: the weaker side, relative to the threshold

    inline float clampf (float v, float lo, float hi) noexcept { return std::min (hi, std::max (lo, v)); }
    inline bool  seen (const Point& p) noexcept                { return p.confidence >= kMinConfidence; }

    /** The middle between the shoulders and their distance; false if they are not both visible. */
    bool shoulders (const Pose& p, float& midX, float& midY, float& width) noexcept
    {
        if (! p.valid || ! seen (p.leftShoulder) || ! seen (p.rightShoulder))
            return false;

        midX = 0.5f * (p.leftShoulder.x + p.rightShoulder.x);
        midY = 0.5f * (p.leftShoulder.y + p.rightShoulder.y);
        width = std::hypot (p.leftShoulder.x - p.rightShoulder.x, p.leftShoulder.y - p.rightShoulder.y);
        return width >= kMinShoulderWidth;
    }
}

//==============================================================================
bool PadTracker::measure (const Pose& p, bool rightHand, float& dx, float& dy) noexcept
{
    const Point& wrist = rightHand ? p.rightWrist : p.leftWrist;
    float midX = 0.0f, midY = 0.0f, width = 0.0f;

    if (! shoulders (p, midX, midY, width) || ! seen (wrist))
        return false;

    dx = wrist.x - midX;
    dy = wrist.y - midY;
    return true;
}

float PadTracker::shape (float deflection, int response) noexcept
{
    const float d = clampf (deflection, -1.0f, 1.0f);
    const float a = std::abs (d);
    float out = a;

    if (response == fine)
        out = a * a;                                        // exponential: gentle near the zone, steep at the edge
    else if (response == fast)
        out = std::log1p (9.0f * a) / std::log (10.0f);     // logarithmic: quick off the mark

    return d < 0.0f ? -out : out;
}

int PadTracker::toValue (float v) noexcept
{
    return (int) std::lround (clampf (v, 0.0f, 1.0f) * 127.0f);
}

bool PadTracker::update (const Pose& p, const Settings& s) noexcept
{
    float dx = 0.0f, dy = 0.0f;
    if (! measure (p, s.rightHand, dx, dy))
    {
        moving = false;
        haveTime = false;   // a gap: the next frame starts the clock afresh
        haveBody = false;
        return false;
    }

    const Point& wrist = s.rightHand ? p.rightWrist : p.leftWrist;
    handX = wrist.x;
    handY = wrist.y;
    midX = handX - dx;
    midY = handY - dy;
    haveBody = true;

    const double step = haveTime ? p.timeSec - lastTime : 0.0;
    lastTime = p.timeSec;
    haveTime = true;

    if (needZero)
    {
        zeroX = dx;
        zeroY = dy;
        needZero = false;
        moving = false;
        return false;
    }

    // Away from the zero point, in units of the dead zone's radius.
    const float zone = clampf (s.deadZone, 0.03f, 0.15f);
    const float ox = dx - zeroX, oy = dy - zeroY;
    const float r = std::sqrt (ox * ox + oy * oy) / zone;

    if (r <= 1.0f)
    {
        moving = false;
        return false;
    }

    moving = true;

    // A step back in time or a long pause moves nothing.
    if (! (step > 0.0) || step > kLongestStep)
        return false;

    // Only what lies beyond the edge of the zone counts, so the values set off from zero.
    const float beyond = 1.0f - 1.0f / r;
    const float ex = clampf (ox * beyond / kReach, -1.0f, 1.0f);
    const float ey = clampf (-oy * beyond / kReach, -1.0f, 1.0f);   // up is more

    const float rate = clampf (s.speed, 0.1f, 4.0f) * (float) step;
    const int beforeX = valueX(), beforeY = valueY();
    px = clampf (px + shape (ex, s.responseX) * rate, 0.0f, 1.0f);
    py = clampf (py + shape (ey, s.responseY) * rate, 0.0f, 1.0f);

    return valueX() != beforeX || valueY() != beforeY;
}

bool PadTracker::zeroPoint (float& x, float& y) const noexcept
{
    if (needZero || ! haveBody)
        return false;
    x = midX + zeroX;
    y = midY + zeroY;
    return true;
}

bool PadTracker::handPoint (float& x, float& y) const noexcept
{
    if (! haveBody)
        return false;
    x = handX;
    y = handY;
    return true;
}

void PadTracker::reset() noexcept
{
    *this = PadTracker();
}

void PadTracker::setValues (float x, float y) noexcept
{
    px = clampf (x, 0.0f, 1.0f);
    py = clampf (y, 0.0f, 1.0f);
}

//==============================================================================
Gesture GestureDetector::update (const Pose& p, const Settings& s) noexcept
{
    float midX = 0.0f, midY = 0.0f, width = 0.0f;
    if (! shoulders (p, midX, midY, width) || ! seen (p.nose))
        return Gesture::None;

    // Where the nose is against the shoulders: turning the head moves it
    // sideways, nodding moves it down (y points down).
    Sample now;
    now.time = p.timeSec;
    now.turn = (p.nose.x - midX) / width;
    now.tilt = (p.nose.y - midY) / width;

    if (count > 0)
    {
        const Sample& last = history[(head + kHistory - 1) % kHistory];
        if (now.time < last.time || now.time - last.time > kLongestStep)
            count = 0;                              // the picture was gone for a while: start afresh
    }

    // During the cooldown nothing is kept either: a movement made then must not
    // be recognised the moment the cooldown ends.
    if (now.time < blockedUntil)
        count = 0;

    history[head] = now;
    head = (head + 1) % kHistory;
    count = std::min (count + 1, kHistory);

    if (now.time < blockedUntil)
        return Gesture::None;

    // The samples of the last second, oldest first.
    int used = 0;
    for (int i = 1; i <= count; ++i)
    {
        if (now.time - history[(head + kHistory - i) % kHistory].time > kGestureWindow)
            break;
        used = i;
    }

    if (used < kMinSamples)
        return Gesture::None;

    const Sample& first = history[(head + kHistory - used) % kHistory];
    float tiltHigh = first.tilt, tiltLow = first.tilt, turnHigh = first.turn, turnLow = first.turn;
    for (int i = 1; i <= used; ++i)
    {
        const Sample& sample = history[(head + kHistory - i) % kHistory];
        tiltHigh = std::max (tiltHigh, sample.tilt);
        tiltLow = std::min (tiltLow, sample.tilt);
        turnHigh = std::max (turnHigh, sample.turn);
        turnLow = std::min (turnLow, sample.turn);
    }

    const float threshold = kLargestThreshold + clampf (s.sensitivity, 0.0f, 1.0f) * (kSmallestThreshold - kLargestThreshold);
    const float tiltRange = tiltHigh - tiltLow, turnRange = turnHigh - turnLow;
    Gesture found = Gesture::None;

    // Nod: down and back up. How far down, measured from where the head was
    // before and is again now.
    const float down = tiltHigh - std::max (first.tilt, now.tilt);
    if (down >= threshold && std::abs (now.tilt - first.tilt) <= kReturnShare * down && turnRange <= kOtherAxisShare * down)
        found = Gesture::Nod;

    // Shake: to one side and to the other, and back.
    const float oneSide = turnHigh - std::max (first.turn, now.turn);
    const float otherSide = std::min (first.turn, now.turn) - turnLow;
    const float stronger = std::max (oneSide, otherSide), weaker = std::min (oneSide, otherSide);
    if (found == Gesture::None
        && stronger >= threshold && weaker >= kSecondSideShare * threshold
        && std::abs (now.turn - first.turn) <= kReturnShare * stronger && tiltRange <= kOtherAxisShare * turnRange)
        found = Gesture::Shake;

    if (found != Gesture::None)
    {
        blockedUntil = now.time + (double) clampf (s.cooldownMs, 300.0f, 5000.0f) * 0.001;
        count = 0;                                  // the same movement is not read twice
    }
    return found;
}

void GestureDetector::reset() noexcept
{
    *this = GestureDetector();
}

//==============================================================================
// 2.1

double frameAge (double, double) noexcept
{
    return kFallbackAgeSec;
}

Flick FlickDetector::update (double, float, float, float, bool, const Params&) noexcept
{
    return Flick::none;
}

bool FlickDetector::fastMovement() const noexcept
{
    return false;
}

void FlickDetector::reset() noexcept {}

bool PadTracker::waitingForZone() const noexcept
{
    return false;
}

PadTracker::Source PadTracker::source() const noexcept
{
    return haveBody ? Source::body : Source::none;
}

bool PadTracker::markerOffsetKnown() const noexcept
{
    return false;
}

bool PadTracker::lastOffset (float&, float&) const noexcept
{
    return false;
}

bool PadTracker::flickHeld() const noexcept
{
    return false;
}

Flick PadTracker::takeFlick() noexcept
{
    return Flick::none;
}

} // namespace cam
