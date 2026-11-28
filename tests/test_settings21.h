// ViolinMIDI - tests for the shared model of 2.1: the settings table in the
// core with its scope column, the text form of the settings, the migration of
// "Ton halten", the preset rule, and the lock-free ring.
//
// Included by test_main.cpp inside its anonymous namespace (it uses check());
// SettingsText.h, SpscRing.h and the standard headers are included at its top.
#pragma once

//==============================================================================
// Helpers

namespace sf = vm::settingsfile;

/** The setting of that key as the table reads it. */
inline double s21Get (const vm::Settings& e, const char* key)
{
    const auto* f = sf::find (key);
    return f != nullptr ? f->get (e) : -12345.0;
}

/** Every table field at a valid value that differs from its default, chosen by `seed`. */
inline vm::Settings s21Random (unsigned seed)
{
    std::mt19937 rng (seed);
    vm::Settings e;
    for (const auto& f : sf::table())
    {
        const double before = f.get (e);
        double v = before;
        for (int attempt = 0; attempt < 20 && ! (v < before || v > before); ++attempt)
        {
            const double r = std::uniform_real_distribution<double> (0.0, 1.0) (rng);
            if (f.type == sf::Field::Type::Flag)        v = before >= 0.5 ? 0.0 : 1.0;
            else if (f.type == sf::Field::Type::Whole)  v = std::floor (f.low + r * (f.high - f.low + 1.0 - 1.0e-9));
            else                                        v = f.low + r * (f.high - f.low);
        }
        f.set (e, v);
    }
    e.pentatonic = false;   // the 1.3 switch is turned into a scale when read
    e.gateOffDb = std::min (e.gateOffDb, e.gateDb);
    e.bendRangeSemis = 2.0f;
    e.freeVoicing = vm::scale::maskBit (-12) | vm::scale::maskBit (0) | vm::scale::maskBit (7) | vm::scale::maskBit (16);
    e.voicingEdited = true;
    e.voicingVoices = vm::scale::presetMask (vm::scale::clampVoicing (e.voicing)) | vm::scale::maskBit (12);
    e.formantCurve = vm::curve::make ({ { 55.0f, 0.1f }, { 70.5f, 0.35f }, { 88.0f, 0.9f } });
    e.hiCutCurve = vm::curve::make ({ { 55.0f, 9000.0f }, { 80.0f, 2800.0f } });
    sf::tidy (e);
    return e;
}

/** `from` with every table field moved to another valid value. */
inline vm::Settings s21Differ (const vm::Settings& from)
{
    vm::Settings e = from;
    for (const auto& f : sf::table())
    {
        const double v = f.get (from);
        if (f.type == sf::Field::Type::Flag)        f.set (e, v >= 0.5 ? 0.0 : 1.0);
        else if (f.type == sf::Field::Type::Whole)  f.set (e, v + 1.0 <= f.high ? v + 1.0 : f.low);
        else
        {
            const double step = (f.high - f.low) / 3.0;
            f.set (e, v + step <= f.high ? v + step : v - step);
        }
    }
    return e;
}

//==============================================================================
// 1. The table: every key of 2.0 unchanged, the four of 2.1, the scope column.

