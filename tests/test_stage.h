// ViolinMIDI - tests for the output stage: key filter, pentatonic sustain,
// sliding into the key, triad on a second channel, small bend ranges and the
// master switch. Rule numbers (R0..R5) refer to the header comment of
// core/OutputStage.h.
//
// Included by test_main.cpp after its helpers (check, info, baseSettings),
// inside that file's anonymous namespace; test_main.cpp includes the headers
// this file needs.
#pragma once

//==============================================================================
// Church modes and pentatonic scales as the user asked for them (semitones
// above the root). Order as Settings::scaleMode.
const int kStageModeSteps[7][7] = {
    { 0, 2, 4, 5, 7, 9, 11 },   // Ionian
    { 0, 2, 3, 5, 7, 9, 10 },   // Dorian
    { 0, 1, 3, 5, 7, 8, 10 },   // Phrygian
    { 0, 2, 4, 6, 7, 9, 11 },   // Lydian
    { 0, 2, 4, 5, 7, 9, 10 },   // Mixolydian
    { 0, 2, 3, 5, 7, 8, 10 },   // Aeolian
    { 0, 1, 3, 5, 6, 8, 10 }    // Locrian
};
const char* const kStageModeNames[7] = { "Ionian", "Dorian", "Phrygian", "Lydian", "Mixolydian", "Aeolian", "Locrian" };
const int kStageMajorPenta[5] = { 0, 2, 4, 7, 9 };
const int kStageMinorPenta[5] = { 0, 3, 5, 7, 10 };

inline bool stageExpectMajorPenta (int mode) { return mode == 0 || mode == 3 || mode == 4; }

/** Expected pitch-class mask, built from the tables above. */
inline unsigned stageExpectedMask (int root, int mode, bool pentatonic)
{
    unsigned m = 0;
    if (pentatonic)
        for (int i = 0; i < 5; ++i)
            m |= 1u << ((root + (stageExpectMajorPenta (mode) ? kStageMajorPenta[i] : kStageMinorPenta[i])) % 12);
    else
        for (int i = 0; i < 7; ++i)
            m |= 1u << ((root + kStageModeSteps[mode][i]) % 12);
    return m;
}

inline bool stageExpectAllowed (int note, int root, int mode, bool pentatonic)
{
    return ((stageExpectedMask (root, mode, pentatonic) >> (note % 12)) & 1u) != 0;
}

//==============================================================================
// MIDI helpers. Channels are 1..16 here, as in Settings.
using Events = std::vector<vm::MidiEvent>;

inline vm::MidiEvent stageMsg (int status, int channel, int d1, int d2)
{
    vm::MidiEvent e;
    e.size = 3;
    e.bytes[0] = (uint8_t) (status | ((channel - 1) & 0x0F));
    e.bytes[1] = (uint8_t) (d1 & 0x7F);
    e.bytes[2] = (uint8_t) (d2 & 0x7F);
    return e;
}

inline vm::MidiEvent noteOn (int ch, int note, int vel)   { return stageMsg (0x90, ch, note, vel); }
inline vm::MidiEvent noteOff (int ch, int note)           { return stageMsg (0x80, ch, note, 0); }
inline vm::MidiEvent bend (int ch, int value14)           { return stageMsg (0xE0, ch, value14 & 0x7F, (value14 >> 7) & 0x7F); }
inline vm::MidiEvent cc (int ch, int number, int value)   { return stageMsg (0xB0, ch, number, value); }

/** 14-bit bend for `semitones` at a bend range of `range` semitones, clamped. */
inline int bendValue (double semitones, double range)
{
    const long v = 8192 + std::lround (semitones / range * 8192.0);
    return (int) std::min (16383L, std::max (0L, v));
}

inline int  stStatus (const vm::MidiEvent& e)  { return e.bytes[0] & 0xF0; }
inline bool stIsOn (const vm::MidiEvent& e)    { return stStatus (e) == 0x90 && e.bytes[2] > 0; }
inline bool stIsOff (const vm::MidiEvent& e)   { return stStatus (e) == 0x80 || (stStatus (e) == 0x90 && e.bytes[2] == 0); }
inline bool stIsNote (const vm::MidiEvent& e)  { return stIsOn (e) || stIsOff (e); }
inline bool stIsBend (const vm::MidiEvent& e)  { return stStatus (e) == 0xE0; }
inline bool stIsCc (const vm::MidiEvent& e)    { return stStatus (e) == 0xB0; }
inline int  stChannel (const vm::MidiEvent& e) { return (e.bytes[0] & 0x0F) + 1; }
inline int  stBend14 (const vm::MidiEvent& e)  { return e.bytes[1] | (e.bytes[2] << 7); }

/** Short name without values: "+1:60" note-on, "-1:60" note-off, "b1" bend, "c1:11" controller. */
inline std::string stKey (const vm::MidiEvent& e)
{
    char b[32];
    const int ch = stChannel (e);
    if (stIsOn (e))        std::snprintf (b, sizeof (b), "+%d:%d", ch, (int) e.bytes[1]);
    else if (stIsOff (e))  std::snprintf (b, sizeof (b), "-%d:%d", ch, (int) e.bytes[1]);
    else if (stIsBend (e)) std::snprintf (b, sizeof (b), "b%d", ch);
    else if (stIsCc (e))   std::snprintf (b, sizeof (b), "c%d:%d", ch, (int) e.bytes[1]);
    else if (stStatus (e) == 0xD0) std::snprintf (b, sizeof (b), "p%d", ch);
    else                   std::snprintf (b, sizeof (b), "?%02X", (int) e.bytes[0]);
    return b;
}

/** Name with value: "+1:60v100", "b1=8192", "c1:11=90", "p2=90" (channel pressure). */
inline std::string stText (const vm::MidiEvent& e)
{
    char b[24] = "";
    if (stIsOn (e))        std::snprintf (b, sizeof (b), "v%d", (int) e.bytes[2]);
    else if (stIsBend (e)) std::snprintf (b, sizeof (b), "=%d", stBend14 (e));
    else if (stIsCc (e))   std::snprintf (b, sizeof (b), "=%d", (int) e.bytes[2]);
    else if (stStatus (e) == 0xD0) std::snprintf (b, sizeof (b), "=%d", (int) e.bytes[1]);
    return stKey (e) + b;
}

inline std::string stList (const Events& v)
{
    std::string s;
    for (const vm::MidiEvent& e : v)
        s += (s.empty() ? "" : " ") + stText (e);
    return s.empty() ? "(nothing)" : s;
}

/** The note events of `v` as keys, in order or sorted. */
inline std::string stNoteKeys (const Events& v, bool sorted = false)
{
    std::vector<std::string> keys;
    for (const vm::MidiEvent& e : v)
        if (stIsNote (e))
            keys.push_back (stKey (e));
    if (sorted)
        std::sort (keys.begin(), keys.end());
    std::string s;
    for (const std::string& k : keys)
        s += (s.empty() ? "" : " ") + k;
    return s;
}

/** Sorts a space separated list of keys, for comparisons where the order is free. */
inline std::string stSorted (const std::string& list)
{
    std::istringstream in (list);
    std::vector<std::string> keys;
    for (std::string k; in >> k;)
        keys.push_back (k);
    std::sort (keys.begin(), keys.end());
    std::string s;
    for (const std::string& k : keys)
        s += (s.empty() ? "" : " ") + k;
    return s;
}

inline int stFind (const Events& v, const std::string& key, int from = 0)
{
    for (int i = std::max (0, from); i < (int) v.size(); ++i)
        if (stKey (v[(size_t) i]) == key)
            return i;
    return -1;
}

inline int stCount (const Events& v, const std::string& key)
{
    int n = 0;
    for (const vm::MidiEvent& e : v)
        n += stKey (e) == key ? 1 : 0;
    return n;
}

inline vm::Settings stageDefaults()
{
    vm::Settings s = baseSettings();
    s.gateDb = -45.0f;
    s.midiChannel = 1;
    s.keyFilter = false;
    s.keyRoot = 0;
    s.scaleMode = 0;
    s.pentatonic = false;
    s.pentaSustain = false;
    s.triad = false;
    s.triadChannel = 2;
    s.voicing = 0;
    s.harmonyMode = vm::HarmonyMode::Diatonic;
    s.mpe = false;
    s.freezeSolo = s.freezeHarmony = false;
    return s;
}

//==============================================================================
// Drives one OutputStage hop by hop and keeps a model of what sounds on every
// channel, built from the output only. Every output event is checked for
// R1e, for being well-formed and for carrying the hop's sample offset.
struct StageRig
{
    vm::OutputStage stage;
    vm::Settings s;
    vm::MidiEventList out;
    Events last;                 // output of the last call
    bool sounding[16][128] = {};
    int lastBend[16];            // last bend sent per channel, -1 = none yet
    int lastCc11[16];            // last CC11 sent per channel, -1 = none yet
    int lastCc[16][128];         // last value of every controller per channel, -1 = none yet
    int lastPressure[16];        // last channel pressure per channel, -1 = none yet
    std::string name;
    int hops = 0;
    int offset = 0;
    float level = -20.0f;        // level of the next hops, the gate is at -45 (off at -51)

