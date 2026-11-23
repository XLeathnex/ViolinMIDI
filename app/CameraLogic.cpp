#include "CameraLogic.h"

#include <algorithm>
#include <cmath>

namespace cam
{

namespace
{
    constexpr float  kMinConfidence = 0.3f;     // below this a point is a guess
    constexpr float  kMinShoulderWidth = 0.03f; // of the frame: narrower, and nothing can be measured against it
    constexpr float  kMinCalibratedSpan = 0.05f; // shoulder widths between "low" and "high"
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
bool HeightTracker::measure (const Pose& p, bool rightHand, float& out) noexcept
{
    const Point& wrist = rightHand ? p.rightWrist : p.leftWrist;
    float midX = 0.0f, midY = 0.0f, width = 0.0f;

    if (! shoulders (p, midX, midY, width) || ! seen (wrist))
        return false;

    out = (midY - wrist.y) / width;   // y points down: above the shoulders is positive
    return true;
}

bool HeightTracker::update (const Pose& p, const Settings& s, int& value) noexcept
{
    float r = 0.0f;
    if (! measure (p, s.rightHand, r))
        return false;

    raw = r;
    haveRaw = true;

    const float span = s.high - s.low;
    if (std::abs (span) < kMinCalibratedSpan)
        return false;                               // not calibrated: low and high are the same place

    const float position = clampf ((r - s.low) / span, 0.0f, 1.0f);
    const double step = p.timeSec - lastTime;
    lastTime = p.timeSec;

    if (! haveValue || step < 0.0 || step > kLongestStep)
    {
        smoothed = held = position;
    }
    else
    {
        const float tau = clampf (s.smoothingMs, 0.0f, 1000.0f) * 0.001f;
        const float share = tau <= 0.0f ? 1.0f : 1.0f - std::exp (-(float) step / tau);
        smoothed += share * (position - smoothed);

        // Dead zone: the output only moves once the hand has left the zone around
        // it, and then stays that far behind. The two ends are always reachable.
        const float zone = clampf (s.deadZone, 0.0f, 0.2f);
        if (smoothed - held > zone)        held = smoothed - zone;
        else if (held - smoothed > zone)   held = smoothed + zone;

        if (smoothed <= 0.005f)            held = 0.0f;
        else if (smoothed >= 0.995f)       held = 1.0f;
    }

    const int next = (int) std::lround (clampf (held, 0.0f, 1.0f) * 127.0f);
    const bool changed = ! haveValue || next != lastValue;
    haveValue = true;
    lastValue = next;
    value = next;
    return changed;
}

bool HeightTracker::lastRaw (float& out) const noexcept
{
    out = raw;
    return haveRaw;
}

void HeightTracker::reset() noexcept
{
    *this = HeightTracker();
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

} // namespace cam
