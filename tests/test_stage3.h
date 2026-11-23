// ViolinMIDI - tests for ringing on (R8: overlapMs, releaseMs) and the harmony
// offset (Settings::harmonyOffset) of the output stage, plus a fuzz test over
// everything with the times switched on. Rule numbers refer to core/OutputStage.h.
//
// Included by test_main.cpp right after test_stage2.h, inside the same anonymous
// namespace; uses StageRig, TrackerSim and the helpers of test_stage.h and
// test_stage2.h.
#pragma once

//==============================================================================
// Helpers.

/** Milliseconds for `hops` at the rigs' 750 hops per second (lround (ms * 0.75) == hops). */
inline float s3Ms (int hops) { return (float) (hops * 1000.0 / 750.0); }

inline StageRig s3Rig (const char* name, int releaseHops, int overlapHops)
{
    StageRig rig (name);
    rig.s.releaseMs = s3Ms (releaseHops);
    rig.s.overlapMs = s3Ms (overlapHops);
    return rig;
}

/** The whole output of the last call, exactly. */
inline void s3Out (StageRig& rig, const char* what, const std::string& expected)
{
    const std::string found = rig.last.empty() ? "" : stList (rig.last);
    check (found == expected, "%s: %s: expected output \"%s\", found \"%s\"", rig.where().c_str(), what, expected.c_str(), found.c_str());
}

/** The value the stage should send on the harmony channel for `v` (1 + offset, the offset as a float). */
inline int s3Scaled (int v, float offset)
{
    const float o = std::min (0.0f, std::max (-0.5f, offset));
    return o >= 0.0f ? v : (int) std::lround ((float) v * (1.0f + o));
}

/** "c<ch>:11=<v>" for every step of a fade of `total` hops from `start` that changes the value at hop i. */
inline std::string s3FadeStep (int channel, int start, int total, int i, int& lastValue)
{
    const int v = start * std::max (0, total - i) / total;
    if (v == lastValue)
        return "";
    lastValue = v;
    return "c" + std::to_string (channel) + ":11=" + std::to_string (v);
}

/** With mpe the fade lowers the channel pressure along with controller 11 (R8c).
    In these tests the pressure starts at the controller's value, so the steps are the same. */
inline std::string s3MpeFadeStep (int channel, int start, int n, int i, int& last)
{
    const std::string step = s3FadeStep (channel, start, n, i, last);
    return step.empty() ? step : step + " p" + std::to_string (channel) + "=" + std::to_string (last);
}

inline std::string s3Join (std::initializer_list<std::string> parts)
{
    std::string s;
    for (const std::string& p : parts)
        if (! p.empty())
            s += (s.empty() ? "" : " ") + p;
    return s;
}

/** The CC11 values sent on `channel` in the last output, in order. */
inline std::vector<int> s3Cc11 (const StageRig& rig, int channel)
{
    std::vector<int> v;
    for (const vm::MidiEvent& e : rig.last)
        if (stIsCc (e) && stChannel (e) == channel && e.bytes[1] == 11)
            v.push_back (e.bytes[2]);
    return v;
}