inline void settingsTable21()
{
    // The keys of 2.0, in its order, with its ranges: they must never change.
    struct Old { const char* key; double low, high; };
    const Old old[] = {
        { "bendMode", 0, 2 }, { "bendRange", 0.1, 96 }, { "dynamics", 0, 2 }, { "response", 0, 2 },
        { "lowestNote", 28, 60 }, { "gainDb", -12, 36 }, { "gateDb", -70, -20 }, { "fixedVelocity", 1, 127 },
        { "a4Hz", 430, 450 }, { "midiChannel", 1, 16 }, { "vibratoSmoothing", 0, 1 }, { "keyFilter", 0, 1 },
        { "keyRoot", 0, 11 }, { "scaleMode", 0, vm::scale::kScaleCount - 1 }, { "pentatonic", 0, 1 }, { "pentaSustain", 0, 1 },
        { "triad", 0, 1 }, { "triadChannel", 1, 16 }, { "gateOffDb", -90, -20 }, { "octaveAttackGuard", 0, 1 },
        { "octaveAttackMs", 0, 60 }, { "octaveSustainGuard", 0, 1 }, { "octaveSustainMs", 0, 100 }, { "articulation", 0, 2 },
        { "dynamicsTarget", 0, 3 }, { "dynamicsSmoothingMs", 0, 150 }, { "voicing", 0, vm::scale::kVoicingCount - 1 },
        { "harmonyMode", 0, 1 }, { "mpe", 0, 1 }, { "jumpGuard", 0, 1 }, { "jumpIntervalSemis", 5, 24 },
        { "jumpWindowMs", 10, 200 }, { "jumpLockMs", 20, 150 }, { "bridgeMs", 0, 200 }, { "overlapMs", 0, 1500 },
        { "releaseMs", 0, 3000 }, { "harmonyOffset", -0.5, 0 }, { "audioHarmony", 0, 1 }, { "vibratoCoupling", 0, 1 },
        { "stereoSpread", 0, 1 }, { "harmonyGainDb", -60, 6 }, { "dryKill", 0, 1 }, { "dropVoicing", 0, 1 },
        { "ensemble", 0, 1 }, { "voicingEdited", 0, 1 }
    };
    const auto& table = sf::table();
    const size_t n = std::size (old);
    check (table.size() == n + 4, "table: %d rows, expected %d (2.0) + 4 (2.1)", (int) table.size(), (int) n);
    for (size_t i = 0; i < n && i < table.size(); ++i)
    {
        const auto& f = table[i];
        const bool flag = f.type == sf::Field::Type::Flag;
        check (std::string (f.key) == old[i].key, "table row %d: key %s, expected %s", (int) i, f.key, old[i].key);
        check (flag || (std::abs (f.low - old[i].low) < 1.0e-9 && std::abs (f.high - old[i].high) < 1.0e-9),
               "table row %s: range %g..%g, expected %g..%g", f.key, f.low, f.high, old[i].low, old[i].high);
    }

    const char* added[] = { "harmonySource", "chordMode", "pentaFilter", "hybridLockMs" };
    for (size_t i = 0; i < std::size (added); ++i)
        check (n + i < table.size() && std::string (table[n + i].key) == added[i], "2.1 row %d is %s", (int) i, added[i]);

    std::set<std::string> keys;
    for (const auto& f : table)
        check (keys.insert (f.key).second, "key %s appears twice", f.key);

    // The scope column: the 13 values of the instrument profile and the wiring.
    const std::vector<std::string> profile = { "lowestNote", "gainDb", "gateDb", "gateOffDb", "octaveAttackGuard", "octaveAttackMs",
                                               "octaveSustainGuard", "octaveSustainMs", "jumpGuard", "jumpIntervalSemis",
                                               "jumpWindowMs", "jumpLockMs", "bridgeMs" };
    check (sf::keysOf (sf::Field::Scope::profile) == profile, "the profile rows are the 13 of the instrument profile");
    check (sf::keysOf (sf::Field::Scope::wiring) == std::vector<std::string> { "dryKill" }, "the wiring row is dryKill");

    // The new fields: defaults, round trip through the table and the text, clamping.
    const vm::Settings defaults;
    check (defaults.harmonySource == vm::HarmonySource::FixedKey && defaults.chordMode == vm::ChordMode::HybridA
               && ! defaults.pentaFilter && std::abs (defaults.hybridLockMs - 330.0f) < 1.0e-6f,
           "2.1 defaults: fixed key, Hybrid A, no pentatonic filter, 330 ms");
    check (defaults.chordSplit == 60 && ! defaults.chordWholeKeyboard && defaults.chordLatch && ! defaults.bowAssist
               && std::abs (defaults.bowCueDipDb - 4.5f) < 1.0e-6f,
           "2.1 defaults of the fields the app fills in: split C4, latch on, no bow assist, 4.5 dB");

    vm::Settings e;
    e.harmonySource = vm::HarmonySource::ExternalChords;
    e.chordMode = vm::ChordMode::HybridB;
    e.pentaFilter = true;
    e.hybridLockMs = 520.0f;
    vm::Settings back;
    check (sf::fromText (sf::toText (e), back), "the text of the 2.1 fields reads back");
    check (back.harmonySource == e.harmonySource && back.chordMode == e.chordMode && back.pentaFilter && std::abs (back.hybridLockMs - 520.0f) < 1.0e-6f,
           "2.1 fields come back: source %d, mode %d, filter %d, lock %g", (int) back.harmonySource, (int) back.chordMode,
           (int) back.pentaFilter, (double) back.hybridLockMs);

    vm::Settings clamped;
    check (sf::fromText ("harmonySource=5\nchordMode=-3\nhybridLockMs=5000\npentaFilter=true\n", clamped), "out-of-range values read");
    check (clamped.harmonySource == vm::HarmonySource::ExternalChords && clamped.chordMode == vm::ChordMode::Pentatonic
               && std::abs (clamped.hybridLockMs - 1000.0f) < 1.0e-6f && clamped.pentaFilter,
           "2.1 fields are clamped: source %d, mode %d, lock %g", (int) clamped.harmonySource, (int) clamped.chordMode, (double) clamped.hybridLockMs);

    // Random settings: text -> settings -> the same text, every field the same.
    for (unsigned seed = 1; seed <= 50; ++seed)
    {
        const auto original = s21Random (seed);
        const auto text = sf::toText (original);
        vm::Settings read;
        std::vector<std::string> unknown;
        const bool ok = sf::fromText (text, read, &unknown);
        check (ok && unknown.empty(), "seed %u: the text reads back without unknown keys", seed);
        check (sf::toText (read) == text, "seed %u: text -> settings -> the same text", seed);
        for (const auto& f : sf::table())
            if (f.get (read) < f.get (original) || f.get (read) > f.get (original))
                check (false, "seed %u: %s is %g, expected %g", seed, f.key, f.get (read), f.get (original));
        check (read.formantCurve == original.formantCurve && read.hiCutCurve == original.hiCutCurve
                   && read.freeVoicing == original.freeVoicing && read.voicingVoices == original.voicingVoices,
               "seed %u: curves and voices come back", seed);
    }

    // The curve and voice formats are those of the settings file of 2.0.
    check (sf::curveText (vm::curve::defaultFormant()) == "55.00:0.0500 69.00:0.2000 76.00:0.7500 88.00:0.9500",
           "formant curve as 2.0 writes it: %s", sf::curveText (vm::curve::defaultFormant()).c_str());
    check (sf::curveText (vm::curve::defaultHiCut()) == "55.00:8000.0000 69.00:6000.0000 76.00:3500.0000 88.00:2500.0000",
           "high-cut curve as 2.0 writes it: %s", sf::curveText (vm::curve::defaultHiCut()).c_str());
    check (sf::voicesText (vm::scale::maskBit (-12) | vm::scale::maskBit (0) | vm::scale::maskBit (7)) == "-12 0 7", "voice list \"-12 0 7\"");

    std::vector<std::string> unknown;
    vm::Settings withUnknown;
    check (! sf::fromText ("future=1\nnot a line\ngainDb=6\n", withUnknown, &unknown), "a line without '=' is reported");
    check (unknown == std::vector<std::string> { "future" } && std::abs (withUnknown.inputGainDb - 6.0f) < 1.0e-6f,
           "unknown keys are reported, the rest is applied");
}

