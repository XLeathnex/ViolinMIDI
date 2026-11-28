// ViolinMIDI - tests for what 2.0 added to the core: voicings as templates
// (diatonic everywhere, voices added to a preset voicing), the key-tracked
// curves, drop voicing and the ensemble decoupling of the audio harmonizer.
//
// Included by test_main.cpp after test_harmonizer.h, inside its anonymous
// namespace (it uses the helpers of test_stage.h and test_harmonizer.h).
#pragma once

//==============================================================================
// 1. Voicings as templates.

/** The voices exactly as 1.5 computed them for a preset voicing (not the free one). */
inline vm::scale::Voices v2OldVoices (int note, int root, int mode, int voicing, bool diatonic, bool withoutPlayedNote)
{
    namespace sc = vm::scale;
    sc::Voices result;
    const auto add = [&] (int n)
    {
        if (n >= 0 && n <= 127 && result.count < sc::kMaxVoices && ! (withoutPlayedNote && n == note))
            result.notes[result.count++] = n;
    };
    const sc::VoicingDef& def = sc::kVoicings[sc::clampVoicing (voicing)];
    for (int i = 0; i < def.count; ++i)
        add (note + (def.steps ? sc::stepInterval (note, root, mode, def.v[i], diatonic) : def.v[i]));
    return result;
}

inline std::string v2Text (const vm::scale::Voices& v)
{
    std::string s;
    for (int i = 0; i < v.count; ++i)
        s += (i == 0 ? "" : " ") + std::to_string (v.notes[i]);
    return s;
}

inline uint64_t v2Mask (std::initializer_list<int> semis)
{
    uint64_t m = 0;
    for (int x : semis)
        m |= vm::scale::maskBit (x);
    return m;
}