//==============================================================================
// 1. Release (R8b, R8c, R8e).
inline void ringOnRelease()
{
    const int N = 10;

    // --- solo, no harmony: exact events per hop, for several start values
    struct Case { const char* what; int sentCc11; int sentCc1; };
    const Case cases[] = {
        { "tracker had sent CC11 = 90", 90, -1 },
        { "tracker had sent no CC11", -1, -1 },
        { "dynamics on CC1 only", -1, 80 },
        { "tracker had sent CC11 = 7 (fewer steps than hops)", 7, -1 },
    };
    for (const Case& c : cases)
    {
        StageRig rig = s3Rig (c.what, N, 0);
        Events start;
        if (c.sentCc11 >= 0) start.push_back (cc (1, 11, c.sentCc11));
        if (c.sentCc1 >= 0) start.push_back (cc (1, 1, c.sentCc1));
        start.push_back (noteOn (1, 60, 100));
        rig.hop (start);
        rig.notes ("start", "+1:60");
        rig.hop ({ noteOff (1, 60) });
        s3Out (rig, "tracker's note-off: nothing yet", "");
        check (! rig.stage.isSounding(), "%s: the stage must not report a main note while it only rings", rig.where().c_str());
        rig.soundingNow ("ringing", "1:60");

        const int from = c.sentCc11 >= 0 ? c.sentCc11 : 127;
        int lastValue = from;
        int previous = from;
        for (int i = 1; i <= N; ++i)
        {
            rig.hop();
            const std::string step = s3FadeStep (1, from, N, i, lastValue);
            if (i < N)
            {
                s3Out (rig, "fade step", step);
            }
            else
            {
                const std::string restore = from == 0 ? "" : "c1:11=" + std::to_string (from);
                s3Out (rig, "last hop: 0, note-off, controller back", s3Join ({ step, "-1:60", restore }));
            }
            // independent of the formula: strictly down, never a value twice; at 0 when the note-off goes out
            // (a small start value may reach 0 a hop or two early: integer steps)
            const std::vector<int> values = s3Cc11 (rig, 1);
            if (i < N)
                for (int v : values)
                {
                    check (v < previous && (v > 0 || from < N), "%s: fade must go down strictly and stay above 0 before the end, found %d after %d", rig.where().c_str(), v, previous);
                    previous = v;
                }
            else if (from > 0)
                check ((! values.empty() && values.front() == 0) || (previous == 0 && values.size() == 1), "%s: the note-off must come at CC11 = 0, found %s",
                       rig.where().c_str(), stList (rig.last).c_str());
            s2HasNot (rig, "CC1 not touched by the fade", "c1:1");
        }
        rig.soundingNow ("after the release", "");
        s2Cc (rig, "CC11 back where it would be", 1, 11, from);
        if (c.sentCc1 >= 0) s2Cc (rig, "CC1 untouched", 1, 1, c.sentCc1);
        rig.hop();
        s3Out (rig, "afterwards", "");
    }

    // --- held back bend and CC11 during the ringing, solo
    {
        StageRig rig = s3Rig ("release: bend and CC11 held back", N, 0);
        rig.hop ({ cc (1, 11, 90), noteOn (1, 60, 100) });
        rig.hop ({ noteOff (1, 60) });
        rig.hop ({ bend (1, 9000) });                 // fade hop 1
        s2HasNot (rig, "bend held back", "b1");
        rig.hop ({ cc (1, 11, 110) });                // fade hop 2
        check (s3Cc11 (rig, 1) == std::vector<int> { 90 * (N - 2) / N }, "%s: CC11 110 must be held back, only the fade step %d sent, found %s",
               rig.where().c_str(), 90 * (N - 2) / N, stList (rig.last).c_str());
        for (int i = 3; i < N; ++i) rig.hop();
        rig.hop();
        s3Out (rig, "end: note-off, then CC11 and bend made up", "c1:11=0 -1:60 c1:11=110 b1=9000");
    }

    // --- harmony: both channels fade, both end, with and without the offset
    for (float offset : { 0.0f, -0.5f, -0.3f })
    {
        char name[80];
        std::snprintf (name, sizeof (name), "release with harmony, offset %.2f", (double) offset);
        StageRig rig = s3Rig (name, N, 0);
        rig.s.triad = true;
        rig.s.triadChannel = 2;
        rig.s.harmonyOffset = offset;
        rig.hop ({ cc (1, 11, 90), noteOn (1, 60, 100) });
        rig.soundingNow ("start", "1:60 2:60 2:64 2:67");
        const int h = s3Scaled (90, offset);
        s2Cc (rig, "harmony CC11 scaled", 2, 11, h);
        rig.hop ({ noteOff (1, 60) });
        s3Out (rig, "tracker's note-off: nothing yet", "");
        int l1 = 90, l2 = h;
        for (int i = 1; i <= N; ++i)
        {
            Events in;
            if (i == 3) in.push_back (bend (1, 9000));
            if (i == 5) in.push_back (cc (1, 11, 100));
            rig.hop (in);
            const std::string a = s3FadeStep (1, 90, N, i, l1), b = s3FadeStep (2, h, N, i, l2);
            if (i < N)
                s3Out (rig, "fade steps on both channels, nothing else", s3Join ({ a, b }));
            else
                s3Out (rig, "end on both channels, controllers and bends made up",
                       s3Join ({ a, "-1:60", "c1:11=100", "b1=9000", b, "-2:60 -2:64 -2:67", "c2:11=" + std::to_string (s3Scaled (100, offset)), "b2=9000" }));
        }
        rig.soundingNow ("after", "");
        rig.hop();
        s3Out (rig, "afterwards", "");
    }

    // --- dynamics on CC1 only, with harmony: CC1 mirrored and never faded
    {
        StageRig rig = s3Rig ("release with harmony, dynamics on CC1", N, 0);
        rig.s.triad = true;
        rig.s.harmonyOffset = -0.5f;
        rig.hop ({ cc (1, 1, 80), noteOn (1, 60, 100) });
        s2Cc (rig, "CC1 mirrored and scaled", 2, 1, 40);
        rig.hop ({ noteOff (1, 60) });
        for (int i = 1; i <= N; ++i)
        {
            rig.hop (i == 4 ? Events { cc (1, 1, 30) } : Events {});
            if (i == 4)
            {
                s2Has (rig, "CC1 not held back", "c1:1");
                s2Cc (rig, "CC1 mirrored during the ringing", 2, 1, 15);
            }
        }
        rig.soundingNow ("after", "");
        s2Cc (rig, "main CC11 back to 127", 1, 11, 127);
        s2Cc (rig, "harmony CC11 back to 127 (nothing mirrored)", 2, 11, 127);
    }

    // --- mpe: the release fades on the note's own channel
    {
        StageRig rig = s2MpeRig ("release, mpe");
        rig.s.releaseMs = s3Ms (N);
        rig.hop();
        rig.hop ({ cc (1, 11, 80), noteOn (1, 60, 100) });
        rig.notes ("start", "+2:60");
        rig.hop ({ noteOff (1, 60) });
        s3Out (rig, "nothing yet", "");
        int l = 80;
        for (int i = 1; i <= N; ++i)
        {
            rig.hop();
            const std::string a = s3MpeFadeStep (2, 80, N, i, l);
            s3Out (rig, "fade on channel 2", i < N ? a : s3Join ({ a, "-2:60 c2:11=127" }));
        }
    }
}