    explicit StageRig (const char* caseName) : name (caseName)
    {
        stage.prepare (750.0);
        out.reserve (512);
        s = stageDefaults();
        for (int c = 0; c < 16; ++c)
        {
            lastBend[c] = lastCc11[c] = lastPressure[c] = -1;
            for (int k = 0; k < 128; ++k)
                lastCc[c][k] = -1;
        }
    }

    void key (int root, int mode, bool pentatonic, bool sustain = false)
    {
        s.keyFilter = true;
        s.keyRoot = root;
        s.scaleMode = mode;
        s.pentatonic = pentatonic;
        s.pentaSustain = sustain;
    }

    /** One analysis hop with the tracker's events `in`. */
    const Events& hop (Events in = {})
    {
        offset = 5 + (hops * 13) % 60;
        for (vm::MidiEvent& e : in)
            e.sampleOffset = offset;
        out.clear();
        stage.process (in.empty() ? nullptr : in.data(), (int) in.size(), level, s, offset, out);
        ++hops;
        absorb ((int) in.size(), "process");
        return last;
    }

    const Events& releaseAll()
    {
        offset = 3;
        out.clear();
        stage.releaseAll (offset, out);
        absorb (-1, "releaseAll");
        return last;
    }

    void absorb (int numIn, const char* what)
    {
        last.assign (out.begin(), out.end());
        if (numIn >= 0)
            check ((int) out.size() <= vm::OutputStage::maxEventsFor (numIn),
                   "%s: %s appended %d events for %d input events, maxEventsFor promises at most %d",
                   where().c_str(), what, (int) out.size(), numIn, vm::OutputStage::maxEventsFor (numIn));
        for (const vm::MidiEvent& e : out)
            apply (e, what);
    }

    void apply (const vm::MidiEvent& e, const char* what)
    {
        check (e.sampleOffset == offset, "%s: %s: event %s at sample offset %d, expected the call's offset %d",
               where().c_str(), what, stText (e).c_str(), e.sampleOffset, offset);
        // 3 bytes, or 2 for channel pressure (MPE)
        const bool pressure = (e.bytes[0] & 0xF0) == 0xD0;
        const bool wellFormed = e.size == (pressure ? 2 : 3) && (e.bytes[0] & 0x80) != 0 && (e.bytes[1] & 0x80) == 0
                             && (pressure || (e.bytes[2] & 0x80) == 0);
        check (wellFormed, "%s: %s: malformed event, size %d, bytes %02X %02X %02X (expected 3 bytes (2 for 0xD0), status >= 0x80, data < 0x80)",
               where().c_str(), what, (int) e.size, (int) e.bytes[0], (int) e.bytes[1], (int) e.bytes[2]);
        if (! wellFormed)
            return;

        const int ch = (e.bytes[0] & 0x0F), n = e.bytes[1];
        if (stIsOn (e))
        {
            check (! sounding[ch][n], "%s: %s: R1e: note-on %s for a note already sounding on that channel (output: %s)",
                   where().c_str(), what, stText (e).c_str(), stList (last).c_str());
            sounding[ch][n] = true;
        }
        else if (stIsOff (e))
        {
            check (sounding[ch][n], "%s: %s: R1e: note-off %s for a note that is not sounding (output: %s)",
                   where().c_str(), what, stText (e).c_str(), stList (last).c_str());
            sounding[ch][n] = false;
        }
        else if (stIsBend (e))
        {
            lastBend[ch] = stBend14 (e);
        }
        else if (stIsCc (e))
        {
            if (n == 11)
                lastCc11[ch] = e.bytes[2];
            lastCc[ch][n] = e.bytes[2];
        }
        else if (pressure)
        {
            lastPressure[ch] = n;
        }
        else
        {
            check (false, "%s: %s: unexpected status byte %02X (expected note, bend, controller or pressure)",
                   where().c_str(), what, (int) e.bytes[0]);
        }
    }

    std::string where() const { return name + ", hop " + std::to_string (hops); }

    int countOn (int channel) const   // channel 1..16
    {
        int n = 0;
        for (int k = 0; k < 128; ++k)
            n += sounding[channel - 1][k] ? 1 : 0;
        return n;
    }

    int countAll() const
    {
        int n = 0;
        for (int c = 1; c <= 16; ++c)
            n += countOn (c);
        return n;
    }

    /** "1:60 2:64 2:67", ordered by channel, then note. */
    std::string soundingText() const
    {
        std::string t;
        for (int c = 0; c < 16; ++c)
            for (int k = 0; k < 128; ++k)
                if (sounding[c][k])
                    t += (t.empty() ? "" : " ") + std::to_string (c + 1) + ":" + std::to_string (k);
        return t;
    }

    // --- expectations on the last call
    void notes (const char* what, const std::string& expected)
    {
        const std::string found = stNoteKeys (last);
        check (found == expected, "%s: %s: expected note events \"%s\" in this order, found \"%s\" (all output: %s)",
               where().c_str(), what, expected.c_str(), found.c_str(), stList (last).c_str());
    }

    void noteSet (const char* what, const std::string& expected)
    {
        const std::string found = stNoteKeys (last, true);
        check (found == stSorted (expected), "%s: %s: expected note events {%s} in any order, found {%s} (all output: %s)",
               where().c_str(), what, stSorted (expected).c_str(), found.c_str(), stList (last).c_str());
    }

    void soundingNow (const char* what, const std::string& expected)
    {
        const std::string found = soundingText();
        check (found == expected, "%s: %s: expected sounding (channel:note) \"%s\", found \"%s\" (last output: %s)",
               where().c_str(), what, expected.c_str(), found.c_str(), stList (last).c_str());
    }

    void noBends (const char* what)
    {
        for (const vm::MidiEvent& e : last)
            check (! stIsBend (e), "%s: %s: expected no pitch bend, found %s (all output: %s)",
                   where().c_str(), what, stText (e).c_str(), stList (last).c_str());
    }

    void bendIs (const char* what, int channel, int expected, int tolerance = 0)
    {
        const int found = lastBend[channel - 1];
        check (found >= 0 && std::abs (found - expected) <= tolerance,
               "%s: %s: expected last bend on channel %d = %d (+-%d), found %d (-1 = none sent; last output: %s)",
               where().c_str(), what, channel, expected, tolerance, found, stList (last).c_str());
    }

    /** Index in `last` of the first event with this key, failing if there is none. */
    int index (const char* what, const std::string& keyText)
    {
        const int i = stFind (last, keyText);
        check (i >= 0, "%s: %s: expected %s in the output, found: %s", where().c_str(), what, keyText.c_str(), stList (last).c_str());
        return i;
    }
};

//==============================================================================
// Produces a well-formed tracker stream, in the tracker's order:
// fresh note "bend, [CC11], note-on", legato "bend, [CC11], note-on new, note-off old".
struct TrackerSim
{
    int channel = 1;
    int note = -1;
    int bend14 = 8192;

    void moveBend (Events& v, int value14)
    {
        if (value14 != bend14)
            v.push_back (bend (channel, value14));
        bend14 = value14;
    }

    void start (Events& v, int n, int vel, int value14, int expression = -1)
    {
        moveBend (v, value14);
        if (expression >= 0)
            v.push_back (cc (channel, 11, expression));
        v.push_back (noteOn (channel, n, vel));
        note = n;
    }

    void change (Events& v, int n, int vel, int value14, int expression = -1)
    {
        moveBend (v, value14);
        if (expression >= 0)
            v.push_back (cc (channel, 11, expression));
        v.push_back (noteOn (channel, n, vel));
        v.push_back (noteOff (channel, note));
        note = n;
    }

    void stop (Events& v)
    {
        v.push_back (noteOff (channel, note));
        note = -1;
    }

    /** midiChannel changes: the tracker ends its note on the old channel (R5c). */
    void moveChannel (Events& v, int newChannel)
    {
        if (note >= 0)
            stop (v);
        if (bend14 != 8192)
            v.push_back (bend (channel, 8192));
        bend14 = 8192;
        channel = newChannel;
    }
};

