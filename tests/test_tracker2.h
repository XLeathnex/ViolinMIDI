// ViolinMIDI - tests for the newer NoteTracker settings: separate note-off
// threshold (gateOffDb), switchable octave guards, articulation velocity,
// the controller that carries the bow loudness and its extra smoothing.
//
// Included by test_main.cpp after test_stage.h, inside that file's anonymous
// namespace (check, info, baseSettings, Events are available). Most groups
// drive NoteTracker directly, hop by hop; the sample offset of every event is
// the hop index, so times can be read straight from the events.
#pragma once

//==============================================================================
struct T2Rig
{
    vm::NoteTracker trk;
    vm::Settings s;
    Events all;
    int hop = 0;

    explicit T2Rig (double hopRate = 750.0) { trk.prepare (hopRate); }

    void feed (const vm::NoteTracker::Input& in)
    {
        Events out;
        trk.process (in, s, hop, out);
        all.insert (all.end(), out.begin(), out.end());
        ++hop;
    }

    void shutdown()
    {
        Events out;
        trk.shutdown (hop, out);
        all.insert (all.end(), out.begin(), out.end());
    }
};

inline vm::NoteTracker::Input t2Voiced (float pitch, float levelDb, bool doubt = false, float hint = 0.0f)
{
    vm::NoteTracker::Input i;
    i.voiced = true;
    i.pitch = pitch;
    i.clarity = 0.95f;
    i.hintClarity = hint;
    i.octaveDoubt = doubt;
    i.levelDb = levelDb;
    return i;
}

inline vm::NoteTracker::Input t2Silent()
{
    vm::NoteTracker::Input i;
    i.levelDb = -120.0f;
    return i;
}

inline bool t2Same (const Events& a, const Events& b)
{
    if (a.size() != b.size())
        return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (a[i].sampleOffset != b[i].sampleOffset || std::memcmp (a[i].bytes, b[i].bytes, 3) != 0)
            return false;
    return true;
}

struct T2On { int hop, note, velocity; };

inline std::vector<T2On> t2NoteOns (const Events& ev)
{
    std::vector<T2On> v;
    for (const vm::MidiEvent& e : ev)
        if (stIsOn (e))
            v.push_back ({ e.sampleOffset, e.bytes[1], e.bytes[2] });
    return v;
}

inline int t2FirstOff (const Events& ev, int fromHop = 0)
{
    for (const vm::MidiEvent& e : ev)
        if (stIsOff (e) && e.sampleOffset >= fromHop)
            return e.sampleOffset;
    return -1;
}

inline int t2CountOn (const Events& ev, int note)
{
    int n = 0;
    for (const vm::MidiEvent& e : ev)
        if (stIsOn (e) && e.bytes[1] == note)
            ++n;
    return n;
}

