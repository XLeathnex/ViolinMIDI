// ViolinMIDI - tests for the playing aids in NoteTracker: bridging of short
// gaps in detection (Settings::bridgeMs) and the jump guard ("wrong string
// filter": jumpGuard, jumpIntervalSemis, jumpWindowMs, jumpLockMs).
//
// Included by test_main.cpp after test_tracker2.h, inside that file's anonymous
// namespace; uses T2Rig and the t2* helpers. Like test_tracker2.h, most groups
// drive NoteTracker directly at 750 hops/s, and the sample offset of every
// event is the hop index.
#pragma once

//==============================================================================
inline vm::NoteTracker::Input t3Unvoiced (float levelDb)
{
    vm::NoteTracker::Input i;
    i.voiced = false;
    i.levelDb = levelDb;
    return i;
}

inline int t3CountOff (const Events& ev, int note)
{
    int n = 0;
    for (const vm::MidiEvent& e : ev)
        if (stIsOff (e) && e.bytes[1] == note)
            ++n;
    return n;
}

inline int t3CountNotes (const Events& ev)
{
    int n = 0;
    for (const vm::MidiEvent& e : ev)
        if (stIsNote (e))
            ++n;
    return n;
}

/** Hops the tracker bridges at 750 hops/s: max (bridge, 2.6 periods of the lowest note). */
inline int t3ExpectedHold (float bridgeMs, int lowestNote)
{
    const int bridge = bridgeMs > 0.0f ? std::max (2, (int) std::lround (bridgeMs * 0.75)) : 0;
    const double periodHops = 750.0 / noteToHz ((double) lowestNote);
    return std::max (bridge, (int) std::ceil (2.6f * (float) periodHops));
}