inline void voicingTemplates()
{
    namespace sc = vm::scale;

    // Every preset voicing gives exactly what 1.5 gave: all notes, keys, scales, both modes.
    int compared = 0, differ = 0;
    for (int v = 0; v < sc::kVoicingCount; ++v)
    {
        if (v == sc::kFreeVoicing)
            continue;
        for (int mode = 0; mode < sc::kScaleCount; ++mode)
            for (int root = 0; root < 12; ++root)
                for (int note = 0; note < 128; ++note)
                    for (int dia = 0; dia < 2; ++dia)
                        for (int without = 0; without < 2; ++without)
                        {
                            const std::string want = v2Text (v2OldVoices (note, root, mode, v, dia != 0, without != 0));
                            const std::string got = v2Text (sc::voices (note, root, mode, sc::presetMask (v), dia != 0, without != 0));
                            ++compared;
                            if (want != got && ++differ <= 5)
                                check (false, "voicing %d, note %d, root %d, mode %d, %s: 1.5 gave \"%s\", now \"%s\"", v, note, root, mode,
                                       dia ? "diatonic" : "chromatic", want.c_str(), got.c_str());
                        }
    }
    check (differ == 0, "%d of %d voicings differ from 1.5", differ, compared);
    info ("%d preset voicings compared with 1.5: %d differ", compared, differ);

    // The templates themselves.
    check (sc::presetMask (0) == v2Mask ({ 0, 4, 7 }) && sc::presetMask (1) == v2Mask ({ 0, 4, 7, 10 }) && sc::presetMask (2) == v2Mask ({ 0, 7, 16 })
           && sc::presetMask (3) == v2Mask ({ 0, 7, 12 }) && sc::presetMask (4) == v2Mask ({ 0, 5, 10 }) && sc::presetMask (5) == v2Mask ({ 0, 4 })
           && sc::presetMask (6) == v2Mask ({ 0, 9 }) && sc::presetMask (7) == v2Mask ({ -12 }) && sc::presetMask (8) == v2Mask ({ -24, -12 })
           && sc::presetMask (9) == v2Mask ({ -12, 12 }) && sc::presetMask (10) == v2Mask ({ 12 }) && sc::presetMask (11) == v2Mask ({ 12, 24 })
           && sc::presetMask (sc::kFreeVoicing) == 0,
           "preset templates");
    check (vm::Settings().voicingVoices == sc::presetMask (vm::Settings().voicing), "the default voices are the default voicing's");

    // Total Free Voicing (and every other template) is diatonic too: distances read as steps.
    struct Case { const char* what; int note, root, mode; uint64_t mask; bool diatonic; const char* expected; };
    const Case cases[] = {
        { "free 0 4 7 on D in C major: D minor",            62, 0, 0, v2Mask ({ 0, 4, 7 }), true,  "62 65 69" },
        { "free 0 3 7 on E in C major: E minor",            64, 0, 0, v2Mask ({ 0, 3, 7 }), true,  "64 67 71" },
        { "free 0 4 7 on B in C major: B diminished",       71, 0, 0, v2Mask ({ 0, 4, 7 }), true,  "71 74 77" },
        { "free 0 3 6 (diminished shape) on C: C major",    60, 0, 0, v2Mask ({ 0, 3, 6 }), true,  "60 64 67" },
        { "free 0 4 7 chromatic on D: D major",             62, 0, 0, v2Mask ({ 0, 4, 7 }), false, "62 66 69" },
        { "both thirds read as one third",                  60, 0, 0, v2Mask ({ 0, 3, 4 }), true,  "60 64" },
        { "a fourth below E in C major: B",                 64, 0, 0, v2Mask ({ -5 }), true,  "59" },
        { "a third below A in C major: F",                  69, 0, 0, v2Mask ({ -4 }), true,  "65" },
        { "a sixth below A in C major: C",                  69, 0, 0, v2Mask ({ -8 }), true,  "60" },
        { "octaves stay octaves",                           64, 0, 0, v2Mask ({ -24, -12, 12, 24 }), true, "40 52 76 88" },
        { "tenth above D in C major: F",                    62, 0, 0, v2Mask ({ 16 }), true,  "77" },
        { "seventh on G in C major: F (dominant)",          67, 0, 0, v2Mask ({ 0, 4, 7, 11 }), true, "67 71 74 77" },
        { "seventh on C in C major: B (major seventh)",     60, 0, 0, v2Mask ({ 0, 4, 7, 10 }), true, "60 64 67 71" },
        { "A harmonic minor, E: E G# B",                    64, 9, 7, v2Mask ({ 0, 3, 7 }), true,  "64 68 71" },
        { "foreign note (C# in C major): as written",       61, 0, 0, v2Mask ({ 0, 3, 7 }), true,  "61 64 68" },
        { "pentatonic scale: counted in its parent",        62, 0, 10, v2Mask ({ 0, 4, 7 }), true, "62 65 69" },
        { "without the played note",                        62, 0, 0, v2Mask ({ 0, 4, 7 }), true,  "65 69" },
    };
    for (const Case& c : cases)
    {
        const bool without = std::string (c.what) == "without the played note";
        const std::string found = v2Text (sc::voices (c.note, c.root, c.mode, c.mask, c.diatonic, without));
        check (found == c.expected, "%s: expected \"%s\", found \"%s\"", c.what, c.expected, found.c_str());
    }

    // Diatonic never leaves the scale for a note of the scale, whatever the template.
    std::mt19937 rng (77);
    int outside = 0;
    for (int i = 0; i < 20000; ++i)
    {
        const int mode = (int) (rng() % 13), root = (int) (rng() % 12), note = 30 + (int) (rng() % 70);
        uint64_t mask = 0;
        for (int b = 0; b < 6; ++b)
            mask |= 1ull << (rng() % 49);
        const unsigned scaleMask = sc::mask (root, sc::kScales[sc::clampMode (mode)].parent, false);
        if (! sc::contains (scaleMask, note))
            continue;
        const sc::Voices v = sc::voices (note, root, mode, mask, true);
        for (int k = 0; k < v.count; ++k)
            outside += sc::contains (scaleMask, v.notes[k]) ? 0 : 1;
        for (int k = 1; k < v.count; ++k)
            check (v.notes[k] > v.notes[k - 1], "voices must rise and not repeat");
    }
    check (outside == 0, "diatonic voices outside the scale: %d", outside);

    // The helpers the window uses.
    vm::Settings s;
    check (! vm::voicingModified (s) && vm::voicingMask (s) == sc::presetMask (0), "default: pure triad");
    s.voicingVoices |= sc::maskBit (12);
    check (! vm::voicingModified (s) && vm::voicingMask (s) == sc::presetMask (0), "voices without the edited flag do not count");
    s.voicingEdited = true;
    check (vm::voicingModified (s) && vm::voicingMask (s) == v2Mask ({ 0, 4, 7, 12 }), "triad with an octave added: FV");
    s.voicingVoices = sc::presetMask (0);
    check (! vm::voicingModified (s), "edited back to the preset's own voices: not FV");
    s.voicingVoices |= sc::maskBit (12);
    s.voicing = sc::kFreeVoicing;
    check (! vm::voicingModified (s) && vm::voicingMask (s) == s.freeVoicing, "Total Free Voicing is never FV and uses its own voices");
}

