// ViolinMIDI - tests for the camera's logic (app/CameraLogic.h): the height of
// the violin hand as a controller value, and nodding / shaking the head.
// Synthetic poses only: a person facing the camera, sizes in shoulder widths.
//
// Included by test_main.cpp after test_stage3.h, inside its anonymous namespace;
// CameraLogic.h itself is included at the top of test_main.cpp.
#pragma once

//==============================================================================
// A synthetic person. Body coordinates in shoulder widths (x right, y down),
// the shoulders at (-0.5, 0) and (+0.5, 0), the nose 0.75 above them.
struct CamBody
{
    double cx = 0.5, cy = 0.55;   ///< where the middle of the shoulders is in the frame
    double width = 0.2;           ///< shoulder width as a share of the frame (0.1 = twice as far away)
    double turn = 0.0, tilt = 0.0; ///< nose against its rest position, shoulder widths (tilt > 0 = down)
    double rightShoulderUp = 0.0; ///< frame units: the bow arm's shoulder rising
    double handUp = 0.3;          ///< the violin hand above the shoulder line, shoulder widths
    bool   rightHand = false;
};

struct CamNoise
{
    float jitter = 0.005f;   ///< +- of the frame, every coordinate of every point
    float dip = 0.03f;       ///< chance per point and frame that its confidence drops to 0.2
};

inline cam::Point camPoint (const CamBody& b, double x, double y, std::mt19937* rng, const CamNoise& n)
{
    cam::Point p;
    p.x = (float) (b.cx + x * b.width);
    p.y = (float) (b.cy + y * b.width);
    p.confidence = 0.9f;
    if (rng != nullptr)
    {
        std::uniform_real_distribution<float> j (-n.jitter, n.jitter), u (0.0f, 1.0f);
        p.x += j (*rng);
        p.y += j (*rng);
        if (u (*rng) < n.dip) p.confidence = 0.2f;
    }
    return p;
}

inline cam::Pose camPose (double t, const CamBody& b, std::mt19937* rng = nullptr, const CamNoise& n = CamNoise())
{
    cam::Pose p;
    p.valid = true;
    p.timeSec = t;
    p.nose = camPoint (b, b.turn, -0.75 + b.tilt, rng, n);
    p.leftShoulder = camPoint (b, 0.5, 0.0, rng, n);
    p.rightShoulder = camPoint (b, -0.5, -b.rightShoulderUp / b.width, rng, n);
    p.neck = camPoint (b, 0.0, -0.1, rng, n);
    const double hand = -b.handUp;
    p.leftWrist = camPoint (b, b.rightHand ? 0.9 : 0.6, b.rightHand ? 0.6 : hand, rng, n);
    p.rightWrist = camPoint (b, b.rightHand ? -0.6 : -0.9, b.rightHand ? hand : 0.6, rng, n);
    p.leftElbow = camPoint (b, 0.8, 0.4, rng, n);
    p.rightElbow = camPoint (b, -0.8, 0.4, rng, n);
    return p;
}

/** Raised cosine 0 -> 1 -> 0 over [t0, t0 + d]. */
inline double camBump (double t, double t0, double d)
{
    if (t < t0 || t > t0 + d) return 0.0;
    return 0.5 * (1.0 - std::cos (2.0 * 3.14159265358979 * (t - t0) / d));
}

/** One full sine period 0 -> 1 -> -1 -> 0 over [t0, t0 + d]. */
inline double camSwing (double t, double t0, double d)
{
    if (t < t0 || t > t0 + d) return 0.0;
    return std::sin (2.0 * 3.14159265358979 * (t - t0) / d);
}

struct CamResult
{
    int nods = 0, shakes = 0;
    double firstAt = -1.0;
    std::string list;
};