//==============================================================================
// 1. Scale masks, pentatonic choice and triads (Scale.h).
inline void scaleTables()
{
    for (int root : { 0, 2, 7, 9, 11 })
    {
        for (int mode = 0; mode < 7; ++mode)
        {
            for (int penta = 0; penta < 2; ++penta)
            {
                const unsigned expected = stageExpectedMask (root, mode, penta != 0);
                const unsigned found = vm::scale::mask (root, mode, penta != 0);
                check (found == expected, "mask (root %d, %s, pentatonic %d): expected 0x%03X, found 0x%03X",
                       root, kStageModeNames[mode], penta, expected, found);
                for (int n = 0; n < 128; ++n)
                {
                    const bool want = ((expected >> (n % 12)) & 1u) != 0;
                    if (vm::scale::contains (found, n) != want)
                    {
                        check (false, "contains (mask root %d %s penta %d, note %d): expected %d, found %d",
                               root, kStageModeNames[mode], penta, n, (int) want, (int) ! want);
                        break;
                    }
                }
            }
            const bool major = stageExpectMajorPenta (mode);
            check (vm::scale::usesMajorPentatonic (mode) == major, "%s: expected the %s pentatonic", kStageModeNames[mode], major ? "major" : "minor");
        }
    }

    // The old scale::triad (note, root, mode, withRoot) is now voices (..., voicing 0, diatonic,
    // no free mask, withoutPlayedNote = ! withRoot).
    auto triadText = [] (int note, int root, int mode, bool withRoot) {
        const vm::scale::Voices t = vm::scale::voices (note, root, mode, vm::scale::presetMask (0), true, ! withRoot);
        std::string s;
        for (int i = 0; i < t.count; ++i)
            s += (i == 0 ? "" : " ") + std::to_string (t.notes[i]);
        return s;
    };
    struct Case { const char* what; int note, root, mode; bool withRoot; const char* expected; };
    const Case cases[] = {
        { "C Ionian, D -> D F A",              62, 0, 0, true,  "62 65 69" },
        { "C Ionian, C -> C E G",              60, 0, 0, true,  "60 64 67" },
        { "C Ionian, B -> B D F",              71, 0, 0, true,  "71 74 77" },
        { "C Ionian, E -> E G B",              64, 0, 0, true,  "64 67 71" },
        { "C Ionian, C# (foreign) -> major",   61, 0, 0, true,  "61 65 68" },
        { "C Ionian, F# (foreign) -> major",   66, 0, 0, true,  "66 70 73" },
        { "C Ionian, D without root",          62, 0, 0, false, "65 69" },
        { "C Ionian, C 120: 127 kept",        120, 0, 0, true,  "120 124 127" },
        { "C Ionian, F 125: above 127 dropped",125, 0, 0, true,  "125" },
        { "C Ionian, F 125 without root",     125, 0, 0, false, "" },
        { "A Aeolian, A -> A C E",             69, 9, 5, true,  "69 72 76" },
        { "A Aeolian, B -> B D F",             71, 9, 5, true,  "71 74 77" },
        { "A Aeolian, C -> C E G",             72, 9, 5, true,  "72 76 79" },
        { "G Mixolydian, G -> G B D",          67, 7, 4, true,  "67 71 74" },
        { "G Mixolydian, F -> F A C",          65, 7, 4, true,  "65 69 72" },
        { "D Dorian, D -> D F A",              50, 2, 1, true,  "50 53 57" },
        { "E Phrygian, F -> F A C",            53, 4, 2, true,  "53 57 60" },
        { "C Ionian, low C 24",                24, 0, 0, true,  "24 28 31" },
    };
    for (const Case& c : cases)
    {
        const std::string found = triadText (c.note, c.root, c.mode, c.withRoot);
        check (found == c.expected, "triad, %s: expected \"%s\", found \"%s\"", c.what, c.expected, found.c_str());
    }
}

//==============================================================================
// 2. R0: key filter and triad off -> output equals input, byte for byte.
inline void stagePassThrough()
{
    std::mt19937 rng (4711);
    auto pick = [&rng] (int lo, int hi) { return std::uniform_int_distribution<int> (lo, hi) (rng); };

    StageRig rig ("pass-through");
    TrackerSim tracker;
    int mismatches = 0, events = 0;

    for (int h = 0; h < 30000 && mismatches < 5; ++h)
    {
        Events in;
        if (h % 700 == 699)
        {
            // settings that must not matter while keyFilter and triad are off
            rig.s.keyRoot = pick (0, 11);
            rig.s.scaleMode = pick (0, 6);
            rig.s.pentatonic = pick (0, 1) == 1;
            rig.s.pentaSustain = pick (0, 1) == 1;
            rig.s.triadChannel = pick (1, 16);
            rig.s.voicing = pick (0, 12);
            rig.s.gateOffDb = (float) pick (-70, -40);
            rig.s.bendRangeSemis = (float) pick (1, 24);
            if (pick (0, 3) == 0)
            {
                const int ch = pick (1, 16);
                tracker.moveChannel (in, ch);
                rig.s.midiChannel = ch;
            }
        }

        const int a = pick (0, 99);
        if (tracker.note < 0 && a < 4)
            tracker.start (in, pick (40, 100), pick (1, 127), pick (0, 16383), pick (0, 1) == 0 ? pick (0, 127) : -1);
        else if (tracker.note >= 0 && a < 4)
        {
            int n = pick (40, 100);
            if (n == tracker.note) ++n;
            tracker.change (in, n, pick (1, 127), pick (0, 16383), pick (0, 1) == 0 ? pick (0, 127) : -1);
        }
        else if (tracker.note >= 0 && a < 7)
            tracker.stop (in);
        else if (a < 40)
            tracker.moveBend (in, pick (0, 16383));
        else if (a < 55)
            in.push_back (cc (tracker.channel, 11, pick (0, 127)));

        rig.level = (float) pick (-90, 0);
        Events expected = in;
        rig.hop (in);
        for (vm::MidiEvent& e : expected)
            e.sampleOffset = rig.offset;
        events += (int) expected.size();

        bool same = rig.last.size() == expected.size();
        for (size_t i = 0; same && i < expected.size(); ++i)
            same = rig.last[i].sampleOffset == expected[i].sampleOffset && rig.last[i].size == expected[i].size
                && std::memcmp (rig.last[i].bytes, expected[i].bytes, 3) == 0;
        if (! same)
        {
            ++mismatches;
            check (false, "%s: R0: expected the input unchanged: %s, found: %s",
                   rig.where().c_str(), stList (expected).c_str(), stList (rig.last).c_str());
        }
    }
    info ("%d hops, %d tracker events passed through unchanged", rig.hops, events);
}

//==============================================================================
// 3. R1a, R1b, R1e with sustain off.
inline void stageKeyFilter()
{
    {
        StageRig rig ("C Ionian, detached allowed");
        rig.key (0, 0, false);
        rig.hop ({ noteOn (1, 60, 100) });
        rig.notes ("on 60", "+1:60");
        const int i = stFind (rig.last, "+1:60");
        check (i < 0 || rig.last[(size_t) i].bytes[2] == 100, "%s: expected velocity 100, found %s", rig.where().c_str(), stList (rig.last).c_str());
        rig.hop ({ noteOff (1, 60) });
        rig.notes ("off 60", "-1:60");
    }
    {
        StageRig rig ("C Ionian, detached foreign");
        rig.key (0, 0, false);
        rig.hop ({ noteOn (1, 61, 100) });
        rig.notes ("on 61", "");
        rig.hop ({ noteOff (1, 61) });
        rig.notes ("off 61", "");
        rig.soundingNow ("after", "");
    }
    {
        StageRig rig ("C Ionian, legato allowed -> allowed");
        rig.key (0, 0, false);
        rig.hop ({ noteOn (1, 60, 100) });
        rig.hop ({ noteOn (1, 62, 90), noteOff (1, 60) });
        rig.noteSet ("60 -> 62", "+1:62 -1:60");
        rig.soundingNow ("60 -> 62", "1:62");
    }
    {
        StageRig rig ("C Ionian, legato allowed -> foreign");
        rig.key (0, 0, false);
        rig.hop ({ noteOn (1, 60, 100) });
        rig.hop ({ noteOn (1, 61, 90), noteOff (1, 60) });
        rig.notes ("60 -> 61: old note ends in the same hop, nothing new", "-1:60");
        rig.hop ({ noteOff (1, 61) });
        rig.notes ("off 61", "");
        rig.soundingNow ("after", "");
    }
    {
        StageRig rig ("C Ionian, legato foreign -> allowed");
        rig.key (0, 0, false);
        rig.hop ({ noteOn (1, 61, 100) });
        rig.hop ({ noteOn (1, 62, 90), noteOff (1, 61) });
        rig.notes ("61 -> 62", "+1:62");
        rig.hop ({ noteOff (1, 62) });
        rig.notes ("off 62", "-1:62");
    }
    {
        StageRig rig ("C Ionian, legato foreign -> foreign");
        rig.key (0, 0, false);
        rig.hop ({ noteOn (1, 61, 100) });
        rig.hop ({ noteOn (1, 63, 90), noteOff (1, 61) });
        rig.notes ("61 -> 63", "");
        rig.hop ({ noteOff (1, 63) });
        rig.notes ("off 63", "");
    }
    {
        StageRig rig ("C major pentatonic, F is foreign");
        rig.key (0, 0, true);
        rig.hop ({ noteOn (1, 65, 100) });
        rig.notes ("on 65", "");
        rig.hop ({ noteOff (1, 65) });
        rig.hop ({ noteOn (1, 64, 100) });
        rig.notes ("on 64", "+1:64");
        rig.hop ({ noteOn (1, 65, 100), noteOff (1, 64) });
        rig.notes ("64 -> 65", "-1:64");
        rig.hop ({ noteOff (1, 65) });
        rig.soundingNow ("after", "");
    }
    {
        StageRig rig ("D Ionian");
        rig.key (2, 0, false);
        rig.hop ({ noteOn (1, 66, 100) });
        rig.notes ("F# allowed", "+1:66");
        rig.hop ({ noteOn (1, 65, 100), noteOff (1, 66) });
        rig.notes ("F foreign", "-1:66");
        rig.hop ({ noteOn (1, 61, 100), noteOff (1, 65) });
        rig.notes ("C# allowed", "+1:61");
        rig.hop ({ noteOff (1, 61) });
        rig.soundingNow ("after", "");
    }
    {   // bends and CC11 follow the tracker while an allowed note sounds (R2)
        StageRig rig ("C Ionian, bends");
        rig.key (0, 0, false);
        rig.hop ({ bend (1, 9000), noteOn (1, 60, 100) });
        const int b = rig.index ("bend before the note", "b1"), on = rig.index ("note-on", "+1:60");
        check (b >= 0 && on >= 0 && b < on, "%s: expected the bend before the note-on, found %s", rig.where().c_str(), stList (rig.last).c_str());
        rig.bendIs ("bend before the note", 1, 9000);
        rig.hop ({ bend (1, 9100) });
        rig.bendIs ("bend during the note", 1, 9100);
        rig.hop ({ cc (1, 11, 77) });
        check (rig.lastCc11[0] == 77, "%s: expected CC11 = 77 on channel 1, found %d", rig.where().c_str(), rig.lastCc11[0]);
    }

    // every mode, every pitch class, 7-note and pentatonic, several roots
    for (int root : { 0, 5, 9 })
        for (int mode = 0; mode < 7; ++mode)
            for (int penta = 0; penta < 2; ++penta)
            {
                char name[64];
                std::snprintf (name, sizeof (name), "root %d %s penta %d", root, kStageModeNames[mode], penta);
                StageRig rig (name);
                rig.key (root, mode, penta != 0);
                for (int n = 60; n < 72; ++n)
                {
                    rig.hop ({ noteOn (1, n, 100) });
                    const bool want = stageExpectAllowed (n, root, mode, penta != 0);
                    const std::string found = stNoteKeys (rig.last);
                    const std::string expected = want ? "+1:" + std::to_string (n) : "";
                    check (found == expected, "%s: note %d: expected \"%s\", found \"%s\"", name, n, expected.c_str(), found.c_str());
                    rig.hop ({ noteOff (1, n) });
                }
                rig.soundingNow ("after all", "");
            }
}