//==============================================================================
// 2. "Ton halten" for every filter: data from before 2.1 keeps its meaning.

inline void settingsMigrateHold()
{
    const int major = 0, majorPenta = 10, blues = 12;
    check (vm::scale::noteCount (major) == 7 && vm::scale::noteCount (majorPenta) == 5 && vm::scale::noteCount (blues) == 6,
           "scales used here: major 7, major pentatonic 5, blues 6 notes");

    const auto run = [] (bool before21, bool hold, int scaleMode)
    {
        vm::Settings e;
        e.pentaSustain = hold;
        e.scaleMode = scaleMode;
        sf::migrateHold (before21, e);
        return e.pentaSustain;
    };
    check (! run (true, true, major), "before 2.1, seven-note scale: the hold had no effect and is switched off");
    check (run (true, true, majorPenta), "before 2.1, pentatonic scale: the hold stays");
    check (run (true, true, blues), "before 2.1, blues scale: the hold stays");
    check (run (false, true, major), "written by 2.1: the hold stays on a seven-note scale");
    check (! run (true, false, majorPenta) && ! run (false, false, major), "a hold that was off stays off");

    // The readers apply it by the presence of "harmonySource" (2.0 writes schema 3 too).
    vm::Settings old;
    check (sf::fromText ("scaleMode=0\npentaSustain=1\nkeyFilter=1\n", old), "text without harmonySource read");
    check (! old.pentaSustain, "text without harmonySource: the hold on a major scale is switched off");
    vm::Settings now;
    check (sf::fromText ("scaleMode=0\npentaSustain=1\nkeyFilter=1\nharmonySource=0\n", now), "text with harmonySource read");
    check (now.pentaSustain, "text with harmonySource: the hold stays");
}