//==============================================================================
// gateDb ("in") starts notes, gateOffDb ("out") ends them.
void trackerGateInOut()
{
    {   // a note sinking into the zone between In and Out keeps sounding, ends below Out
        T2Rig r;
        r.s = baseSettings();
        r.s.gateDb = -40.0f;
        r.s.gateOffDb = -60.0f;
        for (int i = 0; i < 100; ++i) r.feed (t2Silent());
        for (int i = 0; i < 200; ++i) r.feed (t2Voiced (62.0f, -30.0f));
        for (int i = 0; i < 300; ++i) r.feed (t2Voiced (62.0f, -55.0f));   // below In, above Out
        for (int i = 0; i < 20; ++i)  r.feed (t2Voiced (62.0f, -62.0f));   // below Out
        for (int i = 0; i < 50; ++i)  r.feed (t2Silent());
        r.shutdown();
        const auto ons = t2NoteOns (r.all);
        check (ons.size() == 1, "sink zone: expected one note, got %d", (int) ons.size());
        const int off = t2FirstOff (r.all);
        check (off >= 600 && off <= 603, "sink zone: note-off at hop %d, expected right after hop 600 (level below Out)", off);
    }
    {   // a level flickering around In, with Out well below: one note, not many
        auto flicker = [] (float gateOff)
        {
            T2Rig r;
            r.s = baseSettings();
            r.s.gateDb = -42.0f;
            r.s.gateOffDb = gateOff;
            for (int i = 0; i < 50; ++i) r.feed (t2Silent());
            for (int i = 0; i < 1200; ++i)
                r.feed (t2Voiced (62.0f, (i / 6) % 2 == 0 ? -41.5f : -48.5f));
            r.shutdown();
            return (int) t2NoteOns (r.all).size();
        };
        const int wide = flicker (-60.0f), old = flicker (-48.0f);
        check (wide == 1, "flicker with Out 18 dB below In: %d notes, expected 1", wide);
        check (old >= 2, "flicker with Out only 6 dB below In should chatter (test not sensitive): %d notes", old);
        info ("flicker -41.5 / -48.5 dB around In -42: %d note(s) with Out -60, %d with Out -48", wide, old);
    }
    {   // Out above In behaves as Out = In
        auto ramp = [] (float gateOff)
        {
            T2Rig r;
            r.s = baseSettings();
            r.s.gateDb = -45.0f;
            r.s.gateOffDb = gateOff;
            for (int i = 0; i < 50; ++i) r.feed (t2Silent());
            for (int i = 0; i < 200; ++i) r.feed (t2Voiced (69.0f, -30.0f));
            for (int i = 0; i < 800; ++i) r.feed (t2Voiced (69.0f, -30.0173f - 0.05f * (float) i));
            r.shutdown();
            return r.all;
        };
        const Events above = ramp (-30.0f), equal = ramp (-45.0f);
        check (t2Same (above, equal), "Out above In does not behave as Out = In");
        const int off = t2FirstOff (equal);
        check (off >= 250 + 299 && off <= 250 + 302, "Out = In: note-off at hop %d, expected where the ramp crosses -45 dB", off);
    }
    {   // defaults: the old rule, note ends 6 dB below the gate
        T2Rig r;
        r.s = baseSettings();
        int firstBelow = -1;
        for (int i = 0; i < 50; ++i) r.feed (t2Silent());
        for (int i = 0; i < 200; ++i) r.feed (t2Voiced (69.0f, -30.0f));
        for (int i = 0; i < 800; ++i)
        {
            const float level = -30.0173f - 0.05f * (float) i;
            if (firstBelow < 0 && level < -51.0f)
                firstBelow = r.hop;
            r.feed (t2Voiced (69.0f, level));
        }
        r.shutdown();
        const int off = t2FirstOff (r.all);
        check (off == firstBelow + 1, "defaults: note-off at hop %d, the old 6 dB rule gives %d", off, firstBelow + 1);

        // the same with audio, default settings versus explicit -45 / -51
        std::vector<Segment> segs = { rest (0.1) };
        for (int p : { 45, 57, 64, 76 })
        {
            Segment t = tone (p, 0.7, -24.0, 0.02);
            t.decayDbPerSec = 45.0;
            segs.push_back (t);
            segs.push_back (rest (0.2));
        }
        Synth synth (48000.0, 2201);
        const Signal sig = synth.render (segs, Timbre());
        vm::Settings def;
        vm::Settings expl;
        expl.gateDb = -45.0f;
        expl.gateOffDb = -51.0f;
        const RunResult a = run (sig, def), b = run (sig, expl);
        checkProtocol (a, "gate defaults");
        bool same = a.events.size() == b.events.size();
        for (size_t i = 0; same && i < a.events.size(); ++i)
            same = a.events[i].sample == b.events[i].sample
                && std::memcmp (a.events[i].event.bytes, b.events[i].event.bytes, 3) == 0;
        check (same, "default settings and explicit -45 / -51 give different MIDI");
        check (a.notes.size() == 4, "fading notes: got %s", describeNotes (a).c_str());
    }
}