//==============================================================================
// 2. Overlap (R8a).
inline void ringOnOverlap()
{
    const int N = 10;

    {   // without mpe: the old note rings at full level on the shared channel
        StageRig rig = s3Rig ("overlap, solo", 0, N);
        rig.hop ({ cc (1, 11, 90), noteOn (1, 60, 100) });
        rig.hop ({ bend (1, 8300), noteOn (1, 62, 100), noteOff (1, 60) });
        s3Out (rig, "legato: new note on, old one rings", "b1=8300 +1:62v100");
        rig.soundingNow ("both", "1:60 1:62");
        for (int i = 1; i <= N; ++i)
        {
            rig.hop (i == 5 ? Events { cc (1, 11, 70) } : Events {});
            if (i == 5)
                s3Out (rig, "CC11 of the live note goes out at once", "c1:11=70");
            else
                s3Out (rig, "full level, no fade", i < N ? "" : "-1:60");
        }
        rig.soundingNow ("after", "1:62");
        rig.hop ({ noteOff (1, 62) });
        rig.notes ("end", "-1:62");
    }
    {   // mpe: the old note fades on its own channel, the new one is untouched
        StageRig rig = s2MpeRig ("overlap, mpe");
        rig.s.overlapMs = s3Ms (N);
        rig.hop();
        rig.hop ({ cc (1, 11, 80), noteOn (1, 60, 90) });
        rig.notes ("start", "+2:60");
        rig.hop ({ cc (1, 11, 70), noteOn (1, 62, 90), noteOff (1, 60) });
        rig.notes ("legato", "+3:62");
        s2HasNot (rig, "no fade step yet", "c2:11");
        int l2 = 80, l3 = 70;
        bool second = false;
        for (int i = 1; i <= N; ++i)
        {
            Events in;
            if (i == 3) { in = { cc (1, 11, 60), noteOn (1, 64, 90), noteOff (1, 62) }; second = true; }
            rig.hop (in);
            const std::string a = s3MpeFadeStep (2, 80, N, i, l2);
            const std::string b = second && i > 3 ? s3MpeFadeStep (3, 70, N, i - 3, l3) : "";
            if (i == 3)
            {
                rig.notes ("second legato: channel 4, not the ringing 2 or 3", "+4:64");
                s2Has (rig, "fade on 2 goes on", "c2:11");
            }
            else
            {
                s3Out (rig, "fades on 2 and 3 only", s3Join ({ a, i == N ? "-2:60 c2:11=127" : "", b }));
            }
        }
        s2Cc (rig, "channel 2 controller back at 127", 2, 11, 127);
        for (int i = 0; i < 3; ++i) rig.hop();
        s2Cc (rig, "channel 3 controller back at 127", 3, 11, 127);
        rig.soundingNow ("only the live note", "4:64");
        s2Cc (rig, "channel 4 untouched", 4, 11, 60);
    }
    {   // harmony at a legato change, and a voice common to both chords
        StageRig rig = s3Rig ("overlap, harmony at legato", 0, N);
        rig.s.triad = true;
        rig.hop ({ noteOn (1, 60, 100) });
        rig.hop ({ noteOn (1, 62, 100), noteOff (1, 60) });
        rig.notes ("D minor on, C major rings", "+1:62 +2:62 +2:65 +2:69");
        rig.soundingNow ("all", "1:60 1:62 2:60 2:62 2:64 2:65 2:67 2:69");
        for (int i = 1; i < N; ++i) { rig.hop(); s3Out (rig, "ringing, nothing sent", ""); }
        rig.hop();
        rig.noteSet ("old notes end", "-1:60 -2:60 -2:64 -2:67");
        s2HasNot (rig, "no controller", "c2:11");
        rig.soundingNow ("new chord", "1:62 2:62 2:65 2:69");

        rig.hop ({ noteOn (1, 64, 100), noteOff (1, 62) });   // E minor: 64 67 71, nothing in common with D minor
        rig.hop ({ noteOn (1, 60, 100), noteOff (1, 64) });   // C major 60 64 67: 64 and 67 are still ringing
        rig.notes ("common voices: off first, then on, never doubled", "+1:60 +2:60 -2:64 +2:64 -2:67 +2:67");
        for (int i = 0; i < N; ++i) rig.hop();
        rig.soundingNow ("C major only", "1:60 2:60 2:64 2:67");
        check (rig.stage.triadCount() == 3, "%s: triadCount() expected 3, found %d", rig.where().c_str(), rig.stage.triadCount());
    }
    {   // R4c exchange
        StageRig rig = s3Rig ("overlap, harmony exchange", 0, N);
        rig.s.triad = true;
        rig.hop ({ noteOn (1, 60, 100) });
        rig.hop ({ bend (1, 16383) });
        int at = -1;
        for (int i = 0; i < 40 && at < 0; ++i)
        {
            rig.hop();
            if (! stNoteKeys (rig.last).empty()) at = i;
        }
        rig.notes ("exchange: new voices on, old ones ring", "+2:62 +2:65 +2:69");
        for (int i = 1; i < N; ++i) { rig.hop(); s3Out (rig, "ringing", ""); }
        rig.hop();
        rig.notes ("old voices end", "-2:60 -2:64 -2:67");
        rig.soundingNow ("D minor", "1:60 2:62 2:65 2:69");
    }
    {   // the same pitch re-struck (R3: sounding 62 stands for tracker note 61): no overlap
        StageRig rig = s3Rig ("overlap, same pitch", 0, N);
        rig.key (0, 0, false);
        rig.hop ({ noteOn (1, 61, 100) });
        rig.hop ({ bend (1, bendValue (1.0, 2.0)) });
        s2WaitForOn (rig, 40);
        rig.soundingNow ("slid into 62", "1:62");
        rig.hop ({ bend (1, 8192), noteOn (1, 62, 100), noteOff (1, 61) });
        rig.notes ("re-struck: off then on, at once", "-1:62 +1:62");
        for (int i = 0; i <= N; ++i) { rig.hop(); check (stNoteKeys (rig.last).empty(), "%s: nothing may ring on: %s", rig.where().c_str(), stList (rig.last).c_str()); }
        rig.soundingNow ("62", "1:62");
    }
    {   // shared channel (R4f): the old note ends before the new one starts - no overlap (tested reading)
        StageRig rig = s3Rig ("overlap, shared channel", 0, N);
        rig.s.triad = true;
        rig.s.triadChannel = 1;
        rig.hop ({ noteOn (1, 60, 100) });
        rig.soundingNow ("C major on one channel", "1:60 1:64 1:67");
        rig.hop ({ noteOn (1, 62, 100), noteOff (1, 60) });
        rig.notes ("old off, then new on", "-1:60 -1:64 -1:67 +1:62 +1:65 +1:69");
        for (int i = 0; i <= N; ++i) { rig.hop(); s3Out (rig, "nothing rings", ""); }
    }
    {   // shared channel (R4f) release: one fade for main and harmony, all end together
        StageRig rig = s3Rig ("release, shared channel", N, 0);
        rig.s.triad = true;
        rig.s.triadChannel = 1;
        rig.hop ({ noteOn (1, 60, 100) });
        rig.hop ({ noteOff (1, 60) });
        int l = 127;
        for (int i = 1; i <= N; ++i)
        {
            rig.hop();
            const std::string a = s3FadeStep (1, 127, N, i, l);
            if (i < N) s3Out (rig, "one fade step per hop", a);
        }
        rig.noteSet ("all end", "-1:60 -1:64 -1:67");
        s2Cc (rig, "CC11 back", 1, 11, 127);
        check (stCount (rig.last, "c1:11") == 2, "%s: expected the last step and one restore, found %s", rig.where().c_str(), stList (rig.last).c_str());
    }
    {   // a long overlap on the shared main channel, then a short release: nothing may come back to full level after the fade
        StageRig rig = s3Rig ("overlap longer than release", N, 3 * N);
        rig.hop ({ noteOn (1, 60, 100) });
        rig.hop ({ noteOn (1, 62, 100), noteOff (1, 60) });   // 60 rings for 30 hops at full level
        rig.hop ({ noteOff (1, 62) });                        // 62 fades over 10 hops - and with it 60, same channel
        bool faded = false;
        for (int i = 1; i <= 3 * N; ++i)
        {
            rig.hop();
            for (const vm::MidiEvent& e : rig.last)
            {
                if (stIsCc (e) && e.bytes[1] == 11 && e.bytes[2] == 0) faded = true;
                if (faded && stIsCc (e) && e.bytes[1] == 11 && e.bytes[2] > 0)
                    check (rig.countOn (1) == 0, "%s: CC11 back up to %d after the fade while %s still sounds (it was faded to 0 with it and comes back at full level)",
                           rig.where().c_str(), (int) e.bytes[2], rig.soundingText().c_str());
            }
        }
        rig.soundingNow ("end", "");
    }
}