/** Runs a detector over `seconds` of frames; `move` sets up the body at time t. */
inline CamResult camRun (const std::function<void (double, CamBody&)>& move, double seconds, double fps, float sensitivity,
                         unsigned seed, const CamNoise& noise = CamNoise(), float cooldownMs = 1200.0f, bool withNoise = true)
{
    cam::GestureDetector d;
    cam::GestureDetector::Settings s;
    s.sensitivity = sensitivity;
    s.cooldownMs = cooldownMs;
    std::mt19937 rng (seed);
    CamResult r;
    const int frames = (int) std::lround (seconds * fps);
    for (int i = 0; i < frames; ++i)
    {
        const double t = 10.0 + i / fps;   // any steady clock
        CamBody b;
        move (t - 10.0, b);
        const cam::Gesture g = d.update (camPose (t, b, withNoise ? &rng : nullptr, noise), s);
        if (g == cam::Gesture::None) continue;
        (g == cam::Gesture::Nod ? r.nods : r.shakes) += 1;
        if (r.firstAt < 0.0) r.firstAt = t - 10.0;
        char b2[48];
        std::snprintf (b2, sizeof (b2), "%s%s@%.2f", r.list.empty() ? "" : " ", g == cam::Gesture::Nod ? "nod" : "shake", t - 10.0);
        r.list += b2;
    }
    return r;
}

inline std::function<void (double, CamBody&)> camNod (double amount, double t0 = 1.0, double dur = 0.5)
{
    return [=] (double t, CamBody& b) { b.tilt = amount * camBump (t, t0, dur); };
}

inline std::function<void (double, CamBody&)> camShake (double amount, double t0 = 1.0, double dur = 0.7)
{
    return [=] (double t, CamBody& b) { b.turn = amount * camSwing (t, t0, dur); };
}