//==============================================================================
// Octave protection at the start of a note and inside it.
void trackerOctaveGuards()
{
    // a steady A3 (57) with an octave-up estimate (69) lasting `jumpHops` in the middle
    auto sustain = [] (const vm::Settings& s, int jumpHops, double rate = 750.0, float hint = 0.0f, float jumpTo = 69.0f)
    {
        T2Rig r (rate);
        r.s = s;
        for (int i = 0; i < 30; ++i) r.feed (t2Silent());
        for (int i = 0; i < 150; ++i) r.feed (t2Voiced (57.0f, -20.0f, false, hint));
        for (int i = 0; i < jumpHops; ++i) r.feed (t2Voiced (jumpTo, -20.0f, false, hint));
        for (int i = 0; i < 150; ++i) r.feed (t2Voiced (57.0f, -20.0f, false, hint));
        for (int i = 0; i < 30; ++i) r.feed (t2Silent());
        r.shutdown();
        return r.all;
    };
    // an estimate flagged as a possible overtone from the first hop on
    auto attack = [] (const vm::Settings& s, double rate = 750.0)
    {
        T2Rig r (rate);
        r.s = s;
        for (int i = 0; i < 20; ++i) r.feed (t2Silent());
        for (int i = 0; i < 200; ++i) r.feed (t2Voiced (69.0f, -20.0f, true));
        r.shutdown();
        return r.all;
    };
    auto onsetDelay = [] (const Events& ev)
    {
        const auto ons = t2NoteOns (ev);
        return ons.empty() ? -1 : ons[0].hop - 20;
    };

    {   // defaults identical to explicit 50 / 11 ms, at several hop rates
        vm::Settings def = baseSettings();
        vm::Settings expl = baseSettings();
        expl.octaveAttackGuard = true;
        expl.octaveAttackMs = 50.0f;
        expl.octaveSustainGuard = true;
        expl.octaveSustainMs = 11.0f;
        bool same = true;
        for (double rate : { 750.0, 747.457, 300.0, 60.0 })
        {
            same = same && t2Same (attack (def, rate), attack (expl, rate));
            for (int jump : { 3, 6, 9, 14, 40 })
                same = same && t2Same (sustain (def, jump, rate), sustain (expl, jump, rate));
        }
        check (same, "default octave guards differ from explicit 50 / 11 ms");
    }
    {   // sustain guard: shorter than the set time is ignored, longer becomes a new note
        struct Case { float ms; int shortHops, longHops; };
        for (const Case c : { Case { 20.0f, 8, 30 }, Case { 80.0f, 45, 100 } })
        {
            vm::Settings s = baseSettings();
            s.octaveSustainMs = c.ms;
            const Events shortJump = sustain (s, c.shortHops), longJump = sustain (s, c.longHops);
            check (t2CountOn (shortJump, 69) == 0, "sustain guard %.0f ms: a %.0f ms octave jump became a note",
                   c.ms, c.shortHops / 0.75);
            check (t2CountOn (longJump, 69) == 1, "sustain guard %.0f ms: a %.0f ms octave jump gave %d notes on 69, expected 1",
                   c.ms, c.longHops / 0.75, t2CountOn (longJump, 69));
            for (const T2On& on : t2NoteOns (longJump))
                if (on.note == 69)
                {
                    const double afterMs = (on.hop - 180) / 0.75;
                    check (afterMs >= c.ms - 2.0 && afterMs <= c.ms + 6.0,
                           "sustain guard %.0f ms: the octave was believed after %.1f ms", c.ms, afterMs);
                }
        }
    }
    {   // guard off: an octave is an interval like any other and is accepted quickly
        vm::Settings on = baseSettings(), off = baseSettings();
        off.octaveSustainGuard = false;
        const Events guarded = sustain (on, 6), unguarded = sustain (off, 6);
        check (t2CountOn (guarded, 69) == 0, "default sustain guard: a 6-hop octave jump became a note");
        check (t2CountOn (unguarded, 69) == 1, "sustain guard off: a 6-hop octave jump gave %d notes on 69, expected 1",
               t2CountOn (unguarded, 69));
        for (const T2On& o : t2NoteOns (unguarded))
            if (o.note == 69)
                check (o.hop - 180 <= 4, "sustain guard off: octave accepted only after %d hops", o.hop - 180);

        // off also switches the subharmonic guard off: an octave below while the
        // note's own period is still clearly present
        vm::Settings s69on = baseSettings(), s69off = baseSettings();
        s69off.octaveSustainGuard = false;
        auto down = [&] (const vm::Settings& s)
        {
            T2Rig r;
            r.s = s;
            for (int i = 0; i < 30; ++i) r.feed (t2Silent());
            for (int i = 0; i < 150; ++i) r.feed (t2Voiced (69.0f, -20.0f, false, 0.9f));
            for (int i = 0; i < 40; ++i) r.feed (t2Voiced (57.0f, -20.0f, false, 0.9f));
            for (int i = 0; i < 150; ++i) r.feed (t2Voiced (69.0f, -20.0f, false, 0.9f));
            r.shutdown();
            return r.all;
        };
        check (t2CountOn (down (s69on), 57) == 0, "subharmonic guard on: an octave-below estimate became a note");
        check (t2CountOn (down (s69off), 57) == 1, "sustain guard off: the octave below was not accepted");
    }
    {   // attack guard: with octaveDoubt the start waits for the set time
        auto delayFor = [&] (bool guard, float ms)
        {
            vm::Settings s = baseSettings();
            s.octaveAttackGuard = guard;
            s.octaveAttackMs = ms;
            return onsetDelay (attack (s));
        };
        const int dDefault = onsetDelay (attack (baseSettings()));
        const int dOff = delayFor (false, 50.0f), dZero = delayFor (true, 0.0f);
        const int d20 = delayFor (true, 20.0f), d50 = delayFor (true, 50.0f), d60 = delayFor (true, 60.0f);
        check (dDefault == d50, "attack guard: default %d hops, explicit 50 ms %d hops", dDefault, d50);
        check (dOff >= 0 && dOff <= 3 && dZero == dOff, "attack guard off / 0 ms: start after %d / %d hops, expected no wait", dOff, dZero);
        check (d20 >= 13 && d20 <= 15, "attack guard 20 ms: start after %d hops (%.1f ms)", d20, d20 / 0.75);
        check (d50 >= 36 && d50 <= 38, "attack guard 50 ms: start after %d hops (%.1f ms)", d50, d50 / 0.75);
        check (d60 >= 43 && d60 <= 45, "attack guard 60 ms: start after %d hops (%.1f ms)", d60, d60 / 0.75);
        info ("start with octave doubt: off %.1f ms, 20 ms -> %.1f, 50 ms -> %.1f, 60 ms -> %.1f",
              dOff / 0.75, d20 / 0.75, d50 / 0.75, d60 / 0.75);
    }
}