//==============================================================================
// A sounding note is kept through a short gap in detection while the level holds.
void trackerBridging()
{
    constexpr int kGapStart = 30 + 150;
    // D4 (62) at -25 dB, then `gap` hops without a pitch at `gapLevel`, then `after` at -25 dB
    auto scenario = [] (const vm::Settings& s, int gap, float gapLevel, float after = 62.0f,
                        bool gapVoiced = false, double rate = 750.0)
    {
        T2Rig r (rate);
        r.s = s;
        for (int i = 0; i < 30; ++i) r.feed (t2Silent());
        for (int i = 0; i < 150; ++i) r.feed (t2Voiced (62.0f, -25.0f));
        for (int i = 0; i < gap; ++i) r.feed (gapVoiced ? t2Voiced (62.0f, gapLevel) : t3Unvoiced (gapLevel));
        for (int i = 0; i < 150; ++i) r.feed (t2Voiced (after, -25.0f));
        for (int i = 0; i < 30; ++i) r.feed (t2Silent());
        r.shutdown();
        return r.all;
    };
    auto withBridge = [] (float ms, int lowest = 34)
    {
        vm::Settings s = baseSettings();
        s.bridgeMs = ms;
        s.lowestNote = lowest;
        return s;
    };

    {   // a gap shorter than the bridge time changes nothing in the event stream
        struct Case { float ms; int lowest, gap; };
        for (const Case c : { Case { 30.0f, 34, 30 }, Case { 30.0f, 55, 20 }, Case { 150.0f, 55, 100 }, Case { 200.0f, 34, 140 } })
        {
            const vm::Settings s = withBridge (c.ms, c.lowest);
            const Events gapped = scenario (s, c.gap, -25.0f);
            const Events plain  = scenario (s, c.gap, -25.0f, 62.0f, true);
            check (t2Same (gapped, plain), "bridge %.0f ms, lowest %d: a %d-hop gap changed the events", c.ms, c.lowest, c.gap);
            check (t2CountOn (gapped, 62) == 1, "bridge %.0f ms: %d note-ons through a %d-hop gap, expected 1",
                   c.ms, t2CountOn (gapped, 62), c.gap);
        }
    }
    {   // a longer gap ends the note, at the expected time
        std::string list;
        struct Case { float ms; int lowest; };
        for (const Case c : { Case { 0.0f, 55 }, Case { 30.0f, 55 }, Case { 150.0f, 55 },
                              Case { 0.0f, 34 }, Case { 30.0f, 34 }, Case { 60.0f, 34 }, Case { 150.0f, 34 }, Case { 200.0f, 34 } })
        {
            const int hold = t3ExpectedHold (c.ms, c.lowest);
            const Events ev = scenario (withBridge (c.ms, c.lowest), hold + 40, -25.0f);
            const int off = t2FirstOff (ev);
            const int expected = kGapStart + hold; // the (hold + 1)th hop without a pitch
            check (off >= expected - 1 && off <= expected + 1,
                   "bridge %.0f ms, lowest %d: note-off at hop %d, expected %d (%d hops after the gap began)",
                   c.ms, c.lowest, off, expected, hold);
            char buf[96];
            std::snprintf (buf, sizeof (buf), "%.0f ms/low %d -> %.1f ms; ", c.ms, c.lowest, (off - kGapStart) / 0.75);
            list += buf;
        }
        info ("note ends after a gap without a pitch: %s", list.c_str());
        info ("smallest bridge at 750 hops/s: %d hops (%.1f ms) with lowest note 34, %d hops (%.1f ms) with lowest note 55",
              t3ExpectedHold (0.0f, 34), t3ExpectedHold (0.0f, 34) / 0.75, t3ExpectedHold (0.0f, 55), t3ExpectedHold (0.0f, 55) / 0.75);
    }
    {   // the same pitch back inside the time: no new note-on, no note-off before the end
        const Events ev = scenario (withBridge (150.0f), 100, -25.0f);
        check (t2CountOn (ev, 62) == 1, "bridge 150 ms: the same pitch after 133 ms gave %d note-ons", t2CountOn (ev, 62));
        check (t2FirstOff (ev) >= kGapStart + 100 + 150, "bridge 150 ms: note-off at hop %d inside the note", t2FirstOff (ev));
        // ...and with 0 the same gap ends it
        const Events zero = scenario (withBridge (0.0f, 55), 100, -25.0f);
        check (t2FirstOff (zero) >= 0 && t2FirstOff (zero) < kGapStart + 100, "bridge 0 ms: a 133 ms gap did not end the note");
    }
    {   // a different pitch back inside the time: a legato change of note
        const Events ev = scenario (withBridge (150.0f), 60, -25.0f, 65.0f);
        const auto ons = t2NoteOns (ev);
        check (ons.size() == 2 && ons[1].note == 65, "bridge 150 ms, 65 after the gap: %d note-ons", (int) ons.size());
        if (ons.size() == 2)
        {
            check (ons[1].hop >= kGapStart + 60 && ons[1].hop <= kGapStart + 60 + 6,
                   "bridge 150 ms: note 65 at hop %d, expected right after the gap (hop %d)", ons[1].hop, kGapStart + 60);
            check (t2FirstOff (ev) == ons[1].hop, "bridge 150 ms: 62 ended at hop %d, not at the change to 65 (hop %d)",
                   t2FirstOff (ev), ons[1].hop);
            // legato order: new note first, then the old one off
            int iOn = -1, iOff = -1;
            for (size_t i = 0; i < ev.size(); ++i)
            {
                if (stIsOn (ev[i]) && ev[i].bytes[1] == 65 && iOn < 0) iOn = (int) i;
                if (stIsOff (ev[i]) && ev[i].bytes[1] == 62 && iOff < 0) iOff = (int) i;
            }
            check (iOn >= 0 && iOff > iOn, "bridge 150 ms: 65 is not a legato change (on %d, off %d)", iOn, iOff);
        }
    }
    {   // level below Out during the gap ends the note at once, whatever the bridge time
        for (float ms : { 0.0f, 30.0f, 200.0f })
        {
            const Events ev = scenario (withBridge (ms), 100, -60.0f);
            const int off = t2FirstOff (ev);
            check (off >= kGapStart && off <= kGapStart + 2, "bridge %.0f ms, gap below Out: note-off at hop %d, expected %d..%d",
                   ms, off, kGapStart, kGapStart + 2);
        }
    }
    {   // default equals explicit 30 ms, at several hop rates and gap lengths
        vm::Settings def = baseSettings(), expl = baseSettings();
        expl.bridgeMs = 30.0f;
        bool same = true;
        for (double rate : { 750.0, 747.457, 300.0, 60.0 })
            for (int gap : { 1, 5, 20, 33, 34, 35, 60 })
                for (float level : { -25.0f, -48.0f, -60.0f })
                    same = same && t2Same (scenario (def, gap, level, 62.0f, false, rate), scenario (expl, gap, level, 62.0f, false, rate))
                                && t2Same (scenario (def, gap, level, 66.0f, false, rate), scenario (expl, gap, level, 66.0f, false, rate));
        check (same, "default bridging differs from explicit 30 ms");
    }
    {   // values outside the range are clamped
        check (t2Same (scenario (withBridge (-50.0f, 55), 60, -25.0f), scenario (withBridge (0.0f, 55), 60, -25.0f)),
               "bridge -50 ms is not treated as 0");
        check (t2Same (scenario (withBridge (900.0f), 200, -25.0f), scenario (withBridge (200.0f), 200, -25.0f)),
               "bridge 900 ms is not treated as 200");
    }
}