//==============================================================================
// 3. The preset rule: exactly the profile and the wiring stay.

inline void settingsKeepOnPresetLoad()
{
    const auto live = s21Random (7);
    auto preset = s21Differ (live);
    preset.formantCurve = vm::curve::defaultFormant();
    preset.freeVoicing = vm::scale::maskBit (0) | vm::scale::maskBit (3);

    std::vector<std::string> kept;
    for (const auto& f : sf::table())
        check (f.get (live) < f.get (preset) || f.get (live) > f.get (preset), "test setup: %s differs between live and preset", f.key);

    auto loaded = preset;
    vm::keepOnPresetLoad (loaded, live);

    for (const auto& f : sf::table())
    {
        const bool fromLive = ! (f.get (loaded) < f.get (live) || f.get (loaded) > f.get (live));
        const bool fromPreset = ! (f.get (loaded) < f.get (preset) || f.get (loaded) > f.get (preset));
        if (f.scope == sf::Field::Scope::preset)
            check (fromPreset, "%s (preset) comes from the preset", f.key);
        else
            check (fromLive, "%s (profile or wiring) keeps the live value", f.key);
        if (fromLive && ! fromPreset)
            kept.push_back (f.key);
    }

    const std::vector<std::string> expected = { "lowestNote", "gainDb", "gateDb", "gateOffDb", "octaveAttackGuard", "octaveAttackMs",
                                                "octaveSustainGuard", "octaveSustainMs", "jumpGuard", "jumpIntervalSemis",
                                                "jumpWindowMs", "jumpLockMs", "bridgeMs", "dryKill" };
    std::string list;
    for (const auto& k : kept)
        list += k + " ";
    check (kept == expected, "kept exactly the 13 profile fields and dryKill, found: %s", list.c_str());

    // Nothing outside the table changes: curves, voices, the fields the app fills in.
    check (loaded.formantCurve == preset.formantCurve && loaded.freeVoicing == preset.freeVoicing && loaded.voicingVoices == preset.voicingVoices,
           "curves and voices come from the preset");

    // A preset of an older version (fresh defaults) keeps the live detection values.
    vm::Settings fresh;
    fresh.keyRoot = 9;
    fresh.scaleMode = 5;
    vm::keepOnPresetLoad (fresh, live);
    check (std::abs (fresh.inputGainDb - live.inputGainDb) < 1.0e-6f && fresh.lowestNote == live.lowestNote && fresh.dryKill == live.dryKill
               && fresh.keyRoot == 9 && fresh.scaleMode == 5,
           "an older preset gets A minor and keeps the live gain, lowest note and wet routing");
}

//==============================================================================
// 4. A settings text of 2.0 (only its keys) loads unchanged.

