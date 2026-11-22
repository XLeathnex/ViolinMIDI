// ViolinMIDI - tests for the rewritten output stage: voicings, harmony that
// follows the pitch (R4), harmony settings (R5b), dynamics controllers, MPE
// (R5d, R6), freeze (R7), the engine with MPE/freeze/all notes off, and a fuzz
// test over all of it. Rule numbers refer to core/OutputStage.h.
//
// Included by test_main.cpp right after test_stage.h, inside the same anonymous
// namespace; uses StageRig, TrackerSim, ChannelState and the event helpers
// from there.
#pragma once

//==============================================================================
// Small helpers on top of StageRig.

/** Index of `key` in the last output, -1 if missing. */
inline int s2At (const StageRig& rig, const std::string& key) { return stFind (rig.last, key); }

inline void s2Has (StageRig& rig, const char* what, const std::string& key)
{
    check (s2At (rig, key) >= 0, "%s: %s: expected %s in the output, found: %s", rig.where().c_str(), what, key.c_str(), stList (rig.last).c_str());
}

inline void s2HasNot (StageRig& rig, const char* what, const std::string& key)
{
    check (s2At (rig, key) < 0, "%s: %s: expected no %s in the output, found: %s", rig.where().c_str(), what, key.c_str(), stList (rig.last).c_str());
}

/** `a` and `b` both in the output, the first `a` before the first `b`. */
inline void s2Before (StageRig& rig, const char* what, const std::string& a, const std::string& b)
{
    const int i = s2At (rig, a), j = s2At (rig, b);
    check (i >= 0 && j >= 0 && i < j, "%s: %s: expected %s before %s, found: %s", rig.where().c_str(), what, a.c_str(), b.c_str(),
           stList (rig.last).c_str());
}

/** No event at all on `channel` (1..16) in the last output. */
inline void s2Silent (StageRig& rig, const char* what, int channel)
{
    for (const vm::MidiEvent& e : rig.last)
        if (stChannel (e) == channel)
        {
            check (false, "%s: %s: expected nothing on channel %d, found: %s", rig.where().c_str(), what, channel, stList (rig.last).c_str());
            return;
        }
}

inline void s2Nothing (StageRig& rig, const char* what)
{
    check (rig.last.empty(), "%s: %s: expected no output at all, found: %s", rig.where().c_str(), what, stList (rig.last).c_str());
}

inline void s2Cc (StageRig& rig, const char* what, int channel, int number, int expected)
{
    const int found = rig.lastCc[channel - 1][number];
    check (found == expected, "%s: %s: expected controller %d on channel %d = %d, found %d (-1 = never sent; last output: %s)",
           rig.where().c_str(), what, number, channel, expected, found, stList (rig.last).c_str());
}

/** Runs empty hops until a note-on appears; returns the hops waited (the hop with the note-on counts), or -1. */
inline int s2WaitForOn (StageRig& rig, int maxHops)
{
    for (int i = 1; i <= maxHops; ++i)
    {
        rig.hop();
        for (const vm::MidiEvent& e : rig.last)
            if (stIsOn (e))
                return i;
    }
    return -1;
}

inline double s2Ms (int hops) { return hops * 1000.0 / 750.0; }

inline std::string s2VoicesText (const vm::scale::Voices& v)
{
    std::string s;
    for (int i = 0; i < v.count; ++i)
        s += (i == 0 ? "" : " ") + std::to_string (v.notes[i]);
    return s;
}

/** 1 << (semitones + 24) for each entry: a free voicing mask. */
inline uint64_t s2Free (std::initializer_list<int> semis)
{
    uint64_t m = 0;
    for (int x : semis)
        m |= 1ull << (x + 24);
    return m;
}

//==============================================================================
// 1. Scales and voicings (Scale.h).
inline void scalesAndVoicings()
{
    namespace sc = vm::scale;

    // masks of all 13 scales, written out by hand
    const std::vector<std::vector<int>> steps = {
        { 0, 2, 4, 5, 7, 9, 11 }, { 0, 2, 3, 5, 7, 9, 10 }, { 0, 1, 3, 5, 7, 8, 10 }, { 0, 2, 4, 6, 7, 9, 11 },
        { 0, 2, 4, 5, 7, 9, 10 }, { 0, 2, 3, 5, 7, 8, 10 }, { 0, 1, 3, 5, 6, 8, 10 }, { 0, 2, 3, 5, 7, 8, 11 },
        { 0, 2, 3, 5, 7, 9, 11 }, { 0, 1, 4, 5, 7, 8, 10 }, { 0, 2, 4, 7, 9 }, { 0, 3, 5, 7, 10 }, { 0, 3, 5, 6, 7, 10 } };
    check (sc::kScaleCount == 13, "expected 13 scales, found %d", sc::kScaleCount);
    for (int mode = 0; mode < 13; ++mode)
    {
        check (sc::noteCount (mode) == (int) steps[(size_t) mode].size(), "noteCount (%d): expected %d, found %d",
               mode, (int) steps[(size_t) mode].size(), sc::noteCount (mode));
        for (int root : { 0, 3, 7, 11 })
        {
            unsigned want = 0;
            for (int x : steps[(size_t) mode])
                want |= 1u << ((root + x) % 12);
            check (sc::mask (root, mode, false) == want, "mask (root %d, scale %d): expected 0x%03X, found 0x%03X",
                   root, mode, want, sc::mask (root, mode, false));
        }
    }
    // pentatonic flag on the new scales: 10 major pentatonic, 11 and 12 minor pentatonic
    check (sc::mask (0, 10, true) == 0x295u, "mask (C, 10, pentatonic): expected 0x295, found 0x%03X", sc::mask (0, 10, true));
    check (sc::mask (0, 11, true) == 0x4A9u, "mask (C, 11, pentatonic): expected 0x4A9, found 0x%03X", sc::mask (0, 11, true));
    check (sc::mask (0, 12, true) == 0x4A9u, "mask (C, 12, pentatonic): expected 0x4A9, found 0x%03X", sc::mask (0, 12, true));
    check (sc::clampMode (-3) == 0 && sc::clampMode (99) == 12, "clampMode: expected 0 and 12, found %d and %d", sc::clampMode (-3), sc::clampMode (99));

    // every voicing on every degree of C major, diatonic
    const int degree[7] = { 60, 62, 64, 65, 67, 69, 71 };
    const char* const names[7] = { "C", "D", "E", "F", "G", "A", "B" };
    const char* const expected[7][7] = {
        // triad          seventh            1-5-10        1-5-8         1-4-7         1-3      1-6
        { "60 64 67", "60 64 67 71", "60 67 76", "60 67 72", "60 65 71", "60 64", "60 69" },
        { "62 65 69", "62 65 69 72", "62 69 77", "62 69 74", "62 67 72", "62 65", "62 71" },
        { "64 67 71", "64 67 71 74", "64 71 79", "64 71 76", "64 69 74", "64 67", "64 72" },
        { "65 69 72", "65 69 72 76", "65 72 81", "65 72 77", "65 71 76", "65 69", "65 74" },
        { "67 71 74", "67 71 74 77", "67 74 83", "67 74 79", "67 72 77", "67 71", "67 76" },
        { "69 72 76", "69 72 76 79", "69 76 84", "69 76 81", "69 74 79", "69 72", "69 77" },
        { "71 74 77", "71 74 77 81", "71 77 86", "71 77 83", "71 76 81", "71 74", "71 79" } };
    for (int d = 0; d < 7; ++d)
    {
        const int n = degree[d];
        for (int v = 0; v < 7; ++v)
        {
            const std::string found = s2VoicesText (sc::voices (n, 0, 0, v, true, 0));
            check (found == expected[d][v], "C major, %s, voicing %d: expected \"%s\", found \"%s\"", names[d], v, expected[d][v], found.c_str());
        }
        // octave voicings
        const std::string oct[5] = { std::to_string (n - 12), std::to_string (n - 24) + " " + std::to_string (n - 12),
                                     std::to_string (n - 12) + " " + std::to_string (n + 12), std::to_string (n + 12),
                                     std::to_string (n + 12) + " " + std::to_string (n + 24) };
        for (int v = 7; v < 12; ++v)
        {
            const std::string found = s2VoicesText (sc::voices (n, 0, 0, v, true, 0));
            check (found == oct[v - 7], "C major, %s, octave voicing %d: expected \"%s\", found \"%s\"", names[d], v, oct[v - 7].c_str(), found.c_str());
            const std::string chrom = s2VoicesText (sc::voices (n, 0, 0, v, false, 0));
            check (chrom == oct[v - 7], "C major chromatic, %s, octave voicing %d: expected \"%s\", found \"%s\"", names[d], v, oct[v - 7].c_str(), chrom.c_str());
        }
    }

    // chromatic: fixed sizes on every degree and on foreign notes; diatonic foreign notes too
    const int fixedSemis[7][4] = { { 0, 4, 7, -1 }, { 0, 4, 7, 10 }, { 0, 7, 16, -1 }, { 0, 7, 12, -1 }, { 0, 5, 10, -1 }, { 0, 4, -1, -1 }, { 0, 9, -1, -1 } };
    auto fixedText = [&fixedSemis] (int n, int v) {
        std::string s;
        for (int i = 0; i < 4 && fixedSemis[v][i] >= 0; ++i)
            s += (i == 0 ? "" : " ") + std::to_string (n + fixedSemis[v][i]);
        return s;
    };
    for (int n = 60; n < 72; ++n)
        for (int v = 0; v < 7; ++v)
        {
            const std::string want = fixedText (n, v);
            const std::string chrom = s2VoicesText (sc::voices (n, 0, 0, v, false, 0));
            check (chrom == want, "C major chromatic, note %d, voicing %d: expected \"%s\", found \"%s\"", n, v, want.c_str(), chrom.c_str());
            if (! sc::contains (sc::mask (0, 0, false), n))
            {
                const std::string dia = s2VoicesText (sc::voices (n, 0, 0, v, true, 0));
                check (dia == want, "C major diatonic, foreign note %d, voicing %d: expected the fixed sizes \"%s\", found \"%s\"", n, v, want.c_str(), dia.c_str());
            }
        }

    struct Case { const char* what; int note, root, mode, voicing; bool diatonic; uint64_t free; bool without; const char* expected; };
    const Case cases[] = {
        { "C major pentatonic, D: through C major",      62, 0, 10, 0, true, 0, false, "62 65 69" },
        { "C major pentatonic, E: through C major",      64, 0, 10, 1, true, 0, false, "64 67 71 74" },
        { "C minor pentatonic, Eb: through C minor",     63, 0, 11, 0, true, 0, false, "63 67 70" },
        { "C minor pentatonic, C: C minor",              60, 0, 11, 0, true, 0, false, "60 63 67" },
        { "C blues, F (in C minor): F Ab C",             65, 0, 12, 0, true, 0, false, "65 68 72" },
        { "C blues, F# (not in C minor): fixed",         66, 0, 12, 0, true, 0, false, "66 70 73" },
        { "A harmonic minor, V = E: E G# B",             64, 9, 7, 0, true, 0, false, "64 68 71" },
        { "A harmonic minor, V7 = E G# B D",             64, 9, 7, 1, true, 0, false, "64 68 71 74" },
        { "A harmonic minor, i = A C E",                 69, 9, 7, 0, true, 0, false, "69 72 76" },
        { "D Dorian, D minor",                           62, 2, 1, 0, true, 0, false, "62 65 69" },
        { "G major, F# (B dim? no: F# A C)",             66, 7, 0, 0, true, 0, false, "66 69 72" },
        { "free 0 4 7",                                  60, 0, 0, 12, true, s2Free ({ 0, 4, 7 }), false, "60 64 67" },
        { "free -12 0 +12, lowest first",                60, 0, 0, 12, true, s2Free ({ 12, -12, 0 }), false, "48 60 72" },
        { "free -24 and +24 are kept",                   60, 0, 0, 12, true, s2Free ({ -24, 24 }), false, "36 84" },
        { "free: bits above +24 ignored",                60, 0, 0, 12, true, s2Free ({ 3 }) | (1ull << 49) | (1ull << 63), false, "63" },
        { "free: at most 10, the lowest",                60, 0, 0, 12, true, s2Free ({ -24, -20, -16, -12, -8, -4, 0, 4, 8, 12, 16, 20 }), false,
                                                                                          "36 40 44 48 52 56 60 64 68 72" },
        { "free: empty mask",                            60, 0, 0, 12, true, 0, false, "" },
        { "free: mode and key do not matter",            61, 5, 3, 12, false, s2Free ({ 0, 3 }), false, "61 64" },
        { "notes above 127 dropped",                    120, 0, 0, 11, true, 0, false, "" },
        { "notes above 127 dropped, seventh",           122, 0, 0, 1, true, 0, false, "122 125" },
        { "notes below 0 dropped",                        5, 0, 0, 8, true, 0, false, "" },
        { "notes below 0 dropped, free",                 10, 0, 0, 12, true, s2Free ({ -12, -10, 0 }), false, "0 10" },
        { "without the played note, triad",              62, 0, 0, 0, true, 0, true, "65 69" },
        { "without the played note, octave below",       62, 0, 0, 7, true, 0, true, "50" },
        { "without the played note, free",               62, 0, 0, 12, true, s2Free ({ 0, 7 }), true, "69" },
        { "voicing out of range clamped to 12 (free)",   60, 0, 0, 40, true, s2Free ({ 7 }), false, "67" },
        { "voicing -1 clamped to 0",                     60, 0, 0, -1, true, 0, false, "60 64 67" },
    };
    for (const Case& c : cases)
    {
        const std::string found = s2VoicesText (sc::voices (c.note, c.root, c.mode, c.voicing, c.diatonic, c.free, c.without));
        check (found == c.expected, "voices, %s: expected \"%s\", found \"%s\"", c.what, c.expected, found.c_str());
    }

    // cleanFreeMask
    check (sc::cleanFreeMask (~0ull) == 0x3FFull, "cleanFreeMask (all bits): expected 0x3FF, found 0x%llX", (unsigned long long) sc::cleanFreeMask (~0ull));
    check (sc::cleanFreeMask ((1ull << 50) | (1ull << 24)) == (1ull << 24), "cleanFreeMask: bit 50 not dropped");
    check (sc::cleanFreeMask (1ull << 48) == (1ull << 48), "cleanFreeMask: bit 48 (+24) dropped");
    check (sc::cleanFreeMask (0) == 0, "cleanFreeMask (0) not 0");
    check (sc::stepInterval (62, 0, 0, 2, true) == 3 && sc::stepInterval (60, 0, 0, 9, true) == 16 && sc::stepInterval (71, 0, 0, 4, true) == 6,
           "stepInterval: expected D->F 3, C->E' 16, B->F 6, found %d %d %d", sc::stepInterval (62, 0, 0, 2, true),
           sc::stepInterval (60, 0, 0, 9, true), sc::stepInterval (71, 0, 0, 4, true));
}