//==============================================================================
// 4. R1c, R1d, R2: pentatonic sustain. C major pentatonic: C D E G A allowed.
inline void stageSustain()
{
    {
        StageRig rig ("hold through foreign legato");
        rig.key (0, 0, true, true);
        rig.hop ({ noteOn (1, 60, 100) });
        rig.hop ({ noteOn (1, 65, 100), noteOff (1, 60) });
        rig.notes ("60 -> 65", "");
        check (rig.stage.isHeld(), "%s: expected isHeld() after a foreign note", rig.where().c_str());
        rig.hop ({ noteOn (1, 66, 100), noteOff (1, 65) });
        rig.notes ("65 -> 66", "");
        rig.soundingNow ("held", "1:60");
        rig.hop ({ noteOn (1, 67, 100), noteOff (1, 66) });
        rig.notes ("66 -> 67: held note off first, then the new one", "-1:60 +1:67");
        check (! rig.stage.isHeld(), "%s: expected no hold after an allowed note", rig.where().c_str());
    }
    {
        StageRig rig ("hold after the tracker's note-off");
        rig.key (0, 0, true, true);
        rig.hop ({ noteOn (1, 60, 100) });
        rig.hop ({ noteOff (1, 60) });
        rig.notes ("tracker off at -20 dB", "");
        rig.level = -30.0f;
        rig.hop();
        rig.level = -48.0f;   // below gateDb, above gateOffDb (-51): still held (R1c/d use gateOffDb)
        rig.hop();
        rig.level = -51.0f;   // equal to gateOffDb: still held (levelDb >= gateOffDb)
        rig.hop();
        rig.notes ("level at gateOffDb", "");
        rig.soundingNow ("level at gateOffDb", "1:60");
        rig.level = -51.5f;
        rig.hop();
        rig.notes ("first hop below gateOffDb", "-1:60");
    }
    {   // gateOffDb is never above gateDb: with gateOffDb -30 and gateDb -45, -45 counts
        StageRig rig ("gateOffDb above gateDb is clamped");
        rig.key (0, 0, true, true);
        rig.s.gateOffDb = -30.0f;
        rig.hop ({ noteOn (1, 60, 100) });
        rig.hop ({ noteOff (1, 60) });
        rig.level = -40.0f;
        rig.hop();
        rig.soundingNow ("-40 dB, gate -45, gateOff -30 clamped to -45", "1:60");
        rig.level = -46.0f;
        rig.hop();
        rig.notes ("below the gate", "-1:60");
    }
    {
        StageRig rig ("release below the gate during a foreign note");
        rig.key (0, 0, true, true);
        rig.hop ({ noteOn (1, 60, 100) });
        rig.hop ({ noteOn (1, 65, 100), noteOff (1, 60) });
        rig.level = -52.0f;
        rig.hop();
        rig.notes ("below the gate", "-1:60");
        rig.hop ({ noteOff (1, 65) });
        rig.notes ("tracker off 65", "");
        rig.soundingNow ("after", "");
    }
    {
        StageRig rig ("fresh allowed note ends the hold");
        rig.key (0, 0, true, true);
        rig.hop ({ noteOn (1, 60, 100) });
        rig.hop ({ noteOff (1, 60) });
        rig.hop ({ noteOn (1, 64, 80) });
        rig.notes ("on 64", "-1:60 +1:64");
    }
    {
        StageRig rig ("note-off below the gate is sent");
        rig.key (0, 0, true, true);
        rig.hop ({ noteOn (1, 60, 100) });
        rig.level = -52.0f;
        rig.hop ({ noteOff (1, 60) });
        rig.notes ("off at -52 dB", "-1:60");
    }
    {
        StageRig rig ("note-off between gateOffDb and gateDb is held");
        rig.key (0, 0, true, true);
        rig.hop ({ noteOn (1, 60, 100) });
        rig.level = -50.0f;
        rig.hop ({ noteOff (1, 60) });
        rig.notes ("off at -50 dB", "");
        rig.soundingNow ("off at -50 dB", "1:60");
    }
    {
        StageRig rig ("foreign note below the gate");
        rig.key (0, 0, true, true);
        rig.hop ({ noteOn (1, 60, 100) });
        rig.level = -52.0f;
        rig.hop ({ noteOn (1, 65, 100), noteOff (1, 60) });
        rig.soundingNow ("60 -> 65 at -52 dB", "");
    }
    for (int mode : { 10, 11, 12 })
    {   // R1c: a scale of fewer than seven notes activates the sustain without the pentatonic flag
        StageRig rig ("sustain with a pentatonic/blues scale");
        rig.key (0, mode, false, true);
        rig.hop ({ noteOn (1, 60, 100) });
        rig.hop ({ noteOn (1, 61, 100), noteOff (1, 60) });
        rig.notes ("60 -> 61 (foreign in every one)", "");
        rig.soundingNow ("held", "1:60");
        rig.hop ({ noteOff (1, 61) });
        rig.level = -60.0f;
        rig.hop();
        rig.soundingNow ("after", "");
    }
    {   // R2: bend back to the centre when the hold begins, tracker bends kept back
        StageRig rig ("bend during a hold");
        rig.key (0, 0, true, true);
        rig.hop ({ bend (1, 9000), noteOn (1, 60, 100) });
        rig.hop ({ bend (1, 9100), noteOn (1, 65, 100), noteOff (1, 60) });
        rig.bendIs ("hold begins", 1, 8192);
        rig.hop ({ bend (1, 9500) });
        rig.noBends ("tracker bend during the hold");
        rig.hop ({ bend (1, 7000) });
        rig.noBends ("tracker bend during the hold");
        rig.hop ({ cc (1, 11, 80) });
        check (rig.lastCc11[0] == 80, "%s: expected CC11 to follow the tracker during a hold (80), found %d", rig.where().c_str(), rig.lastCc11[0]);
        rig.hop ({ bend (1, 8500), noteOn (1, 67, 100), noteOff (1, 65) });
        rig.notes ("hold ends with 67", "-1:60 +1:67");
        const int on = stFind (rig.last, "+1:67");
        int lastBendBefore = -1;
        for (int i = 0; i < on; ++i)
            if (stIsBend (rig.last[(size_t) i]))
                lastBendBefore = stBend14 (rig.last[(size_t) i]);
        check (lastBendBefore == 8500, "%s: expected the tracker's bend 8500 before the note-on that ends the hold, found %d in %s",
               rig.where().c_str(), lastBendBefore, stList (rig.last).c_str());
    }
    {
        StageRig rig ("bend already centred");
        rig.key (0, 0, true, true);
        rig.hop ({ noteOn (1, 60, 100) });
        rig.hop ({ noteOn (1, 65, 100), noteOff (1, 60) });
        rig.noBends ("hold begins at the centre");
        rig.hop ({ noteOn (1, 67, 100), noteOff (1, 65) });
        rig.noBends ("hold ends, tracker bend unchanged");
        rig.notes ("hold ends", "-1:60 +1:67");
    }
    {
        StageRig rig ("sustain without pentatonic");
        rig.key (0, 0, false, true);
        rig.hop ({ noteOn (1, 60, 100) });
        rig.hop ({ noteOn (1, 61, 100), noteOff (1, 60) });
        rig.notes ("60 -> 61", "-1:60");
        rig.hop ({ noteOn (1, 62, 100), noteOff (1, 61) });
        rig.hop ({ noteOff (1, 62) });
        rig.notes ("off 62 at -20 dB", "-1:62");
    }
    {
        StageRig rig ("sustain without key filter");
        rig.s.pentatonic = true;
        rig.s.pentaSustain = true;
        rig.hop ({ noteOn (1, 60, 100) });
        Events in = { noteOn (1, 61, 100), noteOff (1, 60) };
        rig.hop (in);
        for (vm::MidiEvent& e : in) e.sampleOffset = rig.offset;
        check (stList (rig.last) == stList (in), "%s: R0: expected %s, found %s", rig.where().c_str(), stList (in).c_str(), stList (rig.last).c_str());
        rig.hop ({ noteOff (1, 61) });
        rig.notes ("off 61", "-1:61");
    }
}