// The output stage plays the voices of the voicing as played, and exchanges them when they change.
inline void voicingVoicesStage()
{
    {
        StageRig rig ("triad FV: an octave added");
        rig.s.triad = true;
        rig.s.triadChannel = 2;
        rig.s.voicingEdited = true;
        rig.s.voicingVoices = vm::scale::presetMask (0) | vm::scale::maskBit (12);
        rig.hop ({ noteOn (1, 62, 90) });
        rig.soundingNow ("D minor with the octave", "1:62 2:62 2:65 2:69 2:74");

        rig.s.voicingVoices = vm::scale::presetMask (0);
        rig.hop();
        rig.soundingNow ("back to the pure triad", "1:62 2:62 2:65 2:69");

        rig.s.voicingVoices = vm::scale::presetMask (0) & ~vm::scale::maskBit (0);
        rig.hop();
        rig.soundingNow ("played note taken out", "1:62 2:65 2:69");

        rig.s.freeVoicing = v2Mask ({ -12 });      // not in use: the voicing is not the free one
        rig.hop();
        rig.notes ("free voices do not matter for a preset voicing", "");

        rig.s.voicing = vm::scale::kFreeVoicing;
        rig.hop();
        rig.soundingNow ("Total Free Voicing", "1:62 2:50");
        rig.hop ({ noteOff (1, 62) });
        rig.soundingNow ("after", "");
    }
    {
        StageRig rig ("identical voices kept when switching to the free voicing");
        rig.s.triad = true;
        rig.s.triadChannel = 2;
        rig.hop ({ noteOn (1, 60, 90) });
        rig.s.voicing = vm::scale::kFreeVoicing;
        rig.s.freeVoicing = vm::scale::presetMask (0);
        rig.hop();
        rig.notes ("C E G stays C E G", "");
    }
}

