// ViolinMIDI - tests for the Harmonizer (the harmony voices played with the
// violin's own sound).
//
// Included by test_main.cpp after test_tracker2.h, inside that file's anonymous
// namespace (check, info are available). The frames are built here directly,
// as the engine glue would, so pitch, timing and chord changes are exact.
#pragma once

//==============================================================================
// This header is included inside test_main.cpp's anonymous namespace; the
// Harmonizer header and the operator new replacement belong in the global one.
} // namespace

#include "../core/Harmonizer.h"

#include <atomic>
#include <cstdlib>
#include <new>

// Counting allocations: the global operator new is replaced for the whole test
// program, but only counts while gHzCountAllocs is set (around process/setFrame).

inline std::atomic<long> gHzAllocs { 0 };
inline std::atomic<bool> gHzCountAllocs { false };

void* operator new (std::size_t n)
{
    if (gHzCountAllocs.load (std::memory_order_relaxed))
        gHzAllocs.fetch_add (1, std::memory_order_relaxed);
    if (void* p = std::malloc (n > 0 ? n : 1))
        return p;
    throw std::bad_alloc();
}
void* operator new[] (std::size_t n) { return operator new (n); }
void operator delete (void* p) noexcept { std::free (p); }
void operator delete[] (void* p) noexcept { std::free (p); }
void operator delete (void* p, std::size_t) noexcept { std::free (p); }
void operator delete[] (void* p, std::size_t) noexcept { std::free (p); }