//==============================================================================
// Velocity from the way a note begins.
void trackerArticulation()
{
    const int pitches[] = { 45, 57, 69, 81, 93 };
    const double levels[] = { -14.0, -28.0 };

    // plucked: 2 ms attack and a decay (as the pizzicato group), G3 and up (see
    // the low strings below); bowed: 20 and 40 ms bow attack
    const int pluckPitches[] = { 57, 64, 69, 81, 93 };
    std::vector<Segment> plucked = { rest (0.1) }, bowed = { rest (0.1) };
    for (double level : levels)
    {
        for (int p : pluckPitches)
        {
            Segment pl = tone (p, 0.4, level, 0.002);
            pl.decayDbPerSec = 70.0;
            plucked.push_back (pl);
            plucked.push_back (rest (0.15));
        }
        for (int p : pitches)
            for (double a : { 0.02, 0.04 })
            {
                bowed.push_back (tone (p, 0.4, level, a));
                bowed.push_back (rest (0.15));
            }
    }
    Timbre pluckTimbre;
    pluckTimbre.bowNoiseDb = -60.0;
    Synth synthP (48000.0, 3301), synthB (48000.0, 3302);
    const Signal sigP = synthP.render (plucked, pluckTimbre);
    const Signal sigB = synthB.render (bowed, Timbre());

    for (int kind = 0; kind < 2; ++kind)
    {
        const Signal& sig = kind == 0 ? sigP : sigB;
        const char* name = kind == 0 ? "plucked" : "bowed";
        const size_t expected = kind == 0 ? 10 : 20;
        vm::Settings off = baseSettings(), gentle = baseSettings(), strong = baseSettings();
        gentle.articulation = vm::Articulation::Gentle;
        strong.articulation = vm::Articulation::Strong;
        const RunResult rDef = run (sig, baseSettings()), rOff = run (sig, off);
        const RunResult rG = run (sig, gentle), rS = run (sig, strong);
        checkProtocol (rS, name);
        check (rOff.notes.size() == expected, "%s: expected %d notes, got %s", name, (int) expected, describeNotes (rOff).c_str());

        // Off: exactly as before. Gentle / Strong: same events at the same times, only velocities differ.
        bool sameOff = rDef.events.size() == rOff.events.size();
        for (size_t i = 0; sameOff && i < rDef.events.size(); ++i)
            sameOff = rDef.events[i].sample == rOff.events[i].sample
                   && std::memcmp (rDef.events[i].event.bytes, rOff.events[i].event.bytes, 3) == 0;
        check (sameOff, "%s: articulation Off differs from the default", name);
        for (const RunResult* r : { &rG, &rS })
        {
            bool sameTimes = r->events.size() == rOff.events.size();
            for (size_t i = 0; sameTimes && i < r->events.size(); ++i)
                sameTimes = r->events[i].sample == rOff.events[i].sample
                         && std::memcmp (r->events[i].event.bytes, rOff.events[i].event.bytes, 2) == 0;
            check (sameTimes, "%s: articulation changes the timing or the messages, not only velocities", name);
        }
        if (rS.notes.size() != expected || rG.notes.size() != expected || rOff.notes.size() != expected)
            continue;

        std::string list;
        for (size_t i = 0; i < expected; ++i)
        {
            const int vOff = rOff.notes[i].velocity, vG = rG.notes[i].velocity, vS = rS.notes[i].velocity;
            if (kind == 0)
                check (vS >= 95 && vS <= 127, "plucked note %d: Strong velocity %d, expected 95..127", rS.notes[i].note, vS);
            else
                check (vS >= 45 && vS <= 65, "bowed note %d: Strong velocity %d, expected 45..65", rS.notes[i].note, vS);
            check (vG >= std::min (vOff, vS) && vG <= std::max (vOff, vS) && std::abs (2 * vG - vOff - vS) <= 1,
                   "%s note %d: Gentle %d is not the mean of Off %d and Strong %d", name, rS.notes[i].note, vG, vOff, vS);
            list += std::to_string (vS) + " ";
        }
        // louder = higher inside the range (first half of the notes is the loud level)
        for (size_t i = 0; i < expected / 2; ++i)
            check (rS.notes[i].velocity > rS.notes[i + expected / 2].velocity,
                   "%s: louder note %d has velocity %d, softer one %d", name, rS.notes[i].note,
                   rS.notes[i].velocity, rS.notes[i + expected / 2].velocity);
        info ("%s, Strong: %s", name, list.c_str());

        vm::Settings fixed = strong;
        fixed.dynamics = vm::DynamicsMode::Fixed;
        fixed.fixedVelocity = 96;
        const RunResult rF = run (sig, fixed);
        for (const Note& n : rF.notes)
            check (n.velocity == 96, "%s: Fixed dynamics with articulation sent velocity %d", name, n.velocity);
    }

    {   // a noise floor only 30 dB below the note: the band starts above the floor
        std::vector<Segment> segs = { rest (0.1) };
        for (int p : { 62, 76 })
        {
            Segment pl = tone (p, 0.4, -30.0, 0.002);
            pl.decayDbPerSec = 70.0;
            segs.push_back (pl);
            segs.push_back (rest (0.15));
            segs.push_back (tone (p, 0.4, -30.0, 0.03));
            segs.push_back (rest (0.15));
        }
        Timbre t;
        t.bowNoiseDb = -60.0;
        Synth synth (48000.0, 3304);
        const Signal sig = synth.render (segs, t, -62.0);
        vm::Settings s = baseSettings();
        s.articulation = vm::Articulation::Strong;
        const RunResult r = run (sig, s);
        checkProtocol (r, "noisy articulation");
        check (r.notes.size() == 4, "noise floor -62 dB: got %s", describeNotes (r).c_str());
        // With a high floor the band is narrower and a pluck may read between
        // the ranges; it must still be clearly harder than the bowed start.
        if (r.notes.size() == 4)
            for (size_t i = 0; i < 4; i += 2)
            {
                const int vp = r.notes[i].velocity, vb = r.notes[i + 1].velocity;
                check (vb >= 45 && vb <= 65, "noise floor -62 dB: bowed note %d got velocity %d", r.notes[i + 1].note, vb);
                check (vp >= vb + 15, "noise floor -62 dB: plucked note %d got velocity %d, bowed %d", r.notes[i].note, vp, vb);
            }
        info ("noise floor -62 dB, notes at -30 dB: %s", describeNotes (r).c_str());
    }
    {   // low strings: the level is measured over whole periods, so a pluck reads softer (not checked)
        std::vector<Segment> segs = { rest (0.1) };
        for (int p : { 36, 43, 45, 50 })
        {
            Segment pl = tone (p, 0.4, -14.0, 0.002);
            pl.decayDbPerSec = 70.0;
            segs.push_back (pl);
            segs.push_back (rest (0.15));
        }
        Timbre t;
        t.bowNoiseDb = -60.0;
        Synth synth (48000.0, 3305);
        vm::Settings s = baseSettings();
        s.articulation = vm::Articulation::Strong;
        const RunResult r = run (synth.render (segs, t), s);
        checkProtocol (r, "low plucks");
        info ("low plucks, Strong: %s", describeNotes (r).c_str());
    }
    {   // a finger change under the bow is a soft start
        Synth synth (48000.0, 3303);
        const Signal sig = synth.render ({ rest (0.1), tone (60, 0.3, -16.0, 0.002), tone (64, 0.3, -16.0, 0.0), rest (0.2) }, Timbre());
        vm::Settings s = baseSettings();
        s.articulation = vm::Articulation::Strong;
        const RunResult r = run (sig, s);
        checkProtocol (r, "legato articulation");
        check (r.notes.size() == 2, "legato articulation: got %s", describeNotes (r).c_str());
        if (r.notes.size() == 2)
        {
            check (r.notes[0].velocity >= 95, "legato articulation: hard first start got velocity %d", r.notes[0].velocity);
            check (r.notes[1].velocity >= 45 && r.notes[1].velocity <= 65,
                   "legato articulation: finger change got velocity %d, expected 45..65", r.notes[1].velocity);
        }
    }
}

