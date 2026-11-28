// ViolinMIDI - tests for the camera's logic (app/CameraLogic.h): the violin hand
// as a two-axis pad (since 2.0), and nodding / shaking the head.
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
    double handSide = 0.0;        ///< the violin hand moved to the right (in the frame), shoulder widths
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
    p.leftWrist = camPoint (b, b.rightHand ? 0.9 : 0.6 + b.handSide, b.rightHand ? 0.6 : hand, rng, n);
    p.rightWrist = camPoint (b, b.rightHand ? -0.6 + b.handSide : -0.9, b.rightHand ? hand : 0.6, rng, n);
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
// 1. The pad: the violin hand moving two values, relative to where it rests.
inline void cameraPad()
{
    using PT = cam::PadTracker;
    PT::Settings s;   // dead zone 0.06 of the picture, speed 0.8, linear

    {   // measure: the hand against the middle of the shoulders, in the picture
        CamBody b;
        b.handUp = 0.5;
        float dx = 0.0f, dy = 0.0f;
        check (PT::measure (camPose (0.0, b), false, dx, dy) && std::abs (dx - 0.12f) < 1e-4f && std::abs (dy + 0.10f) < 1e-4f,
               "measure: expected (0.12, -0.10), found (%.4f, %.4f)", (double) dx, (double) dy);
        cam::Pose hidden = camPose (0.0, b);
        hidden.leftWrist.confidence = 0.1f;
        check (! PT::measure (hidden, false, dx, dy), "measure: a hand that is not seen gives nothing");
        b.rightHand = true;
        check (PT::measure (camPose (0.0, b), true, dx, dy) && std::abs (dx + 0.12f) < 1e-4f, "measure: right hand, found dx %.4f", (double) dx);
    }

    {   // response curves
        for (int r = 0; r < PT::numResponses; ++r)
        {
            check (PT::shape (0.0f, r) == 0.0f && std::abs (PT::shape (1.0f, r) - 1.0f) < 1e-5f && std::abs (PT::shape (-1.0f, r) + 1.0f) < 1e-5f,
                   "response %d: 0 -> 0, +-1 -> +-1", r);
            float last = -2.0f;
            for (float d = -1.0f; d <= 1.0001f; d += 0.05f)
            {
                const float v = PT::shape (d, r);
                check (v >= last - 1e-6f, "response %d not monotonic at %.2f", r, (double) d);
                last = v;
            }
            check (std::abs (PT::shape (3.0f, r) - 1.0f) < 1e-5f, "response %d: deflection beyond 1 is 1", r);
        }
        check (std::abs (PT::shape (0.5f, PT::fine) - 0.25f) < 1e-5f, "fine at 0.5: %.4f", (double) PT::shape (0.5f, PT::fine));
        check (PT::shape (0.2f, PT::fine) < PT::shape (0.2f, PT::linear) && PT::shape (0.2f, PT::linear) < PT::shape (0.2f, PT::fast),
               "near the zone: fine < linear < fast");
    }

    const auto feed = [&s] (PT& pad, double t0, double seconds, const CamBody& b, double fps = 15.0)
    {
        bool changed = false;
        for (int i = 0; i < (int) std::lround (seconds * fps); ++i)
            changed = pad.update (camPose (t0 + i / fps, b), s) || changed;
        return changed;
    };

    {   // zero point, dead zone, movement in all four directions
        PT pad;
        CamBody rest;
        check (! pad.hasZero() && pad.valueX() == 64 && pad.valueY() == 64, "start: no zero point, both values in the middle");
        check (! pad.update (camPose (0.0, rest), s) && pad.hasZero(), "the first frame sets the zero point and changes nothing");

        CamBody inside = rest;
        inside.handSide = 0.2;            // 0.04 of the picture: inside the 0.06 zone
        inside.handUp = 0.3 + 0.15;       // 0.03 up: still inside (0.05 away)
        check (! feed (pad, 0.1, 2.0, inside) && ! pad.active() && pad.valueX() == 64 && pad.valueY() == 64,
               "inside the dead zone nothing moves: %d %d", pad.valueX(), pad.valueY());

        CamBody right = rest;
        right.handSide = 1.3;             // 0.26 to the right: 0.2 beyond the zone, full deflection
        // four frames from 2.2 s: 0.37 s since the last frame at rest (2.03 s)
        check (feed (pad, 2.2, 0.25, right) && pad.active(), "out to the right: the values move");
        check (pad.x() > 0.78f && pad.x() < 0.81f && pad.valueY() == 64,
               "right for 0.37 s at full deflection (speed 0.8): x 0.5 -> 0.79, y stays; found x %.3f y %d", (double) pad.x(), pad.valueY());

        const int heldX = pad.valueX();
        check (! feed (pad, 2.8, 1.0, rest) && ! pad.active() && pad.valueX() == heldX, "back at rest: the value stays where it got to");

        CamBody up = rest;
        up.handUp = 0.3 + 0.8;            // 0.16 up: 0.1 beyond the zone, half deflection
        feed (pad, 4.0, 1.0, up);
        check (pad.y() > 0.80f && pad.y() < 0.99f && pad.valueX() == heldX, "up: y grows (found %.3f), x stays", (double) pad.y());

        CamBody leftDown = rest;
        leftDown.handSide = -2.0;
        leftDown.handUp = 0.3 - 2.0;
        feed (pad, 5.2, 3.0, leftDown);
        check (pad.valueX() == 0 && pad.valueY() == 0, "left and down for long: both at 0, found %d %d", pad.valueX(), pad.valueY());
    }

    {   // the player sways: shoulders and hand move together, nothing changes
        PT pad;
        CamBody b;
        pad.update (camPose (0.0, b), s);
        bool changed = false;
        for (int i = 1; i < 60; ++i)
        {
            b.cx = 0.5 + 0.15 * std::sin (i * 0.2);
            b.cy = 0.55 + 0.05 * std::cos (i * 0.3);
            changed = pad.update (camPose (i / 15.0, b), s) || changed;
        }
        check (! changed && ! pad.active(), "swaying with the violin at rest moves nothing");
    }

    {   // jitter inside the zone moves nothing; update() says true exactly when a value changed
        PT pad;
        std::mt19937 rng (7);
        CamBody b;
        pad.update (camPose (0.0, b), s);
        CamNoise n;
        n.jitter = 0.008f;
        n.dip = 0.0f;
        bool changed = false;
        for (int i = 1; i < 300; ++i)
            changed = pad.update (camPose (i / 15.0, b, &rng), s) || changed;
        check (! changed, "jitter of +-0.008 inside the zone changed a value");

        PT pad2;
        pad2.update (camPose (0.0, b), s);
        int falseClaims = 0, lastX = pad2.valueX(), lastY = pad2.valueY();
        std::uniform_real_distribution<double> u (-3.0, 3.0);
        for (int i = 1; i < 2000; ++i)
        {
            CamBody m;
            m.handSide = u (rng);
            m.handUp = u (rng);
            const bool claimed = pad2.update (camPose (i / 15.0 + (i % 7 == 0 ? 0.6 : 0.0), m, &rng), s);
            const bool really = pad2.valueX() != lastX || pad2.valueY() != lastY;
            falseClaims += claimed != really ? 1 : 0;
            lastX = pad2.valueX();
            lastY = pad2.valueY();
            check (lastX >= 0 && lastX <= 127 && lastY >= 0 && lastY <= 127, "value out of 0..127");
        }
        check (falseClaims == 0, "update must return true exactly when a value changed: %d wrong answers", falseClaims);
    }

    {   // pauses, lost hand, time going back, recalibration
        PT pad;
        CamBody rest, right;
        right.handSide = 1.3;
        pad.update (camPose (0.0, rest), s);
        pad.update (camPose (0.1, right), s);
        const float x0 = pad.x();
        pad.update (camPose (3.0, right), s);   // 2.9 s later: a pause, not 2.9 s of movement
        check (std::abs (pad.x() - x0) < 1e-6f, "a long pause between frames moves nothing (%.3f -> %.3f)", (double) x0, (double) pad.x());
        pad.update (camPose (2.0, right), s);   // back in time
        check (std::abs (pad.x() - x0) < 1e-6f, "a step back in time moves nothing");

        cam::Pose lost = camPose (3.1, right);
        lost.leftWrist.confidence = 0.0f;
        check (! pad.update (lost, s) && ! pad.active(), "hand lost: nothing changes, not active");

        pad.recalibrate();
        check (! pad.update (camPose (3.2, right), s) && pad.hasZero(), "recalibrate: the next frame is the new zero point");
        const int x1 = pad.valueX();
        check (! feed (pad, 3.3, 1.0, right) && pad.valueX() == x1, "after recalibrating where the hand is, it is at rest");
        float zx = 0.0f, zy = 0.0f, hx = 0.0f, hy = 0.0f;
        check (pad.zeroPoint (zx, zy) && pad.handPoint (hx, hy) && std::abs (zx - hx) < 1e-4f && std::abs (zy - hy) < 1e-4f,
               "zero point drawn where the hand rests");

        pad.reset();
        check (! pad.hasZero() && pad.valueX() == 64 && pad.valueY() == 64, "reset: middle and no zero point");
    }

    {   // right hand, and the dead zone size is honoured
        PT pad;
        PT::Settings r = s;
        r.rightHand = true;
        r.deadZone = 0.15f;
        CamBody b;
        b.rightHand = true;
        pad.update (camPose (0.0, b), r);
        CamBody moved = b;
        moved.handSide = 0.6;             // 0.12 of the picture: inside a 0.15 zone
        bool changed = false;
        for (int i = 1; i < 30; ++i)
            changed = pad.update (camPose (i / 15.0, moved), r) || changed;
        check (! changed, "right hand, zone 0.15: 0.12 away moves nothing");
        moved.handSide = 2.0;
        for (int i = 30; i < 45; ++i)
            changed = pad.update (camPose (i / 15.0, moved), r) || changed;
        check (changed && pad.x() > 0.5f, "right hand: moving right raises x (%.3f)", (double) pad.x());
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