//==============================================================================
// 2. Curves.
inline void curves()
{
    namespace cv = vm::curve;

    const vm::Curve f = cv::defaultFormant(), h = cv::defaultHiCut();
    const auto near = [] (float a, float b, float tol) { return std::abs (a - b) <= tol; };

    // The standard curves of the spec.
    check (near (cv::value (f, 55.0f), 0.05f, 1e-5f) && near (cv::value (f, 69.0f), 0.20f, 1e-5f)
           && near (cv::value (f, 76.0f), 0.75f, 1e-5f) && near (cv::value (f, 88.0f), 0.95f, 1e-5f), "formant curve at its points");
    for (float n = 55.0f; n <= 69.0f; n += 0.25f)
        check (cv::value (f, n) >= 0.0f && cv::value (f, n) <= 0.20f + 1e-6f, "formant G3..A4 within 0..20 %% at %.2f: %.3f", (double) n, (double) cv::value (f, n));
    for (float n = 76.0f; n <= 88.0f; n += 0.25f)
        check (cv::value (f, n) >= 0.75f - 1e-6f && cv::value (f, n) <= 0.95f + 1e-6f, "formant E5..E6 within 75..95 %% at %.2f", (double) n);
    check (near (cv::value (f, 34.0f), 0.05f, 1e-6f) && near (cv::value (f, 100.0f), 0.95f, 1e-6f), "formant outside the range: the end values");

    for (float n = 55.0f; n <= 69.0f; n += 0.25f)
        check (cv::value (h, n, true) >= 6000.0f - 1.0f && cv::value (h, n, true) <= 8000.0f + 1.0f, "high-cut G..D string 6..8 kHz at %.2f", (double) n);
    for (float n = 76.0f; n <= 88.0f; n += 0.25f)
        check (cv::value (h, n, true) >= 2500.0f - 1.0f && cv::value (h, n, true) <= 3500.0f + 1.0f, "high-cut from E5 2.5..3.5 kHz at %.2f", (double) n);

    // Monotone between points, never past them, smooth (no jumps).
    std::mt19937 rng (5);
    std::uniform_real_distribution<float> ux (cv::kLowNote, cv::kHighNote), uy (0.0f, 1.0f);
    for (int round = 0; round < 300; ++round)
    {
        vm::Curve c;
        c.count = 2 + (int) (rng() % (vm::Curve::kMaxPoints - 1));
        for (int i = 0; i < c.count; ++i) { c.x[i] = ux (rng); c.y[i] = uy (rng); }
        c = cv::sanitised (c, 0.0f, 1.0f, cv::defaultFormant());
        for (int i = 1; i < c.count; ++i)
            check (c.x[i] - c.x[i - 1] >= cv::kMinSpacing - 1e-6f, "sanitised: points too close");
        float last = cv::value (c, cv::kLowNote - 1.0f);
        for (float n = cv::kLowNote - 1.0f; n <= cv::kHighNote + 1.0f; n += 0.01f)
        {
            const float v = cv::value (c, n);
            check (std::abs (v - last) < 0.06f, "curve jumps at %.2f: %.3f -> %.3f", (double) n, (double) last, (double) v);
            last = v;
            for (int k = 0; k + 1 < c.count; ++k)
                if (n >= c.x[k] && n <= c.x[k + 1])
                    check (v >= std::min (c.y[k], c.y[k + 1]) - 1e-5f && v <= std::max (c.y[k], c.y[k + 1]) + 1e-5f,
                           "curve leaves its segment between points %d and %d", k, k + 1);
        }
    }

    // sanitised: order, ranges, invalid values, too many and too close points, the fallback
    vm::Curve messy;
    messy.count = 5;
    const float mx[] = { 80.0f, 40.0f, 60.0f, 60.1f, std::numeric_limits<float>::quiet_NaN() };
    const float my[] = { 2.0f, -1.0f, 0.3f, 0.4f, 0.5f };
    for (int i = 0; i < 5; ++i) { messy.x[i] = mx[i]; messy.y[i] = my[i]; }
    const vm::Curve clean = cv::sanitised (messy, 0.0f, 1.0f, cv::defaultFormant());
    check (clean.count == 3 && clean.x[0] == cv::kLowNote && clean.y[0] == 0.0f && near (clean.x[1], 60.0f, 1e-6f)
           && near (clean.y[1], 0.4f, 1e-6f) && clean.x[2] == 80.0f && clean.y[2] == 1.0f,
           "sanitised: found %d points", clean.count);
    vm::Curve none;
    none.count = 3;   // all NaN
    for (int i = 0; i < 3; ++i) none.x[i] = none.y[i] = std::numeric_limits<float>::quiet_NaN();
    check (cv::sanitised (none, 0.0f, 1.0f, cv::defaultFormant()) == cv::defaultFormant(), "no valid point: the fallback");
    none.count = 1000;
    check (cv::sanitised (none, 0.0f, 1.0f, cv::defaultFormant()).count >= 1, "a count out of range is clamped");
    check (near (cv::value (cv::flat (0.3f), 70.0f), 0.3f, 1e-6f) && cv::value (vm::Curve(), 70.0f) == 0.0f, "flat and empty curves");
}