inline void settingsText20()
{
    // The engine part of a settings file written by 2.0.0, with values away from the defaults.
    const std::string text =
        "bendMode=1\nbendRange=12.0\ndynamics=1\nresponse=2\nlowestNote=48\ngainDb=6.5\ngateDb=-48.0\nfixedVelocity=90\n"
        "a4Hz=442.0\nmidiChannel=3\nvibratoSmoothing=0.25\nkeyFilter=1\nkeyRoot=2\nscaleMode=11\npentatonic=0\npentaSustain=1\n"
        "triad=1\ntriadChannel=4\ngateOffDb=-56.0\noctaveAttackGuard=0\noctaveAttackMs=30.0\noctaveSustainGuard=1\noctaveSustainMs=20.0\n"
        "articulation=2\ndynamicsTarget=3\ndynamicsSmoothingMs=40.0\nvoicing=1\nharmonyMode=1\nmpe=0\njumpGuard=1\njumpIntervalSemis=9\n"
        "jumpWindowMs=120.0\njumpLockMs=60.0\nbridgeMs=45.0\noverlapMs=200.0\nreleaseMs=400.0\nharmonyOffset=-0.2\naudioHarmony=1\n"
        "vibratoCoupling=0.5\nstereoSpread=0.3499999940395355\nharmonyGainDb=-6.0\ndryKill=1\ndropVoicing=0\nensemble=0.75\n"
        "voicingEdited=1\nfreeVoicing=0 4 7\nvoicingVoices=-12 0 4 7 11\nvoicingVoicesOf=1\n"
        "formantCurve=55.00:0.1000 69.00:0.3000 88.00:0.9000\nhiCutCurve=55.00:9000.0000 80.00:2800.0000\n";

    vm::Settings e;
    std::vector<std::string> unknown;
    check (sf::fromText (text, e, &unknown) && unknown.empty(), "the 2.0 text reads without problems");

    struct Want { const char* key; double value; };
    const Want want[] = {
        { "bendMode", 1 }, { "bendRange", 12 }, { "dynamics", 1 }, { "response", 2 }, { "lowestNote", 48 }, { "gainDb", 6.5 },
        { "gateDb", -48 }, { "fixedVelocity", 90 }, { "a4Hz", 442 }, { "midiChannel", 3 }, { "vibratoSmoothing", 0.25 },
        { "keyFilter", 1 }, { "keyRoot", 2 }, { "scaleMode", 11 }, { "pentatonic", 0 }, { "pentaSustain", 1 }, { "triad", 1 },
        { "triadChannel", 4 }, { "gateOffDb", -56 }, { "octaveAttackGuard", 0 }, { "octaveAttackMs", 30 }, { "octaveSustainGuard", 1 },
        { "octaveSustainMs", 20 }, { "articulation", 2 }, { "dynamicsTarget", 3 }, { "dynamicsSmoothingMs", 40 }, { "voicing", 1 },
        { "harmonyMode", 1 }, { "mpe", 0 }, { "jumpGuard", 1 }, { "jumpIntervalSemis", 9 }, { "jumpWindowMs", 120 },
        { "jumpLockMs", 60 }, { "bridgeMs", 45 }, { "overlapMs", 200 }, { "releaseMs", 400 }, { "harmonyOffset", -0.2 },
        { "audioHarmony", 1 }, { "vibratoCoupling", 0.5 }, { "stereoSpread", 0.35 }, { "harmonyGainDb", -6 }, { "dryKill", 1 },
        { "dropVoicing", 0 }, { "ensemble", 0.75 }, { "voicingEdited", 1 }
    };
    for (const auto& w : want)
        check (std::abs (s21Get (e, w.key) - w.value) < 1.0e-6, "2.0 key %s: %g, expected %g", w.key, s21Get (e, w.key), w.value);

    check (e.voicingVoices == sf::voicesFromText ("-12 0 4 7 11") && e.freeVoicing == sf::voicesFromText ("0 4 7"), "2.0 voices unchanged");
    check (e.formantCurve == vm::curve::make ({ { 55.0f, 0.1f }, { 69.0f, 0.3f }, { 88.0f, 0.9f } })
               && e.hiCutCurve == vm::curve::make ({ { 55.0f, 9000.0f }, { 80.0f, 2800.0f } }),
           "2.0 curves unchanged");

    // The 2.1 fields are at their defaults: the fixed key, as 2.0 played.
    check (e.harmonySource == vm::HarmonySource::FixedKey && e.chordMode == vm::ChordMode::HybridA && ! e.pentaFilter
               && std::abs (e.hybridLockMs - 330.0f) < 1.0e-6f,
           "a 2.0 text plays the fixed key, without the pentatonic filter");

    // Written again, the 2.0 keys say what they said.
    vm::Settings again;
    check (sf::fromText (sf::toText (e), again) && sf::toText (again) == sf::toText (e), "written by 2.1 and read again: the same");
}