//==============================================================================
// 2. R4: the harmony follows the sounding pitch. C major, harmony on channel 2,
// bend range 2.
inline StageRig s2HarmonyRig (const char* name)
{
    StageRig rig (name);
    rig.s.triad = true;
    rig.s.triadChannel = 2;
    rig.s.bendRangeSemis = 2.0f;
    return rig;
}

inline void harmonyFollowsPitch()
{
    {   // the customer's case: C bent a whole tone up must become D minor, not D major
        StageRig rig = s2HarmonyRig ("C bent up a whole tone");
        rig.hop ({ noteOn (1, 60, 90) });
        rig.soundingNow ("on 60", "1:60 2:60 2:64 2:67");
        rig.hop ({ bend (1, 16383) });
        rig.bendIs ("bend +2: main", 1, 16383);
        rig.bendIs ("bend +2: harmony, still on root 60", 2, 16383);
        int exchangedAfter = -1;
        for (int i = 1; i <= 60 && exchangedAfter < 0; ++i)
        {
            rig.hop();
            if (! rig.last.empty())
                exchangedAfter = i + 1;   // the hop with the bend counts as the first
        }
        const double ms = s2Ms (exchangedAfter);
        check (exchangedAfter > 0 && ms >= 28.0 && ms <= 34.0, "%s: expected the exchange about 30 ms after the pitch arrived, found %s %.1f ms",
               rig.where().c_str(), exchangedAfter > 0 ? "" : "none within", exchangedAfter > 0 ? ms : s2Ms (60));
        rig.notes ("exchange: old off first, then D minor", "-2:60 -2:64 -2:67 +2:62 +2:65 +2:69");
        s2Silent (rig, "exchange: main channel untouched", 1);
        // 16383 is +1.99976 semitones, so the exact residual on root 62 is -1 (8191)
        rig.bendIs ("exchange: harmony bend back at the centre", 2, 8191);
        rig.bendIs ("exchange: main bend still at +2", 1, 16383);
        s2Before (rig, "exchange: harmony bend before the new voices", "b2", "+2:62");
        rig.soundingNow ("D minor", "1:60 2:62 2:65 2:69");
        check (rig.stage.triadCount() == 3 && rig.stage.triadNote (0) == 62, "%s: triadCount()/triadNote(0): expected 3 / 62, found %d / %d",
               rig.where().c_str(), rig.stage.triadCount(), rig.stage.triadNote (0));
        // residual bend on the harmony channel
        rig.hop ({ bend (1, 16000) });   // 61.90625: 0.09375 below D
        rig.bendIs ("residual", 1, 16000);
        rig.bendIs ("residual on the harmony channel", 2, 7808);
        rig.hop ({ noteOff (1, 60) });
        rig.noteSet ("off", "-1:60 -2:62 -2:65 -2:69");
        rig.soundingNow ("after", "");
    }
    {   // a bend passing through other semitones, under 30 ms in each zone
        StageRig rig = s2HarmonyRig ("glide through");
        rig.hop ({ noteOn (1, 60, 90) });
        for (int z : { 1, 2, 1 })
            for (int i = 0; i < 18; ++i)
            {
                rig.hop ({ bend (1, bendValue (z + (i % 3) * 0.05, 2.0)) });
                check (stNoteKeys (rig.last).empty(), "%s: passing through zone %d: expected no harmony exchange, found %s",
                       rig.where().c_str(), z, stList (rig.last).c_str());
            }
        rig.hop ({ bend (1, 8192) });
        for (int i = 0; i < 40; ++i) rig.hop();
        rig.soundingNow ("back on 60", "1:60 2:60 2:64 2:67");
    }
    {   // vibrato +-0.5 semitone: no exchange, harmony bend = main bend
        StageRig rig = s2HarmonyRig ("vibrato");
        rig.hop ({ noteOn (1, 60, 90) });
        for (int i = 0; i < 400; ++i)
        {
            const int v = bendValue (0.5 * std::sin (i * 2.0 * 3.14159265 * 6.0 / 750.0), 2.0);
            rig.hop ({ bend (1, v) });
            check (stNoteKeys (rig.last).empty(), "%s: vibrato: expected no note events, found %s", rig.where().c_str(), stList (rig.last).c_str());
            check (rig.lastBend[1] == v, "%s: vibrato: expected harmony bend = main bend %d, found %d", rig.where().c_str(), v, rig.lastBend[1]);
            if (gFailuresInTest > 5) break;
        }
    }
    {   // hysteresis: 0.65 from the root keeps it
        StageRig rig = s2HarmonyRig ("hysteresis");
        rig.hop ({ noteOn (1, 60, 90) });
        rig.hop ({ bend (1, bendValue (0.62, 2.0)) });
        for (int i = 0; i < 60; ++i) rig.hop();
        rig.soundingNow ("60.62 for 80 ms: kept", "1:60 2:60 2:64 2:67");
        rig.hop ({ bend (1, bendValue (0.70, 2.0)) });
        const int h = s2WaitForOn (rig, 60);
        check (h > 0 && s2Ms (h + 1) >= 28.0 && s2Ms (h + 1) <= 34.0, "%s: 60.70: expected an exchange after about 30 ms, found %d hops", rig.where().c_str(), h);
        rig.soundingNow ("60.70: C# (foreign: fixed sizes)", "1:60 2:61 2:65 2:68");
        rig.bendIs ("60.70 on root 61", 2, bendValue (-0.30, 2.0), 1);
        rig.hop ({ bend (1, bendValue (0.38, 2.0)) });   // 0.62 from 61
        for (int i = 0; i < 60; ++i) rig.hop();
        rig.soundingNow ("60.38: still root 61", "1:60 2:61 2:65 2:68");
        rig.hop ({ bend (1, bendValue (0.30, 2.0)) });   // 0.70 from 61
        check (s2WaitForOn (rig, 60) > 0, "%s: 60.30: expected the exchange back to 60", rig.where().c_str());
        rig.soundingNow ("60.30: root 60", "1:60 2:60 2:64 2:67");
    }
    {   // delayed start: +0.4 at the note-on, then within 0.25
        StageRig rig = s2HarmonyRig ("delayed start, pitch settles");
        rig.hop ({ bend (1, bendValue (0.4, 2.0)), noteOn (1, 60, 90) });
        rig.notes ("note-on at 60.4: no harmony yet", "+1:60");
        for (int i = 0; i < 5; ++i) rig.hop();
        rig.notes ("waiting", "");
        rig.hop ({ bend (1, bendValue (0.2, 2.0)) });
        rig.noteSet ("60.2: harmony starts at once", "+2:60 +2:64 +2:67");
        rig.bendIs ("60.2", 2, bendValue (0.2, 2.0), 1);
        s2Before (rig, "harmony bend before its notes", "b2", "+2:60");
        rig.hop ({ noteOff (1, 60) });
        rig.soundingNow ("after", "");
    }
    {
        StageRig rig = s2HarmonyRig ("delayed start, pitch stays at +0.4");
        rig.hop ({ bend (1, bendValue (0.4, 2.0)), noteOn (1, 60, 90) });
        const int h = s2WaitForOn (rig, 60);
        check (h > 0 && s2Ms (h + 1) >= 26.0 && s2Ms (h + 1) <= 34.0, "%s: expected the start at the latest after about 30 ms, found %d hops", rig.where().c_str(), h);
        rig.noteSet ("started on the nearest semitone", "+2:60 +2:64 +2:67");
        rig.bendIs ("residual", 2, bendValue (0.4, 2.0), 1);
    }
    {
        StageRig rig = s2HarmonyRig ("delayed start, pitch stays at +0.6");
        rig.hop ({ bend (1, bendValue (0.6, 2.0)), noteOn (1, 60, 90) });
        check (s2WaitForOn (rig, 60) > 0, "%s: expected a start", rig.where().c_str());
        rig.noteSet ("started on 61", "+2:61 +2:65 +2:68");
        rig.bendIs ("residual -0.4", 2, bendValue (-0.4, 2.0), 1);
        for (int i = 0; i < 60; ++i) rig.hop();
        rig.soundingNow ("stays", "1:60 2:61 2:65 2:68");
    }
    {
        StageRig rig = s2HarmonyRig ("legato during a pending start");
        rig.hop ({ bend (1, bendValue (0.4, 2.0)), noteOn (1, 60, 90) });
        rig.hop();
        rig.hop ({ bend (1, 8192), noteOn (1, 62, 80), noteOff (1, 60) });
        rig.noteSet ("legato to 62", "+1:62 -1:60 +2:62 +2:65 +2:69");
        for (int i = 0; i < 40; ++i) rig.hop();
        rig.soundingNow ("nothing left behind", "1:62 2:62 2:65 2:69");
        rig.hop ({ noteOff (1, 62) });
        rig.soundingNow ("after", "");
    }
    {
        StageRig rig = s2HarmonyRig ("note-off during a pending start");
        rig.hop ({ bend (1, bendValue (0.4, 2.0)), noteOn (1, 60, 90) });
        rig.hop();
        rig.hop ({ noteOff (1, 60) });
        rig.notes ("off", "-1:60");
        check (s2WaitForOn (rig, 60) < 0, "%s: a harmony started after the main note ended: %s", rig.where().c_str(), stList (rig.last).c_str());
        rig.soundingNow ("after", "");
    }
    {   // R1c: a held note keeps its harmony whatever the tracker's pitch does
        StageRig rig = s2HarmonyRig ("held note keeps its harmony");
        rig.key (0, 0, true, true);
        rig.hop ({ noteOn (1, 60, 90) });
        rig.hop ({ noteOn (1, 65, 90), noteOff (1, 60) });
        rig.soundingNow ("held", "1:60 2:60 2:64 2:67");
        // 65 bent up to 66 (F#, foreign: R3 must not start anything) and wobbling around it
        for (int i = 0; i < 60; ++i) rig.hop ({ bend (1, bendValue (1.0 + 0.1 * std::sin (i * 0.3), 2.0)) });
        rig.soundingNow ("held, tracker bends far away", "1:60 2:60 2:64 2:67");
        rig.level = -60.0f;
        rig.hop();
        rig.soundingNow ("after", "");
    }
    {   // R4f: shared channel, the root is the main note, no exchange
        StageRig rig = s2HarmonyRig ("shared channel");
        rig.s.triadChannel = 1;
        rig.hop ({ noteOn (1, 60, 90) });
        rig.soundingNow ("on 60", "1:60 1:64 1:67");
        rig.hop ({ bend (1, 16383) });
        check (stCount (rig.last, "b1") == 1, "%s: expected the bend once, found %s", rig.where().c_str(), stList (rig.last).c_str());
        for (int i = 0; i < 60; ++i) rig.hop();
        rig.soundingNow ("bent +2 for 80 ms: unchanged", "1:60 1:64 1:67");
        rig.hop ({ noteOff (1, 60) });
        rig.soundingNow ("after", "");
    }
    {   // the exchange also works with a slid-in note (R3) and ends cleanly
        StageRig rig = s2HarmonyRig ("exchange, then legato");
        rig.hop ({ noteOn (1, 60, 90) });
        rig.hop ({ bend (1, 16383) });
        s2WaitForOn (rig, 60);
        rig.hop ({ bend (1, 8192), noteOn (1, 64, 80), noteOff (1, 60) });
        rig.noteSet ("legato 60(62) -> 64", "-2:62 -2:65 -2:69 +1:64 -1:60 +2:64 +2:67 +2:71");
        rig.bendIs ("harmony bend = main bend again", 2, 8192);
        rig.hop ({ noteOff (1, 64) });
        rig.soundingNow ("after", "");
    }
}