namespace
{

//==============================================================================
struct HzOut
{
    std::vector<float> left, right;
    std::vector<float> mono() const
    {
        std::vector<float> m (left.size());
        for (size_t i = 0; i < m.size(); ++i)
            m[i] = 0.5f * (left[i] + right[i]);
        return m;
    }
};

using HzFrameFn = std::function<vm::Harmonizer::Frame (long long sample)>;

/** Runs the harmonizer over `in`, a new frame every `hop` samples, blocks of `block`. */
inline HzOut hzRun (const std::vector<float>& in, double fs, const vm::Settings& s, const HzFrameFn& frameAt,
                    int block = 64, int hop = 64, vm::Harmonizer* use = nullptr)
{
    vm::Harmonizer local;
    vm::Harmonizer& h = use != nullptr ? *use : local;
    if (use == nullptr)
        h.prepare (fs);
    HzOut out;
    out.left.assign (in.size(), 0.0f);
    out.right.assign (in.size(), 0.0f);
    long long pos = 0, nextHop = 0;
    const long long total = (long long) in.size();
    while (pos < total)
    {
        if (pos == nextHop)
        {
            h.setFrame (frameAt (pos));
            nextHop += hop;
        }
        const int n = (int) std::min<long long> ({ (long long) block, nextHop - pos, total - pos });
        h.process (in.data() + pos, out.left.data() + pos, out.right.data() + pos, n, s);
        pos += n;
    }
    return out;
}

inline vm::Harmonizer::Frame hzFrame (bool sounding, double period, std::initializer_list<float> semis, bool frozen = false)
{
    vm::Harmonizer::Frame f;
    f.sounding = sounding;
    f.periodSamples = (float) period;
    f.frozen = frozen;
    for (float x : semis)
        if (f.voiceCount < vm::Harmonizer::kMaxVoices)
            f.semitones[f.voiceCount++] = x;
    return f;
}

inline vm::Settings hzSettings()
{
    vm::Settings s;
    s.audioHarmony = true;
    s.ensemble = 0.0f;                                  // no micro-delay or detune: the measurements need the voices in time
    s.stereoSpread = 0.0f;
    s.hiCutCurve = vm::curve::flat (16000.0f);
    s.formantCurve = vm::curve::flat (1.0f);            // the body where it is, as 1.5 had it by default
    s.dryKill = true;
    return s;
}

//==============================================================================
// Measurements

inline double hzRms (const std::vector<float>& y, long long a, long long b)
{
    a = std::max (0LL, a);
    b = std::min ((long long) y.size(), b);
    double sum = 0.0;
    for (long long i = a; i < b; ++i)
        sum += (double) y[(size_t) i] * y[(size_t) i];
    return b > a ? std::sqrt (sum / (double) (b - a)) : 0.0;
}

inline double hzDb (double x) { return 20.0 * std::log10 (x + 1.0e-12); }

/** Normalised autocorrelation at an integer lag over [a, b). */
inline double hzAutocorr (const std::vector<float>& y, long long a, long long b, long long lag)
{
    double xy = 0.0, xx = 0.0, yy = 0.0;
    for (long long i = a; i + lag < b; ++i)
    {
        const double p = y[(size_t) i], q = y[(size_t) (i + lag)];
        xy += p * q;
        xx += p * p;
        yy += q * q;
    }
    return xx > 0.0 && yy > 0.0 ? xy / std::sqrt (xx * yy) : 0.0;
}

/** Fundamental near `expectedHz` (within +-40 cents) over [a, b): the lag of K
    periods (K x 40 cents stays below half a period, so the right peak is found)
    with a parabola through the neighbours. Returns 0 if there is no clear peak. */
inline double hzMeasureF0 (const std::vector<float>& y, long long a, long long b, double fs, double expectedHz,
                           double* peakOut = nullptr)
{
    const double t = fs / expectedHz;
    const int k = std::max (1, std::min (16, (int) (0.02 * fs / t)));
    const double centre = k * t;
    const long long lo = (long long) std::floor (centre * std::pow (2.0, -40.0 / 1200.0));
    const long long hi = (long long) std::ceil (centre * std::pow (2.0, 40.0 / 1200.0));
    double best = -2.0;
    long long bestLag = lo;
    for (long long lag = lo; lag <= hi; ++lag)
    {
        const double r = hzAutocorr (y, a, b, lag);
        if (r > best) { best = r; bestLag = lag; }
    }
    const double rm = hzAutocorr (y, a, b, bestLag - 1), rp = hzAutocorr (y, a, b, bestLag + 1);
    const double denom = rm - 2.0 * best + rp;
    const double frac = denom < 0.0 ? std::max (-0.5, std::min (0.5, 0.5 * (rm - rp) / denom)) : 0.0;
    if (peakOut != nullptr)
        *peakOut = best;
    return best > 0.5 ? (double) k * fs / ((double) bestLag + frac) : 0.0;
}

inline double hzCents (double hz, double ref) { return hz > 0.0 && ref > 0.0 ? 1200.0 * std::log2 (hz / ref) : 1.0e9; }

/** Amplitude of a sinusoid at `hz` over [a, b), Hann window. */
inline double hzAmplitude (const std::vector<float>& y, long long a, long long b, double fs, double hz)
{
    const double w = 2.0 * kPi * hz / fs;
    double re = 0.0, im = 0.0, wsum = 0.0;
    const double n = (double) (b - a);
    for (long long i = a; i < b; ++i)
    {
        const double win = 0.5 - 0.5 * std::cos (2.0 * kPi * (double) (i - a) / n);
        re += win * y[(size_t) i] * std::cos (w * (double) i);
        im -= win * y[(size_t) i] * std::sin (w * (double) i);
        wsum += win;
    }
    return 2.0 * std::sqrt (re * re + im * im) / wsum;
}

/** Click measure: largest sample-to-sample step relative to the RMS of the
    surrounding 10 ms, worst window in [a, b). Ignores windows quieter than `floorRms`. */
inline double hzJumpRatio (const std::vector<float>& y, long long a, long long b, double fs, double floorRms)
{
    const long long w = (long long) (0.01 * fs);
    double worst = 0.0;
    for (long long s = a; s + w <= b; s += w)
    {
        const double rms = hzRms (y, s - w, s + 2 * w);
        if (rms < floorRms)
            continue;
        double jump = 0.0;
        for (long long i = std::max (s, 1LL); i < s + w; ++i)
            jump = std::max (jump, (double) std::abs (y[(size_t) i] - y[(size_t) i - 1]));
        worst = std::max (worst, jump / rms);
    }
    return worst;
}

/** Where the spectral envelope peaks between loHz and hiHz: harmonic amplitudes
    times their number (undoes the sawtooth's 1/k), smoothed over +-1.1 harmonic
    spacings (PSOLA's new in-between harmonics are weaker), parabola in dB. */
inline double hzEnvelopePeak (const std::vector<float>& y, long long a, long long b, double fs, double f0,
                              double loHz, double hiHz)
{
    std::vector<double> freq, e2;
    for (int h = 1; h * f0 < std::min (8000.0, 0.45 * fs); ++h)
    {
        const double amp = hzAmplitude (y, a, b, fs, h * f0) * h;
        freq.push_back (h * f0);
        e2.push_back (amp * amp);
    }
    const double width = std::max (150.0, 1.1 * f0);
    std::vector<double> smooth (freq.size(), 0.0);
    for (size_t i = 0; i < freq.size(); ++i)
    {
        double sum = 0.0;
        int n = 0;
        for (size_t j = 0; j < freq.size(); ++j)
            if (std::abs (freq[j] - freq[i]) <= width) { sum += e2[j]; ++n; }
        smooth[i] = sum / std::max (1, n);
    }
    size_t best = 0;
    double bestV = -1.0;
    for (size_t i = 1; i + 1 < freq.size(); ++i)
        if (freq[i] >= loHz && freq[i] <= hiHz && smooth[i] > bestV) { bestV = smooth[i]; best = i; }
    if (best == 0)
        return 0.0;
    const double l = 10.0 * std::log10 (smooth[best - 1] + 1e-30), c = 10.0 * std::log10 (smooth[best] + 1e-30),
                 r = 10.0 * std::log10 (smooth[best + 1] + 1e-30);
    const double denom = l - 2.0 * c + r;
    const double frac = denom < 0.0 ? std::max (-0.5, std::min (0.5, 0.5 * (l - r) / denom)) : 0.0;
    return freq[best] + frac * f0;
}

/** Delay of the output's loudness envelope against the input's (both 5 ms RMS). */
inline double hzEnvelopeDelayMs (const std::vector<float>& in, const std::vector<float>& out, long long a, long long b, double fs)
{
    const long long w = (long long) (0.005 * fs);
    auto env = [&] (const std::vector<float>& y) {
        std::vector<double> e ((size_t) (b - a), 0.0);
        double sum = 0.0;
        for (long long i = a - w; i < b; ++i)
        {
            sum += (double) y[(size_t) i] * y[(size_t) i];
            if (i - w >= a - w && i - w >= 0)
                sum -= (double) y[(size_t) (i - w)] * y[(size_t) (i - w)];
            if (i >= a)
                e[(size_t) (i - a)] = std::sqrt (std::max (0.0, sum) / (double) w);
        }
        double mean = 0.0;
        for (double v : e) mean += v;
        mean /= (double) e.size();
        for (double& v : e) v -= mean;
        return e;
    };
    const std::vector<double> ei = env (in), eo = env (out);
    const long long maxLag = (long long) (0.06 * fs);
    double best = -1e30;
    long long bestLag = 0;
    std::vector<double> score ((size_t) maxLag + 2, 0.0);
    for (long long lag = 0; lag <= maxLag + 1; ++lag)
    {
        double s = 0.0;
        for (size_t i = 0; i + (size_t) lag < eo.size(); i += 4)
            s += ei[i] * eo[i + (size_t) lag];
        score[(size_t) lag] = s;
        if (lag <= maxLag && s > best) { best = s; bestLag = lag; }
    }
    double frac = 0.0;
    if (bestLag > 0)
    {
        const double l = score[(size_t) bestLag - 1], c = score[(size_t) bestLag], r = score[(size_t) bestLag + 1];
        const double denom = l - 2.0 * c + r;
        frac = denom < 0.0 ? std::max (-0.5, std::min (0.5, 0.5 * (l - r) / denom)) : 0.0;
    }
    return ((double) bestLag + frac) * 1000.0 / fs;
}

//==============================================================================
void harmonizerPitch()
{
    const double fs = 48000.0;
    const double notes[] = { 34.0, 55.0, 69.0, 88.0 };
    const char* names[] = { "Bb1", "G3", "A4", "E6" };
    const float shifts[] = { -12.0f, -5.0f, 3.0f, 4.0f, 7.0f, 12.0f, 19.0f };

    double worstCents = 0.0;
    for (int ni = 0; ni < 4; ++ni)
    {
        const double note = notes[ni];
        Synth synth (fs, 11 + (unsigned) ni);
        const Signal sig = synth.render ({ tone (note, 1.6, -20.0, 0.02) }, Timbre());
        const double period = fs / noteToHz (note);
        std::mt19937 rng (5);
        std::normal_distribution<double> jitter (0.0, 0.002);   // the engine's estimate wobbles a little
        char line[512];
        int len = std::snprintf (line, sizeof (line), "%-4s", names[ni]);

        for (float shift : shifts)
        {
            vm::Settings s = hzSettings();
            const HzOut out = hzRun (sig.samples, fs, s, [&] (long long) {
                return hzFrame (true, period * (1.0 + jitter (rng)), { shift });
            });
            const std::vector<float> y = out.mono();
            const double expected = noteToHz (note + shift);
            double peak = 0.0;
            const double hz = hzMeasureF0 (y, (long long) (0.5 * fs), (long long) (1.5 * fs), fs, expected, &peak);
            const double cents = hzCents (hz, expected);
            worstCents = std::max (worstCents, std::abs (cents));
            check (std::abs (cents) < 4.0, "%s %+g: voice at %.2f Hz, expected %.2f (%.1f cents, peak %.2f)",
                   names[ni], (double) shift, hz, expected, cents, peak);
            // The right octave: for a shift down by an octave, half the new period must not repeat as well.
            if (shift == -12.0f)
            {
                const long long tOut = (long long) std::lround (fs / expected);
                const double full = hzAutocorr (y, (long long) (0.5 * fs), (long long) (1.5 * fs), tOut);
                const double halfP = hzAutocorr (y, (long long) (0.5 * fs), (long long) (1.5 * fs), tOut / 2);
                check (halfP < full - 0.1, "%s -12: output also repeats after half its period (%.2f vs %.2f)",
                       names[ni], halfP, full);
            }
            len += std::snprintf (line + len, sizeof (line) - (size_t) len, " %+g:%+.1f", (double) shift, cents);
        }
        info ("%s cents", line);
    }
    info ("worst pitch error %.1f cents", worstCents);

    // Dry path: with the voices silent the output is the input, bit for bit, in both channels.
    {
        Synth synth (fs, 3);
        const Signal sig = synth.render ({ tone (69, 0.5), rest (0.1), tone (57, 0.4) }, Timbre());
        vm::Settings s = hzSettings();
        s.dryKill = false;
        for (float g : { -60.0f, -90.0f })
        {
            s.harmonyGainDb = g;
            const HzOut out = hzRun (sig.samples, fs, s, [&] (long long pos) {
                return hzFrame (pos < (long long) (0.5 * fs) || pos > (long long) (0.6 * fs), fs / 440.0, { 4.0f, 7.0f });
            }, 37);
            long long diff = 0;
            for (size_t i = 0; i < sig.samples.size(); ++i)
                diff += (out.left[i] != sig.samples[i]) + (out.right[i] != sig.samples[i]);
            check (diff == 0, "harmony at %g dB: %lld samples differ from the input", (double) g, diff);
        }
    }

    // Delay of the voices: an 8 Hz loudness wobble on the input, found again in the voice.
    {
        char line[512];
        int len = 0;
        vm::Harmonizer h;
        h.prepare (fs);
        for (int ni = 0; ni < 4; ++ni)
        {
            const double note = notes[ni];
            Segment seg = tone (note, 1.5, -20.0, 0.0);
            seg.vibratoRateHz = 8.0;
            seg.vibratoAmDb = 8.0;
            Synth synth (fs, 21);
            const Signal sig = synth.render ({ seg }, Timbre());
            const double period = fs / noteToHz (note);
            double worst = 0.0, sum = 0.0;
            for (float shift : { -12.0f, 4.0f, 7.0f, 12.0f })
            {
                const HzOut out = hzRun (sig.samples, fs, hzSettings(), [&] (long long) { return hzFrame (true, period, { shift }); });
                const double ms = hzEnvelopeDelayMs (sig.samples, out.mono(), (long long) (0.3 * fs), (long long) (1.4 * fs), fs);
                worst = std::max (worst, ms);
                sum += ms;
            }
            const double periodMsHere = 1000.0 / noteToHz (note);
            len += std::snprintf (line + len, sizeof (line) - (size_t) len, " %s %.1f/%.1f ms (period %.1f, reported %.1f);",
                                  names[ni], sum / 4.0, worst, periodMsHere, h.latencySamples ((float) period) * 1000.0 / fs);
            check (worst < 1.5 * periodMsHere + 1.0, "%s: voices %.1f ms late, period %.1f ms", names[ni], worst, periodMsHere);
        }
        info ("delay mean/worst over -12,+4,+7,+12:%s", line);
    }
}

//==============================================================================
/** Band-limited sawtooth with a resonance bump (+14 dB, 2.5 kHz, about 400 Hz wide). */
inline std::vector<float> hzResonantSaw (double fs, double f0, double seconds, double peakHz = 2500.0)
{
    std::vector<float> y ((size_t) (seconds * fs), 0.0f);
    const int harmonics = (int) (std::min (12000.0, 0.45 * fs) / f0);
    for (int k = 1; k <= harmonics; ++k)
    {
        const double f = k * f0;
        const double d = (f - peakHz) / 250.0;
        const double amp = 0.05 / k * (1.0 + 4.0 * std::exp (-d * d));
        const double w = 2.0 * kPi * f / fs;
        for (size_t i = 0; i < y.size(); ++i)
            y[i] += (float) (amp * std::sin (w * (double) i));
    }
    return y;
}

void harmonizerBody()
{
    const double fs = 48000.0;
    const double f0 = 146.83;   // D3
    const std::vector<float> in = hzResonantSaw (fs, f0, 1.2);
    const double inPeak = hzEnvelopePeak (in, (long long) (0.4 * fs), (long long) (1.1 * fs), fs, f0, 800.0, 6000.0);
    check (std::abs (inPeak / 2500.0 - 1.0) < 0.03, "input resonance measured at %.0f Hz", inPeak);

    for (float shift : { 7.0f, -12.0f, 12.0f })
    {
        const double r = std::pow (2.0, shift / 12.0);
        double last = 0.0;
        char line[256];
        int len = 0;
        for (float body : { 1.0f, 0.75f, 0.5f, 0.25f, 0.0f })
        {
            vm::Settings s = hzSettings();
            s.formantCurve = vm::curve::flat (body);
            const HzOut out = hzRun (in, fs, s, [&] (long long) { return hzFrame (true, fs / f0, { shift }); });
            const std::vector<float> y = out.mono();
            const double fOut = hzMeasureF0 (y, (long long) (0.4 * fs), (long long) (1.1 * fs), fs, f0 * r);
            const double peak = hzEnvelopePeak (y, (long long) (0.4 * fs), (long long) (1.1 * fs), fs, fOut, 600.0, 7000.0);
            const double expected = inPeak * std::pow (r, 1.0 - body);
            len += std::snprintf (line + len, sizeof (line) - (size_t) len, " body %.2f: %.0f Hz (want %.0f);", body, peak, expected);
            check (std::abs (peak / expected - 1.0) < 0.07, "%+g st, body %.2f: resonance at %.0f Hz, expected %.0f",
                   (double) shift, body, peak, expected);
            if (body < 1.0f)
                check (shift > 0 ? peak >= last - 30.0 : peak <= last + 30.0, "%+g st: resonance not monotonic in bodyKeep", (double) shift);
            last = peak;
        }
        info ("%+g st:%s", (double) shift, line);
    }
}

//==============================================================================
void harmonizerReleaseOverlap()
{
    const double fs = 48000.0;
    const double note = 69.0, period = fs / noteToHz (note);
    const long long t0 = (long long) (1.0 * fs);

    // --- release: the tone stops at 1 s, the frame says so at the same moment.
    for (float releaseMs : { 500.0f, 0.0f, 1500.0f })
    {
        Segment seg = tone (note, 1.0, -20.0, 0.03);
        seg.vibratoDepth = 0.15;
        Synth synth (fs, 7);
        const Signal sig = synth.render ({ seg, rest (2.0) }, Timbre());
        vm::Settings s = hzSettings();
        s.releaseMs = releaseMs;
        const HzOut out = hzRun (sig.samples, fs, s, [&] (long long pos) { return hzFrame (pos < t0, period, { 4.0f }); });
        const std::vector<float> y = out.mono();
        const double ref = hzRms (y, t0 - (long long) (0.2 * fs), t0 - (long long) (0.02 * fs));
        const long long rel = (long long) (releaseMs * 0.001 * fs);

        if (releaseMs == 0.0f)
        {
            const double after = hzRms (y, t0 + (long long) (0.010 * fs), t0 + (long long) (0.5 * fs));
            check (hzDb (after / ref) < -60.0, "release 0: %.1f dB left after 10 ms", hzDb (after / ref));
            const double steady = hzJumpRatio (y, (long long) (0.3 * fs), t0 - (long long) (0.05 * fs), fs, 0.0);
            const double end = hzJumpRatio (y, t0 - (long long) (0.02 * fs), t0 + (long long) (0.02 * fs), fs, ref * 0.01);
            check (end < 1.5 * steady, "release 0: click measure %.2f vs %.2f steady", end, steady);
            info ("release 0 ms: %.1f dB after 10 ms, click measure %.2f (steady %.2f)", hzDb (after / ref), end, steady);
            continue;
        }

        // Envelope in 10 ms steps: falls monotonically (1 dB grace for the loop's own movement).
        const long long w = (long long) (0.01 * fs);
        double prev = 1.0e9, worstRise = -100.0;
        for (long long a = t0; a + w < t0 + rel; a += w)
        {
            const double v = hzRms (y, a, a + w);
            if (v > ref * 1.0e-3)
                worstRise = std::max (worstRise, hzDb (v / prev));
            prev = std::min (prev, v);
        }
        const double mid = hzRms (y, t0 + rel / 2 - w, t0 + rel / 2 + w);
        const double after = hzRms (y, t0 + rel + (long long) (0.005 * fs), t0 + rel + (long long) (0.3 * fs));
        check (worstRise < 1.0, "release %g: envelope rises by %.1f dB", (double) releaseMs, worstRise);
        check (std::abs (hzDb (mid / ref) - hzDb (0.5)) < 3.0, "release %g: %.1f dB half way (expected -6)", (double) releaseMs, hzDb (mid / ref));
        check (after == 0.0, "release %g: not silent after the release time (%.1f dB)", (double) releaseMs, hzDb (after / ref));

        // The loop: right pitch, no clicks, its spectrum moves no faster than the live tone's.
        const double expected = noteToHz (note + 4.0);
        const long long la = t0 + (long long) (0.02 * fs), lb = t0 + std::min (rel / 2, (long long) (0.4 * fs));
        const double hz = hzMeasureF0 (y, la, lb, fs, expected);
        const double steadyHz = hzMeasureF0 (y, (long long) (0.5 * fs), t0, fs, expected);
        const double cents = hzCents (hz, expected);
        check (std::abs (cents) < 6.0, "release %g: loop at %.1f cents", (double) releaseMs, cents);

        const double steady = hzJumpRatio (y, (long long) (0.3 * fs), t0 - (long long) (0.05 * fs), fs, 0.0);
        const double loop = hzJumpRatio (y, t0 - (long long) (0.05 * fs), t0 + rel, fs, ref * 0.03);
        check (loop < 1.5 * steady, "release %g: click measure %.2f in the release vs %.2f steady", (double) releaseMs, loop, steady);

        auto profileFlux = [&] (long long a, long long b) {
            // Largest change of the harmonic profile (first 8 harmonics, dB, level removed) between 30 ms frames.
            const long long fw = (long long) (0.03 * fs);
            std::vector<double> last;
            double worst = 0.0;
            for (long long p = a; p + fw <= b; p += fw)
            {
                std::vector<double> prof;
                double mean = 0.0;
                for (int k = 1; k <= 8; ++k)
                {
                    prof.push_back (hzDb (hzAmplitude (y, p, p + fw, fs, k * expected)));
                    mean += prof.back() / 8.0;
                }
                for (double& v : prof) v -= mean;
                if (! last.empty())
                {
                    double d = 0.0;
                    for (int k = 0; k < 8; ++k) d += std::abs (prof[(size_t) k] - last[(size_t) k]) / 8.0;
                    worst = std::max (worst, d);
                }
                last = prof;
            }
            return worst;
        };
        const double fluxSteady = profileFlux ((long long) (0.4 * fs), t0);
        const double fluxLoop = profileFlux (t0, t0 + std::min (rel * 3 / 4, (long long) (0.8 * fs)));
        check (fluxLoop < 2.0 * fluxSteady + 1.0, "release %g: spectrum changes by %.1f dB per frame in the loop vs %.1f steady",
               (double) releaseMs, fluxLoop, fluxSteady);
        info ("release %g ms: half way %.1f dB, rise max %.1f dB, loop %.1f cents (live %.1f), click %.2f (steady %.2f), "
              "spectral flux %.1f dB (steady %.1f)", (double) releaseMs, hzDb (mid / ref), worstRise, cents,
              hzCents (steadyHz, expected), loop, steady, fluxLoop, fluxSteady);
    }

    // --- bow lifted at 1 s, but the frame says "sounding" for another 0.4 s (the
    // engine waits for the string to ring down below the gate).
    {
        Segment seg = tone (note, 1.0, -20.0, 0.03);
        Segment ring = tone (note, 0.4, -20.0, 0.0);
        ring.decayDbPerSec = 60.0;      // the string rings down quickly
        Synth synth (fs, 17);
        const Signal sig = synth.render ({ seg, ring, rest (1.5) }, Timbre());
        vm::Settings s = hzSettings();
        s.releaseMs = 1000.0f;
        const long long off = t0 + (long long) (0.4 * fs);
        const HzOut out = hzRun (sig.samples, fs, s, [&] (long long pos) { return hzFrame (pos < off, period, { 4.0f }); });
        const std::vector<float> y = out.mono();
        const double ref = hzRms (y, t0 - (long long) (0.2 * fs), t0 - (long long) (0.02 * fs));
        const double at300 = hzDb (hzRms (y, t0 + (long long) (0.28 * fs), t0 + (long long) (0.32 * fs)) / ref);
        const double input300 = hzDb (hzRms (sig.samples, t0 + (long long) (0.28 * fs), t0 + (long long) (0.32 * fs))
                                      / hzRms (sig.samples, t0 - (long long) (0.2 * fs), t0 - (long long) (0.02 * fs)));
        const double after = hzRms (y, t0 + (long long) (1.3 * fs), t0 + (long long) (1.6 * fs));
        const double click = hzJumpRatio (y, t0 - (long long) (0.02 * fs), t0 + (long long) (1.0 * fs), fs, ref * 0.03);
        const double clickSteady = hzJumpRatio (y, (long long) (0.3 * fs), t0 - (long long) (0.05 * fs), fs, 0.0);
        check (at300 > input300 + 4.0, "bow lifted: voices %.1f dB after 300 ms, the string itself %.1f dB", at300, input300);
        check (after == 0.0, "bow lifted: not silent after the release");
        check (click < 1.5 * clickSteady, "bow lifted: click measure %.2f vs %.2f steady", click, clickSteady);
        info ("bow lifted (frame ends 0.4 s later), release 1000 ms: voices %.1f dB after 300 ms while the string is at %.1f dB, "
              "click %.2f (steady %.2f)", at300, input300, click, clickSteady);
    }

    // --- a sudden piano (-12 dB) is not a lifted bow: the voices stay with the violin.
    {
        Synth synth (fs, 18);
        const Signal sig = synth.render ({ tone (note, 1.0, -14.0, 0.03), tone (note, 1.0, -26.0, 0.0) }, Timbre());
        vm::Settings s = hzSettings();
        s.releaseMs = 1000.0f;
        const HzOut out = hzRun (sig.samples, fs, s, [&] (long long) { return hzFrame (true, period, { 4.0f }); });
        const std::vector<float> y = out.mono();
        const double loud = hzRms (y, t0 - (long long) (0.3 * fs), t0 - (long long) (0.05 * fs));
        const double soft = hzDb (hzRms (y, (long long) (1.6 * fs), (long long) (1.95 * fs)) / loud);
        check (std::abs (soft + 12.0) < 2.0, "sudden piano: voices at %.1f dB after 0.6 s (the violin -12 dB)", soft);
        info ("sudden piano -12 dB: voices %.1f dB 0.6 .. 0.95 s later", soft);
    }

    // --- overlap: (+4, +7) -> (-5, +12) at 1 s while the violin keeps playing.
    for (float overlapMs : { 400.0f, 0.0f })
    {
        Synth synth (fs, 8);
        const Signal sig = synth.render ({ tone (note, 2.0, -20.0, 0.03) }, Timbre());
        vm::Settings s = hzSettings();
        s.overlapMs = overlapMs;
        const HzOut out = hzRun (sig.samples, fs, s, [&] (long long pos) {
            return pos < t0 ? hzFrame (true, period, { 4.0f, 7.0f }) : hzFrame (true, period, { -5.0f, 12.0f });
        });
        const std::vector<float> y = out.mono();
        const double oldHz = noteToHz (note + 4.0), newHz = noteToHz (note + 12.0);
        const long long w = (long long) (0.08 * fs);
        auto amp = [&] (double hz, double atSec) {
            const long long c = t0 + (long long) (atSec * fs);
            return hzAmplitude (y, c - w / 2, c + w / 2, fs, hz);
        };
        const double oldRef = amp (oldHz, -0.2), newRef = amp (newHz, 0.6);
        const double clickSteady = hzJumpRatio (y, (long long) (0.3 * fs), t0 - (long long) (0.05 * fs), fs, 0.0);
        const double clickChange = hzJumpRatio (y, t0 - (long long) (0.02 * fs), t0 + (long long) (0.5 * fs), fs, 0.0);
        check (clickChange < 1.5 * clickSteady, "overlap %g: click measure %.2f vs %.2f steady", (double) overlapMs, clickChange, clickSteady);
        if (overlapMs > 0.0f)
        {
            const double at40 = hzDb (amp (oldHz, 0.04 + 0.04) / oldRef);    // windows are 80 ms: centre +40 ms
            const double at50pct = hzDb (amp (oldHz, 0.2) / oldRef);
            const double past = hzDb (amp (oldHz, 0.4 + 0.05) / oldRef);
            const double newIn = hzDb (amp (newHz, 0.05) / newRef);
            check (at40 > -3.0 && at50pct > -10.0, "overlap 400: old chord %.1f dB at 40 ms, %.1f dB at 200 ms", at40, at50pct);
            check (past < -40.0, "overlap 400: old chord still %.1f dB after 410 ms", past);
            check (newIn > -6.0, "overlap 400: new chord only %.1f dB after 10..90 ms", newIn);
            info ("overlap 400 ms: old chord %.1f dB at 40 ms, %.1f dB at 200 ms, %.1f dB after 410 ms; new chord %.1f dB at 10..90 ms; "
                  "click %.2f (steady %.2f)", at40, at50pct, past, newIn, clickChange, clickSteady);
        }
        else
        {
            const double past = hzDb (amp (oldHz, 0.06) / oldRef);   // 20 .. 100 ms after the change
            check (past < -40.0, "overlap 0: old chord still %.1f dB 20 ms after the change", past);
            info ("overlap 0 ms: old chord %.1f dB 20..100 ms after the change, click %.2f (steady %.2f)", past, clickChange, clickSteady);
        }
    }

    // --- freeze: captured at 0.8 s, the violin then plays another note and stops; 3 s of frozen voice.
    {
        Segment a = tone (note, 1.0, -20.0, 0.03);
        a.vibratoDepth = 0.2;
        Synth synth (fs, 9);
        const Signal sig = synth.render ({ a, tone (72.0, 1.0, -14.0, 0.02), rest (3.0) }, Timbre());
        const long long fOn = (long long) (0.8 * fs), fOff = (long long) (4.0 * fs);
        vm::Settings s = hzSettings();
        s.releaseMs = 200.0f;
        const HzOut out = hzRun (sig.samples, fs, s, [&] (long long pos) {
            const bool snd = pos < (long long) (2.0 * fs);
            const double p = pos < t0 ? period : fs / noteToHz (72.0);
            return hzFrame (snd, p, { 7.0f }, pos >= fOn && pos < fOff);
        });
        const std::vector<float> y = out.mono();
        const double expected = noteToHz (note + 7.0);
        const double ref = hzRms (y, (long long) (0.5 * fs), fOn);
        double lo = 1e9, hi = -1e9, centsLo = 1e9, centsHi = -1e9;
        for (double t = 0.85; t < 3.95; t += 0.1)
        {
            const double v = hzDb (hzRms (y, (long long) (t * fs), (long long) ((t + 0.1) * fs)) / ref);
            lo = std::min (lo, v);
            hi = std::max (hi, v);
        }
        for (double t = 0.9; t < 3.5; t += 0.5)
        {
            const double c = hzCents (hzMeasureF0 (y, (long long) (t * fs), (long long) ((t + 0.5) * fs), fs, expected), expected);
            centsLo = std::min (centsLo, c);
            centsHi = std::max (centsHi, c);
        }
        const double first = hzDb (hzRms (y, (long long) (0.9 * fs), (long long) (1.4 * fs)) / ref);
        const double last = hzDb (hzRms (y, (long long) (3.4 * fs), (long long) (3.9 * fs)) / ref);
        const double click = hzJumpRatio (y, fOn - (long long) (0.05 * fs), fOff, fs, ref * 0.1);
        const double clickSteady = hzJumpRatio (y, (long long) (0.3 * fs), fOn, fs, 0.0);
        check (lo > -3.0 && hi < 2.0, "freeze: level between %.1f and %.1f dB of the live voice", lo, hi);
        check (std::abs (last - first) < 1.0, "freeze: level drifts by %.1f dB over 2.5 s", last - first);
        check (centsLo > -5.0 && centsHi < 5.0, "freeze: pitch between %.1f and %.1f cents", centsLo, centsHi);
        check (click < 1.5 * clickSteady, "freeze: click measure %.2f vs %.2f steady", click, clickSteady);
        const double tail = hzRms (y, fOff + (long long) (0.21 * fs), fOff + (long long) (0.5 * fs));
        check (tail == 0.0, "freeze: not silent after its release");
        info ("freeze 3.1 s: level %.1f .. %.1f dB (drift %.1f dB), pitch %.1f .. %.1f cents, click %.2f (steady %.2f)",
              lo, hi, last - first, centsLo, centsHi, click, clickSteady);
    }
}

//==============================================================================
void harmonizerRobustness()
{
    std::mt19937 rng (1234);
    std::uniform_real_distribution<double> uni (0.0, 1.0);
    auto pick = [&] (double a, double b) { return a + (b - a) * uni (rng); };

    double worstExcess = 0.0;
    long long nonFinite = 0;
    long long allocs = 0;
    for (double fs : { 44100.0, 48000.0, 88200.0, 96000.0 })
    {
        // Signal: tone, silence, full scale square, DC, noise, slides.
        std::vector<float> in;
        Synth synth (fs, 77);
        const Signal sig = synth.render ({ tone (34, 0.4), tone (100, 0.3), slide (50, 80, 0.4), rest (0.2), tone (69, 0.4, -1.0) }, Timbre());
        in = sig.samples;
        for (int i = 0; i < (int) (0.2 * fs); ++i) in.push_back ((i / 40) % 2 == 0 ? 1.0f : -1.0f);
        for (int i = 0; i < (int) (0.2 * fs); ++i) in.push_back (0.7f);
        for (int i = 0; i < (int) (0.3 * fs); ++i) in.push_back ((float) pick (-1.0, 1.0));
        for (int i = 0; i < (int) (0.2 * fs); ++i) in.push_back (1.0e-39f);   // denormal input

        vm::Harmonizer h;
        h.prepare (fs);
        std::vector<float> left (in.size()), right (in.size());
        vm::Settings s;
        size_t pos = 0;
        while (pos < in.size())
        {
            // Garbage frames now and then, sensible ones otherwise.
            vm::Harmonizer::Frame f;
            const double kind = uni (rng);
            f.sounding = uni (rng) < 0.8;
            f.periodSamples = kind < 0.1 ? 0.0f : kind < 0.15 ? -50.0f : kind < 0.2 ? 1.0e9f
                            : kind < 0.22 ? std::numeric_limits<float>::quiet_NaN()
                            : (float) (fs / pick (40.0, 3000.0) * (uni (rng) < 0.1 ? 2.0 : 1.0));
            f.voiceCount = uni (rng) < 0.05 ? 99 : (uni (rng) < 0.05 ? -3 : (int) pick (0.0, 10.99));
            for (float& x : f.semitones)
                x = uni (rng) < 0.05 ? (uni (rng) < 0.5 ? 48.0f : -48.0f) : (float) pick (-24.0, 24.0);
            if (uni (rng) < 0.02)
                f.semitones[0] = std::numeric_limits<float>::infinity();
            f.frozen = uni (rng) < 0.15;

            s.formantCurve = vm::curve::flat ((float) pick (-0.5, 1.5));   // out of range on purpose: clamped
            s.ensemble = uni (rng) < 0.5 ? 1.0f : (float) pick (0.0, 1.0);
            s.stereoSpread = (float) pick (0.0, 1.0);
            s.hiCutCurve = uni (rng) < 0.3 ? vm::curve::defaultHiCut() : vm::curve::flat (16000.0f);
            s.harmonyGainDb = uni (rng) < 0.5 ? 6.0f : (float) pick (-80.0, 6.0);
            s.dryKill = uni (rng) < 0.3;
            s.overlapMs = uni (rng) < 0.5 ? 1500.0f : 0.0f;
            s.releaseMs = uni (rng) < 0.5 ? 3000.0f : 0.0f;

            const int block = uni (rng) < 0.3 ? 1 : (int) pick (1.0, 4096.0);
            const int n = (int) std::min<size_t> ((size_t) block, in.size() - pos);
            gHzCountAllocs = true;
            h.setFrame (f);
            h.process (in.data() + pos, left.data() + pos, right.data() + pos, n, s);
            gHzCountAllocs = false;
            for (int i = 0; i < n; ++i)
            {
                const float l = left[pos + (size_t) i], r = right[pos + (size_t) i];
                if (! std::isfinite (l) || ! std::isfinite (r))
                    ++nonFinite;
                const double excess = std::max (std::abs (l), std::abs (r)) - std::abs (in[pos + (size_t) i]);
                worstExcess = std::max (worstExcess, excess);
            }
            pos += (size_t) n;
        }
        allocs += gHzAllocs.exchange (0);
    }
    check (nonFinite == 0, "%lld output samples not finite", nonFinite);
    check (worstExcess <= 8.0, "output exceeds |input| + 8 by %.2f", worstExcess - 8.0);
    check (allocs == 0, "%lld allocations inside setFrame/process", allocs);
    info ("random frames, settings, block sizes 1..4096 at 44.1..96 kHz: all finite, max |out| - |in| = %.2f "
          "(bound 8), %lld allocations", worstExcess, allocs);

    // NaN / inf input: dry replaced by silence, voices unharmed.
    {
        std::vector<float> in (4800, 0.1f);
        in[100] = std::numeric_limits<float>::quiet_NaN();
        in[200] = std::numeric_limits<float>::infinity();
        vm::Settings s = hzSettings();
        s.dryKill = false;
        const HzOut out = hzRun (in, 48000.0, s, [] (long long) { return hzFrame (true, 100.0, { 4.0f }); });
        bool finite = true;
        for (size_t i = 0; i < in.size(); ++i)
            finite = finite && std::isfinite (out.left[i]) && std::isfinite (out.right[i]);
        check (finite, "NaN / inf input reaches the output");
    }

    // Denormals: a tone that fades into the denormal range must not be slower than the tone itself.
    {
        const double fs = 48000.0;
        Synth synth (fs, 5);
        const Signal sig = synth.render ({ tone (57, 4.0) }, Timbre());
        std::vector<float> tiny (sig.samples.size());
        for (size_t i = 0; i < tiny.size(); ++i)
            tiny[i] = sig.samples[i] * 1.0e-36f;   // well inside the denormal range after the window
        vm::Settings s = hzSettings();
        auto timeIt = [&] (const std::vector<float>& x) {
            const auto t0 = std::chrono::steady_clock::now();
            hzRun (x, fs, s, [&] (long long) { return hzFrame (true, fs / noteToHz (57), { -12.0f, 4.0f, 7.0f }); }, 256);
            return std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        };
        const double normal = std::min (timeIt (sig.samples), timeIt (sig.samples));
        const double denormal = std::min (timeIt (tiny), timeIt (tiny));
        check (denormal < 1.5 * normal + 0.01, "denormal input: %.3f s vs %.3f s", denormal, normal);
        info ("denormal input %.3f s, normal %.3f s for 4 s of audio", denormal, normal);
    }
}

//==============================================================================
void harmonizerPerformance()
{
    const double fs = 48000.0;
    Segment seg = tone (57, 10.0, -20.0, 0.03);
    seg.vibratoDepth = 0.2;
    Synth synth (fs, 99);
    const Signal sig = synth.render ({ seg }, Timbre());
    vm::Settings s;
    s.formantCurve = vm::curve::flat (0.5f);   // resampled grains: the costlier path
    s.ensemble = 1.0f;                         // delay lines and detune on
    s.stereoSpread = 1.0f;
    const double period = fs / noteToHz (57);
    vm::Harmonizer h;
    h.prepare (fs);
    std::vector<float> left (64), right (64);

    double best = 1.0e9;
    for (int pass = 0; pass < 3; ++pass)
    {
        h.reset();
        const auto t0 = std::chrono::steady_clock::now();
        for (size_t pos = 0; pos + 64 <= sig.samples.size(); pos += 64)
        {
            h.setFrame (hzFrame (true, period, { -12.0f, -7.0f, -5.0f, 3.0f, 4.0f, 7.0f, 9.0f, 12.0f, 16.0f, 19.0f }));
            h.process (sig.samples.data() + pos, left.data(), right.data(), 64, s);
        }
        best = std::min (best, std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count());
    }
    const double load = best / 10.0 * 100.0;
    info ("10 voices (-12 .. +19), A3, 48 kHz: 10 s in %.3f s -> %.1f %% of one core", best, load);
    check (load < 25.0, "10 voices need %.1f %% of a core", load);
}

//==============================================================================
// 2.1: a chord change glides the voices in place (Frame::revoice); a new attack on the same
// note starts them anew (Frame::attack); a frame with both crossfades.
void harmonizerGlideAttack() {}