//==============================================================================
// 3. Drop voicing (audio harmonizer only).
inline void dropVoicing()
{
    vm::Settings s;
    const auto frame = [] (const std::vector<float>& semis)
    {
        vm::Harmonizer::Frame f;
        f.sounding = true;
        for (float x : semis)
            f.semitones[f.voiceCount++] = x;
        return f;
    };
    const auto text = [] (const vm::Harmonizer::Frame& f)
    {
        std::string out;
        for (int i = 0; i < f.voiceCount; ++i)
            out += (i == 0 ? "" : " ") + std::to_string ((int) std::lround (f.semitones[i]));
        return out;
    };

    // The chord is the violin plus the voices: two voices make three notes.
    struct Case { const char* what; std::vector<float> semis; int root; bool on; const char* expected; };
    const Case cases[] = {
        { "triad on A4: the fifth drops",                  { 4.0f, 7.0f },        69, true,  "4 -5" },
        { "triad on E5",                                   { 3.0f, 7.0f },        76, true,  "3 -5" },
        { "triad on G#4 (below A4): nothing",              { 3.0f, 7.0f },        68, true,  "3 7" },
        { "two-note chord on A4: nothing",                 { 4.0f },              69, true,  "4" },
        { "switched off: nothing",                         { 4.0f, 7.0f },        69, false, "4 7" },
        { "seventh chord: the seventh drops",              { 4.0f, 7.0f, 10.0f }, 72, true,  "4 7 -2" },
        { "octave would land on the violin: nothing",      { 4.0f, 12.0f },       72, true,  "4 12" },
        { "would land on another voice: nothing",          { -5.0f, 7.0f },       72, true,  "-5 7" },
        { "all voices below the violin: nothing",          { -12.0f, -5.0f },     72, true,  "-12 -5" },
        { "vibrato drift kept on the dropped voice",       { 3.8f, 6.8f },        70, true,  "4 -5" },
    };
    for (const Case& c : cases)
    {
        s.dropVoicing = c.on;
        vm::Harmonizer::Frame f = frame (c.semis);
        vm::Engine::applyDropVoicing (f, c.root, s);
        check (text (f) == c.expected, "%s: expected \"%s\", found \"%s\"", c.what, c.expected, text (f).c_str());
    }
    s.dropVoicing = true;
    vm::Harmonizer::Frame f = frame ({ 3.8f, 6.8f });
    vm::Engine::applyDropVoicing (f, 70, s);
    check (std::abs (f.semitones[1] - (-5.2f)) < 1e-5f, "dropped by exactly an octave: %.3f", (double) f.semitones[1]);

    // Through the engine: a triad played on A4 with the audio harmonizer sounds the fifth below.
    {
        const double fs = 48000.0;
        vm::Settings es = baseSettings();
        es.triad = true;
        es.audioHarmony = true;
        es.dryKill = true;
        es.ensemble = 0.0f;
        es.stereoSpread = 0.0f;
        es.voicing = 5;                                  // duet in thirds: two notes, nothing drops
        for (int pass = 0; pass < 3; ++pass)
        {
            if (pass >= 1)
                es.voicing = 0;                            // triad: A C E, the E drops below
            if (pass == 2)
                es.triadChannel = es.midiChannel;          // the harmony on the melody's channel: the same chord
            vm::Engine engine;
            engine.prepare (fs);
            Synth synth (fs, 21);
            const Signal sig = synth.render ({ rest (0.1), tone (69.0, 1.0, -20.0, 0.02), rest (0.2) }, Timbre());
            std::vector<float> left (sig.samples.size()), right (sig.samples.size());
            vm::MidiEventList events;
            events.reserve (8192);
            for (size_t pos = 0; pos < sig.samples.size(); pos += 64)
            {
                const int n = (int) std::min<size_t> (64, sig.samples.size() - pos);
                events.clear();
                engine.process (sig.samples.data() + pos, n, es, events, left.data() + pos, right.data() + pos);
            }
            std::vector<float> mono (left.size());
            for (size_t i = 0; i < mono.size(); ++i) mono[i] = 0.5f * (left[i] + right[i]);
            // Energy near 330 Hz (E4, the dropped fifth) against 659 Hz (E5, where it would be).
            const auto power = [&] (double hz)
            {
                double re = 0.0, im = 0.0;
                const long long a = (long long) (0.5 * fs), b = (long long) (1.0 * fs);
                for (long long i = a; i < b; ++i)
                {
                    const double ph = 2.0 * 3.14159265358979 * hz * (double) i / fs;
                    re += mono[(size_t) i] * std::cos (ph);
                    im += mono[(size_t) i] * std::sin (ph);
                }
                return re * re + im * im;
            };
            const double low = power (noteToHz (64.0)), high = power (noteToHz (76.0));
            if (pass == 0)
                check (power (noteToHz (72.0)) > 10.0 * power (noteToHz (60.0)), "duet on A4: the third C5 sounds above, not dropped");
            else
                check (low > 4.0 * high, "triad on A4%s: the fifth sounds as E4 below (E4 %.3g, E5 %.3g)",
                       pass == 2 ? " (harmony on the melody's channel)" : "", low, high);
        }
    }
}