//==============================================================================
// 5. R3: sliding into the key. C Ionian, bend range 2.
inline void stageSlideIn()
{
    const double range = 2.0;
    // Runs empty hops until a note-on appears; returns the hops waited, or -1.
    auto waitForStart = [] (StageRig& rig, int maxHops) {
        for (int i = 1; i <= maxHops; ++i)
        {
            rig.hop();
            for (const vm::MidiEvent& e : rig.last)
                if (stIsOn (e))
                    return i;
        }
        return -1;
    };
    auto checkDelay = [] (StageRig& rig, int hops, const char* what) {
        const double ms = hops * 1000.0 / 750.0;
        check (hops >= 0 && ms >= 20.0 && ms <= 45.0, "%s: %s: expected the note to start 20..45 ms after the pitch arrived, found %s %.1f ms",
               rig.where().c_str(), what, hops < 0 ? "no start within" : "", hops < 0 ? 200 * 1000.0 / 750.0 : ms);
    };

    {
        StageRig rig ("slide up into D");
        rig.s.bendRangeSemis = (float) range;
        rig.key (0, 0, false);
        rig.hop ({ noteOn (1, 61, 77) });
        rig.notes ("on 61", "");
        rig.hop ({ bend (1, bendValue (0.95, range)) });   // 61.95
        rig.notes ("pitch arrives", "");
        const int waited = waitForStart (rig, 200);
        checkDelay (rig, waited, "61 bent to 61.95");
        rig.notes ("start", "+1:62");
        const int on = stFind (rig.last, "+1:62");
        check (on < 0 || rig.last[(size_t) on].bytes[2] == 77, "%s: expected the velocity of the suppressed note-on (77), found %s",
               rig.where().c_str(), stList (rig.last).c_str());
        rig.bendIs ("bent to the exact pitch 61.95 on 62", 1, bendValue (-0.05, range), 2);
        check (rig.stage.pitchOffset() == -1.0f, "%s: expected pitchOffset() -1 (tracker 61, sounding 62), found %.2f",
               rig.where().c_str(), rig.stage.pitchOffset());
        rig.hop ({ bend (1, bendValue (1.0, range)) });
        rig.bendIs ("tracker at 62.0 re-centred", 1, 8192, 1);
        rig.hop ({ bend (1, bendValue (1.2, range)) });
        rig.bendIs ("tracker at 62.2 re-centred", 1, bendValue (0.2, range), 2);
        rig.hop ({ noteOff (1, 61) });
        rig.notes ("tracker off 61 ends 62", "-1:62");
        rig.soundingNow ("after", "");
    }
    {
        StageRig rig ("slide down into C");
        rig.s.bendRangeSemis = (float) range;
        rig.key (0, 0, false);
        rig.hop ({ noteOn (1, 61, 90) });
        rig.hop ({ bend (1, bendValue (-0.9, range)) });   // 60.1
        checkDelay (rig, waitForStart (rig, 200), "61 bent to 60.1");
        rig.notes ("start", "+1:60");
        rig.bendIs ("bent to 60.1 on 60", 1, bendValue (0.1, range), 2);
        rig.hop ({ noteOff (1, 61) });
        rig.notes ("tracker off", "-1:60");
    }
    {
        StageRig rig ("passing through, too far");
        rig.s.bendRangeSemis = (float) range;
        rig.key (0, 0, false);
        rig.hop ({ noteOn (1, 61, 70) });
        rig.hop ({ bend (1, bendValue (0.95, range)) });
        for (int i = 0; i < 9; ++i) rig.hop();             // 13 ms near 62 only
        rig.hop ({ bend (1, bendValue (0.5, range)) });    // 61.5: 0.5 away
        check (waitForStart (rig, 100) < 0, "%s: a pitch that only passed 62 for 13 ms, then sat at 61.5, started a note: %s",
               rig.where().c_str(), stList (rig.last).c_str());
        rig.hop ({ bend (1, bendValue (0.65, range)) });   // 61.65: 0.35 away
        check (waitForStart (rig, 100) < 0, "%s: 61.65 (0.35 from 62) started a note: %s", rig.where().c_str(), stList (rig.last).c_str());
        rig.hop ({ bend (1, bendValue (0.75, range)) });   // 61.75: 0.25 away
        checkDelay (rig, waitForStart (rig, 200), "61.75");
        rig.notes ("start", "+1:62");
        rig.hop ({ noteOff (1, 61) });
        rig.soundingNow ("after", "");
    }
    {
        StageRig rig ("tracker ends before the pitch settled");
        rig.s.bendRangeSemis = (float) range;
        rig.key (0, 0, false);
        rig.hop ({ noteOn (1, 61, 70) });
        rig.hop ({ bend (1, bendValue (0.95, range)) });
        for (int i = 0; i < 5; ++i) rig.hop();
        rig.hop ({ noteOff (1, 61) });
        check (waitForStart (rig, 100) < 0, "%s: a note started after the tracker's note-off: %s", rig.where().c_str(), stList (rig.last).c_str());
        rig.soundingNow ("after", "");
    }
    {
        StageRig rig ("legato after sliding in");
        rig.s.bendRangeSemis = (float) range;
        rig.key (0, 0, false);
        rig.hop ({ noteOn (1, 61, 70) });
        rig.hop ({ bend (1, bendValue (1.0, range)) });
        waitForStart (rig, 200);
        rig.soundingNow ("slid into 62", "1:62");
        rig.hop ({ bend (1, 8192), noteOn (1, 64, 90), noteOff (1, 61) });
        rig.noteSet ("61(62) -> 64", "+1:64 -1:62");
        rig.soundingNow ("after", "1:64");
        rig.hop ({ noteOff (1, 64) });
        rig.notes ("off 64", "-1:64");
    }
}