//==============================================================================
// 1. Height.
inline void cameraHeight()
{
    using HT = cam::HeightTracker;

    {   // measure
        CamBody b;
        b.handUp = 0.5;
        float raw = 0.0f;
        check (HT::measure (camPose (0.0, b), false, raw) && std::abs (raw - 0.5f) < 1e-4f, "measure: hand 0.5 above: found %.4f", (double) raw);
        b.handUp = -0.3;
        check (HT::measure (camPose (0.0, b), false, raw) && std::abs (raw + 0.3f) < 1e-4f, "measure: hand 0.3 below: found %.4f", (double) raw);
        b.width = 0.1;   // half the size: twice as far away, same posture
        b.cx = 0.3;
        check (HT::measure (camPose (0.0, b), false, raw) && std::abs (raw + 0.3f) < 1e-4f, "measure: twice as far away: found %.4f", (double) raw);

        // scale every point by 0.5 around a centre
        CamBody c;
        c.handUp = 0.4;
        cam::Pose p = camPose (0.0, c);
        float r1 = 0.0f, r2 = 0.0f;
        HT::measure (p, false, r1);
        for (cam::Point* q : { &p.nose, &p.leftShoulder, &p.rightShoulder, &p.leftWrist, &p.rightWrist })
        {
            q->x = 0.6f + 0.5f * (q->x - 0.6f);
            q->y = 0.4f + 0.5f * (q->y - 0.4f);
        }
        check (HT::measure (p, false, r2) && std::abs (r1 - r2) < 1e-4f, "measure: scaled by 0.5 around (0.6, 0.4): %.4f vs %.4f", (double) r1, (double) r2);

        // right hand
        CamBody rh;
        rh.rightHand = true;
        rh.handUp = 0.25;
        check (HT::measure (camPose (0.0, rh), true, raw) && std::abs (raw - 0.25f) < 1e-4f, "measure: right hand: found %.4f", (double) raw);
        check (HT::measure (camPose (0.0, rh), false, raw) && raw < -0.5f, "measure: the left hand of that pose is low: found %.4f", (double) raw);

        // missing points
        cam::Pose m = camPose (0.0, CamBody());
        m.leftWrist.confidence = 0.29f;
        check (! HT::measure (m, false, raw), "measure: wrist confidence 0.29 must give false");
        m = camPose (0.0, CamBody());
        m.rightShoulder.confidence = 0.0f;
        check (! HT::measure (m, false, raw), "measure: a shoulder missing must give false");
        m = camPose (0.0, CamBody());
        m.valid = false;
        check (! HT::measure (m, false, raw), "measure: no person must give false");
        m = camPose (0.0, CamBody());
        m.rightShoulder = m.leftShoulder;
        check (! HT::measure (m, false, raw), "measure: both shoulders on one spot (seen edge-on) must give false");
    }

    HT::Settings flat;   // no smoothing, no dead zone
    flat.smoothingMs = 0.0f;
    flat.deadZone = 0.0f;
    flat.low = -0.2f;
    flat.high = 0.6f;

    {   // calibration, clamping, reversed
        struct Case { float hand; int expected; };
        const Case cases[] = { { -0.2f, 0 }, { 0.6f, 127 }, { 0.2f, 64 }, { -1.0f, 0 }, { 2.0f, 127 }, { 0.0f, 32 }, { 0.4f, 95 } };
        for (const Case& c : cases)
        {
            HT h;
            CamBody b;
            b.handUp = c.hand;
            int value = -1;
            const bool changed = h.update (camPose (1.0, b), flat, value);
            check (changed && value == c.expected, "calibration -0.2..0.6: hand %.2f: expected %d (changed), found %d (%s)", (double) c.hand, c.expected, value,
                   changed ? "changed" : "unchanged");
        }
        HT::Settings rev = flat;
        rev.low = 0.6f;
        rev.high = -0.2f;
        HT h;
        CamBody b;
        int value = -1;
        b.handUp = 0.6;
        h.update (camPose (1.0, b), rev, value);
        check (value == 0, "reversed calibration (high < low): the 'low' point 0.6 must give 0, found %d", value);
        b.handUp = -0.2;
        h.update (camPose (1.1, b), rev, value);
        check (value == 127, "reversed calibration: the 'high' point -0.2 must give 127, found %d", value);

        HT::Settings narrow = flat;
        narrow.high = narrow.low + 0.01f;
        HT h2;
        value = 55;
        check (! h2.update (camPose (1.0, b), narrow, value) && value == 55, "calibration narrower than 0.05: no value, found %d", value);
        float raw = 0.0f;
        check (h2.lastRaw (raw) && std::abs (raw + 0.2f) < 1e-4f, "lastRaw must work without a calibration (for the calibration buttons), found %.3f", (double) raw);
    }

    {   // smoothing: a step reaches 63 % after about smoothingMs
        for (float ms : { 150.0f, 400.0f, 1000.0f })
        {
            HT h;
            HT::Settings s = flat;
            s.smoothingMs = ms;
            CamBody b;
            b.handUp = -0.2;
            int value = -1;
            const double fps = 15.0;
            h.update (camPose (0.0, b), s, value);
            b.handUp = 0.6;
            double reached = -1.0;
            for (int i = 1; i < 200 && reached < 0.0; ++i)
            {
                h.update (camPose (i / fps, b), s, value);
                if (value >= (int) std::ceil (0.632 * 127.0)) reached = i / fps;
            }
            check (reached > 0.0 && std::abs (reached - ms * 0.001) <= 1.0 / fps + 1e-6, "smoothing %.0f ms: 63 %% reached after %.3f s, expected %.3f s +- one frame",
                   (double) ms, reached, ms * 0.001);
        }
        HT h;
        CamBody b;
        int value = -1;
        b.handUp = -0.2;
        h.update (camPose (0.0, b), flat, value);
        b.handUp = 0.6;
        h.update (camPose (1.0 / 15.0, b), flat, value);
        check (value == 127, "smoothing 0: the step at once, found %d", value);
    }

    {   // dead zone: jitter inside it changes nothing, a sweep reaches both ends, the output lags by the zone
        HT::Settings s = flat;
        s.deadZone = 0.05f;
        HT h;
        CamBody b;
        int value = -1;
        std::mt19937 rng (5);
        std::uniform_real_distribution<double> j (-0.019, 0.019);   // positions within +-0.024 of 0.5 (0.8 calibrated range)
        b.handUp = 0.2;
        h.update (camPose (0.0, b), s, value);
        const int start = value;
        int changes = 0;
        for (int i = 1; i < 300; ++i)
        {
            b.handUp = 0.2 + j (rng);
            changes += h.update (camPose (i / 15.0, b), s, value) ? 1 : 0;
        }
        check (changes == 0 && value == start, "dead zone 0.05: jitter of +-0.024 must change nothing, %d changes", changes);

        // slow sweep up and down
        HT h2;
        int lo = 999, hi = -1, maxLag = 0;
        double t = 0.0;
        for (int dir = 0; dir < 2; ++dir)
            for (int i = 0; i <= 200; ++i)
            {
                const double pos = dir == 0 ? -0.05 + 1.1 * i / 200.0 : 1.05 - 1.1 * i / 200.0;   // calibrated position
                b.handUp = -0.2 + 0.8 * pos;
                h2.update (camPose (t, b), s, value);
                t += 1.0 / 15.0;
                lo = std::min (lo, value);
                hi = std::max (hi, value);
                const double p = std::min (1.0, std::max (0.0, pos));
                if (p > 0.1 && p < 0.9)
                {
                    const int expected = (int) std::lround ((dir == 0 ? p - 0.05 : p + 0.05) * 127.0);
                    maxLag = std::max (maxLag, std::abs (value - expected));
                }
            }
        check (lo == 0 && hi == 127, "slow sweep: both ends exactly, found %d..%d", lo, hi);
        check (maxLag <= 1, "slow sweep: the output lags by the zone (0.05 = 6.35 steps), largest difference from that %d", maxLag);
    }

    {   // a pause longer than 0.5 s starts afresh; shorter ones are smoothed over
        HT::Settings s = flat;
        s.smoothingMs = 300.0f;
        for (double gap : { 0.4, 0.6 })
        {
            HT h;
            CamBody b;
            int value = -1;
            b.handUp = -0.2;
            h.update (camPose (0.0, b), s, value);
            h.update (camPose (0.0667, b), s, value);
            b.handUp = 0.6;
            h.update (camPose (0.0667 + gap, b), s, value);
            if (gap > 0.5)
                check (value == 127, "pause of %.1f s: starts afresh at the new place, found %d", gap, value);
            else
                check (value > 0 && value < 127, "pause of %.1f s: smoothed, found %d", gap, value);
        }
        // the hand invisible for a while counts as a pause too
        HT h;
        CamBody b;
        int value = -1;
        b.handUp = -0.2;
        h.update (camPose (0.0, b), s, value);
        cam::Pose hidden = camPose (0.1, b);
        hidden.leftWrist.confidence = 0.0f;
        for (int i = 0; i < 15; ++i) { hidden.timeSec = 0.1 + i / 15.0; check (! h.update (hidden, s, value), "hand hidden: no change expected"); }
        b.handUp = 0.6;
        h.update (camPose (1.2, b), s, value);
        check (value == 127, "hand back after 1.1 s: starts afresh, found %d", value);
    }

    {   // update returns true exactly when the value changed; odd timestamps do no harm
        HT::Settings s;   // the defaults
        HT h;
        std::mt19937 rng (11);
        std::uniform_real_distribution<double> u (-0.5, 1.0);
        int value = -1, previous = -1;
        double t = 0.0;
        int changes = 0, falseClaims = 0;
        for (int i = 0; i < 3000; ++i)
        {
            CamBody b;
            b.handUp = (i / 100) % 2 == 0 ? u (rng) * 0.1 + 0.2 : u (rng);
            const int r = (int) (rng() % 20);
            if (r == 0) t -= 0.2;               // time going backwards
            else if (r == 1) { }                // the same timestamp again
            else t += 1.0 / 15.0;
            int v = value;
            const bool changed = h.update (camPose (t, b, &rng), s, v);
            if (changed != (v != previous || previous < 0))
                ++falseClaims;
            check (v >= 0 && v <= 127, "value out of range: %d", v);
            changes += changed ? 1 : 0;
            if (changed) previous = v;
            value = v;
        }
        check (falseClaims == 0, "update must return true exactly when the value changed: %d wrong answers", falseClaims);
        info ("camera height: %d changes in 3000 frames of jittery input with odd timestamps", changes);

        // identical timestamps: the value does not move (no time has passed), and nothing breaks
        HT h2;
        CamBody b;
        b.handUp = -0.2;
        h2.update (camPose (5.0, b), s, value);
        const int before = value;
        b.handUp = 0.6;
        for (int i = 0; i < 10; ++i) h2.update (camPose (5.0, b), s, value);
        check (value == before, "identical timestamps with smoothing: no movement expected, found %d -> %d", before, value);
    }
}