//==============================================================================
// 3. Interrupted ringing (R8c, R8d, R8f).
inline void ringOnInterrupted()
{
    const int N = 10;

    for (int overlap : { 0, 4 })
    {
        char name[80];
        std::snprintf (name, sizeof (name), "new note during a release, overlap %d hops", overlap);
        StageRig rig = s3Rig (name, N, overlap);
        rig.hop ({ cc (1, 11, 90), noteOn (1, 60, 100) });
        rig.hop ({ noteOff (1, 60) });
        for (int i = 0; i < 4; ++i) rig.hop();
        s2Cc (rig, "mid-fade", 1, 11, 90 * (N - 4) / N);
        rig.hop ({ bend (1, 8500), cc (1, 11, 50), noteOn (1, 62, 100) });
        if (overlap == 0)
            s3Out (rig, "fade stops, controller is the new note's, old note ends, then the new note", "c1:11=45 c1:11=50 -1:60 b1=8500 +1:62v100");
        else
            s3Out (rig, "fade stops, controller is the new note's, new note on", "c1:11=45 c1:11=50 b1=8500 +1:62v100");
        for (int i = 1; i <= overlap; ++i)
        {
            rig.hop();
            s3Out (rig, "no fade any more", i < overlap ? "" : "-1:60");
        }
        rig.soundingNow ("new note only", "1:62");
    }
    for (int overlap : { 0, 4 })
    {
        char name[80];
        std::snprintf (name, sizeof (name), "same note again during its release, overlap %d", overlap);
        StageRig rig = s3Rig (name, N, overlap);
        rig.hop ({ noteOn (1, 60, 100) });
        rig.hop ({ noteOff (1, 60) });
        rig.hop();
        rig.hop();
        rig.hop ({ noteOn (1, 60, 110) });
        s3Out (rig, "off, then on", "c1:11=88 c1:11=127 -1:60 +1:60v110");
        for (int i = 0; i <= N; ++i) { rig.hop(); s3Out (rig, "nothing more", ""); }
        rig.soundingNow ("60", "1:60");
    }
    {   // more than 8 ringing at once, mpe (each on its own channel) and harmony legato chains (two per change)
        StageRig rig = s2MpeRig ("many releases, mpe");
        rig.s.releaseMs = s3Ms (200);
        rig.s.triad = true;
        rig.s.triadChannel = 16;
        rig.hop();
        int maxSounding = 0;
        for (int k = 0; k < 14; ++k)
        {
            rig.hop ({ noteOn (1, 60 + k, 100) });
            rig.hop ({ noteOff (1, 60 + k) });
            maxSounding = std::max (maxSounding, rig.countAll());
            for (int ch = 1; ch <= 16; ++ch)
                check (rig.countOn (ch) <= (ch == 16 ? 10 : 1), "%s: channel %d has %d notes", rig.where().c_str(), ch, rig.countOn (ch));
        }
        for (int i = 0; i < 202; ++i) rig.hop();
        rig.soundingNow ("all rang out by themselves", "");
        for (int ch = 1; ch <= 16; ++ch)
            check (rig.lastCc11[ch - 1] != 0, "%s: CC11 left at 0 on channel %d", rig.where().c_str(), ch);
        info ("many releases, mpe: at most %d notes at once", maxSounding);
    }
    {
        StageRig rig = s3Rig ("many overlaps, harmony legato chain", 50, 300);
        rig.s.triad = true;
        TrackerSim t;
        Events in;
        t.start (in, 60, 100, 8192);
        rig.hop (in);
        for (int k = 1; k <= 12; ++k)
        {
            in.clear();
            t.change (in, 60 + (k * 5) % 12, 100, 8192);
            rig.hop (in);
        }
        in.clear();
        t.stop (in);
        rig.hop (in);
        for (int i = 0; i < 302; ++i) rig.hop();
        rig.soundingNow ("all rang out", "");
        s2Cc (rig, "main CC11", 1, 11, 127);
        check (rig.lastCc11[1] != 0, "%s: harmony CC11 left at 0", rig.where().c_str());
    }
    {   // a held note never rings on
        StageRig rig = s3Rig ("held note, release", N, N);
        rig.key (0, 0, true, true);
        rig.hop ({ noteOn (1, 60, 100) });
        rig.hop ({ noteOff (1, 60) });
        s3Out (rig, "held", "");
        check (rig.stage.isHeld(), "%s: expected the note held", rig.where().c_str());
        rig.level = -60.0f;
        rig.hop();
        s3Out (rig, "below the gate: the held note ends at once", "-1:60");
        rig.level = -20.0f;
        rig.hop ({ noteOn (1, 62, 100) });
        rig.hop ({ noteOn (1, 61, 100), noteOff (1, 62) });
        s3Out (rig, "foreign note: 62 held", "");
        rig.hop ({ noteOn (1, 64, 100), noteOff (1, 61) });
        s3Out (rig, "allowed note: held one ends at once, no overlap", "-1:62 +1:64v100");
        for (int i = 0; i <= N; ++i) { rig.hop(); s3Out (rig, "nothing rings", ""); }
    }
    {   // freeze switched on during a fade on that channel
        StageRig rig = s3Rig ("freeze during a fade", N, N);
        rig.hop ({ noteOn (1, 60, 100) });
        rig.hop ({ noteOff (1, 60) });
        rig.hop();
        rig.hop();
        rig.s.freezeSolo = true;
        rig.hop();
        s2Has (rig, "nothing frozen yet, the fade goes on", "c1:11");
        rig.hop ({ noteOn (1, 62, 100) });
        rig.noteSet ("the new note is frozen: the ringing note ends at once", "-1:60 +1:62");
        s2Before (rig, "fade ended before the new note", "c1:11", "+1:62");
        check (rig.lastCc11[0] == 127 && s3Cc11 (rig, 1).back() == 127, "%s: CC11 back at 127 expected (the step of this hop may come first), found %s",
               rig.where().c_str(), stList (rig.last).c_str());
        for (int i = 0; i <= N; ++i) { rig.hop(); s3Out (rig, "frozen note not faded", ""); }
        rig.hop ({ noteOff (1, 62) });
        s3Out (rig, "frozen: the tracker's note-off is not sent", "");
        for (int i = 0; i <= N; ++i) { rig.hop(); s3Out (rig, "frozen track: no tail", ""); }
        rig.s.freezeSolo = false;
        rig.hop();
        s3Out (rig, "freeze off: the frozen note ends", "-1:62");
    }
    {   // freeze on while a live note sounds with an overlap ringing on its channel
        StageRig rig = s3Rig ("freeze with an overlap ringing", N, N);
        rig.hop ({ noteOn (1, 60, 100) });
        rig.hop ({ noteOn (1, 62, 100), noteOff (1, 60) });
        rig.s.freezeSolo = true;
        rig.hop ({ noteOn (1, 64, 100), noteOff (1, 62) });
        rig.noteSet ("the ringing note ends at once, nothing else", "-1:60");
        for (int i = 0; i <= N; ++i) { rig.hop(); s3Out (rig, "frozen: nothing", ""); }
        rig.soundingNow ("frozen 62", "1:62");
    }
    {   // the harmony rings on its own, the solo track is frozen on the SAME channel (R4f): the frozen note must not be faded (R7e)
        StageRig rig = s3Rig ("release of the harmony beside a frozen solo note (R4f)", N, 0);
        rig.s.triad = true;
        rig.s.triadChannel = 1;
        rig.hop ({ noteOn (1, 60, 100) });
        rig.s.freezeSolo = true;
        rig.hop();
        rig.hop ({ noteOn (1, 62, 100), noteOff (1, 60) });
        rig.hop ({ noteOff (1, 62) });
        for (int i = 0; i <= N; ++i)
        {
            rig.hop();
            s2HasNot (rig, "no controller 11 on the channel of the frozen solo note", "c1:11");
        }
        check (rig.sounding[0][60], "%s: the frozen 60 must still sound", rig.where().c_str());
        rig.s.freezeSolo = false;
        rig.hop();
        rig.soundingNow ("end", "");
    }

    {   // the other way round: a solo release on the channel of frozen harmony notes (R4f) does not fade them (tested reading, see report)
        StageRig rig = s3Rig ("solo release beside frozen harmony notes (R4f)", N, 0);
        rig.s.triad = true;
        rig.s.triadChannel = 1;
        rig.hop ({ noteOn (1, 60, 100) });
        rig.s.freezeHarmony = true;
        rig.hop();
        rig.hop ({ noteOff (1, 60) });
        for (int i = 1; i <= N; ++i)
        {
            rig.hop();
            s2HasNot (rig, "no fade on the channel of the frozen chord", "c1:11");
            s3Out (rig, "60 rings at full level, then ends", i < N ? "" : "-1:60");
        }
        rig.soundingNow ("the frozen chord stays", "1:64 1:67");
        rig.s.freezeHarmony = false;
        rig.hop();
        rig.soundingNow ("end", "");
    }

    // settings changes and releaseAll during ringing: everything ends at once
    for (int what = 0; what < 7; ++what)
    {
        const char* names[] = { "voicing", "triadChannel", "midiChannel", "mpe on", "keyRoot", "releaseAll", "harmony off" };
        char name[80];
        std::snprintf (name, sizeof (name), "ringing ends at a change of %s", names[what]);
        StageRig rig = s3Rig (name, 2 * N, 2 * N);
        rig.s.triad = true;
        rig.hop ({ cc (1, 11, 90), noteOn (1, 60, 100) });
        rig.hop ({ bend (1, 8400), noteOn (1, 62, 100), noteOff (1, 60) });
        rig.hop ({ noteOff (1, 62) });
        rig.hop ({ bend (1, 8192) });
        rig.hop();
        rig.hop();
        check (rig.countAll() == 8, "%s: expected 8 notes ringing, found %s", rig.where().c_str(), rig.soundingText().c_str());
        switch (what)
        {
            case 0: rig.s.voicing = 3; break;
            case 1: rig.s.triadChannel = 5; break;
            case 2: rig.s.midiChannel = 3; break;
            case 3: rig.s.mpe = true; break;
            case 4: rig.s.keyRoot = 2; break;
            case 6: rig.s.triad = false; break;
            default: break;
        }
        if (what == 5) rig.releaseAll(); else rig.hop();
        rig.soundingNow ("everything ended at once", "");
        for (int ch = 1; ch <= 2; ++ch)
            check (rig.lastCc11[ch - 1] != 0, "%s: CC11 on channel %d left at %d", rig.where().c_str(), ch, rig.lastCc11[ch - 1]);
        s2Cc (rig, "main CC11 back at the tracker's value", 1, 11, 90);
        if (what == 1 || what == 5 || what == 6)
            s2Cc (rig, "harmony channel left: CC11 neutral", 2, 11, 127);
        else
            s2Cc (rig, "harmony CC11 at the mirrored value", 2, 11, 90);
        if (what == 5)
            for (int ch = 1; ch <= 16; ++ch)
                check (rig.lastBend[ch - 1] < 0 || rig.lastBend[ch - 1] == 8192, "%s: bend on %d left at %d", rig.where().c_str(), ch, rig.lastBend[ch - 1]);
        else
        {
            check (rig.lastBend[0] == 8192, "%s: held-back bend made up on channel 1, found %d", rig.where().c_str(), rig.lastBend[0]);
        }
        for (int i = 0; i < 3 * N; ++i) { rig.hop(); check (stNoteKeys (rig.last).empty(), "%s: nothing may follow: %s", rig.where().c_str(), stList (rig.last).c_str()); }
    }

    {   // a CC11 the tracker sends while an overlap (no fade) rings with no live note must not get lost (R8e)
        StageRig rig = s3Rig ("CC11 held back behind an overlap", 0, N);
        rig.hop ({ cc (1, 11, 90), noteOn (1, 60, 100) });
        rig.hop ({ noteOn (1, 62, 100), noteOff (1, 60) });
        rig.hop ({ noteOff (1, 62) });                // no release: 62 ends at once, 60 rings
        rig.notes ("62 ends", "-1:62");
        rig.hop ({ cc (1, 11, 40), bend (1, 9000) });
        s3Out (rig, "held back", "");
        for (int i = 0; i < N; ++i) rig.hop();
        rig.soundingNow ("end", "");
        rig.bendIs ("bend made up", 1, 9000);
        s2Cc (rig, "CC11 made up", 1, 11, 40);
    }
    {   // the same on the harmony channel after an exchange (R4c), and a new note before the ringing ends
        StageRig rig = s3Rig ("harmony CC11 held back behind an overlap", 0, 3 * N);
        rig.s.triad = true;
        rig.hop ({ cc (1, 11, 90), noteOn (1, 60, 100) });
        rig.hop ({ bend (1, 16383) });
        s2WaitForOn (rig, 40);                     // exchange: C major rings for 30 hops
        rig.hop ({ noteOff (1, 60) });             // harmony ends at once, the old voices still ring
        rig.hop ({ cc (1, 11, 40) });              // mirrored to channel 2: held back there
        s2HasNot (rig, "held back on 2", "c2:11");
        rig.hop ({ bend (1, 8192), noteOn (1, 65, 100) });  // a new note: channel 2 must be brought up to date
        s2Cc (rig, "harmony CC11 up to date at the new note", 2, 11, 40);
        for (int i = 0; i < 3 * N; ++i) rig.hop();
        s2Cc (rig, "harmony CC11 still up to date", 2, 11, 40);
    }
}