//==============================================================================
// 6. R4: triad on a second channel. C Ionian, triad channel 2.
inline void stageTriad()
{
    // all triad note-ons on `ch` after the main note-on, with velocity `vel`
    auto triadAfterMain = [] (StageRig& rig, const std::string& mainOn, int ch, int vel) {
        const int m = stFind (rig.last, mainOn);
        for (int i = 0; i < (int) rig.last.size(); ++i)
        {
            const vm::MidiEvent& e = rig.last[(size_t) i];
            if (stIsOn (e) && stChannel (e) == ch)
                check (i > m && e.bytes[2] == vel, "%s: expected triad note-on %s after %s with velocity %d, found: %s",
                       rig.where().c_str(), stKey (e).c_str(), mainOn.c_str(), vel, stList (rig.last).c_str());
        }
    };
    // all note-offs before the first note-on of the given channel
    auto offsBeforeOns = [] (StageRig& rig, int ch, const char* what) {
        int lastOff = -1, firstOn = 1 << 30;
        for (int i = 0; i < (int) rig.last.size(); ++i)
        {
            const vm::MidiEvent& e = rig.last[(size_t) i];
            if (stChannel (e) != ch) continue;
            if (stIsOff (e)) lastOff = i;
            if (stIsOn (e)) firstOn = std::min (firstOn, i);
        }
        check (lastOff < firstOn, "%s: %s: expected every note-off on channel %d before its first note-on, found: %s",
               rig.where().c_str(), what, ch, stList (rig.last).c_str());
    };

    {
        StageRig rig ("triad D minor");
        rig.s.triad = true;
        rig.hop ({ noteOn (1, 62, 90) });
        rig.noteSet ("on 62", "+1:62 +2:62 +2:65 +2:69");
        triadAfterMain (rig, "+1:62", 2, 90);
        check (rig.stage.triadCount() == 3, "%s: expected triadCount() 3, found %d", rig.where().c_str(), rig.stage.triadCount());
        rig.hop ({ noteOff (1, 62) });
        rig.noteSet ("off 62: triad ends with it", "-1:62 -2:62 -2:65 -2:69");
        rig.hop ({ noteOn (1, 60, 80) });
        rig.noteSet ("on 60", "+1:60 +2:60 +2:64 +2:67");
        rig.hop ({ noteOff (1, 60) });
        rig.hop ({ noteOn (1, 71, 80) });
        rig.noteSet ("on 71", "+1:71 +2:71 +2:74 +2:77");
        rig.hop ({ noteOn (1, 61, 70), noteOff (1, 71) });   // legato to a foreign note: major triad
        rig.soundingNow ("71 -> 61", "1:61 2:61 2:65 2:68");
        rig.hop ({ noteOff (1, 61) });
        rig.hop ({ noteOn (1, 125, 80) });
        rig.soundingNow ("on 125: notes above 127 left out", "1:125 2:125");
        rig.hop ({ noteOff (1, 125) });
        rig.soundingNow ("after", "");
    }
    {
        StageRig rig ("triad legato C -> E, shared notes");
        rig.s.triad = true;
        rig.hop ({ noteOn (1, 60, 90) });
        rig.hop ({ noteOn (1, 64, 85), noteOff (1, 60) });
        rig.noteSet ("60 -> 64", "+1:64 -1:60 -2:60 -2:64 -2:67 +2:64 +2:67 +2:71");
        offsBeforeOns (rig, 2, "60 -> 64");
        triadAfterMain (rig, "+1:64", 2, 85);
        rig.soundingNow ("60 -> 64", "1:64 2:64 2:67 2:71");
    }
    {
        StageRig rig ("triad mirrors bend and CC11");
        rig.s.triad = true;
        rig.hop ({ noteOn (1, 60, 90) });
        rig.hop ({ bend (1, 9000) });
        rig.bendIs ("main", 1, 9000);
        rig.bendIs ("triad channel", 2, 9000);
        rig.hop ({ cc (1, 11, 80) });
        check (rig.lastCc11[0] == 80 && rig.lastCc11[1] == 80, "%s: expected CC11 80 on channels 1 and 2, found %d and %d (%s)",
               rig.where().c_str(), rig.lastCc11[0], rig.lastCc11[1], stList (rig.last).c_str());
    }
    {
        // was "triad without root" (triadWithRoot is gone): now the duet voicing 1 - 3
        StageRig rig ("voicing duet in thirds");
        rig.s.triad = true;
        rig.s.voicing = 5;
        rig.hop ({ noteOn (1, 62, 90) });
        rig.noteSet ("on 62", "+1:62 +2:62 +2:65");
        rig.hop ({ noteOff (1, 62) });
        rig.soundingNow ("after", "");
    }
    {
        StageRig rig ("triad on the main channel");
        rig.s.triad = true;
        rig.s.triadChannel = 1;
        rig.hop ({ noteOn (1, 60, 90) });
        rig.soundingNow ("on 60, root not doubled", "1:60 1:64 1:67");
        check (stCount (rig.last, "+1:60") == 1, "%s: expected one note-on for 60, found %s", rig.where().c_str(), stList (rig.last).c_str());
        rig.hop ({ bend (1, 9000) });
        check (stCount (rig.last, "b1") == 1, "%s: expected the bend once, found %s", rig.where().c_str(), stList (rig.last).c_str());
        rig.hop ({ cc (1, 11, 70) });
        check (stCount (rig.last, "c1:11") == 1, "%s: expected CC11 once, found %s", rig.where().c_str(), stList (rig.last).c_str());
        rig.hop ({ noteOn (1, 64, 90), noteOff (1, 60) });
        offsBeforeOns (rig, 1, "60 -> 64");
        rig.soundingNow ("60 -> 64", "1:64 1:67 1:71");
        rig.hop ({ noteOff (1, 64) });
        rig.soundingNow ("after", "");
    }
    {
        StageRig rig ("triad with key filter");
        rig.s.triad = true;
        rig.key (0, 0, false);
        rig.hop ({ noteOn (1, 61, 90) });
        rig.notes ("foreign 61", "");
        rig.hop ({ noteOff (1, 61) });
        rig.hop ({ noteOn (1, 60, 90) });
        rig.hop ({ noteOn (1, 61, 90), noteOff (1, 60) });
        rig.noteSet ("60 -> 61", "-1:60 -2:60 -2:64 -2:67");
        rig.hop ({ noteOff (1, 61) });
        rig.soundingNow ("after", "");
    }
    {
        StageRig rig ("triad with a held note");
        rig.s.triad = true;
        rig.key (0, 0, true, true);
        rig.hop ({ bend (1, 9000), noteOn (1, 60, 90) });
        rig.hop ({ noteOn (1, 65, 90), noteOff (1, 60) });
        rig.notes ("hold begins", "");
        rig.bendIs ("hold begins, main", 1, 8192);
        rig.bendIs ("hold begins, triad channel", 2, 8192);
        rig.hop ({ noteOff (1, 65) });
        rig.hop ({ noteOn (1, 62, 80) });
        rig.noteSet ("62 ends the hold", "-1:60 -2:60 -2:64 -2:67 +1:62 +2:62 +2:65 +2:69");
        offsBeforeOns (rig, 1, "62 ends the hold");
        offsBeforeOns (rig, 2, "62 ends the hold");
        rig.hop ({ noteOn (1, 65, 90), noteOff (1, 62) });
        rig.notes ("hold 62", "");
        rig.level = -52.0f;
        rig.hop();
        rig.noteSet ("below the gate", "-1:62 -2:62 -2:65 -2:69");
        rig.hop ({ noteOff (1, 65) });
        rig.soundingNow ("after", "");
    }
}

//==============================================================================
// 7. R5 and releaseAll().
inline void stageSettings()
{
    {
        StageRig rig ("key filter switched on, note foreign");
        rig.hop ({ noteOn (1, 61, 90) });
        rig.key (0, 0, false);
        rig.hop();
        rig.notes ("filter on", "-1:61");
        rig.hop ({ noteOff (1, 61) });
        rig.notes ("tracker off", "");
    }
    {
        StageRig rig ("key filter switched on, note allowed");
        rig.hop ({ noteOn (1, 60, 90) });
        rig.key (0, 0, false);
        rig.hop();
        rig.notes ("filter on", "");
        rig.hop ({ noteOff (1, 60) });
        rig.notes ("tracker off", "-1:60");
    }
    {
        StageRig rig ("key root makes the note foreign");
        rig.key (0, 0, false);
        rig.hop ({ noteOn (1, 64, 90) });
        rig.s.keyRoot = 1;   // C# Ionian has no E
        rig.hop();
        rig.notes ("root C -> C#", "-1:64");
        rig.hop ({ noteOff (1, 64) });
        rig.notes ("tracker off", "");
    }
    {
        StageRig rig ("mode makes the note foreign");
        rig.key (0, 0, false);
        rig.hop ({ noteOn (1, 65, 90) });
        rig.s.scaleMode = 3;   // C Lydian: F#, no F
        rig.hop();
        rig.notes ("Ionian -> Lydian", "-1:65");
        rig.hop ({ noteOff (1, 65) });
        rig.soundingNow ("after", "");
    }
    {
        StageRig rig ("pentatonic makes the note foreign");
        rig.key (0, 0, false);
        rig.hop ({ noteOn (1, 65, 90) });
        rig.s.pentatonic = true;
        rig.hop();
        rig.notes ("pentatonic on", "-1:65");
        rig.hop ({ noteOff (1, 65) });
        rig.soundingNow ("after", "");
    }
    {
        StageRig rig ("held note, sustain off");
        rig.key (0, 0, true, true);
        rig.hop ({ noteOn (1, 60, 90) });
        rig.hop ({ noteOff (1, 60) });
        rig.s.pentaSustain = false;
        rig.hop();
        rig.notes ("sustain off", "-1:60");
    }
    {
        StageRig rig ("held note, root changed, still allowed");
        rig.key (0, 0, true, true);
        rig.hop ({ noteOn (1, 60, 90) });
        rig.hop ({ noteOff (1, 60) });
        rig.s.keyRoot = 5;   // F major pentatonic contains C
        rig.hop();
        rig.notes ("root C -> F", "-1:60");
    }
    {
        StageRig rig ("held note, key filter off");
        rig.key (0, 0, true, true);
        rig.hop ({ noteOn (1, 60, 90) });
        rig.hop ({ noteOn (1, 65, 90), noteOff (1, 60) });
        rig.s.keyFilter = false;
        rig.hop();
        check (stFind (rig.last, "-1:60") >= 0, "%s: expected the held note to end, found %s", rig.where().c_str(), stList (rig.last).c_str());
        rig.hop ({ noteOff (1, 65) });
        rig.soundingNow ("after the tracker's note-off", "");
    }
    {   // not specified whether a suppressed note starts when it becomes allowed;
        // either way nothing may hang
        StageRig rig ("suppressed note, filter off / root changed");
        rig.key (0, 0, false);
        rig.hop ({ noteOn (1, 61, 90) });
        rig.s.keyFilter = false;
        rig.hop();
        rig.hop ({ noteOff (1, 61) });
        rig.soundingNow ("filter off, tracker off", "");
        rig.key (0, 0, false);
        rig.hop ({ noteOn (1, 61, 90) });
        rig.s.keyRoot = 1;
        rig.hop();
        rig.hop ({ noteOff (1, 61) });
        rig.soundingNow ("root C -> C#, tracker off", "");
    }
    {
        StageRig rig ("triad exchanged on a mode change");
        rig.s.triad = true;
        rig.hop ({ noteOn (1, 60, 90) });
        rig.s.scaleMode = 5;   // C Aeolian: C Eb G
        rig.hop();
        check (stFind (rig.last, "+1:60") < 0 && stFind (rig.last, "-1:60") < 0, "%s: expected the main note untouched, found %s",
               rig.where().c_str(), stList (rig.last).c_str());
        rig.soundingNow ("Ionian -> Aeolian", "1:60 2:60 2:63 2:67");
    }
    {
        StageRig rig ("triad exchanged on a root change, key filter on");
        rig.s.triad = true;
        rig.key (0, 0, false);
        rig.hop ({ noteOn (1, 60, 90) });
        rig.s.keyRoot = 10;   // B-flat Ionian: C is the 2nd degree, C Eb G
        rig.hop();
        rig.soundingNow ("root C -> Bb", "1:60 2:60 2:63 2:67");
    }
    {
        StageRig rig ("triad switched on and off mid-note");
        rig.hop ({ noteOn (1, 60, 88) });
        rig.s.triad = true;
        rig.hop();
        rig.noteSet ("triad on", "+2:60 +2:64 +2:67");
        for (const vm::MidiEvent& e : rig.last)
            check (! stIsOn (e) || e.bytes[2] == 88, "%s: expected the main note's velocity 88, found %s", rig.where().c_str(), stText (e).c_str());
        rig.hop ({ bend (1, 9000) });
        rig.hop ({ cc (1, 11, 70) });
        rig.s.triad = false;
        rig.hop();
        rig.noteSet ("triad off", "-2:60 -2:64 -2:67");
        rig.bendIs ("triad off: bend back to the centre on channel 2", 2, 8192);
        check (rig.lastCc11[1] == 127, "%s: expected CC11 127 on channel 2 after the triad was switched off, found %d", rig.where().c_str(), rig.lastCc11[1]);
        rig.soundingNow ("triad off", "1:60");
    }
    {
        StageRig rig ("triad channel moved");
        rig.s.triad = true;
        rig.hop ({ noteOn (1, 60, 88) });
        rig.hop ({ bend (1, 9000) });
        rig.s.triadChannel = 3;
        rig.hop();
        rig.noteSet ("channel 2 -> 3", "-2:60 -2:64 -2:67 +3:60 +3:64 +3:67");
        rig.bendIs ("old triad channel back to the centre", 2, 8192);
        rig.soundingNow ("channel 2 -> 3", "1:60 3:60 3:64 3:67");
        rig.hop ({ noteOff (1, 60) });
        rig.soundingNow ("after", "");
    }
    {
        // was "triad root switched off" (triadWithRoot is gone): now a voicing change 1-3-5 -> 1-3
        StageRig rig ("voicing changed mid-note");
        rig.s.triad = true;
        rig.hop ({ noteOn (1, 60, 88) });
        rig.s.voicing = 5;
        rig.hop();
        rig.soundingNow ("1-3-5 -> 1-3", "1:60 2:60 2:64");
    }
    {
        StageRig rig ("main channel changed, held note");
        rig.key (0, 0, true, true);
        rig.hop ({ noteOn (1, 60, 90) });
        rig.hop ({ noteOff (1, 60) });
        rig.s.midiChannel = 3;
        rig.hop();
        rig.notes ("channel 1 -> 3", "-1:60");
        rig.hop ({ noteOn (3, 62, 90) });
        rig.notes ("on 62", "+3:62");
    }
    {
        StageRig rig ("main channel changed, sounding note with triad");
        rig.s.triad = true;
        rig.hop ({ noteOn (1, 60, 90) });
        rig.s.midiChannel = 3;
        rig.hop ({ noteOff (1, 60) });   // the tracker ends its note on the old channel (R5c)
        rig.soundingNow ("channel 1 -> 3", "");
        rig.hop ({ noteOn (3, 64, 90) });
        rig.soundingNow ("on 64", "2:64 2:67 2:71 3:64");
    }
    {   // R5c with sustain active: the tracker's note-off on the old channel
        // ends the note there; it must not start a hold on the old channel
        StageRig rig ("main channel changed, sounding note, sustain on");
        rig.key (0, 0, true, true);
        rig.hop ({ noteOn (1, 60, 90) });
        rig.s.midiChannel = 3;
        rig.hop ({ noteOff (1, 60) });
        rig.soundingNow ("channel 1 -> 3, level above the gate", "");
        rig.hop ({ noteOn (3, 64, 90) });
        rig.soundingNow ("on 64 on the new channel", "3:64");
    }
    {
        StageRig rig ("releaseAll");
        rig.s.triad = true;
        rig.hop ({ bend (1, 9000), cc (1, 11, 70), noteOn (1, 60, 90) });
        rig.releaseAll();
        rig.soundingNow ("releaseAll", "");
        rig.bendIs ("releaseAll, main", 1, 8192);
        rig.bendIs ("releaseAll, triad channel", 2, 8192);
        check (rig.lastCc11[1] == 127, "%s: expected CC11 127 on channel 2 after releaseAll, found %d", rig.where().c_str(), rig.lastCc11[1]);
        check (! rig.stage.isSounding() && rig.stage.triadCount() == 0, "%s: expected isSounding() false and triadCount() 0, found %d and %d",
               rig.where().c_str(), (int) rig.stage.isSounding(), rig.stage.triadCount());
        rig.releaseAll();
        rig.notes ("second releaseAll", "");
    }
    {
        StageRig rig ("releaseAll, held note");
        rig.key (0, 0, true, true);
        rig.hop ({ noteOn (1, 60, 90) });
        rig.hop ({ noteOff (1, 60) });
        rig.releaseAll();
        rig.notes ("releaseAll", "-1:60");
        rig.hop ({ noteOn (1, 62, 90) });
        rig.notes ("fresh note afterwards", "+1:62");
    }
    {
        StageRig rig ("releaseAll, suppressed note");
        rig.key (0, 0, false);
        rig.hop ({ noteOn (1, 61, 90) });
        rig.releaseAll();
        rig.notes ("releaseAll", "");
    }
}