//==============================================================================
// 3. R5b: harmony settings while a note sounds.
inline void harmonySettings()
{
    auto prepared = [] (const char* name) {
        StageRig rig = s2HarmonyRig (name);
        rig.hop ({ bend (1, 9000), cc (1, 11, 80), cc (1, 1, 50), noteOn (1, 62, 90) });
        return rig;
    };
    auto offsBeforeOns = [] (StageRig& rig, const char* what) {
        int lastOff = -1, firstOn = 1 << 30;
        for (int i = 0; i < (int) rig.last.size(); ++i)
        {
            if (stIsOff (rig.last[(size_t) i])) lastOff = i;
            if (stIsOn (rig.last[(size_t) i])) firstOn = std::min (firstOn, i);
        }
        check (lastOff < firstOn, "%s: %s: expected all note-offs before the note-ons, found %s", rig.where().c_str(), what, stList (rig.last).c_str());
    };
    {
        StageRig rig = prepared ("voicing 0 -> 1");
        rig.soundingNow ("D minor", "1:62 2:62 2:65 2:69");
        rig.s.voicing = 1;
        rig.hop();
        rig.soundingNow ("Dm7", "1:62 2:62 2:65 2:69 2:72");
        offsBeforeOns (rig, "voicing change");
        s2Silent (rig, "main channel", 1);
        s2HasNot (rig, "channel not left: no hand-back", "c2:11");
        rig.hop ({ noteOff (1, 62) });
        rig.soundingNow ("after", "");
    }
    {
        StageRig rig = prepared ("harmonyMode diatonic -> chromatic");
        rig.s.harmonyMode = vm::HarmonyMode::Chromatic;
        rig.hop();
        rig.soundingNow ("D major", "1:62 2:62 2:66 2:69");
        offsBeforeOns (rig, "mode change");
    }
    {
        StageRig rig = prepared ("free voicing changed while voicing is free");
        rig.s.voicing = 12;
        rig.s.freeVoicing = s2Free ({ 0, 12 });
        rig.hop();
        rig.soundingNow ("free 0 12", "1:62 2:62 2:74");
        rig.s.freeVoicing = s2Free ({ -12, 0 });
        rig.hop();
        rig.soundingNow ("free -12 0", "1:62 2:50 2:62");
        offsBeforeOns (rig, "free mask change");
        rig.s.freeVoicing = ~0ull;   // invalid: more than 10 bits and bits above 48
        rig.hop();
        rig.soundingNow ("free all bits: the ten lowest", "1:62 2:38 2:39 2:40 2:41 2:42 2:43 2:44 2:45 2:46 2:47");
        rig.hop ({ noteOff (1, 62) });
        rig.soundingNow ("after", "");
    }
    {
        StageRig rig = prepared ("triad channel 2 -> 3");
        rig.s.triadChannel = 3;
        rig.hop();
        rig.noteSet ("moved", "-2:62 -2:65 -2:69 +3:62 +3:65 +3:69");
        rig.bendIs ("channel 2 handed back: bend", 2, 8192);
        s2Cc (rig, "channel 2 handed back: CC11", 2, 11, 127);
        s2Cc (rig, "channel 2 handed back: CC1", 2, 1, 0);
        rig.bendIs ("channel 3: bend", 3, 9000);
        s2Cc (rig, "channel 3: CC11", 3, 11, 80);
        s2Cc (rig, "channel 3: CC1", 3, 1, 50);
        s2Before (rig, "channel 3 brought up before its notes: bend", "b3", "+3:62");
        s2Before (rig, "channel 3 brought up before its notes: CC11", "c3:11", "+3:62");
        offsBeforeOns (rig, "channel change");
        rig.hop ({ noteOff (1, 62) });
        rig.soundingNow ("after", "");
    }
    {
        StageRig rig = prepared ("harmony off and on");
        rig.s.triad = false;
        rig.hop();
        rig.noteSet ("off", "-2:62 -2:65 -2:69");
        rig.bendIs ("handed back: bend", 2, 8192);
        s2Cc (rig, "handed back: CC11", 2, 11, 127);
        s2Cc (rig, "handed back: CC1", 2, 1, 0);
        rig.hop ({ bend (1, 9100) });
        s2Silent (rig, "harmony off: nothing mirrored", 2);
        rig.s.triad = true;
        rig.hop();
        rig.noteSet ("on again", "+2:62 +2:65 +2:69");
        rig.bendIs ("on again: bend", 2, 9100);
        s2Cc (rig, "on again: CC11", 2, 11, 80);
        s2Before (rig, "on again: bend first", "b2", "+2:62");
        rig.hop ({ noteOff (1, 62) });
        rig.soundingNow ("after", "");
    }
    {
        StageRig rig = prepared ("harmony moved onto the main channel");
        rig.s.triadChannel = 1;
        rig.hop();
        rig.noteSet ("2 -> 1 (shared)", "-2:62 -2:65 -2:69 +1:65 +1:69");
        rig.bendIs ("channel 2 handed back", 2, 8192);
        s2Cc (rig, "channel 2 handed back: CC11", 2, 11, 127);
        rig.s.triadChannel = 2;
        rig.hop();
        rig.noteSet ("1 -> 2", "-1:65 -1:69 +2:62 +2:65 +2:69");
        rig.bendIs ("channel 2 brought up", 2, 9000);
        rig.hop ({ noteOff (1, 62) });
        rig.soundingNow ("after", "");
    }
    {   // several changes at once, while bent to another root
        StageRig rig = prepared ("several changes in one hop");
        rig.s.voicing = 3;
        rig.s.triadChannel = 5;
        rig.s.harmonyMode = vm::HarmonyMode::Chromatic;
        rig.hop();
        rig.noteSet ("all at once", "-2:62 -2:65 -2:69 +5:62 +5:69 +5:74");
        rig.hop ({ noteOff (1, 62) });
        rig.soundingNow ("after", "");
    }
}

// Open question (see report): R5b says the harmony notes "end at once" and start
// again; the brief says identical voices are not retriggered. This group tests
// the brief's reading and is kept apart so "harmony settings" stays meaningful.
inline void harmonyNoRetrigger()
{
    {
        StageRig rig = s2HarmonyRig ("identical voices: chromatic on C");
        rig.hop ({ noteOn (1, 60, 90) });
        rig.s.harmonyMode = vm::HarmonyMode::Chromatic;   // C E G either way
        rig.hop();
        rig.notes ("C E G stays C E G", "");
        rig.soundingNow ("C E G stays C E G", "1:60 2:60 2:64 2:67");
    }
    {
        StageRig rig = s2HarmonyRig ("identical voices: free mask changed, voicing not free");
        rig.hop ({ noteOn (1, 60, 90) });
        rig.s.freeVoicing = s2Free ({ 0, 3, 9 });
        rig.hop();
        rig.notes ("free mask does not matter for voicing 0", "");
    }
    {
        StageRig rig = s2HarmonyRig ("identical voices: voicing 9 -> free -12 +12");
        rig.s.voicing = 9;
        rig.hop ({ noteOn (1, 60, 90) });
        rig.s.voicing = 12;
        rig.s.freeVoicing = s2Free ({ -12, 12 });
        rig.hop();
        rig.notes ("48 72 stays 48 72", "");
    }
}