//==============================================================================
// 4. Harmony offset.
inline void harmonyOffsetTests()
{
    for (float offset : { 0.0f, -0.1f, -0.25f, -0.3f, -0.5f })
    {
        char name[80];
        std::snprintf (name, sizeof (name), "harmony offset %.2f", (double) offset);
        for (int vel : { 1, 2, 3, 5, 15, 64, 90, 100, 127 })
        {
            StageRig rig (name);
            rig.s.triad = true;
            rig.s.harmonyOffset = offset;
            rig.hop ({ cc (1, 1, 100), cc (1, 2, 50), cc (1, 11, vel), noteOn (1, 60, vel) });
            const int expected = std::max (1, s3Scaled (vel, offset));
            for (const vm::MidiEvent& e : rig.last)
            {
                if (stIsOn (e) && stChannel (e) == 2)
                    check (e.bytes[2] == expected, "%s: velocity %d: harmony note-on %s, expected velocity %d", rig.where().c_str(), vel, stText (e).c_str(), expected);
                if (stIsOn (e) && stChannel (e) == 1)
                    check (e.bytes[2] == vel, "%s: main velocity unchanged, found %s", rig.where().c_str(), stText (e).c_str());
            }
            s2Cc (rig, "CC1 scaled", 2, 1, s3Scaled (100, offset));
            s2Cc (rig, "CC2 scaled", 2, 2, s3Scaled (50, offset));
            s2Cc (rig, "CC11 scaled", 2, 11, s3Scaled (vel, offset));
            s2Cc (rig, "main CC11 unscaled", 1, 11, vel);
            rig.s.triad = false;
            rig.hop();
            s2Cc (rig, "hand-back CC1 neutral", 2, 1, 0);
            s2Cc (rig, "hand-back CC2 neutral, not scaled", 2, 2, 127);
            s2Cc (rig, "hand-back CC11 neutral, not scaled", 2, 11, 127);
        }
    }
    {   // offset 0 is the old behaviour: identical output
        StageRig a ("offset 0 vs default"), b ("offset 0 vs default");
        a.s.triad = b.s.triad = true;
        b.s.harmonyOffset = 0.0f;
        a.s.harmonyOffset = 0.3f;   // clamped to 0
        TrackerSim t;
        std::mt19937 rng (7);
        for (int h = 0; h < 3000; ++h)
        {
            Events in;
            const int r = (int) (rng() % 100);
            if (t.note < 0 && r < 5) t.start (in, 55 + (int) (rng() % 20), 1 + (int) (rng() % 127), 8192, (int) (rng() % 128));
            else if (t.note >= 0 && r < 8) t.change (in, 55 + (int) (rng() % 20), 1 + (int) (rng() % 127), 8192 + (int) (rng() % 2000) - 1000);
            else if (t.note >= 0 && r < 10) t.stop (in);
            else if (r < 30) in.push_back (cc (1, (int) (rng() % 3) == 0 ? 1 : 11, (int) (rng() % 128)));
            a.hop (in);
            b.hop (in);
            check (stList (a.last) == stList (b.last), "offset +0.3 (clamped) and 0: outputs differ at hop %d: %s vs %s", h, stList (a.last).c_str(), stList (b.last).c_str());
            if (stList (a.last) != stList (b.last)) break;
        }
    }
    {   // changed mid-note: from the next event on
        StageRig rig ("offset changed mid-note");
        rig.s.triad = true;
        rig.hop ({ cc (1, 11, 100), noteOn (1, 60, 100) });
        s2Cc (rig, "before", 2, 11, 100);
        rig.s.harmonyOffset = -0.5f;
        rig.hop();
        s3Out (rig, "nothing sent by the change itself", "");
        rig.hop ({ cc (1, 11, 80) });
        s2Cc (rig, "next controller scaled", 2, 11, 40);
        rig.hop ({ noteOn (1, 62, 100), noteOff (1, 60) });
        for (const vm::MidiEvent& e : rig.last)
            if (stIsOn (e) && stChannel (e) == 2)
                check (e.bytes[2] == 50, "%s: next harmony velocity 50 expected, found %s", rig.where().c_str(), stText (e).c_str());
    }
    {   // clamped below -0.5
        StageRig rig ("offset -0.9 clamped");
        rig.s.triad = true;
        rig.s.harmonyOffset = -0.9f;
        rig.hop ({ cc (1, 11, 100), noteOn (1, 60, 100) });
        s2Cc (rig, "CC11 at half", 2, 11, 50);
        for (const vm::MidiEvent& e : rig.last)
            if (stIsOn (e) && stChannel (e) == 2)
                check (e.bytes[2] == 50, "%s: velocity 50 expected, found %s", rig.where().c_str(), stText (e).c_str());
    }
    {   // mpe: harmony on its own channel inside the zone, scaled as well
        StageRig rig = s2MpeRig ("offset, mpe");
        rig.s.triad = true;
        rig.s.triadChannel = 16;
        rig.s.harmonyOffset = -0.5f;
        rig.hop();
        rig.hop ({ cc (1, 11, 100), noteOn (1, 60, 100) });
        s2Cc (rig, "harmony CC11", 16, 11, 50);
        s2Cc (rig, "main CC11", 2, 11, 100);
    }
}