//==============================================================================
// 8. Fuzz: random tracker stream, levels and settings. The rig checks R1e,
// well-formed bytes, offsets and maxEventsFor on every call.
inline void stageRandom()
{
    std::mt19937 rng (777);
    auto pick = [&rng] (int lo, int hi) { return std::uniform_int_distribution<int> (lo, hi) (rng); };
    std::normal_distribution<float> levelDist (-45.0f, 8.0f);
    const float ranges[] = { 0.2f, 0.3f, 0.5f, 1.0f, 2.0f, 12.0f, 48.0f };

    StageRig rig ("random");
    TrackerSim tracker;
    double semis = 0.0;
    int changes = 0, holds = 0;
    const int startFailures = gFailuresInTest;
    vm::Settings previous = rig.s;

    for (int h = 0; h < 200000 && gFailuresInTest - startFailures < 30; ++h)
    {
        Events in;
        if (pick (0, 150) == 0)
        {
            ++changes;
            switch (pick (0, 9))
            {
                case 0: rig.s.keyFilter = pick (0, 3) != 0; break;
                case 1: rig.s.keyRoot = pick (0, 11); break;
                case 2: rig.s.scaleMode = pick (0, 6); break;
                case 3: rig.s.pentatonic = pick (0, 2) != 0; break;
                case 4: rig.s.pentaSustain = pick (0, 2) != 0; break;
                case 5: rig.s.triad = pick (0, 1) == 1; break;
                case 6: rig.s.triadChannel = pick (0, 3) == 0 ? rig.s.midiChannel : pick (1, 16); break;
                case 7: rig.s.voicing = pick (0, 12); break;
                case 8:
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
                default: rig.s.bendRangeSemis = ranges[pick (0, 6)]; break;
            }
        }

        const double range = rig.s.bendRangeSemis;
        const int a = pick (0, 999);
        if (tracker.note < 0 && a < 40)
        {
            semis = (pick (0, 200) - 100) * 0.005;
            tracker.start (in, pick (55, 80), pick (1, 127), bendValue (semis, range), pick (0, 1) == 0 ? pick (0, 127) : -1);
        }
        else if (tracker.note >= 0 && a < 40)
        {
            int n = pick (55, 80);
            if (n == tracker.note) ++n;
            semis = (pick (0, 200) - 100) * 0.005;
            tracker.change (in, n, pick (1, 127), bendValue (semis, range), pick (0, 1) == 0 ? pick (0, 127) : -1);
        }
        else if (tracker.note >= 0 && a < 60)
            tracker.stop (in);
        else if (a < 400)
        {
            semis = std::max (-range, std::min (range, semis + (pick (0, 200) - 100) * 0.003));
            tracker.moveBend (in, bendValue (semis, range));
        }
        else if (a < 480)
            in.push_back (cc (tracker.channel, 11, pick (0, 127)));

        rig.level = levelDist (rng);
        const vm::Settings before = previous;
        rig.hop (in);
        previous = rig.s;
        holds += rig.stage.isHeld() ? 1 : 0;

        // the hop in words, for failure messages
        auto context = [&] () {
            char b[512];
            const vm::Settings& c = rig.s;
            std::snprintf (b, sizeof (b), "\n          settings: filter %d root %d mode %d penta %d sustain %d triad %d triadCh %d voicing %d mainCh %d (was %d) range %.1f, level %.1f dB"
                                          "\n          held %d, in: %s\n          out: %s",
                           (int) c.keyFilter, c.keyRoot, c.scaleMode, (int) c.pentatonic, (int) c.pentaSustain, (int) c.triad,
                           c.triadChannel, c.voicing, c.midiChannel, before.midiChannel, (double) c.bendRangeSemis,
                           (double) rig.level, (int) rig.stage.isHeld(), stList (in).c_str(), stList (rig.last).c_str());
            return std::string (b);
        };

        // at most one main note; the harmony (up to kMaxVoices) only together with it
        const int mc = rig.s.midiChannel, tc = rig.s.triadChannel;
        const bool separate = rig.s.triad && tc != mc;
        const int total = rig.countAll(), onTriad = separate ? rig.countOn (tc) : 0;
        const int main = total - onTriad;
        const int maxVoices = vm::scale::kMaxVoices;
        bool countOk = true;
        if (! rig.s.triad)
            countOk = total <= 1;
        else if (! separate)
            countOk = total <= 1 + maxVoices;
        else
            countOk = main <= 1 && onTriad <= maxVoices && (main > 0 || onTriad == 0);
        if (! countOk)
            check (false, "%s: expected at most one main note (plus its triad, triad %d, main channel %d, triad channel %d), found %s%s",
                   rig.where().c_str(), (int) rig.s.triad, mc, tc, rig.soundingText().c_str(), context().c_str());
        if (rig.stage.isSounding() && ! rig.sounding[mc - 1][rig.stage.note() & 127])
            check (false, "%s: isSounding() with note %d, expected it sounding on main channel %d, but the output has %s%s",
                   rig.where().c_str(), rig.stage.note(), mc, rig.soundingText().c_str(), context().c_str());
        if (! rig.stage.isSounding() && total != 0)
            check (false, "%s: isSounding() false, expected nothing sounding, but the output has %s%s",
                   rig.where().c_str(), rig.soundingText().c_str(), context().c_str());
    }

    rig.releaseAll();
    rig.soundingNow ("releaseAll at the end", "");
    info ("%d hops, %d setting changes, %d hops with a held note", rig.hops, changes, holds);
}