//==============================================================================
// 4. Dynamics controllers 1, 2 and 11.
inline void dynamicsControllers()
{
    StageRig rig = s2HarmonyRig ("dynamics controllers");
    rig.hop ({ noteOn (1, 60, 90) });
    rig.hop ({ cc (1, 1, 40), cc (1, 2, 50), cc (1, 11, 60), cc (1, 7, 90), cc (1, 64, 127), cc (1, 74, 3) });
    for (int number : { 1, 2, 11, 7, 64, 74 })
    {
        const std::string k = "c1:" + std::to_string (number);
        check (stCount (rig.last, k) == 1, "%s: expected %s once on the main channel, found %s", rig.where().c_str(), k.c_str(), stList (rig.last).c_str());
    }
    s2Cc (rig, "CC1 mirrored", 2, 1, 40);
    s2Cc (rig, "CC2 mirrored", 2, 2, 50);
    s2Cc (rig, "CC11 mirrored", 2, 11, 60);
    for (int number : { 7, 64, 74 })
        s2HasNot (rig, "other controllers not mirrored", "c2:" + std::to_string (number));
    rig.s.triad = false;
    rig.hop();
    s2Cc (rig, "handed back: CC1 = 0", 2, 1, 0);
    s2Cc (rig, "handed back: CC2 = 127", 2, 2, 127);
    s2Cc (rig, "handed back: CC11 = 127", 2, 11, 127);
    s2HasNot (rig, "CC7 not touched on channel 2", "c2:7");

    // releaseAll hands back as well
    StageRig r2 = s2HarmonyRig ("dynamics controllers, releaseAll");
    r2.hop ({ cc (1, 2, 33), noteOn (1, 60, 90) });
    s2Cc (r2, "CC2 mirrored at the start", 2, 2, 33);
    r2.hop ({ cc (1, 1, 20) });
    r2.releaseAll();
    s2Cc (r2, "releaseAll: CC1", 2, 1, 0);
    s2Cc (r2, "releaseAll: CC2", 2, 2, 127);
    r2.soundingNow ("releaseAll", "");

    // shared channel: nothing repeated
    StageRig r3 = s2HarmonyRig ("dynamics controllers, shared channel");
    r3.s.triadChannel = 1;
    r3.hop ({ noteOn (1, 60, 90) });
    r3.hop ({ cc (1, 1, 40), cc (1, 2, 50) });
    check (stCount (r3.last, "c1:1") == 1 && stCount (r3.last, "c1:2") == 1, "%s: expected each controller once, found %s",
           r3.where().c_str(), stList (r3.last).c_str());
}

//==============================================================================
// 5. MPE (R5d, R6).
inline StageRig s2MpeRig (const char* name)
{
    StageRig rig (name);
    rig.s.mpe = true;
    rig.s.bendRangeSemis = 48.0f;
    return rig;
}

inline void mpeTests()
{
    {
        StageRig rig = s2MpeRig ("mpe zone at the first call");
        rig.hop();
        check (stList (rig.last) == "c1:101=0 c1:100=6 c1:6=15", "%s: expected the zone announcement c1:101=0 c1:100=6 c1:6=15, found %s",
               rig.where().c_str(), stList (rig.last).c_str());
        rig.hop();
        s2Nothing (rig, "second call");
        rig.s.mpe = false;
        rig.hop();
        check (stList (rig.last) == "c1:101=0 c1:100=6 c1:6=0", "%s: switched off: expected c1:101=0 c1:100=6 c1:6=0, found %s",
               rig.where().c_str(), stList (rig.last).c_str());
        rig.s.mpe = true;
        rig.hop();
        check (stList (rig.last) == "c1:101=0 c1:100=6 c1:6=15", "%s: switched on: expected the zone again, found %s",
               rig.where().c_str(), stList (rig.last).c_str());
    }
    {
        StageRig rig ("mpe off at the first call: no zone");
        rig.hop();
        s2Nothing (rig, "first call, mpe off");
    }
    {
        StageRig rig = s2MpeRig ("mpe round robin");
        rig.hop();
        std::string seq;
        for (int i = 0; i < 17; ++i)
        {
            rig.hop ({ noteOn (1, 60 + i % 3, 90) });
            for (const vm::MidiEvent& e : rig.last)
                if (stIsOn (e)) seq += std::to_string (stChannel (e)) + " ";
            rig.hop ({ noteOff (1, 60 + i % 3) });
        }
        check (seq == "2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 2 3 ", "%s: expected channels 2..16 then 2 3, found %s", rig.where().c_str(), seq.c_str());
        rig.soundingNow ("after", "");
    }
    {
        StageRig rig = s2MpeRig ("mpe legato");
        rig.hop();   // the zone announcement
        rig.hop ({ bend (1, 8300), cc (1, 11, 60), noteOn (1, 60, 90) });
        s2Before (rig, "fresh note: bend first", "b2", "+2:60");
        s2Before (rig, "fresh note: CC11 first", "c2:11", "+2:60");
        s2Before (rig, "fresh note: pressure first", "p2", "+2:60");
        rig.bendIs ("fresh note", 2, 8300);
        check (rig.lastPressure[1] == 60, "%s: expected pressure 60 on channel 2, found %d", rig.where().c_str(), rig.lastPressure[1]);
        s2Silent (rig, "fresh note: nothing on channel 1", 1);
        rig.hop ({ bend (1, 8400) });
        rig.bendIs ("later bend on the note's channel", 2, 8400);
        rig.hop ({ bend (1, 9000), cc (1, 11, 70), noteOn (1, 62, 90), noteOff (1, 60) });
        rig.notes ("legato", "+3:62 -2:60");
        s2Before (rig, "legato: bend on the new channel first", "b3", "+3:62");
        s2Before (rig, "legato: CC11 on the new channel first", "c3:11", "+3:62");
        s2Before (rig, "legato: pressure on the new channel first", "p3", "+3:62");
        rig.bendIs ("legato: new channel", 3, 9000);
        s2Cc (rig, "legato: new channel CC11", 3, 11, 70);
        check (rig.lastPressure[2] == 70, "%s: expected pressure 70 on channel 3, found %d", rig.where().c_str(), rig.lastPressure[2]);
        s2HasNot (rig, "legato: old channel keeps its pitch", "b2");
        s2HasNot (rig, "legato: old channel no controller", "c2:11");
        s2HasNot (rig, "legato: old channel no pressure", "p2");
        rig.bendIs ("legato: old channel untouched", 2, 8400);
        rig.hop ({ bend (1, 9100) });
        rig.notes ("bend", "");
        rig.bendIs ("bend goes to channel 3", 3, 9100);
        s2HasNot (rig, "bend not on 2", "b2");
        rig.hop ({ cc (1, 11, 75) });
        check (stList (rig.last) == "c3:11=75 p3=75", "%s: expected c3:11=75 p3=75, found %s", rig.where().c_str(), stList (rig.last).c_str());
        rig.hop ({ cc (1, 11, 75) });
        s2HasNot (rig, "unchanged value: no pressure", "p3");
        rig.hop ({ cc (1, 7, 100), cc (1, 64, 0) });
        check (stList (rig.last) == "c1:7=100 c1:64=0", "%s: other controllers go to channel 1, found %s", rig.where().c_str(), stList (rig.last).c_str());
        rig.hop ({ noteOff (1, 62) });
        rig.notes ("off", "-3:62");
        rig.hop ({ cc (1, 11, 127) });
        check (stList (rig.last) == "c3:11=127", "%s: hand-back after the note: expected c3:11=127, found %s", rig.where().c_str(), stList (rig.last).c_str());
        rig.soundingNow ("after", "");
    }
    {
        StageRig rig = s2MpeRig ("mpe skips the harmony channel");
        rig.s.triad = true;
        rig.s.triadChannel = 3;
        std::string seq;
        for (int i = 0; i < 16; ++i)
        {
            rig.hop ({ noteOn (1, 60, 90) });
            for (const vm::MidiEvent& e : rig.last)
                if (stIsOn (e) && stChannel (e) != 3) seq += std::to_string (stChannel (e)) + " ";
            rig.hop ({ noteOff (1, 60) });
        }
        check (seq == "2 4 5 6 7 8 9 10 11 12 13 14 15 16 2 4 ", "%s: expected 3 skipped, found %s", rig.where().c_str(), seq.c_str());
        rig.soundingNow ("after", "");
    }
    for (int hc : { 16, 1 })
    {
        StageRig rig = s2MpeRig (hc == 16 ? "mpe, harmony on channel 16" : "mpe, harmony on channel 1");
        rig.s.triad = true;
        rig.s.triadChannel = hc;
        std::string seq;
        for (int i = 0; i < 16; ++i)
        {
            rig.hop ({ noteOn (1, 60, 90) });
            for (const vm::MidiEvent& e : rig.last)
                if (stIsOn (e) && stChannel (e) != hc) seq += std::to_string (stChannel (e)) + " ";
            check (rig.countOn (hc) == 3, "%s: expected the harmony (3 notes) on channel %d, found %s", rig.where().c_str(), hc, rig.soundingText().c_str());
            rig.hop ({ noteOff (1, 60) });
        }
        const char* want = hc == 16 ? "2 3 4 5 6 7 8 9 10 11 12 13 14 15 2 3 " : "2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 2 ";
        check (seq == want, "%s: expected %s, found %s", rig.where().c_str(), want, seq.c_str());
        rig.soundingNow ("after", "");
    }
    {   // harmony switched on with the default channel 2 while the main note sounds on channel 2
        StageRig rig = s2MpeRig ("mpe, harmony switched on onto the main note's channel");
        rig.hop ({ bend (1, 8300), noteOn (1, 60, 90) });
        rig.soundingNow ("main on 2", "2:60");
        rig.s.triad = true;
        rig.s.triadChannel = 2;
        rig.hop();
        check (rig.countAll() == 4 || rig.countAll() == 1, "%s: expected the main note and its harmony (or the harmony waiting), found %s",
               rig.where().c_str(), rig.soundingText().c_str());
        for (int i = 0; i < 40; ++i) rig.hop();
        check (rig.countAll() == 4 && rig.countOn (2) == 3, "%s: expected the main note on its own channel and 3 harmony notes on 2, found %s",
               rig.where().c_str(), rig.soundingText().c_str());
        rig.hop ({ noteOff (1, 60) });
        for (int i = 0; i < 40; ++i) rig.hop();
        rig.soundingNow ("after", "");
    }
    {
        StageRig rig = s2MpeRig ("mpe, harmony channel moved onto the main note's channel");
        rig.s.triad = true;
        rig.s.triadChannel = 5;
        rig.hop ({ noteOn (1, 60, 90) });
        rig.soundingNow ("main on 2", "2:60 5:60 5:64 5:67");
        rig.s.triadChannel = 2;
        rig.hop();
        for (int i = 0; i < 40; ++i) rig.hop();
        check (rig.countAll() == 4 && rig.countOn (2) == 3 && rig.countOn (5) == 0, "%s: expected the main note on its own channel and 3 harmony notes on 2, found %s",
               rig.where().c_str(), rig.soundingText().c_str());
        rig.hop ({ noteOff (1, 60) });
        for (int i = 0; i < 40; ++i) rig.hop();
        rig.soundingNow ("after", "");
    }
    {   // R4e: a member channel a main note left bent becomes the harmony channel
        StageRig rig = s2MpeRig ("mpe, harmony on a channel a main note left bent");
        rig.hop ({ bend (1, 9000), cc (1, 11, 40), noteOn (1, 60, 90) });
        rig.hop ({ noteOff (1, 60) });
        rig.bendIs ("channel 2 left at 9000", 2, 9000);
        rig.s.triad = true;
        rig.s.triadChannel = 2;
        rig.hop ({ bend (1, 8192), cc (1, 11, 100), noteOn (1, 62, 90) });
        rig.soundingNow ("main on 3, harmony on 2", "2:62 2:65 2:69 3:62");
        rig.bendIs ("harmony channel at the main note's bend", 2, 8192);
        s2Cc (rig, "harmony channel CC11", 2, 11, 100);
        rig.hop ({ noteOff (1, 62) });
        rig.soundingNow ("after", "");
    }
    {
        StageRig rig = s2MpeRig ("mpe skips a frozen note's channel");
        rig.hop ({ noteOn (1, 60, 90) });
        rig.s.freezeSolo = true;
        rig.hop();
        rig.hop ({ noteOff (1, 60) });
        rig.s.freezeSolo = false;
        rig.hop();
        rig.notes ("release in silence", "-2:60");
        // frozen on 3, next notes skip it
        rig.hop ({ noteOn (1, 62, 90) });
        rig.soundingNow ("62 on 3", "3:62");
        rig.s.freezeSolo = true;
        rig.hop();
        rig.hop ({ noteOff (1, 62) });
        rig.hop ({ noteOn (1, 64, 90) });
        rig.hop ({ noteOff (1, 64) });
        rig.s.freezeSolo = false;
        rig.hop ({ noteOn (1, 65, 90) });
        rig.notes ("release, then a new note", "-3:62 +5:65");
        rig.hop ({ noteOff (1, 65) });
        rig.soundingNow ("after", "");
    }
    {   // R5d: mpe off in the middle of a note; the note comes back by R3
        StageRig rig = s2MpeRig ("mpe switched off mid-note");
        rig.s.bendRangeSemis = 2.0f;
        rig.hop ({ bend (1, 8300), cc (1, 11, 90), noteOn (1, 60, 90) });
        rig.soundingNow ("on", "2:60");
        rig.s.mpe = false;
        rig.hop();
        s2Has (rig, "mpe off: note ends on its channel", "-2:60");
        rig.bendIs ("mpe off: bend back to the centre on 2", 2, 8192);
        s2Has (rig, "mpe off: zone off", "c1:6");
        rig.soundingNow ("mpe off", "");
        const int h = s2WaitForOn (rig, 60);
        check (h > 0 && s2Ms (h) <= 45.0, "%s: expected the note back within about 30 ms, found %d hops", rig.where().c_str(), h);
        rig.soundingNow ("back on channel 1", "1:60");
        rig.bendIs ("back on channel 1 with the bend", 1, 8300, 1);
        rig.s.mpe = true;
        rig.hop();
        s2Has (rig, "mpe on: note ends on channel 1", "-1:60");
        rig.bendIs ("mpe on: bend back to the centre on 1", 1, 8192);
        check (s2WaitForOn (rig, 60) > 0, "%s: expected the note back after mpe on", rig.where().c_str());
        check (rig.countAll() == 1 && rig.countOn (1) == 0, "%s: expected the note on a member channel, found %s", rig.where().c_str(), rig.soundingText().c_str());
        rig.hop ({ noteOff (1, 60) });
        rig.soundingNow ("after", "");
        rig.releaseAll();
        for (int c = 0; c < 16; ++c)
            check (rig.lastBend[c] < 0 || rig.lastBend[c] == 8192, "%s: bend on channel %d left at %d", rig.where().c_str(), c + 1, rig.lastBend[c]);
    }
    {
        StageRig rig = s2MpeRig ("mpe with key filter and sustain");
        rig.key (0, 0, true, true);
        rig.hop ({ noteOn (1, 61, 90) });
        rig.notes ("foreign", "");
        rig.hop ({ noteOn (1, 60, 90), noteOff (1, 61) });
        rig.notes ("allowed", "+2:60");
        rig.hop ({ noteOn (1, 65, 90), noteOff (1, 60) });
        rig.notes ("foreign: held", "");
        check (rig.stage.isHeld(), "%s: expected a hold", rig.where().c_str());
        rig.hop ({ bend (1, 9000) });
        s2HasNot (rig, "held: not bent", "b2");
        rig.hop ({ noteOn (1, 64, 90), noteOff (1, 65) });
        rig.notes ("allowed ends the hold", "-2:60 +3:64");
        rig.hop ({ noteOff (1, 64) });
        rig.level = -60.0f;
        rig.hop();
        rig.soundingNow ("after", "");
    }
    {
        StageRig rig = s2MpeRig ("mpe slide-in");
        rig.s.bendRangeSemis = 2.0f;
        rig.key (0, 0, false);
        rig.hop ({ noteOn (1, 61, 77) });
        rig.hop ({ bend (1, bendValue (0.95, 2.0)) });
        const int h = s2WaitForOn (rig, 60);
        check (h > 0, "%s: expected the slide-in to start a note", rig.where().c_str());
        rig.notes ("start", "+2:62");
        rig.bendIs ("exact pitch", 2, bendValue (-0.05, 2.0), 2);
        rig.hop ({ noteOff (1, 61) });
        rig.soundingNow ("after", "");
    }
    {
        StageRig rig = s2MpeRig ("mpe with harmony");
        rig.s.triad = true;
        rig.s.triadChannel = 2;
        rig.s.bendRangeSemis = 2.0f;
        rig.hop ({ bend (1, 8500), cc (1, 11, 70), noteOn (1, 60, 90) });
        rig.soundingNow ("main on 3, harmony on 2", "2:60 2:64 2:67 3:60");
        rig.bendIs ("main", 3, 8500);
        rig.bendIs ("harmony", 2, 8500);
        s2Cc (rig, "harmony CC11", 2, 11, 70);
        rig.hop ({ bend (1, 8600), cc (1, 1, 30) });
        rig.bendIs ("harmony follows", 2, 8600);
        s2Cc (rig, "CC1 mirrored", 2, 1, 30);
        rig.hop ({ bend (1, 8192), noteOn (1, 64, 90), noteOff (1, 60) });
        rig.soundingNow ("legato", "2:64 2:67 2:71 4:64");
        rig.hop ({ noteOff (1, 64) });
        rig.releaseAll();
        rig.soundingNow ("after", "");
        s2Cc (rig, "handed back CC11", 2, 11, 127);
        s2Cc (rig, "handed back CC1", 2, 1, 0);
    }
}