//==============================================================================
// 2. Gestures.
inline void cameraGestures()
{
    const unsigned kSeeds = 20;

    // --- a clear nod and a clear shake at the default sensitivity, on top of noise
    for (double fps : { 10.0, 15.0, 30.0 })
    {
        for (double far : { 1.0, 0.5 })
        {
            int nodOk = 0, shakeOk = 0;
            std::string bad;
            for (unsigned seed = 1; seed <= kSeeds; ++seed)
            {
                auto nod = [far, f = camNod (0.3)] (double t, CamBody& b) { f (t, b); b.width = 0.2 * far; };
                auto shake = [far, f = camShake (0.3)] (double t, CamBody& b) { f (t, b); b.width = 0.2 * far; };
                CamNoise noise;
                noise.jitter *= (float) far;   // the points' jitter in proportion to the person (see the report for frame-sized jitter)
                const CamResult n = camRun (nod, 3.0, fps, 0.5f, seed, noise);
                const CamResult s = camRun (shake, 3.0, fps, 0.5f, seed + 100, noise);
                nodOk += n.nods == 1 && n.shakes == 0 ? 1 : 0;
                shakeOk += s.shakes == 1 && s.nods == 0 ? 1 : 0;
                if (! (n.nods == 1 && n.shakes == 0) && bad.size() < 200) bad += " nod seed " + std::to_string (seed) + ": " + (n.list.empty() ? "none" : n.list) + ";";
                if (! (s.shakes == 1 && s.nods == 0) && bad.size() < 200) bad += " shake seed " + std::to_string (seed) + ": " + (s.list.empty() ? "none" : s.list) + ";";
            }
            check (nodOk == (int) kSeeds && shakeOk == (int) kSeeds,
                   "%.0f fps, %s: nod 0.3 / shake 0.3 shoulder widths must give exactly one gesture each: nods %d/%u, shakes %d/%u:%s",
                   fps, far == 1.0 ? "near" : "twice as far", nodOk, kSeeds, shakeOk, kSeeds, bad.c_str());
        }
    }

    // --- the brief's "clear" nod (0.2) and shake: report how reliable at default sensitivity
    {
        int n2 = 0, s2 = 0;
        for (unsigned seed = 1; seed <= kSeeds; ++seed)
        {
            const CamResult n = camRun (camNod (0.2), 3.0, 15.0, 0.5f, seed);
            const CamResult s = camRun (camShake (0.2), 3.0, 15.0, 0.5f, seed);
            n2 += n.nods == 1 && n.shakes == 0 ? 1 : 0;
            s2 += s.shakes == 1 && s.nods == 0 ? 1 : 0;
        }
        info ("default sensitivity, 15 fps, noise: nod 0.2 recognised %d/%u, shake 0.2 recognised %d/%u", n2, kSeeds, s2, kSeeds);
        check (n2 >= (int) kSeeds - 1, "a clear nod of 0.2 shoulder widths in 0.5 s must be recognised at default sensitivity, %d/%u", n2, kSeeds);
    }

    // --- cooldown
    {
        auto twoNods = [] (double gap) {
            return [gap] (double t, CamBody& b) { b.tilt = 0.3 * (camBump (t, 1.0, 0.5) + camBump (t, 1.0 + gap, 0.5)); };
        };
        const CamResult inside = camRun (twoNods (0.9), 5.0, 15.0, 0.5f, 3);
        check (inside.nods == 1, "second nod inside the cooldown (1.2 s): expected one nod, found %s", inside.list.c_str());
        const CamResult after = camRun (twoNods (2.0), 5.0, 15.0, 0.5f, 3);
        check (after.nods == 2, "second nod after the cooldown: expected two nods, found %s", after.list.c_str());
        // a fast double nod is one gesture, not one now and a late one when the cooldown ends
        int late = 0;
        std::string example;
        for (unsigned seed = 1; seed <= kSeeds; ++seed)
        {
            const CamResult dbl = camRun (twoNods (0.55), 5.0, 15.0, 0.5f, seed);
            if (dbl.nods + dbl.shakes != 1) { ++late; if (example.empty()) example = dbl.list; }
        }
        check (late == 0, "fast double nod (2 x 0.5 s): exactly one gesture expected, %d/%u runs gave more (e.g. %s) - a movement made during the cooldown fires when it ends",
               late, kSeeds, example.c_str());
    }

    // --- things that must not trigger at the default sensitivity
    struct Move { const char* what; std::function<void (double, CamBody&)> f; };
    const Move quiet[] = {
        { "standing still", [] (double, CamBody&) {} },
        { "slow drift down and up (4 s)", [] (double t, CamBody& b) { b.tilt = 0.3 * camBump (t, 1.0, 4.0); } },
        { "slow turn and back (3 s)", [] (double t, CamBody& b) { b.turn = 0.3 * camBump (t, 1.0, 3.0); } },
        { "looking down and staying", [] (double t, CamBody& b) { b.tilt = 0.35 * std::min (1.0, std::max (0.0, (t - 1.0) / 0.3)); } },
        { "one turn to the side that stays", [] (double t, CamBody& b) { b.turn = 0.35 * std::min (1.0, std::max (0.0, (t - 1.0) / 0.3)); } },
        { "whole body swaying sideways", [] (double t, CamBody& b) { b.cx = 0.5 + 0.08 * std::sin (2.0 * 3.14159 * t / 1.2); } },
        { "whole body bending (down, nearer)", [] (double t, CamBody& b) { const double k = camBump (t, 1.0, 0.8); b.cy = 0.55 + 0.1 * k; b.width = 0.2 * (1.0 + 0.3 * k); } },
        { "camera shake: all points jump", [] (double t, CamBody& b) { const int f = (int) (t * 15.0); b.cx = 0.5 + ((f % 3) - 1) * 0.03; b.cy = 0.55 + ((f % 2) * 2 - 1) * 0.02; } },
        { "diagonal down-right and back", [] (double t, CamBody& b) { const double k = camBump (t, 1.0, 0.5); b.tilt = 0.3 * k; b.turn = 0.3 * k; } },
    };
    for (const Move& m : quiet)
    {
        int count = 0;
        std::string example;
        for (unsigned seed = 1; seed <= kSeeds; ++seed)
        {
            const CamResult r = camRun (m.f, 8.0, 15.0, 0.5f, seed);
            count += r.nods + r.shakes;
            if (! r.list.empty() && example.empty()) example = r.list;
        }
        check (count == 0, "%s: no gesture expected at default sensitivity, found %d in %u runs (e.g. %s)", m.what, count, kSeeds, example.c_str());
    }

    // --- sensitivity: smallest movement that is recognised reliably (19 of 20 runs) and at all, with noise
    double reliable[2][3] = {}, any[2][3] = {};
    const float sens[3] = { 0.0f, 0.5f, 1.0f };
    for (int kind = 0; kind < 2; ++kind)
        for (int si = 0; si < 3; ++si)
        {
            reliable[kind][si] = any[kind][si] = -1.0;
            for (int step = 2; step <= 80 && reliable[kind][si] < 0.0; ++step)
            {
                const double a = step * 0.01;
                int ok = 0;
                for (unsigned seed = 1; seed <= kSeeds; ++seed)
                {
                    const CamResult r = camRun (kind == 0 ? camNod (a) : camShake (a), 3.0, 15.0, sens[si], seed);
                    ok += (kind == 0 ? r.nods : r.shakes) >= 1 ? 1 : 0;
                }
                if (ok > 0 && any[kind][si] < 0.0) any[kind][si] = a;
                if (ok >= (int) kSeeds - 1) reliable[kind][si] = a;
            }
            info ("sensitivity %.1f: %s recognised from %.2f shoulder widths at all, reliably from %.2f", (double) sens[si], kind == 0 ? "nod  " : "shake",
                  any[kind][si], reliable[kind][si]);
        }
    for (int kind = 0; kind < 2; ++kind)
        check (reliable[kind][0] > 1.8 * reliable[kind][2] && reliable[kind][0] > reliable[kind][1] && reliable[kind][1] > reliable[kind][2],
               "%s: sensitivity 0 must need a clearly bigger movement than 1: %.2f / %.2f / %.2f", kind == 0 ? "nod" : "shake",
               reliable[kind][0], reliable[kind][1], reliable[kind][2]);

    // --- fragile spots and plausible false triggers: reported, not judged
    struct Probe { const char* what; std::function<void (double, CamBody&)> f; int expectNods, expectShakes; };
    const Probe probes[] = {
        { "nod 0.3 with a little turn (30 %)", [] (double t, CamBody& b) { const double k = camBump (t, 1.0, 0.5); b.tilt = 0.3 * k; b.turn = 0.09 * k; }, 1, 0 },
        { "nod 0.3, head rolled 20 deg on the chin rest", [] (double t, CamBody& b) { const double k = camBump (t, 1.0, 0.5); b.tilt = 0.3 * 0.94 * k; b.turn = -0.25 + 0.3 * 0.34 * k; }, 1, 0 },
        { "nod 0.3, head rolled 40 deg", [] (double t, CamBody& b) { const double k = camBump (t, 1.0, 0.5); b.tilt = 0.3 * 0.77 * k; b.turn = 0.3 * 0.64 * k; }, 1, 0 },
        { "slow nod 0.3 in 1.2 s", [] (double t, CamBody& b) { b.tilt = 0.3 * camBump (t, 1.0, 1.2); }, 1, 0 },
        { "shake to one side only and back, 0.3 in 0.5 s", [] (double t, CamBody& b) { b.turn = 0.3 * camBump (t, 1.0, 0.5); }, 0, 0 },
        { "head lifted up and back 0.3 (upward nod)", [] (double t, CamBody& b) { b.tilt = -0.3 * camBump (t, 1.0, 0.5); }, 0, 0 },
        { "bowing: bow shoulder up and down 0.02 frame at 1.5 Hz", [] (double t, CamBody& b) { b.rightShoulderUp = 0.02 * (0.5 + 0.5 * std::sin (2.0 * 3.14159 * 1.5 * t)); }, 0, 0 },
        { "bowing: bow shoulder up and down 0.04 frame at 1 Hz", [] (double t, CamBody& b) { b.rightShoulderUp = 0.04 * (0.5 + 0.5 * std::sin (2.0 * 3.14159 * t)); }, 0, 0 },
        { "phrasing: head dips 0.15 with the music every 2 s", [] (double t, CamBody& b) { b.tilt = 0.15 * camBump (std::fmod (t, 2.0), 0.5, 0.8); }, 0, 0 },
        { "phrasing: head dips 0.2 with the music every 2 s", [] (double t, CamBody& b) { b.tilt = 0.2 * camBump (std::fmod (t, 2.0), 0.5, 0.8); }, 0, 0 },
        { "looking at the music stand and back (0.25 turn, 1 s)", [] (double t, CamBody& b) { b.turn = 0.25 * camBump (std::fmod (t, 4.0), 1.0, 1.0); }, 0, 0 },
    };
    for (const Probe& p : probes)
        for (float s : { 0.5f, 1.0f })
        {
            int nods = 0, shakes = 0;
            for (unsigned seed = 1; seed <= kSeeds; ++seed)
            {
                const CamResult r = camRun (p.f, 8.0, 15.0, s, seed);
                nods += r.nods;
                shakes += r.shakes;
            }
            info ("probe, sensitivity %.1f: %-55s nods %3d, shakes %3d in %u runs of 8 s (wanted %d / %d per run)", (double) s, p.what, nods, shakes, kSeeds,
                  p.expectNods, p.expectShakes);
        }

    // --- distance without noise: exactly the same result
    for (double fps : { 10.0, 15.0, 30.0 })
    {
        auto nearNod = camNod (0.25);
        auto farNod = [f = camNod (0.25)] (double t, CamBody& b) { f (t, b); b.width = 0.1; b.cx = 0.3; };
        const CamResult a = camRun (nearNod, 3.0, fps, 0.5f, 1, CamNoise(), 1200.0f, false);
        const CamResult b = camRun (farNod, 3.0, fps, 0.5f, 1, CamNoise(), 1200.0f, false);
        check (a.list == b.list && a.nods == 1, "%.0f fps, no noise: near and twice as far must give the same, found \"%s\" and \"%s\"", fps, a.list.c_str(), b.list.c_str());
    }

    // --- a person far away with jitter of 0.5 % of the frame (twice as much relative to the person): reported
    for (double fps : { 15.0, 30.0 })
    {
        const CamResult r = camRun ([] (double, CamBody& b) { b.width = 0.1; }, 600.0, fps, 0.5f, 7);
        info ("noise only, person twice as far (shoulders 10 %% of the frame), %.0f fps, sensitivity 0.5: %d nods, %d shakes in 10 minutes", fps, r.nods, r.shakes);
    }

    // --- noise only, long run: false triggers per hour at each sensitivity
    for (float s : { 0.0f, 0.5f, 1.0f })
    {
        const CamResult r = camRun ([] (double, CamBody&) {}, 3600.0, 15.0, s, 99);
        info ("noise only (0.5 %% jitter, 3 %% confidence dips), sensitivity %.1f: %d nods, %d shakes in one hour", (double) s, r.nods, r.shakes);
        if (s <= 0.5f)
            check (r.nods + r.shakes == 0, "noise alone must not trigger at sensitivity %.1f: %d nods, %d shakes per hour", (double) s, r.nods, r.shakes);
    }
}