//==============================================================================
// 4. Ensemble decoupling and the key-tracked high-cut in the harmonizer.
inline void ensembleAndHiCut()
{
    const double fs = 48000.0;
    const double note = 69.0, period = fs / noteToHz (note);
    const std::vector<float> in = hzResonantSaw (fs, noteToHz (note), 1.0);
    const long long t0 = (long long) (0.3 * fs);

    // The tone and its voices start at 0.3 s; one voice a fifth up, one an octave up.
    const auto onset = [&] (const vm::Settings& s, bool rightChannel)
    {
        const HzOut out = hzRun (in, fs, s, [&] (long long pos) { return hzFrame (pos >= t0, period, { 7.0f, 12.0f }); });
        const std::vector<float>& y = rightChannel ? out.right : out.left;
        double peak = 0.0;
        for (size_t i = (size_t) (0.6 * fs); i < y.size(); ++i) peak = std::max (peak, (double) std::abs (y[i]));
        for (size_t i = (size_t) t0; i < y.size(); ++i)
            if (std::abs (y[i]) > 0.2 * peak)
                return ((double) i - (double) t0) * 1000.0 / fs;
        return -1.0;
    };

    // Voice 1 alone on the left, voice 2 alone on the right: full spread puts them there.
    vm::Settings dry = hzSettings();
    dry.stereoSpread = 1.0f;
    vm::Settings wet = dry;
    wet.ensemble = 1.0f;

    const double plain1 = onset (dry, false), plain2 = onset (dry, true);
    const double late1 = onset (wet, false), late2 = onset (wet, true);
    info ("voice onsets without / with ensemble: voice 1 %.1f / %.1f ms, voice 2 %.1f / %.1f ms", plain1, late1, plain2, late2);
    check (plain1 >= 0.0 && plain1 < 6.0 && plain2 >= 0.0 && plain2 < 6.0, "without ensemble the voices come at once (%.1f, %.1f ms)", plain1, plain2);
    check (late1 - plain1 > 7.0 && late1 - plain1 < 13.0, "voice 1: 8..12 ms later with ensemble, found %.1f ms", late1 - plain1);
    check (late2 - plain2 > 15.0 && late2 - plain2 < 23.0, "voice 2: 16..22 ms later with ensemble, found %.1f ms", late2 - plain2);

    // Detune: the voice's pitch moves by about +-4..6 cents over a few seconds, not more.
    {
        const std::vector<float> longIn = hzResonantSaw (fs, noteToHz (note), 6.0);
        vm::Settings s = hzSettings();
        s.ensemble = 1.0f;
        const HzOut out = hzRun (longIn, fs, s, [&] (long long) { return hzFrame (true, period, { 7.0f }); });
        const std::vector<float> y = out.mono();
        double lo = 1e9, hi = -1e9;
        for (double t = 0.5; t + 0.2 < 6.0; t += 0.1)
        {
            const double f = hzMeasureF0 (y, (long long) (t * fs), (long long) ((t + 0.2) * fs), fs, noteToHz (note + 7.0));
            const double cents = 1200.0 * std::log2 (f / noteToHz (note + 7.0));
            lo = std::min (lo, cents);
            hi = std::max (hi, cents);
        }
        info ("ensemble detune: %.1f .. %.1f cents", lo, hi);
        check (hi - lo > 3.0 && hi < 9.0 && lo > -9.0, "detune should swing a few cents (4..6) around the note: %.1f .. %.1f", lo, hi);
    }

    // After the end of a chord the delay lines play out: no click, then silence.
    {
        vm::Settings s = hzSettings();
        s.ensemble = 1.0f;
        s.releaseMs = 0.0f;
        const long long stop = (long long) (0.7 * fs);
        const HzOut out = hzRun (in, fs, s, [&] (long long pos) { return hzFrame (pos >= t0 && pos < stop, period, { 7.0f, 12.0f }); });
        const std::vector<float> y = out.mono();
        double jump = 0.0;
        for (size_t i = (size_t) stop; i + 1 < (size_t) (stop + 0.06 * fs); ++i)
            jump = std::max (jump, (double) std::abs (y[i + 1] - y[i]));
        double during = 0.0;
        for (size_t i = (size_t) (0.5 * fs); i + 1 < (size_t) (0.6 * fs); ++i)
            during = std::max (during, (double) std::abs (y[i + 1] - y[i]));
        check (jump <= during * 1.2 + 1e-4, "the end of a delayed chord must not click: step %.4f against %.4f while playing", jump, during);
        check (hzRms (y, stop + (long long) (0.08 * fs), stop + (long long) (0.2 * fs)) < 1e-4, "silence once the delay lines have played out");
    }

    // High-cut follows the played pitch: with a curve falling from 16 kHz (G3) to 1 kHz (E6),
    // a voice played high is much darker than with the cut at 16 kHz throughout.
    {
        const auto brightness = [&] (const vm::Curve& cut, double playNote)
        {
            const double p = fs / noteToHz (playNote);
            const std::vector<float> tone = hzResonantSaw (fs, noteToHz (playNote), 0.8);
            vm::Settings s = hzSettings();
            s.hiCutCurve = cut;
            const HzOut out = hzRun (tone, fs, s, [&] (long long) { return hzFrame (true, p, { 0.0f }); });
            const std::vector<float> y = out.mono();
            // high-pass difference as a crude measure of energy at the top
            double hf = 0.0, all = 0.0;
            for (size_t i = (size_t) (0.3 * fs); i + 1 < y.size(); ++i)
            {
                hf += (double) (y[i + 1] - y[i]) * (y[i + 1] - y[i]);
                all += (double) y[i] * y[i];
            }
            return hf / std::max (all, 1e-12);
        };
        const vm::Curve falling = vm::curve::make ({ { 55.0f, 16000.0f }, { 88.0f, 1000.0f } });
        const vm::Curve open = vm::curve::flat (16000.0f);
        const double highOpen = brightness (open, 86.0), highCut = brightness (falling, 86.0);
        const double lowOpen = brightness (open, 56.0), lowCut = brightness (falling, 56.0);
        info ("high-cut key tracking: high note %.3g -> %.3g, low note %.3g -> %.3g", highOpen, highCut, lowOpen, lowCut);
        check (highCut < 0.5 * highOpen, "a high note under a falling curve is darker (%.3g vs %.3g)", highCut, highOpen);
        check (lowCut > 0.8 * lowOpen, "a low note under the same curve keeps its top (%.3g vs %.3g)", lowCut, lowOpen);
    }
}