//==============================================================================
// 6. Freeze (R7).
inline void freezeTests()
{
    {
        StageRig rig ("freeze solo during a note");
        rig.hop ({ bend (1, 8500), noteOn (1, 60, 90) });
        rig.s.freezeSolo = true;
        rig.hop();
        s2Nothing (rig, "switched on");
        check (rig.stage.frozenNote() == 60, "%s: frozenNote(): expected 60, found %d", rig.where().c_str(), rig.stage.frozenNote());
        std::mt19937 rng (5);
        int playing = 60;
        for (int i = 0; i < 300; ++i)
        {
            Events in;
            const int a = (int) (rng() % 10);
            if (a < 2)
            {
                const int next = playing == 60 ? 64 : 60;
                in = { bend (1, 8000 + (int) (rng() % 400)), cc (1, 11, (int) (rng() % 128)), noteOn (1, next, 90), noteOff (1, playing) };
                playing = next;
            }
            else if (a < 6) in = { bend (1, 8192 + (int) (rng() % 2000)) };
            else in = { cc (1, a == 6 ? 1 : (a == 7 ? 2 : 11), (int) (rng() % 128)) };
            rig.hop (in);
            s2Nothing (rig, "frozen: nothing on the solo track");
            if (! rig.last.empty()) break;
        }
        rig.soundingNow ("still the frozen note", "1:60");
        rig.hop ({ noteOff (1, rig.stage.note()) });
        rig.s.freezeSolo = false;
        rig.hop();
        rig.notes ("released in silence", "-1:60");
        rig.soundingNow ("after", "");
    }
    {
        StageRig rig ("freeze solo in silence");
        rig.s.freezeSolo = true;
        rig.hop();
        rig.hop ({ noteOn (1, 60, 90) });
        rig.notes ("next note sounds, then frozen", "+1:60");
        check (rig.stage.frozenNote() == 60, "%s: frozenNote(): expected 60, found %d", rig.where().c_str(), rig.stage.frozenNote());
        rig.hop ({ noteOn (1, 62, 90), noteOff (1, 60) });
        s2Nothing (rig, "frozen");
        // switched off during a different note: frozen off, bend/controllers up to date, the played note starts
        rig.hop ({ bend (1, 9000), cc (1, 11, 66) });
        rig.s.freezeSolo = false;
        rig.hop();
        rig.notes ("released during 62", "-1:60 +1:62");
        rig.bendIs ("brought up to date", 1, 9000);
        s2Cc (rig, "brought up to date", 1, 11, 66);
        s2Before (rig, "bend before the note-on", "b1", "+1:62");
        rig.hop ({ noteOff (1, 62) });
        rig.soundingNow ("after", "");
    }
    {
        StageRig rig ("freeze solo armed, legato");
        rig.hop ({ noteOn (1, 60, 90) });
        rig.s.freezeSolo = true;
        rig.hop();
        rig.hop ({ noteOff (1, 60) });
        rig.s.freezeSolo = false;
        rig.hop();
        rig.notes ("released", "-1:60");
        rig.hop ({ noteOn (1, 62, 90) });
        rig.s.freezeSolo = true;
        rig.hop ({ noteOn (1, 64, 90), noteOff (1, 62) });   // frozen on 62 at once
        s2Nothing (rig, "frozen during 62, legato to 64 not sent");
        rig.hop ({ noteOff (1, 64) });
        rig.s.freezeSolo = false;
        rig.hop();
        rig.notes ("released", "-1:62");
        // armed in silence, the next note arrives as a legato change of a note that is not frozen yet
        rig.s.freezeSolo = true;
        rig.hop();
        rig.s.freezeSolo = false;
        rig.hop();
        s2Nothing (rig, "armed and disarmed in silence");
    }
    {   // armed while a note is held back by the key filter, the next note is a legato change
        StageRig rig ("freeze armed, legato change with the old note still sounding");
        rig.s.freezeSolo = true;
        rig.key (0, 0, false);
        rig.hop ({ noteOn (1, 61, 90) });   // not allowed: nothing to freeze
        rig.hop ({ noteOn (1, 62, 90), noteOff (1, 61) });
        rig.notes ("62 starts and is frozen", "+1:62");
        rig.hop ({ noteOn (1, 64, 90), noteOff (1, 62) });
        s2Nothing (rig, "frozen");
        rig.hop ({ noteOff (1, 64) });
        rig.s.freezeSolo = false;
        rig.hop();
        rig.notes ("release", "-1:62");
    }
    {   // the old note of a legato change gets its note-off when the new one is frozen at once
        StageRig rig ("freeze armed in silence, released during the next legato (mpe)");
        rig.s.mpe = true;
        rig.hop ({ noteOn (1, 60, 90) });
        rig.hop ({ noteOff (1, 60) });
        rig.s.freezeSolo = true;
        rig.hop ({ noteOn (1, 62, 90) });
        rig.notes ("frozen at once", "+3:62");
        rig.s.freezeSolo = false;
        rig.hop ({ noteOn (1, 64, 90), noteOff (1, 62) });
        rig.soundingNow ("released during 64", "4:64");
        rig.hop ({ noteOff (1, 64) });
        rig.soundingNow ("after", "");
    }
    {
        StageRig rig ("freeze solo off during the same note");
        rig.hop ({ noteOn (1, 60, 90) });
        rig.s.freezeSolo = true;
        rig.hop();
        rig.hop ({ bend (1, 9000) });
        rig.hop ({ bend (1, 9100), noteOn (1, 62, 90), noteOff (1, 60) });
        rig.hop ({ bend (1, 8192), noteOn (1, 60, 90), noteOff (1, 62) });
        rig.hop ({ cc (1, 11, 50) });
        rig.s.freezeSolo = false;
        rig.hop();
        check (stNoteKeys (rig.last).empty(), "%s: same note on the same channel: expected no note-off/on, found %s", rig.where().c_str(), stList (rig.last).c_str());
        s2Cc (rig, "controllers brought up to date", 1, 11, 50);
        rig.soundingNow ("carries on", "1:60");
        rig.hop ({ noteOff (1, 60) });
        rig.soundingNow ("after", "");
    }
    {   // harmony frozen, the solo track behaves exactly as without freeze
        StageRig a = s2HarmonyRig ("freeze harmony: solo as without freeze");
        StageRig b = s2HarmonyRig ("reference without freeze");
        const std::vector<Events> script = {
            { bend (1, 8300), cc (1, 11, 70), noteOn (1, 60, 90) }, {}, { bend (1, 8400) }, { cc (1, 1, 20) },
            { bend (1, 8192), noteOn (1, 64, 90), noteOff (1, 60) }, { bend (1, 16383) }, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
            {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, { bend (1, 8192) }, { noteOff (1, 64) }, { noteOn (1, 67, 90) } };
        a.hop (script[0]);
        b.hop (script[0]);
        a.s.freezeHarmony = true;
        a.hop();
        b.hop();
        s2Nothing (a, "harmony frozen in place");
        check (a.stage.triadCount() == 3 && a.stage.triadNote (0) == 60, "%s: triadCount/Note: expected the frozen 60 64 67, found %d / %d",
               a.where().c_str(), a.stage.triadCount(), a.stage.triadNote (0));
        for (size_t i = 1; i < script.size(); ++i)
        {
            a.hop (script[i]);
            b.hop (script[i]);
            Events soloA, soloB;
            for (const vm::MidiEvent& e : a.last) if (stChannel (e) != 2) soloA.push_back (e);
            for (const vm::MidiEvent& e : b.last) if (stChannel (e) != 2) soloB.push_back (e);
            check (stList (soloA) == stList (soloB), "%s: solo track differs from the reference: %s vs %s", a.where().c_str(), stList (soloA).c_str(), stList (soloB).c_str());
            s2Silent (a, "harmony frozen: nothing on channel 2", 2);
        }
        check (a.stage.triadCount() == 3 && a.stage.triadNote (0) == 60, "%s: triadCount/Note after: expected the frozen 60 64 67, found %d / %d",
               a.where().c_str(), a.stage.triadCount(), a.stage.triadNote (0));
        a.soundingNow ("frozen 60 64 67, playing 67", "1:67 2:60 2:64 2:67");
        a.hop ({ bend (1, 9000), cc (1, 11, 99) });
        a.s.freezeHarmony = false;
        a.hop();
        a.soundingNow ("released during 67", "1:67 2:67 2:71 2:74");
        a.bendIs ("harmony brought up to date: bend", 2, 9000);
        s2Cc (a, "harmony brought up to date: CC11", 2, 11, 99);
        s2Cc (a, "harmony brought up to date: CC1", 2, 1, 20);
        s2Before (a, "bend before the notes", "b2", "+2:67");
        a.hop ({ noteOff (1, 67) });
        a.soundingNow ("after", "");
    }
    {
        StageRig rig = s2HarmonyRig ("freeze harmony, same chord at release");
        rig.hop ({ noteOn (1, 60, 90) });
        rig.s.freezeHarmony = true;
        rig.hop();
        rig.hop ({ noteOn (1, 64, 90), noteOff (1, 60) });
        rig.hop ({ noteOn (1, 60, 90), noteOff (1, 64) });
        rig.s.freezeHarmony = false;
        rig.hop();
        check (stNoteKeys (rig.last).empty(), "%s: same chord on the same channel: expected no note events, found %s", rig.where().c_str(), stList (rig.last).c_str());
        rig.soundingNow ("carries on", "1:60 2:60 2:64 2:67");
    }
    {   // frozen harmony while the channel changes, then released
        StageRig rig = s2HarmonyRig ("freeze harmony, channel moved");
        rig.hop ({ bend (1, 8700), cc (1, 11, 80), noteOn (1, 60, 90) });
        rig.s.freezeHarmony = true;
        rig.hop();
        rig.s.triadChannel = 3;
        rig.hop();
        s2Nothing (rig, "channel change while frozen");
        rig.s.voicing = 5;
        rig.hop ({ bend (1, 8800) });
        s2Silent (rig, "voicing change while frozen", 2);
        s2Silent (rig, "voicing change while frozen", 3);
        rig.s.freezeHarmony = false;
        rig.hop();
        rig.noteSet ("released: frozen notes end on 2, new on 3", "-2:60 -2:64 -2:67 +3:60 +3:64");
        rig.bendIs ("channel 2 handed back", 2, 8192);
        s2Cc (rig, "channel 2 handed back: CC11", 2, 11, 127);
        rig.bendIs ("channel 3 brought up to date", 3, 8800);
        s2Cc (rig, "channel 3 brought up to date: CC11", 3, 11, 80);
        s2Before (rig, "channel 3: bend before its notes", "b3", "+3:60");
        rig.hop ({ noteOff (1, 60) });
        rig.soundingNow ("after", "");
    }
    {   // the same with mpe: the channel the frozen harmony was on is handed back as well
        StageRig rig = s2MpeRig ("freeze harmony, channel moved, mpe");
        rig.s.triad = true;
        rig.s.triadChannel = 2;
        rig.s.bendRangeSemis = 2.0f;
        rig.hop ({ bend (1, 8300), cc (1, 11, 80), noteOn (1, 60, 90) });
        rig.soundingNow ("on", "2:60 2:64 2:67 3:60");
        rig.s.freezeHarmony = true;
        rig.hop();
        rig.s.triadChannel = 9;
        rig.hop ({ bend (1, 8350) });
        s2Silent (rig, "frozen", 2);
        s2Silent (rig, "frozen", 9);
        rig.s.freezeHarmony = false;
        rig.hop();
        rig.noteSet ("released", "-2:60 -2:64 -2:67 +9:60 +9:64 +9:67");
        rig.bendIs ("channel 2 handed back", 2, 8192);
        s2Cc (rig, "channel 2 handed back: CC11", 2, 11, 127);
        rig.bendIs ("channel 9 brought up to date", 9, 8350);
        s2Cc (rig, "channel 9 brought up to date: CC11", 9, 11, 80);
        rig.hop ({ noteOff (1, 60) });
        rig.releaseAll();
        rig.soundingNow ("after", "");
    }
    {
        StageRig rig = s2HarmonyRig ("freeze harmony, harmony switched off");
        rig.hop ({ bend (1, 8700), noteOn (1, 60, 90) });
        rig.s.freezeHarmony = true;
        rig.hop();
        rig.s.triad = false;
        rig.hop();
        s2Nothing (rig, "harmony off while frozen");
        rig.soundingNow ("frozen chord stays", "1:60 2:60 2:64 2:67");
        rig.s.freezeHarmony = false;
        rig.hop();
        rig.noteSet ("released", "-2:60 -2:64 -2:67");
        rig.bendIs ("channel 2 handed back", 2, 8192);
        rig.hop ({ noteOff (1, 60) });
        rig.soundingNow ("after", "");
    }
    {
        StageRig rig = s2HarmonyRig ("freeze harmony armed in silence");
        rig.s.freezeHarmony = true;
        rig.hop();
        rig.hop ({ noteOn (1, 62, 90) });
        rig.noteSet ("first chord sounds", "+1:62 +2:62 +2:65 +2:69");
        rig.hop ({ noteOn (1, 64, 90), noteOff (1, 62) });
        rig.notes ("solo only", "+1:64 -1:62");
        rig.hop ({ noteOff (1, 64) });
        rig.soundingNow ("frozen chord without main", "2:62 2:65 2:69");
        check (rig.stage.triadCount() == 3, "%s: triadCount(): expected 3, found %d", rig.where().c_str(), rig.stage.triadCount());
        rig.releaseAll();
        rig.soundingNow ("releaseAll ends the frozen chord", "");
        check (rig.stage.triadCount() == 0, "%s: triadCount() after releaseAll: expected 0, found %d", rig.where().c_str(), rig.stage.triadCount());
        rig.hop ({ noteOn (1, 60, 90) });
        rig.hop ({ noteOn (1, 62, 90), noteOff (1, 60) });
        rig.soundingNow ("switch still on: the next chord is frozen again", "1:62 2:60 2:64 2:67");
        rig.hop ({ noteOff (1, 62) });
        rig.s.freezeHarmony = false;
        rig.hop();
        rig.soundingNow ("after", "");
    }
    {
        StageRig rig = s2HarmonyRig ("freeze both");
        rig.hop ({ noteOn (1, 60, 90) });
        rig.s.freezeSolo = rig.s.freezeHarmony = true;
        rig.hop();
        for (int i = 0; i < 50; ++i)
        {
            rig.hop ({ bend (1, 8192 + i * 10), noteOn (1, 62 + (i % 2) * 2, 90), noteOff (1, i == 0 ? 60 : 62 + ((i + 1) % 2) * 2) });
            s2Nothing (rig, "both frozen");
        }
        rig.soundingNow ("frozen", "1:60 2:60 2:64 2:67");
        check (rig.stage.frozenNote() == 60, "%s: frozenNote(): expected 60, found %d", rig.where().c_str(), rig.stage.frozenNote());
        rig.releaseAll();
        rig.soundingNow ("releaseAll", "");
        check (rig.stage.frozenNote() == -1, "%s: frozenNote() after releaseAll: expected -1, found %d", rig.where().c_str(), rig.stage.frozenNote());
        for (int c = 0; c < 16; ++c)
            check (rig.lastBend[c] < 0 || rig.lastBend[c] == 8192, "%s: bend on channel %d left at %d", rig.where().c_str(), c + 1, rig.lastBend[c]);
        rig.hop ({ noteOn (1, 65, 90) });
        rig.soundingNow ("next note frozen again", "1:65 2:65 2:69 2:72");
        rig.hop ({ noteOn (1, 67, 90), noteOff (1, 65) });
        s2Nothing (rig, "frozen again");
        rig.s.freezeSolo = rig.s.freezeHarmony = false;
        rig.hop();
        rig.soundingNow ("released", "1:67 2:67 2:71 2:74");
        rig.hop ({ noteOff (1, 67) });
        rig.soundingNow ("after", "");
    }
    {
        StageRig rig = s2MpeRig ("freeze with mpe");
        rig.s.triad = true;
        rig.s.triadChannel = 2;
        rig.s.bendRangeSemis = 2.0f;
        rig.hop ({ noteOn (1, 60, 90) });
        rig.soundingNow ("on", "2:60 2:64 2:67 3:60");
        rig.s.freezeSolo = rig.s.freezeHarmony = true;
        rig.hop();
        rig.hop ({ noteOn (1, 62, 90), noteOff (1, 60) });
        s2Nothing (rig, "frozen");
        rig.s.freezeHarmony = false;
        rig.hop();
        rig.noteSet ("harmony released", "-2:60 -2:64 -2:67 +2:62 +2:65 +2:69");
        rig.s.freezeSolo = false;
        rig.hop ({ bend (1, 8500) });
        rig.soundingNow ("solo released", "2:62 2:65 2:69 4:62");
        rig.bendIs ("solo bend", 4, 8500);
        rig.hop ({ noteOff (1, 62) });
        rig.soundingNow ("after", "");
    }
}

// Open bug / contract gap (see report): a frozen track's notes on a channel the
// other track also uses. R1e and R7b cannot both hold without a rule for it.
// Minimal sequences found by the fuzz test.
inline void freezeCollisions()
{
    auto finish = [] (StageRig& rig) {
        rig.s.freezeSolo = rig.s.freezeHarmony = false;
        rig.hop();
        rig.releaseAll();
        rig.soundingNow ("end", "");
    };
    {   // R4f shared channel, harmony frozen: the next main note is one of the frozen voices
        StageRig rig = s2HarmonyRig ("collision: shared channel, harmony frozen");
        rig.s.triadChannel = 1;
        rig.hop ({ noteOn (1, 60, 90) });
        rig.soundingNow ("C E G on channel 1", "1:60 1:64 1:67");
        rig.s.freezeHarmony = true;
        rig.hop();
        rig.hop ({ noteOn (1, 64, 90), noteOff (1, 60) });   // R1e: +1:64 while the frozen 64 sounds
        rig.hop ({ noteOff (1, 64) });                         // ... and its note-off ends the frozen 64
        finish (rig);
    }
    {   // R4f shared channel, solo frozen: a harmony voice is the frozen note
        StageRig rig = s2HarmonyRig ("collision: shared channel, solo frozen");
        rig.s.triadChannel = 1;
        rig.hop ({ noteOn (1, 60, 90) });
        rig.s.freezeSolo = true;
        rig.hop();
        rig.hop ({ noteOn (1, 57, 90), noteOff (1, 60) });   // A minor without A: +1:60 while the frozen 60 sounds
        rig.hop ({ noteOff (1, 57) });
        finish (rig);
    }
    {   // mpe: the harmony channel is moved onto the channel of a frozen solo note
        StageRig rig = s2MpeRig ("collision: mpe, harmony moved onto a frozen note's channel");
        rig.s.triad = true;
        rig.s.triadChannel = 5;
        rig.s.bendRangeSemis = 2.0f;
        rig.hop ({ noteOn (1, 60, 90) });
        rig.soundingNow ("on", "2:60 5:60 5:64 5:67");
        rig.s.freezeSolo = true;
        rig.hop();
        rig.s.triadChannel = 2;
        rig.hop ({ bend (1, 8600) });                          // +2:60 while the frozen 60 sounds; b2 bends the frozen note
        s2HasNot (rig, "frozen note's channel is not bent (R7b)", "b2");
        rig.hop ({ noteOff (1, 60) });
        finish (rig);
    }
}

//==============================================================================
// 7. The engine with MPE, freeze and all notes off.
inline void s2Feed (vm::Engine& engine, const Signal& sig, vm::Settings& s, int block, ChannelState& st,
                    const std::function<void (double, vm::Settings&)>& change, std::vector<vm::MidiEvent>* all = nullptr)
{
    vm::MidiEventList out;
    out.reserve ((size_t) engine.maxEventsFor (block));
    for (long long pos = 0; pos < (long long) sig.samples.size(); pos += block)
    {
        change ((double) pos / sig.sampleRate, s);
        const int n = (int) std::min<long long> (block, (long long) sig.samples.size() - pos);
        out.clear();
        engine.process (sig.samples.data() + pos, n, s, out);
        check ((int) out.size() <= engine.maxEventsFor (n), "engine: %d events in one block, maxEventsFor %d", (int) out.size(), engine.maxEventsFor (n));
        for (const vm::MidiEvent& e : out)
        {
            st.apply (e);
            if (all != nullptr) all->push_back (e);
        }
    }
}

inline void engineMpeFreeze()
{
    {   // MPE: channels 2..16 only, bend range 48
        Synth synth (48000.0, 911);
        std::vector<Segment> segs = { rest (0.1) };
        for (int i = 0; i < 18; ++i) { segs.push_back (tone (64.3, 0.25, -20.0, 0.02)); segs.push_back (rest (0.12)); }
        const Signal sig = synth.render (segs, Timbre());
        vm::Engine engine;
        engine.prepare (sig.sampleRate);
        vm::Settings s = baseSettings();
        s.mpe = true;
        ChannelState st;
        std::vector<vm::MidiEvent> all;
        s2Feed (engine, sig, s, 128, st, [] (double, vm::Settings&) {}, &all);
        vm::MidiEventList out;
        engine.shutdown (out);
        for (const vm::MidiEvent& e : out) st.apply (e);
        bool used[16] = {};
        int notes = 0, onCh = -1;
        const int want = 8192 + (int) std::lround (0.30 / 48.0 * 8192.0);
        std::vector<int> bendsWhileOn;
        for (const vm::MidiEvent& e : all)
        {
            const int ch = e.bytes[0] & 0x0F;
            if (stIsOn (e)) { used[ch] = true; ++notes; onCh = ch; }
            if (stIsOff (e) && ch == onCh) onCh = -1;
            if (stIsBend (e) && ch == onCh) bendsWhileOn.push_back (stBend14 (e));
            if (stIsNote (e))
                check (ch >= 1, "engine mpe: note event on channel 1: %s", stText (e).c_str());
        }
        int channels = 0;
        for (int c = 1; c < 16; ++c) channels += used[c] ? 1 : 0;
        std::sort (bendsWhileOn.begin(), bendsWhileOn.end());
        const int median = bendsWhileOn.empty() ? -1 : bendsWhileOn[bendsWhileOn.size() / 2];
        check (notes == 18, "engine mpe: expected 18 notes, found %d", notes);
        check (channels == 15, "engine mpe: expected all 15 member channels used, found %d", channels);
        check (std::abs (median - want) <= 8, "engine mpe: expected the bends about %d (64.30 at range 48), median %d of %d bends",
               want, median, (int) bendsWhileOn.size());
        check (st.problems == 0 && st.text().empty(), "engine mpe: %d protocol problems, sounding at the end: %s", st.problems, st.text().c_str());
        info ("mpe: %d notes on %d channels, bend target %d, median %d", notes, channels, want, median);
    }
    {   // master switch off with mpe and harmony, frozen notes included
        Synth synth (48000.0, 912);
        const Signal sig = synth.render ({ rest (0.1), tone (60, 1.6, -20.0, 0.02), rest (0.3) }, Timbre());
        vm::Engine engine;
        engine.prepare (sig.sampleRate);
        vm::Settings s = baseSettings();
        s.mpe = true;
        s.triad = true;
        s.triadChannel = 2;
        ChannelState st;
        std::vector<vm::MidiEvent> all;
        bool sawOff = false, checkNext = false;
        std::string atOff, afterOff = "(not checked)";
        s2Feed (engine, sig, s, 96, st, [&] (double t, vm::Settings& x) {
            if (checkNext) { afterOff = st.text(); checkNext = false; }
            x.freezeSolo = x.freezeHarmony = t >= 0.5;
            const bool on = ! (t >= 0.9 && t < 1.3);
            if (! on && x.enabled) { sawOff = true; atOff = st.text(); checkNext = true; }
            x.enabled = on;
        }, &all);
        check (sawOff && ! atOff.empty(), "engine master switch: expected notes sounding (frozen) when switched off, found \"%s\"", atOff.c_str());
        check (afterOff.empty(), "engine master switch: expected nothing sounding after the first block switched off, found \"%s\"", afterOff.c_str());
        bool cc123[16] = {};
        for (const vm::MidiEvent& e : all)
            if (stIsCc (e) && e.bytes[1] == 123) cc123[e.bytes[0] & 0x0F] = true;
        int n123 = 0;
        for (int c = 1; c < 16; ++c) n123 += cc123[c] ? 1 : 0;
        check (n123 == 15 && ! cc123[0], "engine master switch, mpe: expected controller 123 on channels 2..16, found %d of them (channel 1 %s)",
               n123, cc123[0] ? "included" : "not included");
        vm::MidiEventList out;
        engine.shutdown (out);
        for (const vm::MidiEvent& e : out) st.apply (e);
        check (st.problems == 0 && st.text().empty(), "engine master switch with freeze: %d protocol problems, sounding at the end: %s", st.problems, st.text().c_str());
    }
    {   // appendAllNotesOff
        struct Case { bool mpe, triad; int main, harmony; const char* expected; };
        const Case cases[] = {
            { false, false, 1, 2, "1" },
            { false, true, 3, 5, "3 5" },
            { true, false, 1, 2, "2 3 4 5 6 7 8 9 10 11 12 13 14 15 16" },
            { true, true, 1, 1, "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16" },
        };
        for (const Case& c : cases)
        {
            vm::Settings s = baseSettings();
            s.mpe = c.mpe;
            s.triad = c.triad;
            s.midiChannel = c.main;
            s.triadChannel = c.harmony;
            vm::MidiEventList out;
            vm::Engine::appendAllNotesOff (s, 7, out);
            std::string found;
            bool ok = true;
            for (const vm::MidiEvent& e : out)
            {
                found += (found.empty() ? "" : " ") + std::to_string ((e.bytes[0] & 0x0F) + 1);
                ok = ok && stIsCc (e) && e.bytes[1] == 123 && e.bytes[2] == 0 && e.sampleOffset == 7 && e.size == 3;
            }
            check (ok && found == c.expected, "appendAllNotesOff (mpe %d, triad %d, main %d, harmony %d): expected CC123 on \"%s\", found \"%s\"%s",
                   (int) c.mpe, (int) c.triad, c.main, c.harmony, c.expected, found.c_str(), ok ? "" : " (malformed)");
        }
    }
    for (int mpe = 0; mpe < 2; ++mpe)
    {   // freeze switched on and off during audio
        Synth synth (48000.0, 913 + mpe);
        const Signal sig = synth.render ({ rest (0.1), tone (60, 0.5, -20.0, 0.02), tone (62, 0.5, -20.0, 0.0), rest (0.2),
                                           tone (64, 0.5, -20.0, 0.02), rest (0.3) }, Timbre());
        vm::Engine engine;
        engine.prepare (sig.sampleRate);
        vm::Settings s = baseSettings();
        s.mpe = mpe != 0;
        s.triad = true;
        s.triadChannel = 3;
        ChannelState st;
        int maxNotes = 0;
        s2Feed (engine, sig, s, 64, st, [&] (double t, vm::Settings& x) {
            x.freezeSolo = (t > 0.3 && t < 0.9) || t > 1.4;
            x.freezeHarmony = (t > 0.4 && t < 1.25);
            int n = 0;
            for (int c = 0; c < 16; ++c) for (int k = 0; k < 128; ++k) n += st.sounding[c][k] ? 1 : 0;
            maxNotes = std::max (maxNotes, n);
        });
        vm::MidiEventList out;
        engine.shutdown (out);
        for (const vm::MidiEvent& e : out) st.apply (e);
        check (st.problems == 0 && st.text().empty(), "engine freeze (mpe %d): %d protocol problems, sounding at the end: %s", mpe, st.problems, st.text().c_str());
        check (maxNotes <= 8, "engine freeze (mpe %d): at most 8 notes at once expected (2 solo, 6 harmony), found %d", mpe, maxNotes);
        for (int c = 0; c < 16; ++c)
            check (st.lastBend[c] < 0 || st.lastBend[c] == 8192, "engine freeze (mpe %d): bend on channel %d left at %d", mpe, c + 1, st.lastBend[c]);
    }
}

//==============================================================================
// 8. Fuzz over everything. Invariants after every hop, nothing left at the end.
inline void stageRandom2()
{
    const unsigned seeds[] = { 1, 2024, 31337, 99991 };
    const int hopsPerSeed = 80000;
    const float ranges[] = { 0.5f, 1.0f, 2.0f, 12.0f, 48.0f };
    long long totalHops = 0;
    int r0Hops = 0, frozenHops = 0, mpeHops = 0;

    for (unsigned seed : seeds)
    {
        std::mt19937 rng (seed);
        auto pick = [&rng] (int lo, int hi) { return std::uniform_int_distribution<int> (lo, hi) (rng); };
        std::normal_distribution<float> levelDist (-48.0f, 8.0f);

        char name[64];
        std::snprintf (name, sizeof (name), "random 2, seed %u", seed);
        StageRig rig (name);
        TrackerSim tracker;
        double semis = 0.0;
        const int startFailures = gFailuresInTest;
        bool clean = true;   // R0 applies: everything off since a hop with nothing sounding

        auto allOff = [] (const vm::Settings& c) { return ! c.keyFilter && ! c.triad && ! c.mpe && ! c.freezeSolo && ! c.freezeHarmony; };

        for (int h = 0; h < hopsPerSeed && gFailuresInTest - startFailures < 20; ++h)
        {
            Events in;
            const vm::Settings before = rig.s;
            if (pick (0, 120) == 0)
            {
                switch (pick (0, 16))
                {
                    case 0: rig.s.keyFilter = pick (0, 2) != 0; break;
                    case 1: rig.s.keyRoot = pick (0, 11); break;
                    case 2: rig.s.scaleMode = pick (-1, 13); break;
                    case 3: rig.s.pentatonic = pick (0, 2) == 0; break;
                    case 4: rig.s.pentaSustain = pick (0, 1) != 0; break;
                    case 5: rig.s.triad = pick (0, 2) != 0; break;
                    case 6: rig.s.triadChannel = pick (0, 3) == 0 ? rig.s.midiChannel : pick (1, 16); break;
                    case 7: rig.s.voicing = pick (0, 12); break;
                    case 8: rig.s.harmonyMode = pick (0, 1) ? vm::HarmonyMode::Chromatic : vm::HarmonyMode::Diatonic; break;
                    case 9:
                    {
                        uint64_t m = 0;
                        const int bits = pick (0, 14);
                        for (int b = 0; b < bits; ++b) m |= 1ull << pick (0, pick (0, 1) ? 48 : 63);
                        rig.s.freeVoicing = m;
                        break;
                    }
                    case 10: rig.s.mpe = pick (0, 2) == 0; break;
                    case 11: rig.s.freezeSolo = pick (0, 2) == 0; break;
                    case 12: rig.s.freezeHarmony = pick (0, 2) == 0; break;
                    case 13: rig.s.gateOffDb = (float) pick (-70, -40); break;
                    case 14:
                    {
                        const int ch = pick (1, 4);
                        if (ch != rig.s.midiChannel)
                        {
                            tracker.moveChannel (in, ch);
                            rig.s.midiChannel = ch;
                            semis = 0.0;
                        }
                        break;
                    }
                    case 15: rig.s.bendRangeSemis = ranges[pick (0, 4)]; break;
                    default:   // everything off now and then, so R0 gets checked
                        rig.s.keyFilter = rig.s.triad = rig.s.mpe = rig.s.freezeSolo = rig.s.freezeHarmony = false;
                        break;
                }
            }

            const double range = rig.s.bendRangeSemis;
            auto dyn = [&] () { return pick (0, 2) == 0 ? -1 : pick (0, 127); };
            const int a = pick (0, 999);
            if (tracker.note < 0 && a < 40)
            {
                semis = (pick (0, 200) - 100) * 0.006;
                tracker.moveBend (in, bendValue (semis, range));
                const int d = dyn();
                if (d >= 0) in.push_back (cc (tracker.channel, pick (0, 1) ? 11 : pick (1, 2), d));
                in.push_back (noteOn (tracker.channel, pick (55, 80), pick (1, 127)));
                tracker.note = in.back().bytes[1];
            }
            else if (tracker.note >= 0 && a < 50)
            {
                int n = pick (55, 80);
                if (n == tracker.note) ++n;
                semis = (pick (0, 200) - 100) * 0.006;
                tracker.moveBend (in, bendValue (semis, range));
                const int d = dyn();
                if (d >= 0) in.push_back (cc (tracker.channel, pick (0, 1) ? 11 : pick (1, 2), d));
                if (pick (0, 3) == 0) in.push_back (cc (tracker.channel, 1, pick (0, 127)));
                in.push_back (noteOn (tracker.channel, n, pick (1, 127)));
                in.push_back (noteOff (tracker.channel, tracker.note));
                tracker.note = n;
            }
            else if (tracker.note >= 0 && a < 70)
            {
                tracker.stop (in);
                if (pick (0, 2) == 0) in.push_back (cc (tracker.channel, 11, 127));
            }
            else if (a < 450)
            {
                semis = std::max (-range, std::min (range, semis + (pick (0, 200) - 100) * 0.004));
                tracker.moveBend (in, bendValue (semis, range));
            }
            else if (a < 520)
                in.push_back (cc (tracker.channel, pick (0, 2) == 0 ? 2 : (pick (0, 1) ? 1 : 11), pick (0, 127)));
            else if (a < 530)
                in.push_back (cc (tracker.channel, 7, pick (0, 127)));

            rig.level = levelDist (rng);
            const int failuresBefore = gFailuresInTest;
            rig.hop (in);
            const bool rigFailed = gFailuresInTest > failuresBefore;
            ++totalHops;
            frozenHops += rig.stage.frozenNote() >= 0 ? 1 : 0;
            mpeHops += rig.s.mpe ? 1 : 0;

            auto context = [&] () {
                char b[700];
                const vm::Settings& c = rig.s;
                std::snprintf (b, sizeof (b), "\n          seed %u hop %d; settings: filter %d root %d mode %d penta %d sustain %d triad %d triadCh %d voicing %d hmode %d free %llX "
                                              "mpe %d fzS %d fzH %d mainCh %d range %.1f gateOff %.0f, level %.1f dB\n          in: %s\n          out: %s\n          sounding: %s",
                               seed, h, (int) c.keyFilter, c.keyRoot, c.scaleMode, (int) c.pentatonic, (int) c.pentaSustain, (int) c.triad, c.triadChannel, c.voicing,
                               (int) c.harmonyMode, (unsigned long long) c.freeVoicing, (int) c.mpe, (int) c.freezeSolo, (int) c.freezeHarmony, c.midiChannel,
                               (double) c.bendRangeSemis, (double) c.gateOffDb, (double) rig.level, stList (in).c_str(), stList (rig.last).c_str(), rig.soundingText().c_str());
                return std::string (b);
            };
            if (rigFailed)
                info ("context of the failure above:%s", context().c_str());

            // R0
            const bool off = allOff (rig.s) && allOff (before);
            if (! off)
                clean = false;
            else if (! clean && ! rig.stage.isSounding() && rig.countAll() == 0 && tracker.note < 0 && rig.stage.triadCount() == 0)
                clean = true;   // from the next hop on
            else if (clean)
            {
                ++r0Hops;
                Events expected = in;
                for (vm::MidiEvent& e : expected) e.sampleOffset = rig.offset;
                bool same = rig.last.size() == expected.size();
                for (size_t i = 0; same && i < expected.size(); ++i)
                    same = rig.last[i].size == expected[i].size && std::memcmp (rig.last[i].bytes, expected[i].bytes, 3) == 0;
                if (! same)
                    check (false, "%s: R0: expected the input unchanged: %s, found %s%s", rig.where().c_str(), stList (expected).c_str(), stList (rig.last).c_str(), context().c_str());
            }

            // counts: no freeze, no mpe -> at most one main note, everything on the main or the harmony channel
            const vm::Settings& c = rig.s;
            if (! c.mpe && ! c.freezeSolo && ! c.freezeHarmony && ! before.freezeSolo && ! before.freezeHarmony && ! before.mpe)
            {
                const int mc = c.midiChannel, tc = c.triadChannel;
                const bool separate = c.triad && tc != mc;
                const int total = rig.countAll(), onTriad = separate ? rig.countOn (tc) : 0, onMain = rig.countOn (mc);
                const int mainNotes = separate || ! c.triad ? onMain : std::min (onMain, 1);
                bool ok = total == onMain + onTriad && mainNotes <= 1 && onTriad <= vm::scale::kMaxVoices && (onMain > 0 || onTriad == 0);
                if (! c.triad) ok = ok && onTriad == 0 && onMain <= 1;
                if (! ok)
                    check (false, "%s: expected at most one main note on channel %d plus harmony on %d, found %s%s", rig.where().c_str(), mc, tc,
                           rig.soundingText().c_str(), context().c_str());
            }
            // with mpe: one main note at most on the member channels besides the harmony channel (freeze off)
            if (c.mpe && before.mpe && ! c.freezeSolo && ! c.freezeHarmony && ! before.freezeSolo && ! before.freezeHarmony)
            {
                int mains = 0;
                for (int ch = 2; ch <= 16; ++ch)
                    if (! (c.triad && ch == c.triadChannel)) mains += rig.countOn (ch);
                if (mains > 1)
                    check (false, "%s: mpe: expected at most one main note, found %s%s", rig.where().c_str(), rig.soundingText().c_str(), context().c_str());
            }
            // (a note frozen before mpe was switched on stays where it is, R7)
            if (rig.countOn (1) > 0 && c.mpe && before.mpe && ! (c.triad && c.triadChannel == 1)
                && ! c.freezeHarmony && ! before.freezeHarmony && ! c.freezeSolo && ! before.freezeSolo)
                check (false, "%s: mpe: a note on channel 1 (not the harmony channel), found %s%s", rig.where().c_str(), rig.soundingText().c_str(), context().c_str());
        }

        // switch everything off: freeze off, the tracker ends its note, releaseAll
        rig.s.freezeSolo = rig.s.freezeHarmony = false;
        rig.hop();
        Events in;
        if (tracker.note >= 0) tracker.stop (in);
        rig.hop (in);
        rig.level = -90.0f;
        rig.hop();
        rig.releaseAll();
        rig.soundingNow ("end: freeze off, tracker off, releaseAll", "");
        for (int ch = 0; ch < 16; ++ch)
            check (rig.lastBend[ch] < 0 || rig.lastBend[ch] == 8192, "%s: end: bend on channel %d left at %d (seed %u; mpe %d triad %d triadCh %d mainCh %d)",
                   rig.where().c_str(), ch + 1, rig.lastBend[ch], seed, (int) rig.s.mpe, (int) rig.s.triad, rig.s.triadChannel, rig.s.midiChannel);
        check (! rig.stage.isSounding() && rig.stage.triadCount() == 0 && rig.stage.frozenNote() < 0, "%s: end: stage still reports a note", rig.where().c_str());
    }
    info ("%lld hops over %d seeds; R0 checked on %d hops, %d hops with a frozen solo note, %d with mpe", totalHops, (int) (sizeof (seeds) / sizeof (seeds[0])),
          r0Hops, frozenHops, mpeHops);
}