//==============================================================================
// Which controller carries the bow loudness, and how it is handed back.
namespace t2dyn
{
    struct Cc { int hop, channel, number, value; };

    inline std::vector<Cc> ccs (const Events& ev)
    {
        std::vector<Cc> v;
        for (const vm::MidiEvent& e : ev)
            if (stIsCc (e))
                v.push_back ({ e.sampleOffset, stChannel (e), e.bytes[1], e.bytes[2] });
        return v;
    }

    inline int neutral (int number) { return number == 1 ? 0 : 127; }

    /** Every controller touched on any channel ends at its neutral value. */
    inline bool allHandedBack (const std::vector<Cc>& v, std::string& why)
    {
        std::map<int, int> last;
        for (const Cc& c : v)
            last[c.channel * 128 + c.number] = c.value;
        for (const auto& kv : last)
            if (kv.second != neutral (kv.first % 128))
            {
                why = "ch " + std::to_string (kv.first / 128) + " CC " + std::to_string (kv.first % 128)
                    + " left at " + std::to_string (kv.second);
                return false;
            }
        return true;
    }

    /** A note swelling and fading; `change` may alter the settings at hop 300. */
    template <class Change>
    Events scenario (vm::Settings s, Change change)
    {
        T2Rig r;
        r.s = s;
        for (int i = 0; i < 20; ++i) r.feed (t2Silent());
        for (int i = 0; i < 600; ++i)
        {
            if (i == 280)
                change (r.s);
            const float level = i < 300 ? -40.0f + 26.0f * (float) i / 300.0f : -14.0f - 21.0f * (float) (i - 300) / 300.0f;
            r.feed (t2Voiced (62.0f, level));
        }
        for (int i = 0; i < 100; ++i) r.feed (t2Voiced (62.0f, -30.0f));
        r.shutdown();
        return r.all;
    }
}