//==============================================================================
// 5. The lock-free ring.

static_assert (std::is_trivially_copyable_v<vm::calib::BlockStamp>, "the capture stamp goes through a SpscRing");
static_assert (std::is_trivially_copyable_v<float> && ! std::is_trivially_copyable_v<std::string>,
               "SpscRing takes only trivially copyable items (its own static_assert)");

inline void spscRing()
{
    vm::SpscRing<int> ring (8);
    check (ring.capacity() == 8 && ring.freeSpace() == 8 && ring.available() == 0, "an empty ring of 8");

    const int a[] = { 1, 2, 3, 4, 5 };
    check (ring.push (a, 5) && ring.available() == 5 && ring.freeSpace() == 3, "push 5");
    check (! ring.push (a, 4) && ring.available() == 5, "4 more do not fit: nothing is written (all or nothing)");
    check (ring.push (a, 3) && ring.freeSpace() == 0, "3 more fill it");
    check (! ring.push (a, 1), "full");

    int out[16] = {};
    check (ring.pop (out, 6) == 6 && out[0] == 1 && out[4] == 5 && out[5] == 1, "pop 6 in order");
    check (ring.available() == 2 && ring.freeSpace() == 6, "2 left");

    // Wrap around the end of the buffer.
    const int b[] = { 10, 11, 12, 13, 14, 15 };
    check (ring.push (b, 6) && ring.available() == 8, "push 6 across the end");
    check (ring.pop (out, 16) == 8 && out[0] == 2 && out[1] == 3 && out[2] == 10 && out[7] == 15, "pop everything across the end, in order");
    check (ring.pop (out, 4) == 0, "an empty ring gives nothing");

    check (ring.push (b, 5), "push 5 again");
    ring.discard (3);
    check (ring.available() == 2 && ring.pop (out, 2) == 2 && out[0] == 13 && out[1] == 14, "discard skips 3");
    ring.discard (10);
    check (ring.available() == 0, "discard never skips more than there is");

    // Stamps as the calibration's capture pushes them.
    vm::SpscRing<vm::calib::BlockStamp> stamps (4);
    vm::calib::BlockStamp st;
    st.firstSample = 1234567890123LL;
    st.frames = 256;
    st.hopsAfter = 42;
    st.hostTimeNs = 99;
    st.generation = 3;
    vm::calib::BlockStamp got;
    check (stamps.push (&st, 1) && stamps.pop (&got, 1) == 1 && got.firstSample == st.firstSample && got.frames == 256
               && got.hopsAfter == 42 && got.hostTimeNs == 99 && got.generation == 3,
           "a block stamp goes through unchanged");

    // Two threads, random chunk sizes: the sequence arrives complete and in order.
    vm::SpscRing<uint32_t> shared (1u << 10);
    constexpr uint32_t kCount = 2000000;
    std::atomic<bool> wrong { false };
    std::thread consumer ([&]
    {
        std::mt19937 rng (5);
        uint32_t expected = 0, buffer[300];
        while (expected < kCount)
        {
            const size_t n = shared.pop (buffer, 1 + rng() % 300);
            for (size_t i = 0; i < n; ++i)
                if (buffer[i] != expected++)
                    wrong = true;
        }
    });
    std::mt19937 rng (9);
    uint32_t next = 0, chunk[300];
    while (next < kCount)
    {
        const uint32_t n = std::min<uint32_t> (kCount - next, 1 + rng() % 300);
        for (uint32_t i = 0; i < n; ++i)
            chunk[i] = next + i;
        if (shared.push (chunk, n))
            next += n;
        else
            std::this_thread::yield();
    }
    consumer.join();
    check (! wrong, "two threads: 2 000 000 values arrive complete and in order");
}