//==============================================================================
// Engine-level helpers: own per-channel bookkeeping, since decode() assumes one
// monophonic channel.
struct ChannelState
{
    bool sounding[16][128] = {};
    int lastBend[16], lastCc11[16];
    int problems = 0;
    ChannelState() { for (int c = 0; c < 16; ++c) lastBend[c] = lastCc11[c] = -1; }

    void apply (const vm::MidiEvent& e)
    {
        const int ch = e.bytes[0] & 0x0F, n = e.bytes[1] & 0x7F;
        if (stIsOn (e)) { problems += sounding[ch][n] ? 1 : 0; sounding[ch][n] = true; }
        else if (stIsOff (e)) { problems += sounding[ch][n] ? 0 : 1; sounding[ch][n] = false; }
        else if (stIsBend (e)) lastBend[ch] = stBend14 (e);
        else if (stIsCc (e) && e.bytes[1] == 11) lastCc11[ch] = e.bytes[2];
    }

    std::string text() const
    {
        std::string t;
        for (int c = 0; c < 16; ++c)
            for (int k = 0; k < 128; ++k)
                if (sounding[c][k])
                    t += (t.empty() ? "" : " ") + std::to_string (c + 1) + ":" + std::to_string (k);
        return t;
    }
};

// 9. Fractional bend ranges, through the engine.
inline void bendRangeSmall()
{
    struct Case { double cents; float range; int expected; int tolerance; };
    const Case cases[] = {
        { +30.0, 0.5f, 8192 + (int) std::lround (0.30 / 0.5 * 8192.0), (int) std::lround (0.04 / 0.5 * 8192.0) },
        { +30.0, 1.0f, 8192 + (int) std::lround (0.30 / 1.0 * 8192.0), (int) std::lround (0.04 / 1.0 * 8192.0) },
        { +30.0, 0.2f, 16383, 0 },
        { -30.0, 0.2f, 0, 0 },
    };
    for (const Case& c : cases)
    {
        Synth synth (48000.0, 901);
        const Signal sig = synth.render ({ rest (0.1), tone (64.0 + c.cents / 100.0, 0.6, -20.0, 0.02), rest (0.3) }, Timbre());
        vm::Settings s = baseSettings();
        s.bendRangeSemis = c.range;
        const RunResult r = run (sig, s);
        char what[64];
        std::snprintf (what, sizeof (what), "%+.0f cents, range %.1f", c.cents, (double) c.range);
        checkProtocol (r, what);
        check (noteNumbers (r) == std::vector<int> { 64 }, "%s: expected one note 64, found %s", what, describeNotes (r).c_str());
        int worst = 0;
        for (double t = 0.3; t < 0.68; t += 0.02)
        {
            int value = -1;
            for (const TimedValue& b : r.bends)
                if (b.time <= t) value = b.value;
            worst = std::max (worst, std::abs (value - c.expected));
            check (std::abs (value - c.expected) <= c.tolerance, "%s at %.2f s: expected bend %d (+-%d), found %d",
                   what, t, c.expected, c.tolerance, value);
        }
        info ("%s: settled bend within %d of %d", what, worst, c.expected);
    }
}

// 10. Key filter through the engine, quantized.
inline void keyFilterAudio()
{
    Synth synth (48000.0, 902);
    const Signal sig = synth.render ({ rest (0.1), tone (60, 0.3, -20.0, 0.02), rest (0.15), tone (61, 0.3, -20.0, 0.02),
                                       rest (0.15), tone (62, 0.3, -20.0, 0.02), rest (0.3) }, Timbre());
    vm::Settings s = baseSettings();
    s.bendMode = vm::BendMode::Quantized;
    s.keyFilter = true;
    const RunResult r = run (sig, s);
    checkProtocol (r, "key filter, C D-flat D");
    check (noteNumbers (r) == (std::vector<int> { 60, 62 }), "C Ionian filter, detached C4 C#4 D4: expected 60 62, found %s",
           toString (noteNumbers (r)).c_str());
}

// 11. Master switch: off mid-note ends everything at the next hop, then silence.
inline void masterSwitch()
{
    for (int withTriad = 0; withTriad < 2; ++withTriad)
    {
        const char* name = withTriad ? "master switch, triad on" : "master switch";
        Segment vib = tone (64, 2.2, -20.0, 0.03);
        vib.vibratoDepth = 0.2;
        Synth synth (48000.0, 903);
        const Signal sig = synth.render ({ rest (0.1), vib, rest (0.3) }, Timbre());

        vm::Engine engine;
        engine.prepare (sig.sampleRate);
        vm::MidiEventList out;
        out.reserve (4096);
        vm::Settings s = baseSettings();
        s.triad = withTriad != 0;
        s.triadChannel = 2;

        const int block = 96;
        ChannelState st;
        bool wasSounding = false, sawOffBlock = false, cc123Main = false, cc123Triad = false;
        int eventsWhileOff = 0;
        bool movedBend[16] = {}, movedCc[16] = {};
        bool reOnMain = false, reOnTriad = false;
        double reEnabledAt = -1.0;

        for (long long pos = 0; pos < (long long) sig.samples.size(); pos += block)
        {
            const double t = (double) pos / sig.sampleRate;
            const bool wasEnabled = s.enabled;
            s.enabled = ! (t >= 0.8 && t < 1.4);
            const bool firstOff = wasEnabled && ! s.enabled;
            if (firstOff)
                wasSounding = ! st.text().empty() && st.sounding[0][64];
            if (! wasEnabled && s.enabled)
                reEnabledAt = t;

            const int n = (int) std::min<long long> (block, (long long) sig.samples.size() - pos);
            out.clear();
            engine.process (sig.samples.data() + pos, n, s, out);
            for (const vm::MidiEvent& e : out)
            {
                st.apply (e);
                const int ch = e.bytes[0] & 0x0F;
                if (! firstOff && s.enabled)
                {
                    if (stIsBend (e) && stBend14 (e) != 8192) movedBend[ch] = true;
                    if (stIsCc (e) && e.bytes[1] == 11 && e.bytes[2] != 127) movedCc[ch] = true;
                    if (reEnabledAt >= 0.0 && stIsOn (e) && t < reEnabledAt + 0.15)
                    {
                        reOnMain = reOnMain || (ch == 0 && e.bytes[1] == 64);
                        reOnTriad = reOnTriad || ch == 1;
                    }
                }
                if (firstOff && stIsCc (e) && e.bytes[1] == 123 && e.bytes[2] == 0)
                {
                    cc123Main = cc123Main || ch == 0;
                    cc123Triad = cc123Triad || ch == 1;
                }
            }
            if (firstOff)
            {
                sawOffBlock = true;
                check (st.text().empty(), "%s: expected nothing sounding after the first hop switched off, found %s", name, st.text().c_str());
                for (int ch : { 0, 1 })
                {
                    if (movedBend[ch])
                        check (st.lastBend[ch] == 8192, "%s: expected the bend on channel %d back at 8192, found %d", name, ch + 1, st.lastBend[ch]);
                    if (movedCc[ch])
                        check (st.lastCc11[ch] == 127, "%s: expected CC11 on channel %d back at 127, found %d", name, ch + 1, st.lastCc11[ch]);
                    movedBend[ch] = movedCc[ch] = false;
                }
                check (cc123Main, "%s: expected controller 123 (value 0) on channel 1 when switched off", name);
                check (withTriad ? cc123Triad : ! cc123Triad, "%s: expected controller 123 on the triad channel %s",
                       name, withTriad ? "as well" : "not to be sent (triad off)");
            }
            else if (! s.enabled)
            {
                eventsWhileOff += (int) out.size();
            }
        }
        out.clear();
        engine.shutdown (out);
        for (const vm::MidiEvent& e : out)
            st.apply (e);

        check (wasSounding, "%s: test setup: expected note 64 sounding when switched off at 0.8 s", name);
        check (sawOffBlock, "%s: test setup: the switch was never turned off", name);
        check (eventsWhileOff == 0, "%s: expected no events while switched off, found %d", name, eventsWhileOff);
        check (reOnMain, "%s: expected note 64 again within 150 ms after switching back on", name);
        if (withTriad)
            check (reOnTriad, "%s: expected the triad again on channel 2 after switching back on", name);
        check (st.problems == 0, "%s: %d note-on/note-off protocol violation(s)", name, st.problems);
        check (st.text().empty(), "%s: expected nothing sounding after shutdown, found %s", name, st.text().c_str());
    }
}