void trackerDynamicsTarget()
{
    using namespace t2dyn;
    struct Case { vm::DynamicsTarget target; std::vector<int> numbers; const char* name; };
    const Case cases[] = {
        { vm::DynamicsTarget::Expression,       { 11 },    "Expression" },
        { vm::DynamicsTarget::ModWheel,         { 1 },     "ModWheel" },
        { vm::DynamicsTarget::Breath,           { 2 },     "Breath" },
        { vm::DynamicsTarget::ModAndExpression, { 1, 11 }, "ModAndExpression" },
    };
    auto noChange = [] (vm::Settings&) {};

    for (const Case& c : cases)
    {
        vm::Settings s = baseSettings();
        s.dynamicsTarget = c.target;
        const Events ev = scenario (s, noChange);
        const auto v = ccs (ev);
        std::map<int, int> count, maxValue;
        for (const Cc& x : v)
        {
            ++count[x.number];
            maxValue[x.number] = std::max (maxValue[x.number], x.value);
        }
        for (const auto& kv : count)
            check (std::find (c.numbers.begin(), c.numbers.end(), kv.first) != c.numbers.end(),
                   "%s: sent CC %d", c.name, kv.first);
        for (int n : c.numbers)
        {
            check (count[n] >= 10, "%s: CC %d sent only %d times", c.name, n, count[n]);
            check (maxValue[n] >= 110, "%s: CC %d never followed the loud part (max %d)", c.name, n, maxValue[n]);
        }
        std::string why;
        check (allHandedBack (v, why), "%s: after shutdown %s", c.name, why.c_str());
        check (! v.empty() && v.back().value == neutral (v.back().number), "%s: no hand-back at the end", c.name);

        if (c.target == vm::DynamicsTarget::ModAndExpression)
        {
            // CC 1 first, then CC 11, same value, same offset (the hand-back differs in value only)
            int pairs = 0;
            for (size_t i = 0; i < v.size(); ++i)
            {
                if (v[i].number != 1)
                    continue;
                const bool ok = i + 1 < v.size() && v[i + 1].number == 11 && v[i + 1].hop == v[i].hop
                             && (v[i + 1].value == v[i].value || (v[i].value == 0 && v[i + 1].value == 127));
                check (ok, "ModAndExpression: CC 1 = %d at hop %d not followed by CC 11 with the same value", v[i].value, v[i].hop);
                pairs += ok ? 1 : 0;
            }
            check (pairs >= 10, "ModAndExpression: only %d pairs", pairs);
        }
    }

    {   // default target is CC 11 exactly as before
        vm::Settings def = baseSettings(), expl = baseSettings();
        expl.dynamicsTarget = vm::DynamicsTarget::Expression;
        check (t2Same (scenario (def, noChange), scenario (expl, noChange)), "explicit Expression differs from the default");
    }

    {   // switching the target in the middle of a note hands the old controller back at once
        struct Switch { vm::DynamicsTarget from, to; int gone, goneValue; std::vector<int> after; const char* name; };
        const Switch switches[] = {
            { vm::DynamicsTarget::Expression,       vm::DynamicsTarget::ModWheel,         11, 127, { 1 },     "Expression -> ModWheel" },
            { vm::DynamicsTarget::Breath,           vm::DynamicsTarget::ModAndExpression, 2,  127, { 1, 11 }, "Breath -> ModAndExpression" },
            { vm::DynamicsTarget::ModAndExpression, vm::DynamicsTarget::Expression,       1,  0,   { 11 },    "ModAndExpression -> Expression" },
        };
        for (const Switch& sw : switches)
        {
            vm::Settings s = baseSettings();
            s.dynamicsTarget = sw.from;
            const vm::DynamicsTarget to = sw.to;
            const auto v = ccs (scenario (s, [to] (vm::Settings& x) { x.dynamicsTarget = to; }));
            const int switchHop = 20 + 280;
            bool handedBack = false;
            for (const Cc& x : v)
            {
                if (x.hop == switchHop && x.number == sw.gone && x.value == sw.goneValue)
                    handedBack = true;
                if (x.hop > switchHop)
                    check (std::find (sw.after.begin(), sw.after.end(), x.number) != sw.after.end(),
                           "%s: CC %d sent after the switch", sw.name, x.number);
            }
            check (handedBack, "%s: CC %d not handed back at the switch", sw.name, sw.gone);
            int newOnes = 0;
            for (const Cc& x : v)
                if (x.hop > switchHop && x.hop <= switchHop + 10)
                    ++newOnes;
            check (newOnes >= (int) sw.after.size(), "%s: the new controller did not take over at once", sw.name);
            std::string why;
            check (allHandedBack (v, why), "%s: after shutdown %s", sw.name, why.c_str());
        }
    }

    {   // other hand-back paths: dynamics mode change and channel change
        vm::Settings s = baseSettings();
        s.dynamicsTarget = vm::DynamicsTarget::ModAndExpression;
        auto v = ccs (scenario (s, [] (vm::Settings& x) { x.dynamics = vm::DynamicsMode::VelocityOnly; }));
        std::string why;
        check (allHandedBack (v, why), "mode change: %s", why.c_str());
        check (! v.empty() && v.back().hop == 300, "mode change: controllers sent after leaving Continuous");

        s.dynamicsTarget = vm::DynamicsTarget::ModWheel;
        v = ccs (scenario (s, [] (vm::Settings& x) { x.midiChannel = 3; }));
        check (allHandedBack (v, why), "channel change: %s", why.c_str());
        bool oldChannelBack = false, newChannelUsed = false;
        for (const Cc& x : v)
        {
            if (x.hop == 300 && x.channel == 1 && x.number == 1 && x.value == 0)
                oldChannelBack = true;
            if (x.hop > 300 && x.channel == 3 && x.number == 1)
                newChannelUsed = true;
            if (x.hop > 300)
                check (x.channel == 3, "channel change: CC %d on channel %d afterwards", x.number, x.channel);
        }
        check (oldChannelBack && newChannelUsed, "channel change: CC 1 not handed back on channel 1 or not used on 3");
    }
}

