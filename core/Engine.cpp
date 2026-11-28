#include "Engine.h"

#include <algorithm>
#include <cmath>

namespace vm
{

namespace
{
    constexpr double kLowpassHz = 5000.0;        // bow hiss above this carries no pitch
    constexpr double kLevelWindowSec = 0.005;
    constexpr float  kRangeMarginSemis = 1.2f;   // search a little beyond the note range
    constexpr float  kIdleSkipDb = 18.0f;        // this far below the gate nothing can start
    constexpr float  kMpeBendRange = 48.0f;      // what MPE instruments expect on the member channels
    constexpr int    kDropFromNote = 69;         // drop voicing: from A4 up, the A and E string


    inline double noteToHz (double note, double a4) noexcept
    {
        return a4 * std::pow (2.0, (note - 69.0) / 12.0);
    }

    inline float hzToNote (double hz, double a4) noexcept
    {
        return (float) (69.0 + 12.0 * std::log2 (hz / a4));
    }
}

void Engine::prepare (double sampleRate)
{
    fsIn = sampleRate > 1000.0 ? sampleRate : 48000.0;

    // Work at 44.1/48 kHz. Higher rates only add cost, not pitch information.
    decim = std::max (1, (int) std::lround (fsIn / 48000.0));
    fs = fsIn / (double) decim;
    decimPhase = 0;

    // About 1.4 ms between analyses.
    hop = std::max (16, (int) std::lround (fs / 750.0));
    hopBuffer.assign ((size_t) hop, 0.0f);
    hopFill = 0;
    hopCount = 0;

    // Fourth-order Butterworth low-pass, also the anti-alias filter when decimating.
    const double cutoff = std::min (kLowpassHz, 0.4 * fs);
    lowpassA.setLowpass (fsIn, cutoff, 0.54119610);
    lowpassB.setLowpass (fsIn, cutoff, 1.30656296);
    lowpassA.reset();
    lowpassB.reset();
    highpass.reset();

    const float absMinHz = (float) noteToHz ((double) kAbsLowestNote - kRangeMarginSemis, kMinA4Hz);
    const float absMaxHz = (float) noteToHz ((double) kAbsHighestNote + kRangeMarginSemis, kMaxA4Hz);
    detector.prepare (fs, absMinHz, absMaxHz);
    tracker.prepare (fs / (double) hop);
    stage.prepare (fs / (double) hop);
    harmonizer.prepare (fsIn);
    harmonizerOn = false;
    frame = Harmonizer::Frame();
    frameFrozen = false;
    lastRoot = -1;
    hopEvents.clear();
    hopEvents.reserve (64);
    wasEnabled = true;

    configured = false;
    currentMeter = Meter();
}

void Engine::reset()
{
    lowpassA.reset();
    lowpassB.reset();
    highpass.reset();
    detector.reset();
    tracker.reset();
    stage.reset();
    hopFill = 0;
    decimPhase = 0;
    currentMeter = Meter();
}

int Engine::maxEventsFor (int numSamples) const noexcept
{
    const int hops = numSamples / std::max (1, hop * decim) + 2;
    return hops * kMaxOutputEventsPerHop + 8;
}

void Engine::configure (const Settings& s)
{
    const int lowest  = std::min (std::max (s.lowestNote, kAbsLowestNote), kAbsHighestNote - 12);
    const int highest = std::min (std::max (s.highestNote, lowest + 12), kAbsHighestNote);
    const float a4    = std::min (std::max (s.a4Hz, kMinA4Hz), kMaxA4Hz);

    if (! configured || s.inputGainDb != cfgGainDb)
    {
        cfgGainDb = s.inputGainDb;
        gain = std::pow (10.0, (double) std::min (std::max (s.inputGainDb, -60.0f), 60.0f) / 20.0);
    }

    if (configured && lowest == cfgLowest && highest == cfgHighest && a4 == cfgA4 && s.response == cfgResponse)
        return;

    cfgLowest = lowest;
    cfgHighest = highest;
    cfgA4 = a4;
    cfgResponse = s.response;
    configured = true;

    detector.setRange ((float) noteToHz ((double) lowest - kRangeMarginSemis, a4),
                       (float) noteToHz ((double) highest + kRangeMarginSemis, a4));

    switch (s.response)
    {
        case Response::Fast:   detector.setWindow (0.80f, 2.5f); break;
        case Response::Normal: detector.setWindow (1.00f, 3.0f); break;
        case Response::Safe:   detector.setWindow (1.25f, 4.0f); break;
    }

    // Rumble, bow handling and the strings below the range are kept out.
    highpass.setHighpass (fs, 0.6 * noteToHz ((double) lowest, a4), 0.70710678);
}

Settings Engine::effective (const Settings& s) noexcept
{
    Settings e = s;
    if (e.mpe)
        e.bendRangeSemis = kMpeBendRange;
    return e;
}

void Engine::process (const float* input, int numSamples, const Settings& userSettings, MidiEventList& out,
                      float* left, float* right)
{
    const Settings settings = effective (userSettings);
    configure (settings);
    lastSettings = settings;

    // The sound output: between two analyses the harmonizer runs with what the
    // last analysis found; the new state takes effect right after its hop.
    const bool hasOutput = left != nullptr && right != nullptr;
    const bool audio = hasOutput && settings.audioHarmony && settings.enabled;
    if (audio != harmonizerOn)
    {
        harmonizer.reset();
        frame = Harmonizer::Frame();
        harmonizerOn = audio;
    }

    if (hasOutput && ! audio)
    {
        std::fill (left, left + numSamples, 0.0f);
        std::fill (right, right + numSamples, 0.0f);
    }

    int spanStart = 0;

    for (int i = 0; i < numSamples; ++i)
    {
        double x = (double) input[i] * gain;
        if (! (std::abs (x) < 1.0e6)) // also rejects NaN and infinity
            x = 0.0;

        x = lowpassB.process (lowpassA.process (x));

        if (++decimPhase < decim)
            continue;
        decimPhase = 0;

        hopBuffer[(size_t) hopFill] = (float) highpass.process (x);

        if (++hopFill == hop)
        {
            hopFill = 0;
            analyseHop (settings, i, out);

            if (audio)
            {
                harmonizer.process (input + spanStart, left + spanStart, right + spanStart, i + 1 - spanStart, settings);
                spanStart = i + 1;
                updateHarmonizerFrame (settings);
                harmonizer.setFrame (frame);
            }
        }
    }

    if (audio && spanStart < numSamples)
        harmonizer.process (input + spanStart, left + spanStart, right + spanStart, numSamples - spanStart, settings);
}

void Engine::updateHarmonizerFrame (const Settings& s) noexcept
{
    frame = Harmonizer::Frame();
    if (! s.enabled)
        return;

    const auto period = [this] (float note) { return (float) (fsIn / noteToHz ((double) note, cfgA4)); };
    const int root = stage.harmonyRoot();
    if (root >= 0)
        lastRoot = root;

    if (stage.harmonyFrozen())
    {
        // The frozen voices are played from the tone as it was when the freeze began.
        if (! frameFrozen)
            frozenPitch = tracker.isActive() ? tracker.trackedPitch() : (float) std::max (lastRoot, 0);
        frameFrozen = true;

        frame.frozen = true;
        frame.sounding = true;
        frame.periodSamples = period (frozenPitch);
        for (int i = 0; i < stage.triadCount() && frame.voiceCount < Harmonizer::kMaxVoices; ++i)
            if (stage.triadNote (i) != lastRoot)      // the unison is the violin itself
                frame.semitones[frame.voiceCount++] = (float) stage.triadNote (i) - frozenPitch;
        applyDropVoicing (frame, stage.triadCount(), lastRoot, s);
        return;
    }

    frameFrozen = false;
    if (root < 0 || ! tracker.isActive())
        return;

    // Settings::vibratoCoupling: at 1 the voices move with the player's pitch
    // (vibrato, intonation), at 0 they stay on the tempered note.
    const float pitch = tracker.trackedPitch();
    const float follow = std::min (std::max (s.vibratoCoupling, 0.0f), 1.0f);
    const float drift = (1.0f - follow) * (pitch - (float) root);

    frame.sounding = true;
    frame.periodSamples = period (pitch);
    for (int i = 0; i < stage.triadCount() && frame.voiceCount < Harmonizer::kMaxVoices; ++i)
        if (stage.triadNote (i) != root)
            frame.semitones[frame.voiceCount++] = (float) (stage.triadNote (i) - root) - drift;
    applyDropVoicing (frame, stage.triadCount(), root, s);
}

void Engine::analyseHop (const Settings& s, int offset, MidiEventList& out)
{
    detector.push (hopBuffer.data(), hop);
    ++hopCount;

    if (! s.enabled)
    {
        // Master switch off: everything ends once, then only the level is measured.
        // The history keeps filling, so detection is there at once when it returns.
        if (wasEnabled)
            silenceOutput (s, offset, out);
        wasEnabled = false;

        const int window = (int) std::lround (fs * kLevelWindowSec);
        currentMeter = Meter();
        currentMeter.levelDb = (float) (10.0 * std::log10 (detector.meanSquare (window) + 1.0e-12));
        return;
    }
    wasEnabled = true;

    const float hint = tracker.isActive()
                         ? (float) (fs / noteToHz ((double) tracker.trackedPitch(), cfgA4))
                         : 0.0f;

    // Nothing sounding and the input far below the gate: skip the search.
    // The history keeps filling, so a note that starts is found just as fast.
    PitchDetector::Result r;
    const int shortWindow = (int) std::lround (fs * kLevelWindowSec);
    const float shortLevelDb = (float) (10.0 * std::log10 (detector.meanSquare (shortWindow) + 1.0e-12));
    if (tracker.isActive() || shortLevelDb >= s.gateDb - kIdleSkipDb)
        r = detector.analyse (hint, tracker.wantsHint());

    // Level over a whole number of periods (at least 5 ms), so it does not
    // flutter at the rate of a low string. While a note sounds, its own period
    // is used: a stray estimate must not change how the level is measured.
    double period = 0.0;
    if (tracker.isActive())
        period = fs / noteToHz ((double) tracker.trackedPitch(), cfgA4);
    else if (r.valid)
        period = r.period;

    int levelWindow = (int) std::lround (fs * kLevelWindowSec);
    if (period > 1.0)
        levelWindow = (int) std::lround (std::ceil ((double) levelWindow / period) * period);
    const float levelDb = (float) (10.0 * std::log10 (detector.meanSquare (levelWindow) + 1.0e-12));

    NoteTracker::Input in;
    in.voiced = r.valid;
    in.pitch = r.valid ? hzToNote (fs / (double) r.period, cfgA4) : 0.0f;
    in.clarity = r.clarity;
    in.hintClarity = r.hintClarity;
    in.octaveDoubt = r.octaveDoubt;
    in.levelDb = levelDb;

    hopEvents.clear();
    tracker.process (in, s, offset, hopEvents);
    stage.process (hopEvents.data(), (int) hopEvents.size(), levelDb, s, offset, out);

    currentMeter.levelDb = levelDb;
    currentMeter.clarity = r.clarity;
    currentMeter.voiced = r.valid;
    currentMeter.pitch = in.pitch;
    currentMeter.active = stage.isSounding();
    currentMeter.note = stage.note();
    currentMeter.held = stage.isHeld();
    currentMeter.outputPitch = ! stage.isSounding() ? 0.0f
                             : (stage.isHeld() || ! tracker.isActive() ? (float) stage.note() : tracker.outputPitch());
    currentMeter.triadCount = stage.triadCount();
    for (int i = 0; i < scale::kMaxVoices; ++i)
        currentMeter.triad[i] = i < stage.triadCount() ? stage.triadNote (i) : -1;
    currentMeter.frozenNote = stage.frozenNote();

    if (trace != nullptr)
    {
        TraceRow row;
        row.timeSec = (double) hopCount * (double) hop / fs;
        row.voiced = r.valid;
        row.pitch = in.pitch;
        row.clarity = r.clarity;
        row.levelDb = levelDb;
        row.active = tracker.isActive();
        row.note = tracker.isActive() ? tracker.note() : -1;
        trace->push_back (row);
    }
}

void Engine::shutdown (MidiEventList& out)
{
    hopEvents.clear();
    tracker.shutdown (0, hopEvents);
    stage.process (hopEvents.data(), (int) hopEvents.size(), -120.0f, lastSettings, 0, out);
    stage.releaseAll (0, out);
    currentMeter.active = currentMeter.held = false;
    currentMeter.note = -1;
    currentMeter.triadCount = 0;
    currentMeter.frozenNote = -1;
}

void Engine::silenceOutput (const Settings& s, int offset, MidiEventList& out)
{
    hopEvents.clear();
    tracker.shutdown (offset, hopEvents);
    stage.process (hopEvents.data(), (int) hopEvents.size(), -120.0f, s, offset, out);
    stage.releaseAll (offset, out);

    // All notes off as well, in case the instrument missed something earlier.
    appendAllNotesOff (s, offset, out);
}

void Engine::appendAllNotesOff (const Settings& s, int offset, MidiEventList& out)
{
    const int mainChannel = std::min (std::max (s.midiChannel, 1), 16) - 1;
    const int harmonyChannel = std::min (std::max (s.triadChannel, 1), 16) - 1;

    for (int channel = 0; channel < 16; ++channel)
    {
        const bool solo = s.mpe ? channel >= 1 : channel == mainChannel;
        if (! solo && ! (s.triad && channel == harmonyChannel))
            continue;

        MidiEvent e;
        e.sampleOffset = offset;
        e.size = 3;
        e.bytes[0] = (uint8_t) (0xB0 | channel);
        e.bytes[1] = 123;
        e.bytes[2] = 0;
        out.push_back (e);
    }
}

void Engine::applyDropVoicing (Harmonizer::Frame& f, int chordNotes, int root, const Settings& s) noexcept
{
    if (! s.dropVoicing || chordNotes < 3 || root < kDropFromNote || f.voiceCount < 1)
        return;

    int top = 0;
    for (int i = 1; i < f.voiceCount; ++i)
        if (f.semitones[i] > f.semitones[top])
            top = i;

    if (f.semitones[top] < 0.5f)
        return;

    const float dropped = f.semitones[top] - 12.0f;
    if (std::abs (dropped) < 0.5f)
        return;
    for (int i = 0; i < f.voiceCount; ++i)
        if (i != top && std::abs (f.semitones[i] - dropped) < 0.5f)
            return;

    f.semitones[top] = dropped;
}

} // namespace vm
