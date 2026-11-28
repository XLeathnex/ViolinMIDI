// ViolinMIDI - automated tests for the pitch-to-MIDI core.
//
// Every test renders a violin-like signal with known pitch, runs it through
// the engine exactly as the app does, and checks the MIDI that comes out.

#include "Harness.h"
#include "../core/OutputStage.h"
#include "../core/Scale.h"
#include "CameraLogic.h"

#include <vector>
#include <string>
#include <sstream>
#include <random>
#include <algorithm>
#include <chrono>
#include <cstdarg>
#include <cstring>
#include <functional>
#include <limits>

using namespace vmtest;

namespace
{

int  gFailuresInTest = 0;
bool gVerbose = false;

void check (bool condition, const char* format, ...)
{
    if (condition)
        return;
    ++gFailuresInTest;
    char buffer[1024];
    va_list args;
    va_start (args, format);
    std::vsnprintf (buffer, sizeof (buffer), format, args);
    va_end (args);
    std::printf ("    FAIL: %s\n", buffer);
}

void info (const char* format, ...)
{
    char buffer[1024];
    va_list args;
    va_start (args, format);
    std::vsnprintf (buffer, sizeof (buffer), format, args);
    va_end (args);
    std::printf ("    %s\n", buffer);
}

void checkProtocol (const RunResult& r, const char* what)
{
    for (const std::string& p : r.problems)
        check (false, "%s: %s", what, p.c_str());
    check (r.stuckNotes == 0, "%s: %d note(s) never released", what, r.stuckNotes);
}

std::vector<int> noteNumbers (const RunResult& r)
{
    std::vector<int> v;
    for (const Note& n : r.notes)
        v.push_back (n.note);
    return v;
}

std::string toString (const std::vector<int>& v)
{
    std::string s;
    for (int x : v)
        s += std::to_string (x) + " ";
    return s;
}

vm::Settings baseSettings()
{
    vm::Settings s;
    s.bendMode = vm::BendMode::Continuous;
    s.dynamics = vm::DynamicsMode::Continuous;
    s.response = vm::Response::Normal;
    s.bendRangeSemis = 2;
    return s;
}

double periodMs (double note) { return 1000.0 / noteToHz (note); }

#include "test_stage.h"
#include "test_stage2.h"
#include "test_stage3.h"
#include "test_camera.h"
#include "test_tracker2.h"
#include "test_tracker3.h"
#include "test_harmonizer.h"
#include "test_v2.h"

//==============================================================================
// Single bowed notes over the whole range of a 7-string violin.
void steadyNotes (double fs)
{
    std::mt19937 rng (1234);
    std::uniform_real_distribution<double> detuneDist (-0.35, 0.35);
    double worstCents = 0.0, worstLatencyPeriods = 0.0;
    int wrong = 0;

    for (int note = 34; note <= 105; ++note)
    {
        const double detune = detuneDist (rng);
        const double pitch = note + detune;
        Synth synth (fs, (unsigned) (note * 7 + 1));
        const double lead = 0.15, length = 0.5, attack = 0.02;
        const Signal sig = synth.render ({ rest (lead), tone (pitch, length, -20.0, attack), rest (0.3) }, Timbre());
        const RunResult r = run (sig, baseSettings());
        checkProtocol (r, "steady");

        if (r.notes.size() != 1 || r.notes[0].note != note)
        {
            ++wrong;
            check (false, "note %d (%.1f Hz): expected one note, got: %s", note, noteToHz (pitch), describeNotes (r).c_str());
            continue;
        }

        const Note& n = r.notes[0];
        const PitchError e = pitchError (r, sig, 2, n.on + 0.06, lead + length - 0.02);
        worstCents = std::max (worstCents, e.maxCents);
        check (e.maxCents < 6.0, "note %d: pitch off by up to %.1f cents", note, e.maxCents);

        const double latencyMs = (n.on - lead) * 1000.0;
        const double allowedMs = attack * 1000.0 + 2.3 * periodMs (pitch) + 12.0;
        worstLatencyPeriods = std::max (worstLatencyPeriods, (latencyMs - attack * 1000.0) / periodMs (pitch));
        check (latencyMs <= allowedMs, "note %d: note-on after %.1f ms (allowed %.1f)", note, latencyMs, allowedMs);
        check (latencyMs > 0.0, "note %d: note-on before the note started", note);

        const double release = (n.off - (lead + length)) * 1000.0;
        check (release > -8.0 && release < 60.0, "note %d: note-off %.1f ms after the bow stopped", note, release);

        if (gVerbose || note == 34 || note == 41 || note == 48 || note == 55 || note == 62 || note == 69 || note == 76 || note == 88 || note == 100)
            info ("note %3d %7.1f Hz: note-on after %5.1f ms (incl. %.0f ms bow attack), pitch error max %.1f cents",
                  note, noteToHz (pitch), latencyMs, attack * 1000.0, e.maxCents);
    }

    info ("worst pitch error %.1f cents, wrong notes %d", worstCents, wrong);
}

void steadyNotes48() { steadyNotes (48000.0); }
void steadyNotes44() { steadyNotes (44100.0); }

//==============================================================================
// The three response settings trade speed for certainty.
void responseSettings()
{
    const vm::Response responses[] = { vm::Response::Fast, vm::Response::Normal, vm::Response::Safe };
    const char* names[] = { "fast", "normal", "safe" };

    for (int note : { 34, 41, 48, 55, 62, 69, 76, 88 })
    {
        Synth synth (48000.0, (unsigned) (note + 300));
        const double lead = 0.15;
        // a firm attack: the tone is there within 5 ms, so the numbers show the detector alone
        const Signal sig = synth.render ({ rest (lead), tone (note, 0.6, -20.0, 0.005), rest (0.3) }, Timbre());

        double latency[3] = { 0, 0, 0 };
        for (int i = 0; i < 3; ++i)
        {
            vm::Settings s = baseSettings();
            s.response = responses[i];
            const RunResult r = run (sig, s);
            checkProtocol (r, names[i]);
            const bool good = r.notes.size() == 1 && r.notes[0].note == note;
            check (good, "%s, note %d: got %s", names[i], note, describeNotes (r).c_str());
            if (good)
                latency[i] = (r.notes[0].on - lead) * 1000.0;
        }
        check (latency[0] <= latency[1] + 0.01 && latency[1] <= latency[2] + 0.01,
               "note %d: response times not ordered: %.1f / %.1f / %.1f ms", note, latency[0], latency[1], latency[2]);
        info ("note %3d %7.1f Hz: note-on after %5.1f ms (fast), %5.1f ms (normal), %5.1f ms (safe)",
              note, noteToHz (note), latency[0], latency[1], latency[2]);
    }
}

//==============================================================================
// Different string/pickup characters must not cause octave errors.
void timbres()
{
    struct Named { const char* name; Timbre t; };
    std::vector<Named> list;
    { Timbre t; t.rolloff = 0.6;            list.push_back ({ "bright", t }); }
    { Timbre t; t.rolloff = 2.0;            list.push_back ({ "dark", t }); }
    { Timbre t; t.fundamentalGain = 0.1;    list.push_back ({ "weak fundamental", t }); }
    { Timbre t; t.oddGain = 0.4;            list.push_back ({ "weak odd harmonics", t }); }
    { Timbre t; t.evenGain = 0.3;           list.push_back ({ "weak even harmonics", t }); }
    { Timbre t; t.bowNoiseDb = -18.0;       list.push_back ({ "scratchy bow", t }); }
    { Timbre t; t.randomPhases = true;      list.push_back ({ "random phases", t }); }
    { Timbre t; t.formants = false; t.rolloff = 1.0; list.push_back ({ "plain sawtooth", t }); }

    const int notes[] = { 34, 38, 41, 48, 55, 62, 69, 76, 83, 90, 97, 103 };
    for (const Named& named : list)
    {
        int ok = 0, total = 0;
        for (int note : notes)
        {
            Synth synth (48000.0, (unsigned) (note + 99));
            const Signal sig = synth.render ({ rest (0.1), tone (note + 0.1, 0.5, -22.0, 0.03), rest (0.25) }, named.t);
            const RunResult r = run (sig, baseSettings());
            checkProtocol (r, named.name);
            ++total;
            const bool good = r.notes.size() == 1 && r.notes[0].note == note;
            ok += good ? 1 : 0;
            check (good, "%s, note %d: got %s", named.name, note, describeNotes (r).c_str());
        }
        info ("%-20s %d / %d correct", named.name, ok, total);
    }
}

//==============================================================================
void vibrato()
{
    const int notes[] = { 36, 50, 62, 76, 90 };
    for (int note : notes)
    {
        Segment seg = tone (note, 2.0, -20.0, 0.03);
        seg.vibratoDepth = 0.30;
        seg.vibratoRateHz = 6.0;
        seg.vibratoAmDb = 2.0;
        Synth synth (48000.0, (unsigned) note);
        const Signal sig = synth.render ({ rest (0.1), seg, rest (0.2) }, Timbre());

        // Continuous: the bend follows the vibrato.
        {
            const RunResult r = run (sig, baseSettings());
            checkProtocol (r, "vibrato/continuous");
            check (r.notes.size() == 1 && r.notes[0].note == note, "continuous, note %d: got %s", note, describeNotes (r).c_str());
            const PitchError e = pitchError (r, sig, 2, 0.3, 2.0);
            check (e.rmsCents < 7.0, "continuous, note %d: vibrato followed with %.1f cents rms error", note, e.rmsCents);
            info ("note %d continuous: rms %.1f cents, max %.1f cents, lag %.0f ms", note, e.rmsCents, e.maxCents, e.lagMs);
        }

        // Smoothed: the instrument stands still at the centre of the vibrato.
        {
            vm::Settings s = baseSettings();
            s.bendMode = vm::BendMode::Smoothed;
            const RunResult r = run (sig, s);
            checkProtocol (r, "vibrato/smoothed");
            check (r.notes.size() == 1 && r.notes[0].note == note, "smoothed, note %d: got %s", note, describeNotes (r).c_str());
            double lo = 1.0e9, hi = -1.0e9;
            for (double t = 0.6; t < 2.0; t += 0.002)
            {
                const double p = soundingPitch (r, t, 2);
                if (p > 0.0) { lo = std::min (lo, p); hi = std::max (hi, p); }
            }
            check ((hi - lo) * 100.0 < 14.0, "smoothed, note %d: still wobbling by %.1f cents", note, (hi - lo) * 100.0);
            check (std::abs ((hi + lo) * 0.5 - note) * 100.0 < 6.0, "smoothed, note %d: centre off by %.1f cents", note, ((hi + lo) * 0.5 - note) * 100.0);
            info ("note %d smoothed: residual wobble %.1f cents", note, (hi - lo) * 100.0);
        }

        // Quantized: one note, no bend at all.
        {
            vm::Settings s = baseSettings();
            s.bendMode = vm::BendMode::Quantized;
            const RunResult r = run (sig, s);
            checkProtocol (r, "vibrato/quantized");
            check (r.notes.size() == 1 && r.notes[0].note == note, "quantized, note %d: got %s", note, describeNotes (r).c_str());
            for (const TimedValue& b : r.bends)
                check (b.value == 8192, "quantized, note %d: pitch bend %d was sent", note, b.value);
        }
    }

    // A wide, fast vibrato on a low string must not be mistaken for finger steps.
    {
        Segment seg = tone (41, 2.0, -20.0, 0.03);
        seg.vibratoDepth = 0.5;
        seg.vibratoRateHz = 7.0;
        Synth synth (48000.0, 5);
        const Signal sig = synth.render ({ rest (0.1), seg, rest (0.2) }, Timbre());
        const RunResult r = run (sig, baseSettings());
        checkProtocol (r, "wide vibrato");
        check (r.notes.size() == 1, "wide vibrato on a low string: got %s", describeNotes (r).c_str());
    }
}

//==============================================================================
void glissando()
{
    const std::vector<Segment> up = { rest (0.1), tone (69, 0.3, -20.0, 0.03), slide (69, 81, 1.0), tone (81, 0.4, -20.0, 0.0), rest (0.2) };
    Synth synth (48000.0, 77);
    const Signal sig = synth.render (up, Timbre());
    const double end = 0.1 + 0.3 + 1.0 + 0.4;

    {   // wide bend range: one note, the bend does all the work
        vm::Settings s = baseSettings();
        s.bendRangeSemis = 24;
        const RunResult r = run (sig, s);
        checkProtocol (r, "slide/range24");
        check (r.notes.size() == 1, "range 24: expected one note for the whole slide, got %s", describeNotes (r).c_str());
        const PitchError e = pitchError (r, sig, 24, 0.3, end - 0.02);
        check (e.rmsCents < 10.0, "range 24: slide followed with %.1f cents rms error", e.rmsCents);
        const PitchError tail = pitchError (r, sig, 24, end - 0.15, end - 0.02);
        check (tail.maxCents < 6.0, "range 24: arrival note off by %.1f cents", tail.maxCents);
        info ("octave slide, bend range 24: rms %.1f cents, lag %.0f ms, %d note(s)", e.rmsCents, e.lagMs, (int) r.notes.size());
    }
    {   // narrow bend range: re-triggered on the way, but always at the right pitch
        const RunResult r = run (sig, baseSettings());
        checkProtocol (r, "slide/range2");
        check (r.notes.size() >= 3 && r.notes.size() <= 7, "range 2: unexpected number of notes: %s", describeNotes (r).c_str());
        const PitchError e = pitchError (r, sig, 2, 0.3, end - 0.02);
        check (e.rmsCents < 12.0, "range 2: slide followed with %.1f cents rms error", e.rmsCents);
        info ("octave slide, bend range 2: rms %.1f cents, %d notes", e.rmsCents, (int) r.notes.size());
    }
    {   // quantized: every semitone exactly once, in order
        vm::Settings s = baseSettings();
        s.bendMode = vm::BendMode::Quantized;
        const RunResult r = run (sig, s);
        checkProtocol (r, "slide/quantized");
        std::vector<int> expected;
        for (int n = 69; n <= 81; ++n) expected.push_back (n);
        check (noteNumbers (r) == expected, "quantized: got %s", toString (noteNumbers (r)).c_str());
    }
    {   // smoothed: one note, arrives in tune
        vm::Settings s = baseSettings();
        s.bendMode = vm::BendMode::Smoothed;
        s.bendRangeSemis = 24;
        const RunResult r = run (sig, s);
        checkProtocol (r, "slide/smoothed");
        check (r.notes.size() == 1, "smoothed: expected one note, got %s", describeNotes (r).c_str());
        const PitchError tail = pitchError (r, sig, 24, end - 0.1, end - 0.02);
        check (tail.maxCents < 6.0, "smoothed: arrival note off by %.1f cents", tail.maxCents);
    }
    {   // downward, quantized
        Synth s2 (48000.0, 78);
        const Signal down = s2.render ({ rest (0.1), tone (81, 0.3, -20.0, 0.03), slide (81, 69, 1.0), tone (69, 0.4, -20.0, 0.0), rest (0.2) }, Timbre());
        vm::Settings s = baseSettings();
        s.bendMode = vm::BendMode::Quantized;
        const RunResult r = run (down, s);
        checkProtocol (r, "slide/down");
        std::vector<int> expected;
        for (int n = 81; n >= 69; --n) expected.push_back (n);
        check (noteNumbers (r) == expected, "quantized downward: got %s", toString (noteNumbers (r)).c_str());
    }
    {   // slow slide on the lowest string
        Synth s3 (48000.0, 79);
        const Signal low = s3.render ({ rest (0.1), tone (34, 0.4, -20.0, 0.04), slide (34, 41, 1.5), tone (41, 0.4, -20.0, 0.0), rest (0.2) }, Timbre());
        vm::Settings s = baseSettings();
        s.bendRangeSemis = 12;
        const RunResult r = run (low, s);
        checkProtocol (r, "slide/low");
        check (r.notes.size() == 1, "low string slide: expected one note, got %s", describeNotes (r).c_str());
        const PitchError e = pitchError (r, low, 12, 0.4, 2.35, 80.0);
        check (e.rmsCents < 12.0, "low string slide followed with %.1f cents rms error", e.rmsCents);
        info ("slide on the lowest string: rms %.1f cents, lag %.0f ms", e.rmsCents, e.lagMs);
    }
    {   // fast slide: an octave in a quarter of a second
        Synth s4 (48000.0, 80);
        const Signal fast = s4.render ({ rest (0.1), tone (60, 0.3, -20.0, 0.03), slide (60, 72, 0.25), tone (72, 0.4, -20.0, 0.0), rest (0.2) }, Timbre());
        vm::Settings s = baseSettings();
        s.bendRangeSemis = 24;
        const RunResult r = run (fast, s);
        checkProtocol (r, "slide/fast");
        check (r.notes.size() == 1, "fast slide: expected one note, got %s", describeNotes (r).c_str());
        const PitchError e = pitchError (r, fast, 24, 0.3, 1.0);
        check (e.rmsCents < 15.0, "fast slide followed with %.1f cents rms error", e.rmsCents);
        info ("fast slide (48 semitones/s): rms %.1f cents, lag %.0f ms", e.rmsCents, e.lagMs);
    }
}

//==============================================================================
// Finger changes under one bow: every step must be a new note, in every mode.
void legatoSequence (const char* name, const std::vector<int>& pitches, double each, bool report)
{
    std::vector<Segment> segs = { rest (0.1) };
    for (size_t i = 0; i < pitches.size(); ++i)
        segs.push_back (tone (pitches[i], each, -20.0, i == 0 ? 0.03 : 0.0));
    segs.push_back (rest (0.2));

    Synth synth (48000.0, (unsigned) (pitches[0] * 13 + pitches.size()));
    const Signal sig = synth.render (segs, Timbre());

    const vm::BendMode modes[] = { vm::BendMode::Continuous, vm::BendMode::Smoothed, vm::BendMode::Quantized };
    const char* modeNames[] = { "continuous", "smoothed", "quantized" };
    for (int m = 0; m < 3; ++m)
    {
        vm::Settings s = baseSettings();
        s.bendMode = modes[m];
        const RunResult r = run (sig, s);
        checkProtocol (r, name);
        const bool same = noteNumbers (r) == pitches;
        check (same, "%s (%s): expected %s\n          got      %s", name, modeNames[m],
               toString (pitches).c_str(), toString (noteNumbers (r)).c_str());

        if (same && m == 0)
        {
            double worst = 0.0, worstPeriods = 0.0;
            for (size_t i = 1; i < pitches.size(); ++i)
            {
                const double changeAt = 0.1 + each * (double) i;
                const double delayMs = (r.notes[i].on - changeAt) * 1000.0;
                worst = std::max (worst, delayMs);
                worstPeriods = std::max (worstPeriods, delayMs / periodMs (pitches[i]));
                check (delayMs < 2.3 * periodMs (pitches[i]) + 14.0 && delayMs > 0.0,
                       "%s: step to %d detected after %.1f ms", name, pitches[i], delayMs);
            }
            if (report)
                info ("%-24s slowest finger change detected after %.1f ms", name, worst);
        }
    }
}

void legato()
{
    std::vector<int> mid, low, high, run;
    for (int n = 60; n <= 72; ++n) mid.push_back (n);
    for (int n = 34; n <= 46; ++n) low.push_back (n);
    for (int n = 88; n <= 100; ++n) high.push_back (n);
    for (int n = 62; n <= 74; ++n) run.push_back (n);

    legatoSequence ("chromatic, middle", mid, 0.15, true);
    legatoSequence ("chromatic, low strings", low, 0.25, true);
    legatoSequence ("chromatic, high", high, 0.12, true);
    legatoSequence ("fast run (12 notes/s)", run, 0.08, true);
    legatoSequence ("leaps and octaves", { 55, 67, 62, 74, 55, 79, 60, 48, 67, 43, 62, 86, 74 }, 0.2, true);

    std::vector<int> trill;
    for (int i = 0; i < 16; ++i) trill.push_back (i % 2 == 0 ? 69 : 70);
    legatoSequence ("semitone trill", trill, 0.06, true);

    std::vector<int> wholeTrill;
    for (int i = 0; i < 16; ++i) wholeTrill.push_back (i % 2 == 0 ? 76 : 78);
    legatoSequence ("whole-tone trill, high", wholeTrill, 0.05, true);
}

//==============================================================================
void bowStrokes()
{
    {   // separate strokes on one pitch, with the bow stopping in between
        std::vector<Segment> segs = { rest (0.1) };
        for (int i = 0; i < 6; ++i) { segs.push_back (tone (67, 0.18, -20.0, 0.015)); segs.push_back (rest (0.03)); }
        segs.push_back (rest (0.2));
        Synth synth (48000.0, 11);
        const RunResult r = run (synth.render (segs, Timbre()), baseSettings());
        checkProtocol (r, "detache");
        check (r.notes.size() == 6, "six separate strokes: got %s", describeNotes (r).c_str());
        for (const Note& n : r.notes) check (n.note == 67, "stroke detected as note %d", n.note);
    }
    {   // bow changes without a gap: the level dips and comes back
        std::vector<Segment> segs = { rest (0.1), tone (62, 0.2, -20.0, 0.02) };
        for (int i = 0; i < 4; ++i) { segs.push_back (tone (62, 0.02, -38.0, 0.0)); segs.push_back (tone (62, 0.2, -20.0, 0.0)); }
        segs.push_back (rest (0.2));
        Synth synth (48000.0, 12);
        const RunResult r = run (synth.render (segs, Timbre()), baseSettings());
        checkProtocol (r, "bow change");
        check (r.notes.size() == 5, "five strokes joined by bow changes: got %s", describeNotes (r).c_str());
    }
    {   // the same on a low string
        std::vector<Segment> segs = { rest (0.1), tone (38, 0.3, -20.0, 0.04) };
        for (int i = 0; i < 3; ++i) { segs.push_back (tone (38, 0.04, -38.0, 0.0)); segs.push_back (tone (38, 0.3, -20.0, 0.0)); }
        segs.push_back (rest (0.2));
        Synth synth (48000.0, 13);
        const RunResult r = run (synth.render (segs, Timbre()), baseSettings());
        checkProtocol (r, "bow change low");
        check (r.notes.size() == 4, "four strokes on a low string: got %s", describeNotes (r).c_str());
    }
    {   // a smooth bow change must stay one note
        std::vector<Segment> segs = { rest (0.1), tone (62, 0.3, -20.0, 0.02) };
        for (int i = 0; i < 4; ++i) { segs.push_back (tone (62, 0.02, -23.0, 0.0)); segs.push_back (tone (62, 0.3, -20.0, 0.0)); }
        segs.push_back (rest (0.2));
        Synth synth (48000.0, 14);
        const RunResult r = run (synth.render (segs, Timbre()), baseSettings());
        checkProtocol (r, "smooth bow change");
        check (r.notes.size() == 1, "smooth bow changes: got %s", describeNotes (r).c_str());
    }
    {   // swelling and fading inside one note
        Segment up = tone (69, 1.5, -20.0, 0.05);  up.levelStartDb = -42.0; up.levelEndDb = -14.0;
        Segment down = tone (69, 1.5, -20.0, 0.0); down.levelStartDb = -14.0; down.levelEndDb = -42.0;
        Synth synth (48000.0, 15);
        const RunResult r = run (synth.render ({ rest (0.1), up, down, rest (0.2) }, Timbre()), baseSettings());
        checkProtocol (r, "swell");
        check (r.notes.size() == 1, "swell: got %s", describeNotes (r).c_str());
    }
    {   // tremolo, 12 strokes per second
        std::vector<Segment> segs = { rest (0.1) };
        for (int i = 0; i < 12; ++i) { segs.push_back (tone (74, 0.063, -20.0, 0.008)); segs.push_back (tone (74, 0.02, -40.0, 0.0)); }
        segs.push_back (rest (0.2));
        Synth synth (48000.0, 16);
        const RunResult r = run (synth.render (segs, Timbre()), baseSettings());
        checkProtocol (r, "tremolo");
        check (r.notes.size() == 12, "tremolo of 12 strokes: got %d notes: %s", (int) r.notes.size(), describeNotes (r).c_str());
    }
    {   // strong vibrato with loudness changes is not a bow stroke
        Segment seg = tone (64, 2.0, -20.0, 0.03);
        seg.vibratoDepth = 0.35; seg.vibratoRateHz = 6.5; seg.vibratoAmDb = 3.5;
        Synth synth (48000.0, 17);
        const RunResult r = run (synth.render ({ rest (0.1), seg, rest (0.2) }, Timbre()), baseSettings());
        checkProtocol (r, "vibrato AM");
        check (r.notes.size() == 1, "vibrato with loudness changes: got %s", describeNotes (r).c_str());
    }
}

//==============================================================================
void silenceAndNoise()
{
    auto noteCount = [] (const Signal& sig) {
        const RunResult r = run (sig, baseSettings());
        checkProtocol (r, "noise");
        return (int) r.notes.size();
    };

    {
        Synth synth (48000.0, 21);
        const Signal sig = synth.render ({ rest (3.0) }, Timbre(), -75.0);
        check (noteCount (sig) == 0, "quiet input produced notes");
    }
    {   // loud hiss, well above the gate
        Synth synth (48000.0, 22);
        const Signal sig = synth.render ({ rest (5.0) }, Timbre(), -28.0);
        check (noteCount (sig) == 0, "white noise at -28 dBFS produced %d notes", noteCount (sig));
    }
    {   // clicks and thumps
        Signal sig;
        sig.sampleRate = 48000.0;
        sig.samples.assign (48000 * 4, 0.0f);
        sig.truePitch.assign (sig.samples.size(), -1.0f);
        std::mt19937 rng (5);
        std::normal_distribution<float> g (0.0f, 1.0f);
        for (size_t i = 0; i < sig.samples.size(); ++i)
            sig.samples[i] = g (rng) * 0.0003f;
        for (int k = 0; k < 16; ++k)
        {
            const size_t at = (size_t) (4000 + k * 11000);
            for (size_t i = 0; i < 400; ++i)
                sig.samples[at + i] += 0.6f * std::exp (-(float) i / 60.0f) * g (rng);
        }
        const int n = noteCount (sig);
        check (n == 0, "clicks produced %d notes", n);
    }
    {   // bow noise without a pitch (bow resting and scraping lightly)
        Timbre t;
        Synth synth (48000.0, 23);
        Segment seg = tone (60, 2.0, -70.0, 0.0);
        const Signal sig = synth.render ({ seg }, t, -34.0);
        check (noteCount (sig) == 0, "noise with a buried tone produced notes");
    }
}

//==============================================================================
void pizzicato()
{
    const std::vector<int> pitches = { 55, 62, 69, 76, 64, 43, 36 };
    std::vector<Segment> segs = { rest (0.1) };
    for (int p : pitches)
    {
        Segment s = tone (p, 0.4, -14.0, 0.002);
        s.decayDbPerSec = 70.0;
        segs.push_back (s);
        segs.push_back (rest (0.08));
    }
    Synth synth (48000.0, 31);
    Timbre t;
    t.bowNoiseDb = -60.0;
    const Signal sig = synth.render (segs, t);
    const RunResult r = run (sig, baseSettings());
    checkProtocol (r, "pizzicato");
    check (noteNumbers (r) == pitches, "pizzicato: expected %s got %s", toString (pitches).c_str(), toString (noteNumbers (r)).c_str());
}

//==============================================================================
// The result must not depend on the audio buffer size of the interface.
void blockSizes()
{
    std::vector<Segment> segs = { rest (0.1), tone (60, 0.2, -20.0, 0.02), tone (64, 0.2, -20.0, 0.0), slide (64, 67, 0.3) };
    Segment vib = tone (67, 0.6, -20.0, 0.0);
    vib.vibratoDepth = 0.25;
    segs.push_back (vib);
    segs.push_back (rest (0.1));
    segs.push_back (tone (45, 0.3, -18.0, 0.03));
    segs.push_back (rest (0.2));

    for (double fs : { 48000.0, 96000.0 })
    {
        Synth synth (fs, 41);
        const Signal sig = synth.render (segs, Timbre());
        const RunResult reference = run (sig, baseSettings(), 64);
        checkProtocol (reference, "block reference");
        check (reference.notes.size() >= 4, "reference run produced too few notes: %s", describeNotes (reference).c_str());

        for (int block : { 1, 7, 32, 100, 256, 1024, 4096 })
        {
            const RunResult r = run (sig, baseSettings(), block);
            bool same = r.events.size() == reference.events.size();
            for (size_t i = 0; same && i < r.events.size(); ++i)
                same = r.events[i].sample == reference.events[i].sample
                    && std::memcmp (r.events[i].event.bytes, reference.events[i].event.bytes, 3) == 0;
            check (same, "block size %d at %.0f Hz gives different MIDI than block size 64", block, fs);
        }
    }
}

//==============================================================================
void sampleRates()
{
    for (double fs : { 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 })
    {
        int ok = 0, total = 0;
        double worst = 0.0;
        for (int note : { 34, 45, 57, 69, 81, 93, 103 })
        {
            Synth synth (fs, (unsigned) note);
            const Signal sig = synth.render ({ rest (0.1), tone (note + 0.2, 0.5, -20.0, 0.02), rest (0.25) }, Timbre());
            const RunResult r = run (sig, baseSettings(), 128);
            checkProtocol (r, "sample rate");
            ++total;
            const bool good = r.notes.size() == 1 && r.notes[0].note == note;
            check (good, "%.0f Hz, note %d: got %s", fs, note, describeNotes (r).c_str());
            if (good)
            {
                ++ok;
                const PitchError e = pitchError (r, sig, 2, r.notes[0].on + 0.06, 0.58);
                worst = std::max (worst, e.maxCents);
                check (e.maxCents < 6.0, "%.0f Hz, note %d: pitch off by %.1f cents", fs, note, e.maxCents);
            }
        }
        info ("%6.0f Hz: %d / %d correct, worst pitch error %.1f cents", fs, ok, total, worst);
    }
}

//==============================================================================
void dynamics()
{
    {   // louder bowing -> higher velocity
        std::vector<Segment> segs = { rest (0.1) };
        const double levels[] = { -42.0, -34.0, -26.0, -18.0, -12.0 };
        for (double l : levels) { segs.push_back (tone (67, 0.3, l, 0.01)); segs.push_back (rest (0.15)); }
        Synth synth (48000.0, 51);
        const Signal sig = synth.render (segs, Timbre());

        vm::Settings s = baseSettings();
        s.dynamics = vm::DynamicsMode::VelocityOnly;
        const RunResult r = run (sig, s);
        checkProtocol (r, "velocity");
        check (r.notes.size() == 5, "velocity test: got %s", describeNotes (r).c_str());
        for (size_t i = 1; i < r.notes.size(); ++i)
            check (r.notes[i].velocity > r.notes[i - 1].velocity, "velocity does not rise with level: %s", describeNotes (r).c_str());
        check (r.expression.empty(), "velocity-only mode sent expression");
        if (r.notes.size() == 5)
            info ("velocities for -42 / -34 / -26 / -18 / -12 dB: %d %d %d %d %d",
                  r.notes[0].velocity, r.notes[1].velocity, r.notes[2].velocity, r.notes[3].velocity, r.notes[4].velocity);

        s.dynamics = vm::DynamicsMode::Fixed;
        s.fixedVelocity = 96;
        const RunResult f = run (sig, s);
        checkProtocol (f, "fixed velocity");
        for (const Note& n : f.notes) check (n.velocity == 96, "fixed velocity mode sent velocity %d", n.velocity);
        check (f.expression.empty(), "fixed mode sent expression");
    }
    {   // swelling inside one note is followed by expression
        Segment up = tone (62, 2.0, -20.0, 0.05);
        up.levelStartDb = -44.0;
        up.levelEndDb = -14.0;
        Synth synth (48000.0, 52);
        const Signal sig = synth.render ({ rest (0.1), up, rest (0.3) }, Timbre());
        const RunResult r = run (sig, baseSettings());
        checkProtocol (r, "expression");
        check (r.notes.size() == 1, "expression test: got %s", describeNotes (r).c_str());

        int atStart = -1, atEnd = -1, drops = 0, previous = -1;
        for (const TimedValue& v : r.expression)
        {
            if (v.time < 0.3) atStart = v.value;
            if (v.time < 2.05) atEnd = v.value;
            if (v.time > 0.3 && v.time < 2.05 && previous >= 0 && v.value < previous - 3) ++drops;
            previous = v.value;
        }
        check (atStart >= 0 && atEnd - atStart >= 45, "expression rose only from %d to %d during the swell", atStart, atEnd);
        check (drops == 0, "expression jumped down %d times during a steady swell", drops);
        check (! r.expression.empty() && r.expression.back().value == 127, "expression was not returned to 127 at shutdown");
        info ("swell from -44 to -14 dB: expression %d -> %d", atStart, atEnd);
    }
}

//==============================================================================
void rangeAndTuning()
{
    {   // notes below the chosen range are ignored
        Synth synth (48000.0, 61);
        const Signal sig = synth.render ({ rest (0.1), tone (48, 0.4, -20.0, 0.02), rest (0.2), tone (55, 0.4, -20.0, 0.02), rest (0.2) }, Timbre());
        vm::Settings s = baseSettings();
        s.lowestNote = 55;
        const RunResult r = run (sig, s);
        checkProtocol (r, "range");
        check (noteNumbers (r) == std::vector<int> { 55 }, "lowest note G3: got %s", toString (noteNumbers (r)).c_str());
    }
    {   // tuning reference
        Synth synth (48000.0, 62);
        const double a443 = hzToNote (443.0);
        const Signal sig = synth.render ({ rest (0.1), tone (a443, 0.6, -20.0, 0.02), rest (0.2) }, Timbre());

        vm::Settings s = baseSettings();
        s.a4Hz = 443.0f;
        const RunResult r = run (sig, s);
        check (r.notes.size() == 1 && r.notes[0].note == 69, "A = 443 Hz: got %s", describeNotes (r).c_str());
        const double p443 = soundingPitch (r, 0.5, 2);
        check (std::abs (p443 - 69.0) * 100.0 < 3.0, "A = 443 Hz: 443 Hz is %.1f cents away from A4", (p443 - 69.0) * 100.0);

        s.a4Hz = 440.0f;
        const RunResult r2 = run (sig, s);
        const double p440 = soundingPitch (r2, 0.5, 2);
        check (std::abs ((p440 - 69.0) * 100.0 - 11.8) < 3.0, "A = 440 Hz: 443 Hz measured as %.1f cents sharp, expected 11.8", (p440 - 69.0) * 100.0);
    }
}

//==============================================================================
void ringOut()
{
    {   // an open string ringing after the bow has left it
        Segment ring = tone (62, 2.5, -20.0, 0.0);
        ring.decayDbPerSec = 22.0;
        Synth synth (48000.0, 71);
        Timbre t;
        t.bowNoiseDb = -60.0;
        const Signal sig = synth.render ({ rest (0.1), tone (62, 0.4, -20.0, 0.02), ring, rest (0.2) }, t);
        const RunResult r = run (sig, baseSettings());
        checkProtocol (r, "ring out");
        check (r.notes.size() == 1, "ringing string: got %s", describeNotes (r).c_str());
        if (r.notes.size() == 1)
        {
            // -45 dB gate, 6 dB hysteresis -> released around 31 dB below the start, about 1.4 s
            check (r.notes[0].off > 1.2 && r.notes[0].off < 2.4, "ringing string released at %.2f s", r.notes[0].off);
            info ("ringing open string released %.2f s after the bow left", r.notes[0].off - 0.5);
        }
    }
    {   // quiet sympathetic ringing of another string must not start a note
        Synth synth (48000.0, 72);
        const Signal sig = synth.render ({ rest (0.1), tone (69, 0.4, -16.0, 0.02), tone (62, 1.0, -52.0, 0.0), rest (0.2) }, Timbre());
        const RunResult r = run (sig, baseSettings());
        checkProtocol (r, "sympathetic");
        check (noteNumbers (r) == std::vector<int> { 69 }, "sympathetic ringing: got %s", describeNotes (r).c_str());
        if (! r.notes.empty())
            check (r.notes[0].off < 0.62, "note hung for %.0f ms after the bow stopped", (r.notes[0].off - 0.5) * 1000.0);
    }
}

//==============================================================================
// Behaviour learned from real bowed-string recordings.
void realWorldCases()
{
    {   // A bowed note starts flat and rises to pitch. Without pitch bend the
        // semitone must still be the right one, and only one note may sound.
        for (int note : { 45, 62, 76, 88 })
        {
            Segment scoop = slide (note - 0.45, note - 0.05, 0.09);
            scoop.attackSec = 0.03;
            Synth synth (48000.0, (unsigned) (note + 500));
            const Signal sig = synth.render ({ rest (0.1), scoop, tone (note - 0.05, 0.6, -20.0, 0.0), rest (0.2) }, Timbre());
            vm::Settings s = baseSettings();
            s.bendMode = vm::BendMode::Quantized;
            const RunResult r = run (sig, s);
            checkProtocol (r, "flat attack");
            check (noteNumbers (r) == std::vector<int> { note }, "flat attack, quantized, note %d: got %s", note, describeNotes (r).c_str());
        }
    }
    {   // While the bow bites, a low string wanders in pitch before it settles.
        // The note must start on the settled pitch, once.
        std::vector<Segment> segs = { rest (0.1) };
        const double wander[] = { 43.2, 42.7, 44.6, 46.1, 45.0, 46.6, 44.2, 47.4, 46.3 };
        int i = 0;
        for (double p : wander)
        {
            Segment s = tone (p, 0.005, -34.0 + 1.2 * i, i == 0 ? 0.004 : 0.0);
            segs.push_back (s);
            ++i;
        }
        Segment body = tone (47.0, 0.8, -20.0, 0.0);
        body.levelStartDb = -23.0; // the level keeps rising while the string settles
        segs.push_back (body);
        segs.push_back (rest (0.2));
        Timbre t;
        t.bowNoiseDb = -22.0;
        Synth synth (48000.0, 601);
        const Signal sig = synth.render (segs, t);
        {   // without pitch bend the semitone has to be right from the start
            vm::Settings s = baseSettings();
            s.bendMode = vm::BendMode::Quantized;
            const RunResult r = run (sig, s);
            checkProtocol (r, "wandering attack");
            check (noteNumbers (r) == std::vector<int> { 47 }, "wandering attack (quantized): got %s", describeNotes (r).c_str());
        }
        {   // with pitch bend the note is struck once and bent to where it settles
            const RunResult r = run (sig, baseSettings());
            checkProtocol (r, "wandering attack");
            check (r.notes.size() == 1, "wandering attack (continuous): struck %d times: %s", (int) r.notes.size(), describeNotes (r).c_str());
            const double settled = soundingPitch (r, 0.6, 2);
            check (std::abs (settled - 47.0) * 100.0 < 5.0, "wandering attack (continuous): settled %.1f cents away", (settled - 47.0) * 100.0);
        }
    }
    {   // As a note dies away, its octave can outlast the fundamental. That is
        // the same note ringing out, not a new one.
        Segment tail = tone (57, 0.5, -34.0, 0.0);
        tail.decayDbPerSec = 40.0;
        Segment body = tone (45, 0.8, -18.0, 0.03);
        Segment fade = tone (45, 0.08, -18.0, 0.0);
        fade.levelEndDb = -34.0;
        Synth synth (48000.0, 602);
        const Signal sig = synth.render ({ rest (0.1), body, fade, tail, rest (0.3) }, Timbre());
        const RunResult r = run (sig, baseSettings());
        checkProtocol (r, "octave in the tail");
        check (noteNumbers (r) == std::vector<int> { 45 }, "octave ringing in the tail: got %s", describeNotes (r).c_str());
    }
    {   // A plucked string beats as it fades. Each swell is not a new stroke.
        Segment pluck = tone (69, 1.2, -12.0, 0.002);
        pluck.decayDbPerSec = 28.0;
        pluck.vibratoRateHz = 7.0;
        pluck.vibratoAmDb = 5.0;
        Timbre t;
        t.bowNoiseDb = -60.0;
        Synth synth (48000.0, 603);
        const Signal sig = synth.render ({ rest (0.1), pluck, rest (0.3) }, t);
        const RunResult r = run (sig, baseSettings());
        checkProtocol (r, "beating pluck");
        check (noteNumbers (r) == std::vector<int> { 69 }, "plucked note with beating: got %s", describeNotes (r).c_str());
    }
    {   // The tail of a note hovering around the gate must not chatter.
        Segment body = tone (74, 0.6, -24.0, 0.03);
        Segment tail = tone (74, 1.0, -41.0, 0.0);
        tail.levelEndDb = -55.0;
        tail.vibratoRateHz = 9.0;
        tail.vibratoAmDb = 4.0;
        Segment fade = tone (74, 0.15, -24.0, 0.0);
        fade.levelEndDb = -41.0;
        Synth synth (48000.0, 604);
        const Signal sig = synth.render ({ rest (0.1), body, fade, tail, rest (0.3) }, Timbre());
        const RunResult r = run (sig, baseSettings());
        checkProtocol (r, "tail at the gate");
        check (noteNumbers (r) == std::vector<int> { 74 }, "tail hovering at the gate: got %s", describeNotes (r).c_str());
    }
}

//==============================================================================
// Changing settings while playing must never leave a note hanging.
void settingsChanges()
{
    Segment vib = tone (64, 6.0, -20.0, 0.03);
    vib.vibratoDepth = 0.2;
    Synth synth (48000.0, 81);
    const Signal sig = synth.render ({ rest (0.1), vib, rest (0.3) }, Timbre());

    vm::Engine engine;
    engine.prepare (sig.sampleRate);
    RunResult r;
    r.sampleRate = sig.sampleRate;
    vm::MidiEventList out;
    out.reserve (4096);
    vm::Settings s = baseSettings();

    const int block = 96;
    int step = 0;
    std::mt19937 rng (3);
    for (long long pos = 0; pos < (long long) sig.samples.size(); pos += block)
    {
        if (pos / block % 150 == 149) // roughly every 0.3 s
        {
            switch (step++ % 9)
            {
                case 0: s.bendMode = vm::BendMode::Smoothed; break;
                case 1: s.dynamics = vm::DynamicsMode::VelocityOnly; break;
                case 2: s.bendRangeSemis = 12; break;
                case 3: s.bendMode = vm::BendMode::Quantized; break;
                case 4: s.midiChannel = 5; break;
                case 5: s.dynamics = vm::DynamicsMode::Continuous; s.response = vm::Response::Safe; break;
                case 6: s.bendMode = vm::BendMode::Continuous; s.lowestNote = 48; break;
                case 7: s.dynamics = vm::DynamicsMode::Fixed; s.a4Hz = 442.0f; s.inputGainDb = 6.0f; break;
                case 8: s.midiChannel = 1; s.response = vm::Response::Fast; s.lowestNote = 70; break; // note falls out of range
            }
        }
        const int n = (int) std::min<long long> (block, (long long) sig.samples.size() - pos);
        out.clear();
        engine.process (sig.samples.data() + pos, n, s, out);
        for (const vm::MidiEvent& e : out) r.events.push_back ({ pos + e.sampleOffset, e });
    }
    out.clear();
    engine.shutdown (out);
    for (const vm::MidiEvent& e : out) r.events.push_back ({ (long long) sig.samples.size(), e });
    decode (r);
    checkProtocol (r, "settings changes");
    check (! r.notes.empty(), "no notes at all while changing settings");

    // After leaving continuous dynamics the instrument's volume must be handed back.
    bool handedBack = false;
    for (const TimedValue& v : r.expression)
        if (v.time > 0.5 && v.time < 0.75 && v.value == 127)
            handedBack = true;
    check (handedBack, "expression was not reset to 127 when continuous dynamics was switched off");

    // The very last bend must be the centre.
    check (! r.bends.empty() && r.bends.back().value == 8192, "pitch bend not centred at shutdown");
    info ("%d notes, %d bend and %d expression messages, nothing left hanging",
          (int) r.notes.size(), (int) r.bends.size(), (int) r.expression.size());
}

//==============================================================================
// Random settings, signals, sample rates and block sizes, with the settings
// changing while notes sound. Whatever happens, the MIDI must stay well-formed
// and no note may be left hanging. Built with a memory checker this also
// catches any read or write outside the buffers.
void randomStress()
{
    std::mt19937 rng (20261121);
    auto uniform = [&rng] (double lo, double hi) { return std::uniform_real_distribution<double> (lo, hi) (rng); };
    auto pick = [&rng] (int lo, int hi) { return std::uniform_int_distribution<int> (lo, hi) (rng); };

    auto randomSettings = [&] ()
    {
        vm::Settings s;
        s.bendMode = (vm::BendMode) pick (0, 2);
        s.dynamics = (vm::DynamicsMode) pick (0, 2);
        s.response = (vm::Response) pick (0, 2);
        s.bendRangeSemis = (float) pick (-2, 60);           // includes values outside the valid range
        s.lowestNote = pick (20, 70);
        s.highestNote = pick (60, 120);
        s.a4Hz = (float) uniform (390.0, 490.0);
        s.inputGainDb = (float) uniform (-20.0, 30.0);
        s.gateDb = (float) uniform (-75.0, -15.0);
        s.fixedVelocity = pick (-5, 140);
        s.midiChannel = pick (0, 17);
        return s;
    };

    const double rates[] = { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0, 22050.0, 32000.0 };
    int totalNotes = 0;

    for (int round = 0; round < 40; ++round)
    {
        const double fs = rates[pick (0, 6)];
        std::vector<Segment> segs;
        const int count = pick (4, 14);
        for (int i = 0; i < count; ++i)
        {
            const int kind = pick (0, 5);
            const double a = uniform (26.0, 108.0), b = uniform (26.0, 108.0);
            const double level = uniform (-55.0, -6.0);
            if (kind == 0)      segs.push_back (rest (uniform (0.005, 0.25)));
            else if (kind == 1) segs.push_back (slide (a, a + uniform (-14.0, 14.0), uniform (0.02, 0.5), level));
            else if (kind == 2) { Segment s = tone (a, uniform (0.01, 0.4), level, uniform (0.0, 0.05)); s.vibratoDepth = uniform (0.0, 1.5); s.vibratoRateHz = uniform (2.0, 12.0); s.vibratoAmDb = uniform (0.0, 8.0); segs.push_back (s); }
            else if (kind == 3) { Segment s = tone (a, uniform (0.05, 0.6), level, 0.002); s.decayDbPerSec = uniform (10.0, 200.0); segs.push_back (s); }
            else if (kind == 4) { Segment s = tone (a, uniform (0.003, 0.03), level, 0.0); segs.push_back (s); segs.push_back (tone (b, uniform (0.003, 0.3), level + uniform (-25.0, 10.0), 0.0)); }
            else                segs.push_back (tone (a, uniform (0.02, 0.5), level, uniform (0.0, 0.08)));
        }

        Timbre timbre;
        timbre.rolloff = uniform (0.3, 2.5);
        timbre.fundamentalGain = uniform (0.02, 1.5);
        timbre.oddGain = uniform (0.1, 1.5);
        timbre.evenGain = uniform (0.1, 1.5);
        timbre.bowNoiseDb = uniform (-60.0, -6.0);
        timbre.randomPhases = pick (0, 1) == 1;
        Synth synth (fs, (unsigned) (round * 31 + 7));
        const Signal sig = synth.render (segs, timbre, uniform (-90.0, -40.0));

        vm::Engine engine;
        engine.prepare (fs);
        RunResult r;
        r.sampleRate = fs;
        vm::MidiEventList out;
        vm::Settings s = randomSettings();
        int maxPerCall = 0;

        long long pos = 0;
        const long long total = (long long) sig.samples.size();
        while (pos < total)
        {
            if (pick (0, 40) == 0)
                s = randomSettings();
            if (pick (0, 400) == 0)
            {
                out.clear();
                engine.shutdown (out); // as the app does on "all notes off" and when the device stops
                for (const vm::MidiEvent& e : out) r.events.push_back ({ pos, e });
            }
            if (pick (0, 600) == 0)
            {
                out.clear();
                engine.shutdown (out);
                for (const vm::MidiEvent& e : out) r.events.push_back ({ pos, e });
                engine.prepare (fs);   // device restarted
            }

            const int n = (int) std::min<long long> (pick (1, 3000), total - pos);
            out.clear();
            engine.process (sig.samples.data() + pos, n, s, out);
            maxPerCall = std::max (maxPerCall, (int) out.size() - engine.maxEventsFor (n));
            for (const vm::MidiEvent& e : out)
            {
                check (e.sampleOffset >= 0 && e.sampleOffset < n, "round %d: event offset %d outside the block of %d", round, e.sampleOffset, n);
                r.events.push_back ({ pos + e.sampleOffset, e });
            }
            pos += n;
        }

        out.clear();
        engine.shutdown (out);
        for (const vm::MidiEvent& e : out) r.events.push_back ({ total, e });

        decode (r);
        // Notes may legitimately sit on different channels when the channel
        // setting changes, but each must be released and none may overlap.
        for (const std::string& p : r.problems)
            check (false, "round %d (%.0f Hz): %s", round, fs, p.c_str());
        check (r.stuckNotes == 0, "round %d: %d note(s) never released", round, r.stuckNotes);
        check (maxPerCall <= 0, "round %d: more events in one call than maxEventsFor() promises (%d over)", round, maxPerCall);
        totalNotes += (int) r.notes.size();
    }

    info ("40 random sessions, %d notes, every one released, MIDI well-formed", totalNotes);
}

//==============================================================================
// Broken input must neither crash nor produce broken MIDI.
void garbage()
{
    Signal sig;
    sig.sampleRate = 48000.0;
    std::mt19937 rng (91);
    std::uniform_real_distribution<float> uni (-1.0f, 1.0f);
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();

    auto add = [&] (int count, const std::function<float (int)>& f) {
        for (int i = 0; i < count; ++i) sig.samples.push_back (f (i));
    };

    add (24000, [&] (int) { return uni (rng); });                             // full-scale noise
    add (4800,  [&] (int) { return nan; });
    add (24000, [&] (int i) { return (i / 109) % 2 == 0 ? 1.0f : -1.0f; });   // full-scale square, 220 Hz
    add (4800,  [&] (int i) { return i % 2 == 0 ? inf : -inf; });
    add (24000, [&] (int i) { return 0.5f * std::sin (0.0288f * (float) i); }); // 220 Hz sine
    add (4800,  [&] (int) { return 1.0e30f; });
    add (24000, [&] (int) { return 0.9f; });                                  // DC
    add (24000, [&] (int i) { return 0.5f * std::sin (0.0288f * (float) i); });
    add (24000, [&] (int i) { return (i % 3 == 0) ? nan : 0.3f * std::sin (0.05f * (float) i); });
    add (48000, [&] (int) { return 0.0f; });                                  // digital silence
    add (24000, [&] (int i) { return 0.4f * std::sin (0.0576f * (float) i); }); // 440 Hz sine
    sig.truePitch.assign (sig.samples.size(), -1.0f);

    for (int block : { 64, 333 })
    {
        const RunResult r = run (sig, baseSettings(), block);
        checkProtocol (r, "garbage");
        bool heard220 = false, heard440 = false;
        for (const Note& n : r.notes)
        {
            heard220 = heard220 || n.note == 57;
            heard440 = heard440 || n.note == 69;
        }
        check (heard220 && heard440, "engine did not recover after broken input: %s", describeNotes (r).c_str());
    }
}

//==============================================================================
void performance()
{
    // Worst case: the lowest string, full range, so the longest windows are used.
    Segment seg = tone (34, 20.0, -20.0, 0.03);
    seg.vibratoDepth = 0.2;
    Synth synth (48000.0, 99);
    const Signal sig = synth.render ({ seg }, Timbre());

    vm::Engine engine;
    engine.prepare (48000.0);
    vm::MidiEventList out;
    out.reserve (4096);
    const vm::Settings s = baseSettings();

    double best = 1.0e9;
    for (int pass = 0; pass < 3; ++pass)
    {
        engine.reset();
        const auto t0 = std::chrono::steady_clock::now();
        for (size_t pos = 0; pos + 64 <= sig.samples.size(); pos += 64)
        {
            out.clear();
            engine.process (sig.samples.data() + pos, 64, s, out);
        }
        const double seconds = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        best = std::min (best, seconds);
    }

    const double load = best / 20.0 * 100.0;
    info ("20 s of audio processed in %.2f s -> %.1f %% of one processor core", best, load);
    // The limit is generous on purpose: emulated or instrumented builds run several
    // times slower than the real thing, and this test is only meant to catch a blow-up.
    check (load < 40.0, "processing needs %.1f %% of a core", load);
}

//==============================================================================
struct TestCase
{
    const char* name;
    void (*function)();
};

const TestCase kTests[] = {
    { "steady notes, 48 kHz", steadyNotes48 },
    { "steady notes, 44.1 kHz", steadyNotes44 },
    { "response settings", responseSettings },
    { "timbres", timbres },
    { "vibrato", vibrato },
    { "glissando", glissando },
    { "legato finger changes", legato },
    { "bow strokes", bowStrokes },
    { "silence and noise", silenceAndNoise },
    { "pizzicato", pizzicato },
    { "ring-out", ringOut },
    { "real-world cases", realWorldCases },
    { "dynamics", dynamics },
    { "range and tuning", rangeAndTuning },
    { "sample rates", sampleRates },
    { "block sizes", blockSizes },
    { "settings changes", settingsChanges },
    { "broken input", garbage },
    { "random stress", randomStress },
    { "scale tables", scaleTables },
    { "stage pass-through", stagePassThrough },
    { "stage key filter", stageKeyFilter },
    { "stage sustain", stageSustain },
    { "stage slide-in", stageSlideIn },
    { "stage triad", stageTriad },
    { "stage settings", stageSettings },
    { "stage random", stageRandom },
    { "bend range small", bendRangeSmall },
    { "key filter audio", keyFilterAudio },
    { "master switch", masterSwitch },
    { "scales and voicings", scalesAndVoicings },
    { "harmony follows pitch", harmonyFollowsPitch },
    { "harmony settings", harmonySettings },
    { "harmony settings: identical voices kept", harmonyNoRetrigger },
    { "dynamics controllers", dynamicsControllers },
    { "mpe", mpeTests },
    { "freeze", freezeTests },
    { "freeze collisions", freezeCollisions },
    { "engine: mpe, freeze, all notes off", engineMpeFreeze },
    { "stage random 2", stageRandom2 },
    { "performance", performance },
    { "gate in/out", trackerGateInOut },
    { "octave guards", trackerOctaveGuards },
    { "articulation", trackerArticulation },
    { "dynamics target", trackerDynamicsTarget },
    { "dynamics smoothing", trackerDynamicsSmoothing },
    { "bridging", trackerBridging },
    { "jump guard", trackerJumpGuard },
    { "playing aids audio", playingAidsAudio },
    { "harmonizer pitch", harmonizerPitch },
    { "harmonizer body", harmonizerBody },
    { "harmonizer release and overlap", harmonizerReleaseOverlap },
    { "harmonizer robustness", harmonizerRobustness },
    { "harmonizer performance", harmonizerPerformance },
    { "ring-on release", ringOnRelease },
    { "ring-on overlap", ringOnOverlap },
    { "ring-on interrupted", ringOnInterrupted },
    { "harmony offset", harmonyOffsetTests },
    { "stage random 3", stageRandom3 },
    { "camera pad", cameraPad },
    { "voicing templates", voicingTemplates },
    { "voicing voices in the stage", voicingVoicesStage },
    { "curves", curves },
    { "drop voicing", dropVoicing },
    { "ensemble and high-cut", ensembleAndHiCut },
    { "armed harmony freeze in the audio harmonizer", armedFreezeAudio },
    { "camera gestures", cameraGestures },
};

} // namespace

int main (int argc, char** argv)
{
    const char* only = nullptr;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp (argv[i], "--verbose") == 0) gVerbose = true;
        else if (std::strcmp (argv[i], "--only") == 0 && i + 1 < argc) only = argv[++i];
    }

    int failedTests = 0, ran = 0;
    for (const TestCase& test : kTests)
    {
        if (only != nullptr && std::strstr (test.name, only) == nullptr)
            continue;
        ++ran;
        gFailuresInTest = 0;
        std::printf ("[ RUN  ] %s\n", test.name);
        std::fflush (stdout);
        test.function();
        std::printf ("[ %s ] %s\n", gFailuresInTest == 0 ? " OK " : "FAIL", test.name);
        std::fflush (stdout);
        failedTests += gFailuresInTest == 0 ? 0 : 1;
    }

    std::printf ("\n%d test group(s) run, %d failed\n", ran, failedTests);
    return failedTests == 0 ? 0 : 1;
}