//==============================================================================
// Extra smoothing of the loudness controller.
void trackerDynamicsSmoothing()
{
    using namespace t2dyn;

    auto ccValues = [] (const Events& ev, int number)
    {
        std::vector<Cc> v;
        for (const Cc& c : ccs (ev))
            if (c.number == number)
                v.push_back (c);
        return v;
    };
    // controller value in force at `hop`
    auto valueAt = [] (const std::vector<Cc>& v, int hop)
    {
        int value = -1;
        for (const Cc& c : v)
            if (c.hop <= hop)
                value = c.value;
        return value;
    };

    auto flutter = [] (float ms)
    {
        T2Rig r;
        r.s = baseSettings();
        r.s.dynamicsSmoothingMs = ms;
        for (int i = 0; i < 20; ++i) r.feed (t2Silent());
        for (int i = 0; i < 1000; ++i)
            r.feed (t2Voiced (62.0f, -30.0f + 4.0f * (float) std::sin (2.0 * kPi * i / 30.0)));
        for (int i = 0; i < 50; ++i) r.feed (t2Silent());
        for (int i = 0; i < 300; ++i) r.feed (t2Voiced (64.0f, -20.0f));
        r.shutdown();
        return r.all;
    };

    {   // 0 ms is the default exactly
        vm::Settings def = baseSettings();
        const Events d = scenario (def, [] (vm::Settings&) {});
        vm::Settings zero = baseSettings();
        zero.dynamicsSmoothingMs = 0.0f;
        check (t2Same (d, scenario (zero, [] (vm::Settings&) {})), "smoothing 0 ms differs from the default");
    }
    {   // fast flutter varies clearly less
        auto range = [&] (float ms)
        {
            const auto v = ccValues (flutter (ms), 11);
            int lo = 999, hi = -1;
            for (int hop = 20 + 400; hop < 20 + 1000; ++hop)
            {
                const int x = valueAt (v, hop);
                lo = std::min (lo, x);
                hi = std::max (hi, x);
            }
            return hi - lo;
        };
        const int r0 = range (0.0f), r100 = range (100.0f);
        check (r100 * 2 < r0 && r0 >= 4, "flutter: controller range %d with 100 ms, %d without", r100, r0);
        info ("25 Hz flutter of +-4 dB: controller range %d without smoothing, %d with 100 ms", r0, r100);
    }
    {   // a level step arrives clearly later
        auto stepTime = [&] (float ms)
        {
            T2Rig r;
            r.s = baseSettings();
            r.s.dynamicsSmoothingMs = ms;
            for (int i = 0; i < 20; ++i) r.feed (t2Silent());
            for (int i = 0; i < 300; ++i) r.feed (t2Voiced (62.0f, -40.0f));
            for (int i = 0; i < 600; ++i) r.feed (t2Voiced (62.0f, -16.0f));
            r.shutdown();
            const auto v = ccValues (r.all, 11);
            const int final = valueAt (v, 20 + 899);
            for (int hop = 320; hop < 920; ++hop)
                if (valueAt (v, hop) >= final - 2)
                    return std::make_pair (hop - 320, final);
            return std::make_pair (-1, final);
        };
        const auto t0 = stepTime (0.0f), t100 = stepTime (100.0f);
        check (t0.first >= 0 && t100.first >= 0 && t100.first - t0.first >= 75,
               "level step: reached after %.1f ms without, %.1f ms with 100 ms smoothing", t0.first / 0.75, t100.first / 0.75);
        check (std::abs (t0.second - t100.second) <= 1, "level step: final values %d and %d differ", t0.second, t100.second);
        info ("24 dB step: controller there after %.1f ms without smoothing, %.1f ms with 100 ms",
              t0.first / 0.75, t100.first / 0.75);
    }
    {   // no fade-in: the first value at a note-on is already the level
        const Events e0 = flutter (0.0f), e150 = flutter (150.0f);
        const auto on0 = t2NoteOns (e0), on150 = t2NoteOns (e150);
        check (on0.size() == 2 && on150.size() == 2 && on0[1].hop == on150[1].hop, "smoothing changed the notes");
        if (on0.size() == 2 && on150.size() == 2)
            for (int k = 0; k < 2; ++k)
            {
                const int a = valueAt (ccValues (e0, 11), on0[(size_t) k].hop);
                const int b = valueAt (ccValues (e150, 11), on150[(size_t) k].hop);
                check (std::abs (a - b) <= 2, "note %d: controller at the note-on %d with 150 ms smoothing, %d without", k + 1, b, a);
            }
    }
    {   // with smoothing the controller still reaches 127 and is handed back
        T2Rig r;
        r.s = baseSettings();
        r.s.dynamicsSmoothingMs = 150.0f;
        r.s.dynamicsTarget = vm::DynamicsTarget::ModWheel;
        for (int i = 0; i < 20; ++i) r.feed (t2Silent());
        for (int i = 0; i < 300; ++i) r.feed (t2Voiced (62.0f, -40.0f));
        for (int i = 0; i < 1500; ++i) r.feed (t2Voiced (62.0f, -8.0f));
        r.shutdown();
        const auto v = ccValues (r.all, 1);
        check (valueAt (v, 20 + 1799) == 127, "smoothed controller reached only %d", valueAt (v, 20 + 1799));
        check (! v.empty() && v.back().value == 0, "smoothed ModWheel not handed back to 0");
    }
}