//==============================================================================
// Jump guard: a wide leap right after a note began must last before it is believed.
void trackerJumpGuard()
{
    // Note `from` (fresh, or reached legato from `via` when via > 0), `before`
    // hops after its first voiced hop a leap to `to` for `strayHops`, then back
    // to `from`.
    struct Run { Events ev; int leapHop; };
    auto scenario = [] (const vm::Settings& s, float from, float to, int before, int strayHops, float via = 0.0f)
    {
        T2Rig r;
        r.s = s;
        for (int i = 0; i < 30; ++i) r.feed (t2Silent());
        if (via > 0.0f)
            for (int i = 0; i < 150; ++i) r.feed (t2Voiced (via, -20.0f));
        for (int i = 0; i < before; ++i) r.feed (t2Voiced (from, -20.0f));
        const int leapHop = r.hop;
        for (int i = 0; i < strayHops; ++i) r.feed (t2Voiced (to, -20.0f));
        for (int i = 0; i < 150; ++i) r.feed (t2Voiced (from, -20.0f));
        for (int i = 0; i < 30; ++i) r.feed (t2Silent());
        r.shutdown();
        return Run { r.all, leapHop };
    };
    auto guarded = [] (int interval = 7, float windowMs = 80.0f, float lockMs = 50.0f)
    {
        vm::Settings s = baseSettings();
        s.jumpGuard = true;
        s.jumpIntervalSemis = interval;
        s.jumpWindowMs = windowMs;
        s.jumpLockMs = lockMs;
        return s;
    };
    const int lockHops = (int) std::lround (50.0 * 0.75); // 38
    auto delayOf = [] (const Run& run, int note)
    {
        for (const T2On& on : t2NoteOns (run.ev))
            if (on.note == note && on.hop >= run.leapHop)
                return on.hop - run.leapHop;
        return -1;
    };

    // fresh note and a note reached by a legato change (57 -> 62, 5 semitones, outside any window)
    for (float via : { 0.0f, 57.0f })
    {
        const char* how = via > 0.0f ? "legato note" : "fresh note";
        for (float to : { 69.0f, 55.0f, 78.0f, 46.0f })   // fifth up and down, tenth up and down
        {
            const vm::Settings on = guarded(), off = baseSettings();
            const int toNote = (int) to;
            {   // shorter than the lock time: no events at all (same as no leap)
                for (int stray : { 3, 20, lockHops - 1 })
                {
                    const Run g = scenario (on, 62.0f, to, 20, stray, via);
                    const Run none = scenario (on, 62.0f, 62.0f, 20, stray, via);
                    check (t2Same (g.ev, none.ev), "%s, %d hops of %d: guarded leap produced events", how, stray, toNote);
                    if (stray >= 5)
                        check (t2CountOn (scenario (off, 62.0f, to, 20, stray, via).ev, toNote) == 1,
                               "%s, %d hops of %d: without the guard the leap is not a note (test not sensitive)", how, stray, toNote);
                }
            }
            {   // longer than the lock time: the new note, about the lock time after the leap began
                const Run g = scenario (on, 62.0f, to, 20, 80, via);
                const int d = delayOf (g, toNote);
                check (t2CountOn (g.ev, toNote) == 1, "%s, 80 hops of %d: %d note-ons, expected 1", how, toNote, t2CountOn (g.ev, toNote));
                check (d >= lockHops - 2 && d <= lockHops + 1, "%s, leap to %d: note-on %d hops after the leap, expected about %d",
                       how, toNote, d, lockHops);
                const int dOff = delayOf (scenario (off, 62.0f, to, 20, 80, via), toNote);
                if (to == 69.0f)
                    info ("%s, wanted leap 62 -> 69 (lock 50 ms): note-on %d hops (%.1f ms) after the leap began, %d hops (%.1f ms) without the guard",
                          how, d, d / 0.75, dOff, dOff / 0.75);
            }
        }
        {   // a leap smaller than the interval is not delayed
            const vm::Settings on = guarded(), off = baseSettings();
            for (float to : { 67.0f, 57.0f, 68.0f })
                for (int stray : { 3, 20, 80 })
                    check (t2Same (scenario (on, 62.0f, to, 20, stray, via).ev, scenario (off, 62.0f, to, 20, stray, via).ev),
                           "%s: a leap of %.0f semitones (below 7) was changed by the guard", how, std::abs (to - 62.0f));
        }
        {   // a leap after the window is not delayed
            const vm::Settings on = guarded(), off = baseSettings();
            for (float to : { 69.0f, 55.0f, 78.0f })
                for (int stray : { 80, 150 }) // the way back comes after the new note's own window
                    check (t2Same (scenario (on, 62.0f, to, 75, stray, via).ev, scenario (off, 62.0f, to, 75, stray, via).ev),
                           "%s: a leap to %.0f 100 ms after the start was changed by the guard", how, to);
            // The leap itself is a change of note, so a quick way back is a leap
            // inside the new note's window and is held for the lock time.
            const Run quick = scenario (on, 62.0f, 69.0f, 75, 20, via);
            const Run quickOff = scenario (off, 62.0f, 69.0f, 75, 20, via);
            const auto ons = t2NoteOns (quick.ev), onsOff = t2NoteOns (quickOff.ev);
            const size_t n = ons.size();
            check (n >= 2 && onsOff.size() == n && ons[n - 2].note == 69 && ons[n - 2].hop == onsOff[n - 2].hop,
                   "%s: a leap after the window was delayed", how);
            if (n >= 2 && onsOff.size() == n)
            {
                const int back = ons[n - 1].hop - (quick.leapHop + 20), backOff = onsOff[n - 1].hop - (quick.leapHop + 20);
                check (ons[n - 1].note == 62 && back >= lockHops - 2 && back <= lockHops + 1,
                       "%s: the way back 27 ms after a leap came %d hops after the return, expected about %d", how, back, lockHops);
                if (via == 0.0f)
                    info ("leap after the window: not delayed; the way back 27 ms later is a leap inside the new note's window: "
                          "%.1f ms with the guard, %.1f ms without", back / 0.75, backOff / 0.75);
            }
            // and the window's edge: 40 ms window, leaps at about 27 ms (guarded) and 67 ms (not)
            const vm::Settings w40 = guarded (7, 40.0f);
            check (t2CountOn (scenario (w40, 62.0f, 69.0f, 20, 20, via).ev, 69) == 0, "%s: 40 ms window, leap at 27 ms not guarded", how);
            check (t2CountOn (scenario (w40, 62.0f, 69.0f, 50, 20, via).ev, 69) == 1, "%s: 40 ms window, leap at 67 ms guarded", how);
        }
    }

    {   // guard off: identical to today, whatever the other jump settings
        vm::Settings off = baseSettings();
        off.jumpGuard = false;
        off.jumpIntervalSemis = 5;
        off.jumpWindowMs = 200.0f;
        off.jumpLockMs = 150.0f;
        bool same = true;
        for (float to : { 69.0f, 55.0f, 74.0f, 50.0f, 64.0f })
            for (int before : { 5, 20, 75 })
                for (int stray : { 2, 6, 20, 37, 80 })
                    same = same && t2Same (scenario (off, 62.0f, to, before, stray).ev, scenario (baseSettings(), 62.0f, to, before, stray).ev)
                                && t2Same (scenario (off, 62.0f, to, before, stray, 57.0f).ev, scenario (baseSettings(), 62.0f, to, before, stray, 57.0f).ev);
        check (same, "jump guard off differs from the default");
    }
    {   // a new bow stroke out of silence is not delayed (the note before has ended)
        auto twoStrokes = [] (const vm::Settings& s)
        {
            T2Rig r;
            r.s = s;
            for (int i = 0; i < 30; ++i) r.feed (t2Silent());
            for (int i = 0; i < 20; ++i) r.feed (t2Voiced (62.0f, -20.0f));
            for (int i = 0; i < 10; ++i) r.feed (t2Silent());
            for (int i = 0; i < 100; ++i) r.feed (t2Voiced (74.0f, -20.0f));
            r.shutdown();
            return r.all;
        };
        check (t2Same (twoStrokes (guarded()), twoStrokes (baseSettings())), "a new stroke after silence was delayed by the guard");
    }
    {   // octave leap with Z = 12: the jump guard and the octave sustain guard, the longer time wins
        struct Case { bool jump; bool octave; float octaveMs; int expected; const char* name; };
        const int oct11 = (int) std::lround (11.0 * 0.75), oct100 = (int) std::lround (100.0 * 0.75);
        const Case cases[] = {
            { false, true,  11.0f,  oct11,    "octave guard 11 ms only" },
            { true,  true,  11.0f,  lockHops, "jump guard 50 ms + octave guard 11 ms" },
            { true,  false, 11.0f,  lockHops, "jump guard 50 ms, octave guard off" },
            { true,  true,  100.0f, oct100,   "jump guard 50 ms + octave guard 100 ms" },
        };
        for (const Case& c : cases)
        {
            vm::Settings s = guarded (12);
            s.jumpGuard = c.jump;
            s.octaveSustainGuard = c.octave;
            s.octaveSustainMs = c.octaveMs;
            const Run run = scenario (s, 57.0f, 69.0f, 20, 120);
            const int d = delayOf (run, 69);
            check (d >= c.expected - 2 && d <= c.expected + 1, "%s: octave believed after %d hops, expected about %d", c.name, d, c.expected);
            const Run shortRun = scenario (s, 57.0f, 69.0f, 20, c.expected - 3);
            check (t2CountOn (shortRun.ev, 69) == 0, "%s: an octave of %d hops became a note", c.name, c.expected - 3);
        }
        // Z = 12 leaves a fifth alone
        check (t2Same (scenario (guarded (12), 62.0f, 69.0f, 20, 20).ev, scenario (baseSettings(), 62.0f, 69.0f, 20, 20).ev),
               "Z = 12: a fifth was changed by the guard");
    }
    {   // nothing sent towards the stray pitch: no bend, no note while it lasts
        const Run g = scenario (guarded(), 62.0f, 69.0f, 20, 30);
        for (const vm::MidiEvent& e : g.ev)
            if (e.sampleOffset >= g.leapHop && e.sampleOffset < g.leapHop + 30)
                check (! stIsBend (e) && ! stIsNote (e), "event 0x%02X during a guarded stray at hop %d", e.bytes[0], e.sampleOffset);
    }
    {   // quantized mode as well
        vm::Settings q = guarded();
        q.bendMode = vm::BendMode::Quantized;
        vm::Settings qOff = baseSettings();
        qOff.bendMode = vm::BendMode::Quantized;
        check (t2CountOn (scenario (q, 62.0f, 69.0f, 20, 20).ev, 69) == 0, "quantized: guarded stray became a note");
        check (t2CountOn (scenario (qOff, 62.0f, 69.0f, 20, 20).ev, 69) == 1, "quantized: stray without the guard not a note");
    }
}

