// Runs the engine over a signal and decodes the MIDI it produced.
#pragma once

#include "../core/Engine.h"
#include "Synth.h"

#include <cstdio>
#include <map>
#include <string>
#include <vector>

namespace vmtest
{

struct Note
{
    double on = 0.0, off = -1.0; ///< seconds; off < 0 while the note is still open
    int note = 0, velocity = 0, channel = 0;
};

struct TimedValue
{
    double time;
    int value;
};

struct TimedEvent
{
    long long sample;
    vm::MidiEvent event;
};

struct RunResult
{
    double sampleRate = 48000.0;
    std::vector<TimedEvent> events;
    std::vector<Note> notes;
    std::vector<TimedValue> bends;       ///< 14-bit values
    std::vector<TimedValue> expression;  ///< CC11
    std::vector<vm::Engine::TraceRow> trace;
    std::vector<std::string> problems;   ///< protocol violations
    int maxPolyphony = 0;
    int stuckNotes = 0;
};

inline void decode (RunResult& r)
{
    std::map<int, size_t> open; // channel * 128 + note -> index into notes
    long long lastSample = -1;
    int sounding = 0;

    for (const TimedEvent& te : r.events)
    {
        const vm::MidiEvent& e = te.event;
        const double time = (double) te.sample / r.sampleRate;

        if (te.sample < lastSample)
            r.problems.push_back ("events out of order");
        lastSample = te.sample;

        if (e.size != 3 || (e.bytes[0] & 0x80) == 0 || (e.bytes[1] & 0x80) != 0 || (e.bytes[2] & 0x80) != 0)
        {
            r.problems.push_back ("malformed MIDI message");
            continue;
        }

        const int status = e.bytes[0] & 0xF0, channel = e.bytes[0] & 0x0F;
        const int key = channel * 128 + e.bytes[1];

        if (status == 0x90 && e.bytes[2] > 0)
        {
            if (open.count (key) != 0)
                r.problems.push_back ("note-on for a note that is already sounding");
            Note n;
            n.on = time;
            n.note = e.bytes[1];
            n.velocity = e.bytes[2];
            n.channel = channel;
            open[key] = r.notes.size();
            r.notes.push_back (n);
            ++sounding;
            // A legato change is "new note on, old note off" at the same instant,
            // so two notes may overlap for zero time only.
        }
        else if (status == 0x80 || status == 0x90)
        {
            auto it = open.find (key);
            if (it == open.end())
            {
                r.problems.push_back ("note-off without note-on");
            }
            else
            {
                r.notes[it->second].off = time;
                open.erase (it);
                --sounding;
            }
        }
        else if (status == 0xE0)
        {
            r.bends.push_back ({ time, e.bytes[1] | (e.bytes[2] << 7) });
        }
        else if (status == 0xB0)
        {
            if (e.bytes[1] == 11)
                r.expression.push_back ({ time, e.bytes[2] });
            else if (e.bytes[1] == 123)
                {} // all notes off, sent when the master switch goes off
            else
                r.problems.push_back ("unexpected controller");
        }
        else
        {
            r.problems.push_back ("unexpected status byte");
        }

        r.maxPolyphony = std::max (r.maxPolyphony, sounding);
    }

    // Overlaps longer than zero time are a protocol violation for a mono source.
    for (size_t i = 1; i < r.notes.size(); ++i)
        if (r.notes[i - 1].off < 0.0 || r.notes[i].on < r.notes[i - 1].off - 1.0e-9)
            if (! (r.notes[i - 1].off >= 0.0 && std::abs (r.notes[i].on - r.notes[i - 1].off) < 1.0e-9))
                r.problems.push_back ("two notes sounding at once");

    r.stuckNotes = (int) open.size();
}

/** Processes the whole signal in blocks of `blockSize` samples. */
inline RunResult run (const Signal& signal, const vm::Settings& settings, int blockSize = 64,
                      bool shutdownAtEnd = true, bool wantTrace = false)
{
    RunResult r;
    r.sampleRate = signal.sampleRate;

    vm::Engine engine;
    engine.prepare (signal.sampleRate);
    if (wantTrace)
        engine.setTrace (&r.trace);

    vm::MidiEventList out;
    out.reserve (4096);

    const long long total = (long long) signal.samples.size();
    for (long long pos = 0; pos < total; pos += blockSize)
    {
        const int n = (int) std::min<long long> (blockSize, total - pos);
        out.clear();
        engine.process (signal.samples.data() + pos, n, settings, out);
        for (const vm::MidiEvent& e : out)
            r.events.push_back ({ pos + e.sampleOffset, e });
    }

    if (shutdownAtEnd)
    {
        out.clear();
        engine.shutdown (out);
        for (const vm::MidiEvent& e : out)
            r.events.push_back ({ total, e });
    }

    decode (r);
    return r;
}

/** Pitch the instrument is asked to play at `time`, or a negative value. */
inline double soundingPitch (const RunResult& r, double time, int bendRange)
{
    const Note* current = nullptr;
    for (const Note& n : r.notes)
        if (n.on <= time && (n.off < 0.0 || time < n.off))
            current = &n; // the last one wins at a legato boundary

    if (current == nullptr)
        return -1.0;

    int bend = 8192;
    for (const TimedValue& b : r.bends)
    {
        if (b.time > time)
            break;
        bend = b.value;
    }

    return (double) current->note + (double) (bend - 8192) / 8192.0 * (double) bendRange;
}

struct PitchError
{
    double rmsCents = 0.0, maxCents = 0.0;
    double lagMs = 0.0;
    int points = 0;
};

/** Compares what the instrument plays with what was bowed, between t0 and t1.
    The best time alignment up to maxLagMs is used, since any tracker runs
    a little behind the string. */
inline PitchError pitchError (const RunResult& r, const Signal& s, int bendRange,
                              double t0, double t1, double maxLagMs = 45.0)
{
    PitchError best;
    best.rmsCents = 1.0e9;

    for (double lagMs = 0.0; lagMs <= maxLagMs; lagMs += 1.0)
    {
        double sumSq = 0.0, maxAbs = 0.0;
        int count = 0;
        for (double t = t0; t < t1; t += 0.002)
        {
            const double played = soundingPitch (r, t, bendRange);
            const long long idx = (long long) std::llround ((t - lagMs * 0.001) * s.sampleRate);
            if (idx < 0 || idx >= (long long) s.truePitch.size())
                continue;
            const double truth = s.truePitch[(size_t) idx];
            if (truth < 0.0)
                continue;
            if (played < 0.0)
            {
                sumSq += 1.0e6; // nothing sounding where something should: heavy penalty
                maxAbs = 1.0e3;
                ++count;
                continue;
            }
            const double cents = (played - truth) * 100.0;
            sumSq += cents * cents;
            maxAbs = std::max (maxAbs, std::abs (cents));
            ++count;
        }
        if (count > 0)
        {
            const double rms = std::sqrt (sumSq / (double) count);
            if (rms < best.rmsCents)
            {
                best.rmsCents = rms;
                best.maxCents = maxAbs;
                best.lagMs = lagMs;
                best.points = count;
            }
        }
    }
    return best;
}

inline std::string describeNotes (const RunResult& r)
{
    std::string s;
    char buf[96];
    for (const Note& n : r.notes)
    {
        std::snprintf (buf, sizeof (buf), "%d@%.3f-%.3f(v%d) ", n.note, n.on, n.off, n.velocity);
        s += buf;
    }
    return s;
}

} // namespace vmtest