//==============================================================================
// 5. Fuzz with ringing on: invariants after every hop, nothing left at the end.
inline void stageRandom3()
{
    const unsigned seeds[] = { 3, 77, 4242, 65537 };
    const int hopsPerSeed = 80000;
    const float ranges[] = { 0.5f, 1.0f, 2.0f, 12.0f, 48.0f };
    const float times[] = { 0.0f, 0.0f, 5.0f, 40.0f, 400.0f, 1500.0f };
    const float offsets[] = { 0.0f, -0.1f, -0.25f, -0.5f, -0.8f, 0.3f };
    long long totalHops = 0;
    int r0Hops = 0, ringHops = 0, mpeHops = 0, frozenHops = 0;

    for (unsigned seed : seeds)
    {
        std::mt19937 rng (seed);
        auto pick = [&rng] (int lo, int hi) { return std::uniform_int_distribution<int> (lo, hi) (rng); };
        std::normal_distribution<float> levelDist (-48.0f, 8.0f);

        char name[64];
        std::snprintf (name, sizeof (name), "random 3, seed %u", seed);
        StageRig rig (name);
        rig.s.overlapMs = times[pick (0, 5)];
        rig.s.releaseMs = times[pick (0, 5)];
        TrackerSim tracker;
        double semis = 0.0;
        const int startFailures = gFailuresInTest;
        bool clean = true;
        bool silenced[16][128] = {};   // sounding when CC11 on that channel was faded to 0

        auto allOff = [] (const vm::Settings& c) { return ! c.keyFilter && ! c.triad && ! c.mpe && ! c.freezeSolo && ! c.freezeHarmony
                                                          && c.overlapMs == 0.0f && c.releaseMs == 0.0f; };

        for (int h = 0; h < hopsPerSeed && gFailuresInTest - startFailures < 20; ++h)
        {
            Events in;
            const vm::Settings before = rig.s;
            if (pick (0, 120) == 0)
            {
                switch (pick (0, 19))
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
                    case 16: rig.s.overlapMs = times[pick (0, 5)]; break;
                    case 17: rig.s.releaseMs = times[pick (0, 5)]; break;
                    case 18: rig.s.harmonyOffset = offsets[pick (0, 5)]; break;
                    default:
                        rig.s.keyFilter = rig.s.triad = rig.s.mpe = rig.s.freezeSolo = rig.s.freezeHarmony = false;
                        if (pick (0, 1) == 0) rig.s.overlapMs = rig.s.releaseMs = 0.0f;
                        break;
                }
            }

            // CC11 from the tracker is never 0 here, so a channel at 0 can only come from a fade.
            const double range = rig.s.bendRangeSemis;
            auto dyn = [&] () { return pick (0, 2) == 0 ? -1 : pick (1, 127); };
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
            else if (tracker.note >= 0 && a < 55)
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
            else if (tracker.note >= 0 && a < 75)
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
            {
                const int number = pick (0, 2) == 0 ? 2 : (pick (0, 1) ? 1 : 11);
                in.push_back (cc (tracker.channel, number, number == 11 ? pick (1, 127) : pick (0, 127)));
            }
            else if (a < 530)
                in.push_back (cc (tracker.channel, 7, pick (0, 127)));

            rig.level = levelDist (rng);
            const int failuresBefore = gFailuresInTest;
            bool soundingBefore[16][128];
            std::memcpy (soundingBefore, rig.sounding, sizeof (soundingBefore));
            int cc11Before[16];
            std::memcpy (cc11Before, rig.lastCc11, sizeof (cc11Before));
            rig.hop (in);
            ++totalHops;
            frozenHops += rig.stage.frozenNote() >= 0 ? 1 : 0;
            mpeHops += rig.s.mpe ? 1 : 0;

            auto context = [&] () {
                char b[900];
                const vm::Settings& c = rig.s;
                std::snprintf (b, sizeof (b), "\n          seed %u hop %d; settings: filter %d root %d mode %d penta %d sustain %d triad %d triadCh %d voicing %d hmode %d "
                                              "mpe %d fzS %d fzH %d mainCh %d range %.1f gateOff %.0f overlap %.0f release %.0f offset %.2f, level %.1f dB\n          in: %s\n          out: %s\n          sounding: %s",
                               seed, h, (int) c.keyFilter, c.keyRoot, c.scaleMode, (int) c.pentatonic, (int) c.pentaSustain, (int) c.triad, c.triadChannel, c.voicing,
                               (int) c.harmonyMode, (int) c.mpe, (int) c.freezeSolo, (int) c.freezeHarmony, c.midiChannel,
                               (double) c.bendRangeSemis, (double) c.gateOffDb, (double) c.overlapMs, (double) c.releaseMs, (double) c.harmonyOffset, (double) rig.level,
                               stList (in).c_str(), stList (rig.last).c_str(), rig.soundingText().c_str());
                return std::string (b);
            };

            // Replay the output event by event: a note-on never on a channel whose CC11 is at 0, and
            // a channel faded to 0 never comes back up while a note that was faded there still sounds.
            {
                bool snd[16][128];
                std::memcpy (snd, soundingBefore, sizeof (snd));
                int c11[16];
                std::memcpy (c11, cc11Before, sizeof (c11));
                bool rose[16][128] = {};   // faded to 0, then the controller came back up in this hop
                bool newNote[16] = {};     // R8c: a new note there may take over the controller; what rings keeps overlapMs
                int roseTo[16] = {};
                std::string offLate[16];
                for (const vm::MidiEvent& e : rig.last)
                {
                    const int ch = e.bytes[0] & 0x0F, n = e.bytes[1] & 0x7F;
                    if (stIsOn (e))
                    {
                        if (c11[ch] == 0)
                            check (false, "%s: note-on %s on a channel whose CC11 is at 0 (inaudible)%s", rig.where().c_str(), stText (e).c_str(), context().c_str());
                        snd[ch][n] = true;
                        silenced[ch][n] = false;
                        newNote[ch] = true;
                    }
                    else if (stIsOff (e))
                    {
                        if (rose[ch][n])
                            offLate[ch] += " " + std::to_string (n);   // its note-off came after the controller went back up
                        snd[ch][n] = false;
                        silenced[ch][n] = false;
                    }
                    else if (stIsCc (e) && n == 11)
                    {
                        c11[ch] = e.bytes[2];
                        if (e.bytes[2] == 0)
                        {
                            for (int k = 0; k < 128; ++k) silenced[ch][k] = snd[ch][k];
                        }
                        else
                        {
                            for (int k = 0; k < 128; ++k)
                            {
                                if (silenced[ch][k] && snd[ch][k]) { rose[ch][k] = true; roseTo[ch] = e.bytes[2]; }
                                silenced[ch][k] = false;
                            }
                        }
                    }
                }
                // still sounding at the end of the hop, back at a level, and no new note there: it came back
                for (int ch = 0; ch < 16; ++ch)
                {
                    std::string back;
                    for (int k = 0; k < 128; ++k)
                        if (rose[ch][k] && snd[ch][k] && ! newNote[ch]) back += " " + std::to_string (k);
                    if (! back.empty())
                        check (false, "%s: CC11 on channel %d back to %d while note(s)%s, faded to 0 there, still sound%s", rig.where().c_str(),
                               ch + 1, roseTo[ch], back.c_str(), context().c_str());
                    if (! offLate[ch].empty() && ! newNote[ch])
                        check (false, "%s: note-off(s)%s on channel %d only after CC11 went back to %d (a synth's release would sound at that level)%s",
                               rig.where().c_str(), offLate[ch].c_str(), ch + 1, roseTo[ch], context().c_str());
                }
            }
            // after every hop: a channel at CC11 0 has something sounding on it
            for (int ch = 0; ch < 16; ++ch)
                if (rig.lastCc11[ch] == 0 && rig.countOn (ch + 1) == 0)
                    check (false, "%s: channel %d left at CC11 = 0 with nothing sounding%s", rig.where().c_str(), ch + 1, context().c_str());

            if (gFailuresInTest > failuresBefore)
                info ("context of the failure above:%s", context().c_str());

            // R0: with everything off, both times 0 and nothing left from before, the output is the input
            const bool off = allOff (rig.s) && allOff (before);
            if (! off)
                clean = false;
            else if (! clean && ! rig.stage.isSounding() && rig.countAll() == 0 && tracker.note < 0 && rig.stage.triadCount() == 0)
                clean = true;
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
            if (rig.s.overlapMs > 0.0f || rig.s.releaseMs > 0.0f) ++ringHops;

            const vm::Settings& c = rig.s;
            if (rig.countOn (1) > 0 && c.mpe && before.mpe && ! (c.triad && c.triadChannel == 1)
                && ! c.freezeHarmony && ! before.freezeHarmony && ! c.freezeSolo && ! before.freezeSolo)
                check (false, "%s: mpe: a note on channel 1 (not the harmony channel), found %s%s", rig.where().c_str(), rig.soundingText().c_str(), context().c_str());
        }

        // The end: times stay on. Freeze off, the tracker ends its note, silent hops longer than the longest time.
        rig.s.freezeSolo = rig.s.freezeHarmony = false;
        rig.hop();
        Events in;
        if (tracker.note >= 0) tracker.stop (in);
        rig.hop (in);
        rig.level = -90.0f;
        for (int i = 0; i < 1200; ++i)
            rig.hop();
        rig.soundingNow ("end: rang out by itself, before releaseAll", "");
        rig.releaseAll();
        rig.soundingNow ("end: freeze off, tracker off, releaseAll", "");
        for (int ch = 0; ch < 16; ++ch)
        {
            check (rig.lastBend[ch] < 0 || rig.lastBend[ch] == 8192, "%s: end: bend on channel %d left at %d (seed %u)", rig.where().c_str(), ch + 1, rig.lastBend[ch], seed);
            check (rig.lastCc11[ch] != 0, "%s: end: CC11 on channel %d left at 0 - the next note there would be silent (seed %u)", rig.where().c_str(), ch + 1, seed);
        }
        check (! rig.stage.isSounding() && rig.stage.triadCount() == 0 && rig.stage.frozenNote() < 0, "%s: end: stage still reports a note", rig.where().c_str());
    }
    info ("%lld hops over %d seeds; R0 checked on %d hops, ringing on in %d hops, %d with a frozen solo note, %d with mpe", totalHops,
          (int) (sizeof (seeds) / sizeof (seeds[0])), r0Hops, ringHops, frozenHops, mpeHops);
}