//==============================================================================
// The playing aids with audio through vm::Engine.
void playingAidsAudio()
{
    {   // a 20 ms stray a tenth up, right after a note start
        const std::vector<Segment> segs = {
            rest (0.15),
            tone (62, 0.05, -20.0, 0.02), tone (78, 0.02, -20.0, 0.0), tone (62, 0.35, -20.0, 0.0),
            tone (69, 0.4, -20.0, 0.0),   // a wanted leap, long after the start: not delayed
            rest (0.25),
            tone (64, 0.4, -20.0, 0.02), rest (0.25),
        };
        Synth synth (48000.0, 4401);
        const Signal sig = synth.render (segs, Timbre());
        vm::Settings off = baseSettings(), on = baseSettings();
        on.jumpGuard = true;
        const RunResult a = run (sig, off), b = run (sig, on);
        checkProtocol (a, "stray, guard off");
        checkProtocol (b, "stray, guard on");
        check (toString (noteNumbers (a)) == "62 78 62 69 64 ", "stray, guard off: got %s", describeNotes (a).c_str());
        check (toString (noteNumbers (b)) == "62 69 64 ", "stray, guard on: got %s", describeNotes (b).c_str());
        if (a.notes.size() == 5 && b.notes.size() == 3)
        {
            const double d69 = (b.notes[1].on - a.notes[3].on) * 1000.0;
            check (std::abs (d69) < 1.0, "the wanted leap to 69 moved by %.1f ms with the guard", d69);
        }
        info ("stray a tenth up: guard off %s", describeNotes (a).c_str());
        info ("                  guard on  %s", describeNotes (b).c_str());
    }
    {   // a 60 ms gap in the bow: a quiet unpitched noise in place of the tone
        Synth synth (48000.0, 4402);
        const double fs = 48000.0;
        Signal sig = synth.render ({ rest (0.15), tone (62, 0.5, -24.0, 0.02), rest (0.06), tone (62, 0.8, -24.0, 0.0), rest (0.3) }, Timbre());
        std::mt19937 rng (4403);
        std::normal_distribution<double> gauss (0.0, 1.0);
        const size_t gapFrom = (size_t) std::lround (0.65 * fs), gapTo = (size_t) std::lround (0.71 * fs);
        const double noiseRms = dbToLin (-21.0); // about -28 dB after the 5 kHz low-pass, 4 dB below the tone
        for (size_t i = gapFrom - 48; i < gapTo + 48 && i < sig.samples.size(); ++i)
        {
            const double edge = std::min (1.0, std::min ((double) (i - (gapFrom - 48)), (double) (gapTo + 48 - i)) / 96.0);
            sig.samples[i] += (float) (gauss (rng) * noiseRms * edge);
        }
        auto withBridge = [] (float ms, int lowest)
        {
            vm::Settings s = baseSettings();
            s.bridgeMs = ms;
            s.lowestNote = lowest;
            return s;
        };
        for (int lowest : { 34, 55 })
        {
            const RunResult b120 = run (sig, withBridge (120.0f, lowest), 64, true, true);
            const RunResult b0 = run (sig, withBridge (0.0f, lowest), 64, true, true);
            checkProtocol (b120, "bow gap, bridge 120");
            checkProtocol (b0, "bow gap, bridge 0");
            check (b120.notes.size() == 1 && b120.notes[0].off > 1.4, "bow gap, lowest %d, bridge 120 ms: got %s",
                   lowest, describeNotes (b120).c_str());
            if (lowest == 55)
                check (! b0.notes.empty() && b0.notes[0].off > 0.64 && b0.notes[0].off < 0.72,
                       "bow gap, lowest 55, bridge 0: the note did not end in the gap: %s", describeNotes (b0).c_str());
            // how the gap looks to the tracker
            int unvoiced = 0;
            float minLevel = 0.0f, maxLevel = -200.0f;
            for (const vm::Engine::TraceRow& row : b120.trace)
                if (row.timeSec > 0.65 && row.timeSec < 0.72)
                {
                    unvoiced += row.voiced ? 0 : 1;
                    minLevel = std::min (minLevel, row.levelDb);
                    maxLevel = std::max (maxLevel, row.levelDb);
                }
            info ("bow gap 60 ms, lowest %d: %d hops without a pitch, level %.1f .. %.1f dB; bridge 120: %s; bridge 0: %s",
                  lowest, unvoiced, minLevel, maxLevel, describeNotes (b120).c_str(), describeNotes (b0).c_str());
        }
    }
}
