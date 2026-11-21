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

void Engine::process (const float* input, int numSamples, const Settings& settings, MidiEventList& out)
{
    configure (settings);
    lastSettings = settings;

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
        }
    }
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
    for (int i = 0; i < 3; ++i)
        currentMeter.triad[i] = i < stage.triadCount() ? stage.triadNote (i) : -1;

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
}

void Engine::silenceOutput (const Settings& s, int offset, MidiEventList& out)
{
    hopEvents.clear();
    tracker.shutdown (offset, hopEvents);
    stage.process (hopEvents.data(), (int) hopEvents.size(), -120.0f, s, offset, out);
    stage.releaseAll (offset, out);

    // All notes off as well, in case the instrument missed something earlier.
    const int mainChannel = std::min (std::max (s.midiChannel, 1), 16) - 1;
    const int triadChannel = std::min (std::max (s.triadChannel, 1), 16) - 1;
    const int channels[2] = { mainChannel, s.triad && triadChannel != mainChannel ? triadChannel : -1 };
    for (int channel : channels)
    {
        if (channel < 0)
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

} // namespace vm
