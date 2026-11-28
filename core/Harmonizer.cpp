// ViolinMIDI core - the harmony as sound (see Harmonizer.h).
//
// Method: pitch-synchronous overlap-add (TD-PSOLA). Every voice is built from
// short grains of the input, each two periods long, Hann-windowed and centred
// on a "mark" of the waveform. Placing the grains one *output* period apart
// (input period / pitch ratio) sets the new pitch; what is inside a grain - the
// shape of one period, i.e. the body resonance - is kept. Reading a grain
// faster or slower than real time (resampling factor f) moves that resonance
// by f, so the formant control costs nothing extra:
//
//     pitch ratio r = 2^(semitones / 12),   f = r^(1 - bodyKeep)
//
// f = 1 keeps the body where it is (classic PSOLA), f = r is plain resampling
// (the cello effect an octave down). bodyKeep is not one number: it follows the
// played pitch along Settings::formantCurve, and so does the cut-off of the
// low-pass on the voices (Settings::hiCutCurve), smoothed over ~25 ms.
//
// Ensemble decoupling (Settings::ensemble). The voices are the violin's own
// tone, so played exactly in time with it they comb-filter against it and
// against each other. Each voice therefore comes a little late (odd voices
// 8..12 ms, even ones 16..22 ms, through a delay line after the grains, so it
// never reads the previous note) and is detuned by a slow sine of 4..6 cents
// at 0.2..0.5 Hz; the values are drawn for each new chord. A layer that has
// faded out keeps running until its delay lines are empty.
//
// Marks. A bowed string has no glottal-pulse-like epochs, so the marks are a
// phase grid one period apart: every period a new mark is placed one period
// after the last and moved to where the waveform correlates best with the
// period around the last mark. The grid therefore stays locked to the wave
// even when the period estimate is a little off, and the spacing it measures
// is the pitch the voices are built on. Voices may use grid points predicted
// beyond the last refined mark, so a grain can start as soon as its first
// sample exists: the voices lag the input by half a period on average.
//
// Loops. Release, overlap at a change of chord and freeze all need a tone that
// is no longer coming in. The best stretch of recent marks (stable period,
// loud, 4..6 periods or 120..230 ms) is copied away and the grains then walk
// back and forth through it at real-time speed, turning at random points: no
// splice, no fixed loop length, the natural vibrato and bow movement go on.
//
// Real time: process() and setFrame() only touch memory allocated in prepare().
// std::pow / exp2 appear once per grain or per block, never per sample.

#include "Harmonizer.h"

#include "Biquad.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace vm
{

namespace
{
    constexpr double kPi = 3.14159265358979323846;

    // --- interpolation: windowed sinc, 8 taps, 1024 sub-sample phases (phase
    // error below -60 dB up to 10 kHz). One table per anti-alias cutoff: a grain
    // read faster than real time (f > 1) would fold everything above fs / 2f
    // back into the audio band, so it uses a kernel cut off there.
    constexpr int kTaps = 8;
    constexpr int kTapsBefore = 3;                 // taps at i-3 .. i+4
    constexpr int kPhases = 1024;
    constexpr int kKernelTables = 7;               // f = 1, 2^(1/3) .. 4
    constexpr double kKaiserBeta = 6.0;
    constexpr double kCutoffMargin = 0.92;         // of the (reduced) Nyquist frequency

    constexpr int kWindowSize = 4096;              // Hann table

    // A grain reads the input this far behind the newest sample (the taps after i).
    constexpr double kReadMargin = 5.0;

    // Grains per voice: overlap is 2 r^bodyKeep, so +24 semitones with the body
    // kept needs 8. More than this (garbage intervals) and grains are skipped.
    constexpr int kMaxGrains = 12;

    // Live chord + up to four ringing ones + one being replaced.
    constexpr int kLayers = 6;
    constexpr int kSlots = kLayers + 1;

    constexpr double kMinHz = 30.0, kMaxHz = 4500.0;  // valid tone (E7 is 2637 Hz)
    constexpr float  kMaxSemis = 48.0f;
    constexpr float  kChordJumpSemis = 0.7f;       // larger hop-to-hop jump of a voice = new chord, not vibrato
    constexpr double kOctaveJumpOct = 0.55;        // period jumps larger than this must last ...
    constexpr int    kOctaveJumpHops = 3;          // ... this many hops (about 4 ms) before they are believed

    constexpr double kRingSec = 1.2;               // input history
    constexpr int    kHistory = 4096;              // refined marks remembered (0.9 s at 4.5 kHz)
    constexpr double kSearchSec = 0.6;             // how far back a loop may come from
    constexpr double kCaptureSec = 0.3;            // storage per loop
    constexpr double kLoopTargetSec = 0.2;         // preferred loop length ...
    constexpr double kLoopTargetPeriods = 6.0;     // ... but at least this many periods
    constexpr float  kJoinPeriodJump = 0.04f;
    // A loop replays its vibrato in random order, so its pitch would wander by
    // the vibrato depth around a centre that depends on where the loop came
    // from. Half of each grain's deviation from the loop's mean period is kept:
    // the tone still moves, the chord stays in tune.
    constexpr double kLoopVibrato = 0.5;      // marks further apart in period than this break a loop region
    constexpr float  kGoodCorrelation = 0.7f;
    constexpr float  kUsableCorrelation = 0.5f;

    // Bow lifted: the engine ends a note only once the string has rung down below
    // the gate, by then the voices would have faded with it and a release would
    // start from almost nothing. So a fast drop of the level (9 dB below the
    // peak of the last moments, which itself falls 10 dB/s) ends the live voices
    // at once and starts the release from there. Played again (back within 6 dB
    // of that peak, or a new chord) or still bowing after all (the level holds:
    // less than 1.5 dB lost in 100 ms, a sudden piano rather than a ring-down):
    // live voices return.
    constexpr double kBowLevelSec = 0.02;
    constexpr double kPeakFallDbPerSec = 10.0;
    constexpr double kBowStopDb = 9.0;
    constexpr double kBowBackDb = 6.0;
    constexpr double kBowHoldSec = 0.1;
    constexpr double kBowHoldDb = 1.5;

    constexpr double kShortFadeMs = 10.0;          // chord change with overlap 0, freeze off
    constexpr double kStartFadeMs = 5.0;           // voices of a new note
    constexpr double kMinReleaseMs = 5.0;
    constexpr double kGainSmoothMs = 5.0;
    constexpr double kDryRampMs = 10.0;
    constexpr double kBusLimit = 4.0;              // safety net: +12 dBFS on the voice bus

    // Ensemble decoupling at 1: odd voices (1, 3, ...) 8..12 ms late, even ones
    // 16..22 ms; each detuned by a sine of 4..6 cents at 0.2..0.5 Hz.
    constexpr double kEnsembleDelayMs[2][2] = { { 8.0, 12.0 }, { 16.0, 22.0 } };
    constexpr double kEnsembleMaxDelayMs = 22.0;
    constexpr double kDetuneCentsLow = 4.0, kDetuneCentsHigh = 6.0;
    constexpr double kDetuneHzLow = 0.2, kDetuneHzHigh = 0.5;

    // The key-tracked low-pass moves to a new cut-off over about this time.
    constexpr double kHiCutSmoothMs = 25.0;
    constexpr double kReferenceA4 = 440.0;    // for the key note of a period (curves are coarse)

    inline double besselI0 (double x) noexcept
    {
        double sum = 1.0, term = 1.0;
        for (int k = 1; k < 40; ++k)
        {
            term *= (x * 0.5 / (double) k) * (x * 0.5 / (double) k);
            sum += term;
            if (term < 1.0e-12 * sum)
                break;
        }
        return sum;
    }

    inline float smoothstep (double x) noexcept { return (float) (x * x * (3.0 - 2.0 * x)); }

    inline uint32_t xorshift (uint32_t& s) noexcept
    {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return s;
    }

    inline double random01 (uint32_t& s) noexcept { return (double) (xorshift (s) >> 8) * (1.0 / 16777216.0); }

    inline uint32_t nextPow2 (uint32_t n) noexcept
    {
        uint32_t p = 1;
        while (p < n)
            p <<= 1;
        return p;
    }
}

//==============================================================================
struct Harmonizer::Impl
{
    struct Mark
    {
        double pos = 0.0;       // absolute input sample index (fractional)
        float  period = 0.0f;   // measured distance to the previous mark
        float  energy = 0.0f;   // mean square over one period around the mark
        bool   joined = false;  // continuous with the previous mark (same tone, good correlation)
    };

    struct Grain
    {
        const float* buf = nullptr;
        uint32_t mask = 0;
        const float* kernels = nullptr;   // table for this grain's anti-alias cutoff
        double pos = 0.0, step = 1.0;     // source position and advance per output sample
        double phase = 0.0, phaseInc = 0.0;
        float gain = 1.0f;
        int remaining = 0;
    };

    struct Capture
    {
        std::vector<float> data;
        uint32_t mask = 0;
        std::vector<double> pos;
        std::vector<float> period;
        int marks = 0;
        double lo = 0.0, hi = 0.0;
        double meanPeriod = 100.0;
        float gain = 1.0f;
        int users = 0;
    };

    struct Voice
    {
        float semis = 0.0f;
        Grain grains[kMaxGrains];
        int grainCount = 0;
        bool started = false;
        double nextS = 0.0;      // output time of the next grain centre
        double nextStart = 0.0;  // when to look at it again
        // loop clock (capture-relative)
        double u = 0.0, turn = 0.0;
        int dir = -1, markIndex = 0;
        // ensemble decoupling, drawn when the layer starts
        uint32_t rng = 1;
        int    delay = 0;                    // samples late, through the voice's delay line
        double lfoPhase = 0.0, lfoRate = 0.3, lfoDepth = 5.0;   // radians, Hz, cents
        // stereo
        float gL = 1.0f, gR = 1.0f, dL = 0.0f, dR = 0.0f;
    };

    enum class State { Off, Active, Fading, Draining };   // Draining: faded out, the delay lines empty themselves

    struct Layer
    {
        State state = State::Off;
        int capture = -1;        // -1: live input
        int count = 0;
        float env = 0.0f, fadeInInc = 1.0f, fadeFrom = 1.0f;
        double fadeX = 0.0, fadeInc = 0.0;
        int maxDelay = 0, drain = 0;     // longest delay of its voices; samples left while draining
        Voice voices[kMaxVoices];
    };

    // --- configuration
    double fs = 48000.0;
    double pMin = 10.0, pMax = 1600.0;
    double gainCoeff = 0.01, dryStep = 0.001;

    // --- tables
    std::vector<float> kernels;   // [table][phase 0..kPhases][tap]
    std::vector<float> window;    // Hann, kWindowSize + 1 points

    // --- input and marks
    std::vector<float> ring;
    uint32_t ringMask = 0;
    long long written = 0;

    bool tracking = false, gridValid = false;
    double anchor = 0.0, periodUsed = 100.0, nextRefine = 0.0;
    std::vector<Mark> history;
    long long histCount = 0;

    // --- frame state
    double framePeriod = 0.0, pendingPeriod = 0.0;
    int pendingHops = 0;
    bool sounding = false, frozen = false;
    double fastEnergy = 0.0, peakEnergy = 0.0, stopPeak = 0.0, holdEnergy = 0.0, holdFrom = 0.0;
    bool bowStopped = false, heldOff = false;
    int prevCount = 0;
    float prevSemis[kMaxVoices] = {};
    int live = -1;
    uint32_t seedCounter = 0x9E3779B9u;

    Layer layers[kLayers];
    Capture caps[kSlots];

    // --- parameters (from process, used by setFrame too)
    double overlapMs = 0.0, releaseMs = 0.0;
    double bodyKeep = 1.0, ensemble = 0.0;

    // --- ensemble delay lines, one per voice of every layer
    std::vector<float> delayBuf;
    uint32_t delayMask = 0;
    int delaySize = 1;

    // --- output
    Biquad cutL, cutR;
    double cutHz = -1.0, cutSetHz = -1.0;   // smoothed cut-off and the one the filters have
    float gain = 0.0f, dryGain = 1.0f;
    bool firstBlock = true;

    //==========================================================================
    void prepare (double sampleRate)
    {
        fs = (sampleRate > 1000.0 && sampleRate < 1.0e6) ? sampleRate : 48000.0;
        pMin = fs / kMaxHz;
        pMax = fs / kMinHz;
        gainCoeff = 1.0 - std::exp (-1.0 / (kGainSmoothMs * 0.001 * fs));
        dryStep = 1.0 / (kDryRampMs * 0.001 * fs);

        buildTables();

        ring.assign (nextPow2 ((uint32_t) (kRingSec * fs)), 0.0f);
        ringMask = (uint32_t) ring.size() - 1;
        history.assign ((size_t) kHistory, Mark());

        delaySize = (int) nextPow2 ((uint32_t) std::ceil (kEnsembleMaxDelayMs * 0.001 * fs) + 4);
        delayMask = (uint32_t) delaySize - 1;
        delayBuf.assign ((size_t) kLayers * kMaxVoices * (size_t) delaySize, 0.0f);

        const uint32_t capSize = nextPow2 ((uint32_t) (kCaptureSec * fs) + 64);
        for (Capture& c : caps)
        {
            c.data.assign (capSize, 0.0f);
            c.mask = capSize - 1;
            c.pos.assign ((size_t) kHistory, 0.0);
            c.period.assign ((size_t) kHistory, 0.0f);
        }
        reset();
    }

    void buildTables()
    {
        kernels.assign ((size_t) kKernelTables * (kPhases + 1) * kTaps, 0.0f);
        const double i0Beta = besselI0 (kKaiserBeta);
        for (int t = 0; t < kKernelTables; ++t)
        {
            const double cutoff = kCutoffMargin / std::exp2 ((double) t / 3.0);
            for (int p = 0; p <= kPhases; ++p)
            {
                const double frac = (double) p / (double) kPhases;
                double h[kTaps], sum = 0.0;
                for (int j = 0; j < kTaps; ++j)
                {
                    const double d = (double) (j - kTapsBefore) - frac;   // distance from the read position
                    const double x = cutoff * d;
                    const double sinc = std::abs (x) < 1.0e-9 ? 1.0 : std::sin (kPi * x) / (kPi * x);
                    const double u = d / (double) (kTaps / 2);
                    const double kaiser = std::abs (u) >= 1.0 ? 0.0 : besselI0 (kKaiserBeta * std::sqrt (1.0 - u * u)) / i0Beta;
                    h[j] = sinc * kaiser;
                    sum += h[j];
                }
                // Unity gain at DC for every phase, so a steady level carries no phase ripple.
                float* row = kernels.data() + ((size_t) t * (kPhases + 1) + (size_t) p) * kTaps;
                for (int j = 0; j < kTaps; ++j)
                    row[j] = (float) (h[j] / sum);
            }
        }

        window.assign ((size_t) kWindowSize + 1, 0.0f);
        for (int k = 0; k <= kWindowSize; ++k)
            window[(size_t) k] = (float) (0.5 - 0.5 * std::cos (2.0 * kPi * (double) k / (double) kWindowSize));
    }

    void reset() noexcept
    {
        std::fill (ring.begin(), ring.end(), 0.0f);
        written = 0;
        tracking = gridValid = false;
        anchor = 0.0;
        periodUsed = 100.0;
        nextRefine = 0.0;
        histCount = 0;
        framePeriod = pendingPeriod = 0.0;
        pendingHops = 0;
        sounding = frozen = false;
        prevCount = 0;
        live = -1;
        for (Layer& l : layers)
        {
            l.state = State::Off;
            l.capture = -1;
            l.count = 0;
            l.maxDelay = l.drain = 0;
            for (Voice& v : l.voices)
                v.grainCount = 0;
        }
        for (Capture& c : caps)
            c.users = 0;
        std::fill (delayBuf.begin(), delayBuf.end(), 0.0f);
        cutL.reset();
        cutR.reset();
        cutHz = cutSetHz = -1.0;
        firstBlock = true;
    }

    //==========================================================================
    // Marks

    const Mark& hist (long long i) const noexcept { return history[(size_t) (i % kHistory)]; }

    void pushMark (double pos, float period, float energy, bool joined) noexcept
    {
        Mark& m = history[(size_t) (histCount % kHistory)];
        m.pos = pos;
        m.period = period;
        m.energy = energy;
        m.joined = joined;
        ++histCount;
    }

    float sampleAt (long long i) const noexcept { return ring[(size_t) ((uint64_t) i & ringMask)]; }

    void startTracking() noexcept
    {
        periodUsed = framePeriod;
        const int delta = std::max (1, (int) (0.2 * periodUsed));
        // Any phase will do for the first mark; put it where the next one can be refined at once.
        anchor = (double) written - 1.5 * periodUsed - (double) delta - 2.0;
        nextRefine = (double) written;
        tracking = gridValid = true;
        fastEnergy = peakEnergy = 0.0;
        bowStopped = heldOff = false;
        pushMark (anchor, (float) periodUsed, 0.0f, false);
    }

    /** Normalised correlation of one period around `a` with one period around `b`. */
    double correlate (long long a, long long b, int half, int stride, double& energyB) const noexcept
    {
        double ab = 0.0, aa = 0.0, bb = 0.0;
        for (int i = -half; i < half; i += stride)
        {
            const double x = sampleAt (a + i), y = sampleAt (b + i);
            ab += x * y;
            aa += x * x;
            bb += y * y;
        }
        energyB = bb * (double) stride / (double) (2 * half);
        return aa > 1.0e-20 && bb > 1.0e-20 ? ab / std::sqrt (aa * bb) : 0.0;
    }

    void refine() noexcept
    {
        // A new note: the engine's period wins over the measured one.
        const double guess = std::abs (framePeriod / periodUsed - 1.0) > 0.03 ? framePeriod : periodUsed;
        const int delta = std::max (1, (int) (0.2 * guess));
        const int half = std::max (2, (int) (0.5 * guess));
        // About 48 points per period are plenty to find the alignment; keeps low notes cheap.
        const int stride = std::max (1, (int) (guess / 48.0));

        const double candidate = anchor + guess;
        const long long ia = (long long) std::llround (anchor);
        const long long ic = (long long) std::llround (candidate);

        double best = -2.0, energy = 0.0, e = 0.0;
        int bestTau = 0;
        for (int tau = -delta; tau <= delta; tau += stride)
        {
            const double c = correlate (ia, ic + tau, half, stride, e);
            if (c > best) { best = c; bestTau = tau; energy = e; }
        }
        if (stride > 1)
        {
            const int from = std::max (-delta, bestTau - stride), to = std::min (delta, bestTau + stride);
            for (int tau = from; tau <= to; ++tau)
            {
                const double c = correlate (ia, ic + tau, half, stride, e);
                if (c > best) { best = c; bestTau = tau; energy = e; }
            }
        }

        double newAnchor = candidate;
        bool joined = false;
        if (best >= kUsableCorrelation)
        {
            // Sub-sample position from a parabola through the neighbours.
            double dummy = 0.0;
            const double cm = correlate (ia, ic + bestTau - 1, half, stride, dummy);
            const double cp = correlate (ia, ic + bestTau + 1, half, stride, dummy);
            const double denom = cm - 2.0 * best + cp;
            double frac = denom < -1.0e-12 ? 0.5 * (cm - cp) / denom : 0.0;
            frac = std::min (0.5, std::max (-0.5, frac));
            newAnchor = anchor + ((double) (ic + bestTau) + frac - (double) ia);
        }
        else
        {
            correlate (ia, ic, half, stride, energy);
        }

        const double measured = newAnchor - anchor;
        if (best >= kGoodCorrelation)
        {
            joined = std::abs (measured / periodUsed - 1.0) < kJoinPeriodJump;
            periodUsed = std::abs (measured / guess - 1.0) < 0.03 ? 0.5 * (guess + measured) : measured;
        }
        else
        {
            periodUsed = guess;
        }
        periodUsed = std::min (pMax, std::max (pMin, periodUsed));

        anchor = newAnchor;
        pushMark (anchor, (float) measured, (float) energy, joined);

        fastEnergy += (energy - fastEnergy) * std::min (1.0, guess / (kBowLevelSec * fs));
        peakEnergy = std::max (fastEnergy, peakEnergy * std::pow (10.0, -0.1 * kPeakFallDbPerSec * guess / fs));
        if (! bowStopped && peakEnergy > 1.0e-10 && fastEnergy < peakEnergy * std::pow (10.0, -0.1 * kBowStopDb))
        {
            bowStopped = true;
            stopPeak = peakEnergy;
            holdEnergy = fastEnergy;
            holdFrom = anchor;
        }
        else if (bowStopped)
        {
            if (fastEnergy > stopPeak * std::pow (10.0, -0.1 * kBowBackDb))
                bowStopped = false;
            else if (anchor - holdFrom >= kBowHoldSec * fs)
            {
                if (fastEnergy > holdEnergy * std::pow (10.0, -0.1 * kBowHoldDb))
                {
                    bowStopped = false;
                    peakEnergy = fastEnergy;   // the new, softer level is the reference now
                }
                holdEnergy = fastEnergy;
                holdFrom = anchor;
            }
        }
        nextRefine = anchor + 1.5 * periodUsed + (double) delta + 2.0;
    }

    //==========================================================================
    // Loops

    int freeSlot() const noexcept
    {
        for (int i = 0; i < kSlots; ++i)
            if (caps[i].users == 0)
                return i;
        return -1;
    }

    /** Copies the best recent stretch of the tone. `recent`: freeze, the tone as it is now. */
    int capture (bool recent) noexcept
    {
        const int slot = freeSlot();
        if (slot < 0 || histCount < 1)
            return -1;

        const long long newest = histCount - 1;
        const long long oldestKept = std::max (0LL, histCount - kHistory + 1);
        const double searchFrom = (double) written - kSearchSec * fs;
        long long start = newest;
        while (start > oldestKept && hist (start - 1).pos >= searchFrom)
            --start;

        Capture& c = caps[slot];
        const double maxSpan = (double) c.data.size() - 2.0 * pMax - 4.0 * kTaps;
        const double target = std::min (maxSpan, std::max (kLoopTargetPeriods * periodUsed, kLoopTargetSec * fs));

        long long bestFrom = newest, bestTo = newest, i0 = start;
        double bestScore = -1.0, sum = 0.0;
        for (long long i1 = start; i1 <= newest; ++i1)
        {
            const Mark& m = hist (i1);
            if (i1 == start || ! m.joined)
            {
                i0 = i1;
                sum = 0.0;
            }
            sum += (double) m.energy;
            while (i0 < i1 && m.pos - hist (i0).pos > target)
            {
                sum -= (double) hist (i0).energy;
                ++i0;
            }
            const double span = m.pos - hist (i0).pos + (double) m.period;
            const double age = ((double) written - m.pos) / fs;
            const double recency = recent ? std::exp (-age / 0.03) : 1.0 - 0.5 * age / kSearchSec;
            const double score = sum / (double) (i1 - i0 + 1) * std::min (1.0, span / target) * recency;
            if (score > bestScore)
            {
                bestScore = score;
                bestFrom = i0;
                bestTo = i1;
            }
        }

        double longest = 0.0, energy = 0.0;
        for (long long i = bestFrom; i <= bestTo; ++i)
        {
            longest = std::max (longest, (double) hist (i).period);
            energy += (double) hist (i).energy;
        }
        longest = std::min (pMax, std::max (longest, periodUsed));
        energy /= (double) (bestTo - bestFrom + 1);

        const long long from = (long long) std::floor (hist (bestFrom).pos - longest) - 2 * kTaps;
        const long long to = (long long) std::ceil (hist (bestTo).pos + longest) + 2 * kTaps;
        const long long len = std::min<long long> (to - from + 1, (long long) c.data.size());
        for (long long j = 0; j < len; ++j)
            c.data[(size_t) j] = sampleAt (from + j);
        for (long long j = len; j < (long long) c.data.size(); ++j)
            c.data[(size_t) j] = 0.0f;

        c.marks = 0;
        for (long long i = bestFrom; i <= bestTo && c.marks < kHistory; ++i)
        {
            const Mark& m = hist (i);
            c.pos[(size_t) c.marks] = m.pos - (double) from;
            const double p = m.period > (float) pMin ? (double) m.period : periodUsed;
            c.period[(size_t) c.marks] = (float) std::min (pMax, p);
            ++c.marks;
        }
        double periodSum = 0.0;
        for (int k = 0; k < c.marks; ++k)
            periodSum += (double) c.period[(size_t) k];
        c.meanPeriod = periodSum / (double) c.marks;
        c.lo = c.pos[0];
        c.hi = c.pos[(size_t) c.marks - 1];

        // Level as it is now, never louder than the loop itself: no bump when
        // the loop comes from a louder moment than the tone that just ended.
        double now = 0.0;
        int n = 0;
        for (long long i = newest; i >= std::max (oldestKept, newest - 1); --i, ++n)
            now += (double) hist (i).energy;
        now /= (double) std::max (1, n);
        c.gain = recent || energy <= 1.0e-20 ? 1.0f : (float) std::min (1.0, std::sqrt (now / energy));
        return slot;
    }

    //==========================================================================
    // Layers

    void fadeOut (Layer& l, double ms) noexcept
    {
        if (l.state == State::Off || l.state == State::Draining)
            return;
        if (l.state == State::Fading)
        {
            // Already going: only ever faster.
            const double inc = 1.0 / (std::max (1.0, ms) * 0.001 * fs);
            l.fadeInc = std::max (l.fadeInc, inc);
            return;
        }
        l.state = State::Fading;
        l.fadeFrom = l.env;
        l.fadeX = 0.0;
        l.fadeInc = 1.0 / (std::max (1.0, ms) * 0.001 * fs);
    }

    void release (Layer& l) noexcept
    {
        if (l.capture >= 0)
            --caps[l.capture].users;
        l.capture = -1;
        l.state = State::Off;
        l.count = 0;
        l.maxDelay = l.drain = 0;
        for (Voice& v : l.voices)
            v.grainCount = 0;
    }

    void useCapture (Layer& l, int slot) noexcept
    {
        if (l.capture >= 0)
            --caps[l.capture].users;
        l.capture = slot;
        if (slot < 0)
            return;
        ++caps[slot].users;
        const Capture& c = caps[slot];
        for (Voice& v : l.voices)
        {
            // Start at the newest end, closest to what was playing, and walk back.
            v.u = c.hi;
            v.dir = -1;
            v.markIndex = c.marks - 1;
            v.turn = c.lo + 0.5 * (c.hi - c.lo) * random01 (v.rng);
        }
    }

    int newLayer (int count, const float* semis, double fadeInMs) noexcept
    {
        int idx = -1, fading = 0;
        for (int i = 0; i < kLayers; ++i)
        {
            if (layers[i].state == State::Off && idx < 0)
                idx = i;
            if (layers[i].state == State::Fading || layers[i].state == State::Draining)
                ++fading;
        }
        // Keep a free layer for the next change: the quietest ringing one hurries up
        // (a draining one is the quietest: it has faded out already).
        const auto quietness = [this] (int i) { return layers[i].state == State::Draining ? 2.0 : layers[i].fadeX; };
        if (idx < 0 || fading >= kLayers - 2)
        {
            int quietest = -1;
            for (int i = 0; i < kLayers; ++i)
                if ((layers[i].state == State::Fading || layers[i].state == State::Draining)
                    && (quietest < 0 || quietness (i) > quietness (quietest)))
                    quietest = i;
            if (quietest >= 0 && idx < 0)
            {
                release (layers[quietest]);
                idx = quietest;
            }
            else if (quietest >= 0)
            {
                fadeOut (layers[quietest], 20.0);
            }
        }
        if (idx < 0)
            return -1;

        Layer& l = layers[idx];
        l.state = State::Active;
        l.capture = -1;
        l.count = count;
        l.env = 0.0f;
        l.fadeInInc = (float) (1.0 / (std::max (1.0, fadeInMs) * 0.001 * fs));
        l.maxDelay = l.drain = 0;
        for (int i = 0; i < kMaxVoices; ++i)
        {
            Voice& v = l.voices[i];
            v.semis = i < count ? semis[i] : 0.0f;
            v.grainCount = 0;
            v.started = false;
            v.nextStart = 0.0;
            seedCounter = seedCounter * 1664525u + 1013904223u;
            v.rng = seedCounter | 1u;

            // Ensemble decoupling: voice 1, 3, ... 8..12 ms late, voice 2, 4, ... 16..22 ms;
            // fixed for this chord, so the delay never moves under a sounding voice.
            const double* range = kEnsembleDelayMs[(i % 2 == 0) ? 0 : 1];
            const double ms = range[0] + (range[1] - range[0]) * random01 (v.rng);
            v.delay = std::min (delaySize - 1, (int) std::lround (ensemble * ms * 0.001 * fs));
            v.lfoRate = kDetuneHzLow + (kDetuneHzHigh - kDetuneHzLow) * random01 (v.rng);
            v.lfoDepth = kDetuneCentsLow + (kDetuneCentsHigh - kDetuneCentsLow) * random01 (v.rng);
            v.lfoPhase = 2.0 * kPi * random01 (v.rng);
            if (i < count)
                l.maxDelay = std::max (l.maxDelay, v.delay);

            // what an earlier chord left in this voice's delay line is not this one's
            float* line = delayLine (idx, i);
            std::fill (line, line + delaySize, 0.0f);

            v.gL = v.gR = 1.0f;   // centre; process() moves it to its place within one block
            v.dL = v.dR = 0.0f;
        }
        return idx;
    }

    //==========================================================================
    void setFrame (const Frame& in) noexcept
    {
        // --- validate
        double period = (double) in.periodSamples;
        bool nowSounding = in.sounding && std::isfinite (period) && period >= pMin && period <= pMax;
        const int count = std::min (std::max (in.voiceCount, 0), kMaxVoices);
        float semis[kMaxVoices] = {};
        for (int i = 0; i < count; ++i)
        {
            const float s = in.semitones[i];
            semis[i] = std::isfinite (s) ? std::min (kMaxSemis, std::max (-kMaxSemis, s)) : 0.0f;
        }

        // --- octave errors: a big jump of the period must last a few hops
        if (nowSounding)
        {
            if (tracking && framePeriod > 0.0 && std::abs (std::log2 (period / framePeriod)) > kOctaveJumpOct)
            {
                if (pendingHops > 0 && std::abs (std::log2 (period / pendingPeriod)) < 0.05)
                    ++pendingHops;
                else
                {
                    pendingPeriod = period;
                    pendingHops = 1;
                }
                if (pendingHops >= kOctaveJumpHops)
                {
                    framePeriod = period;
                    pendingHops = 0;
                }
            }
            else
            {
                framePeriod = period;
                pendingHops = 0;
            }
        }

        if (nowSounding && ! tracking)
            startTracking();
        else if (! nowSounding)
            tracking = false;

        bool change = count != prevCount;
        for (int i = 0; i < count && ! change; ++i)
            change = std::abs (semis[i] - prevSemis[i]) > kChordJumpSemis;

        const bool wasFrozen = frozen;
        frozen = in.frozen;
        sounding = nowSounding;

        if (frozen && ! wasFrozen && live >= 0)
        {
            const int slot = capture (true);
            if (slot >= 0)
                useCapture (layers[live], slot);
            else
            {
                fadeOut (layers[live], kShortFadeMs);
                live = -1;
            }
        }
        else if (! frozen && wasFrozen && live >= 0)
        {
            fadeOut (layers[live], sounding ? kShortFadeMs : std::max (kMinReleaseMs, releaseMs));
            live = -1;
            change = false;   // the new live voices start below
        }

        if (frozen)
        {
            if (live >= 0 && change)
            {
                // A new chord on the frozen tone: same loop, the old chord rings on.
                Layer& old = layers[live];
                const int slot = old.capture;
                fadeOut (old, std::max (kShortFadeMs, overlapMs));
                live = newLayer (count, semis, kShortFadeMs);
                if (live >= 0)
                    useCapture (layers[live], slot);
            }
        }
        else if (sounding)
        {
            if (change)
            {
                // A new chord measures its own bow: the old peak no longer applies.
                peakEnergy = fastEnergy;
                bowStopped = heldOff = false;
            }
            if (heldOff && ! bowStopped)
                heldOff = false;

            if (live < 0)
            {
                if (! heldOff)
                    live = newLayer (count, semis, kStartFadeMs);
            }
            else if (change)
            {
                Layer& old = layers[live];
                useCapture (old, capture (false));
                fadeOut (old, std::max (kShortFadeMs, overlapMs));
                live = newLayer (count, semis, kShortFadeMs);
            }
            else if (bowStopped && releaseMs > 0.0)
            {
                Layer& old = layers[live];
                useCapture (old, capture (false));
                fadeOut (old, releaseMs);
                live = -1;
                heldOff = true;
            }
        }
        else if (live >= 0)
        {
            Layer& old = layers[live];
            useCapture (old, capture (false));
            fadeOut (old, std::max (kMinReleaseMs, releaseMs));
            live = -1;
        }

        if (live >= 0 && ! change)
        {
            Layer& l = layers[live];
            for (int i = 0; i < count && i < l.count; ++i)
                l.voices[i].semis = semis[i];
        }

        prevCount = count;
        for (int i = 0; i < kMaxVoices; ++i)
            prevSemis[i] = i < count ? semis[i] : 0.0f;
    }

    //==========================================================================
    // Voices

    void addGrain (Voice& v, const float* buf, uint32_t mask, double centre, double halfSrc,
                   double f, double s, double halfOut, double t, float g) noexcept
    {
        if (v.grainCount >= kMaxGrains)
            return;
        Grain& gr = v.grains[v.grainCount++];
        const double offset = t - (s - halfOut);   // 0 .. 1 sample late
        gr.buf = buf;
        gr.mask = mask;
        gr.step = f;
        gr.pos = centre - halfSrc + offset * f;
        gr.phaseInc = 1.0 / (2.0 * halfOut);
        gr.phase = offset * gr.phaseInc;
        gr.remaining = std::max (1, (int) std::ceil (2.0 * halfOut - offset));
        gr.gain = g;
        int table = 0;
        if (f > 1.0001)
            table = std::min (kKernelTables - 1, (int) std::ceil (3.0 * std::log2 (f) - 1.0e-6));
        gr.kernels = kernels.data() + (size_t) table * (kPhases + 1) * kTaps;
    }

    float* delayLine (int layer, int voice) noexcept
    {
        return delayBuf.data() + ((size_t) layer * kMaxVoices + (size_t) voice) * (size_t) delaySize;
    }

    /** Starts the next grain of a voice if it is due. Returns false when nothing is due. */
    bool launch (Layer& l, Voice& v, double t) noexcept
    {
        const bool loop = l.capture >= 0;
        if (! loop && ! gridValid)
        {
            v.nextStart = t + 32.0;
            return false;
        }

        // ensemble detune: a slow sine, its phase from the output clock
        const double cents = ensemble * v.lfoDepth * std::sin (v.lfoPhase + 2.0 * kPi * v.lfoRate * t / fs);
        double r = std::exp2 ((double) v.semis / 12.0 + cents / 1200.0);
        double f = std::pow (r, 1.0 - bodyKeep);
        f = std::min (4.0, std::max (0.25, f));

        Capture* c = loop ? &caps[l.capture] : nullptr;
        double period = periodUsed;
        if (loop)
        {
            int& k = v.markIndex;
            k = std::min (std::max (k, 0), c->marks - 1);
            while (k + 1 < c->marks && std::abs (c->pos[(size_t) k + 1] - v.u) < std::abs (c->pos[(size_t) k] - v.u)) ++k;
            while (k > 0 && std::abs (c->pos[(size_t) k - 1] - v.u) < std::abs (c->pos[(size_t) k] - v.u)) --k;
            period = (double) c->period[(size_t) k];
        }

        const double halfOut = period / f;
        if (! v.started || t - (v.nextS - halfOut) > 1.0)
        {
            v.nextS = t + halfOut;
            v.started = true;
        }
        const double startOut = v.nextS - halfOut;
        if (t < startOut)
        {
            v.nextStart = startOut;
            return false;
        }

        const double tonePeriod = loop ? c->meanPeriod * std::pow (period / c->meanPeriod, kLoopVibrato) : period;
        const double spacing = std::max (tonePeriod / r, 2.0);
        // Grains placed further apart than half their length leave gaps: keep the power up.
        const double overlap = 2.0 * halfOut / spacing;
        const float g = overlap < 2.0 ? (float) std::min (2.0, std::sqrt (2.0 / overlap)) : 1.0f;

        if (loop)
        {
            addGrain (v, c->data.data(), c->mask, c->pos[(size_t) v.markIndex], period, f, v.nextS, halfOut, t, g * c->gain);
        }
        else
        {
            // Latest grid point whose grain never reads ahead of the input.
            const double latest = v.nextS - kReadMargin - halfOut * std::abs (f - 1.0);
            const double a = anchor + std::floor ((latest - anchor) / period) * period;
            addGrain (v, ring.data(), ringMask, a, period, f, v.nextS, halfOut, t, g);
        }

        v.nextS += spacing;

        if (loop)
        {
            const double mid = 0.5 * (c->lo + c->hi);
            v.u += (double) v.dir * spacing;
            if (v.dir > 0 && v.u >= v.turn)
            {
                v.u = std::max (c->lo, v.turn - (v.u - v.turn));
                v.dir = -1;
                v.turn = c->lo + (mid - c->lo) * random01 (v.rng);
            }
            else if (v.dir < 0 && v.u <= v.turn)
            {
                v.u = std::min (c->hi, v.turn + (v.turn - v.u));
                v.dir = 1;
                v.turn = mid + (c->hi - mid) * random01 (v.rng);
            }
            v.u = std::min (c->hi, std::max (c->lo, v.u));
        }

        v.nextStart = v.nextS - period / f;
        return true;
    }

    float voiceSample (Voice& v) noexcept
    {
        float acc = 0.0f, wsum = 0.0f;
        for (int g = 0; g < v.grainCount;)
        {
            Grain& gr = v.grains[g];
            const int wi = std::min (kWindowSize, (int) (gr.phase * (double) kWindowSize));
            const float w = window[(size_t) wi];

            const double ipos = std::floor (gr.pos);
            const int phase = (int) ((gr.pos - ipos) * (double) kPhases + 0.5);
            const float* k = gr.kernels + (size_t) phase * kTaps;
            const uint32_t base = (uint32_t) ((long long) ipos - kTapsBefore);
            float s = 0.0f;
            for (int j = 0; j < kTaps; ++j)
                s += k[j] * gr.buf[(base + (uint32_t) j) & gr.mask];

            acc += w * s * gr.gain;
            wsum += w;
            gr.pos += gr.step;
            gr.phase += gr.phaseInc;
            if (--gr.remaining <= 0)
                gr = v.grains[--v.grainCount];
            else
                ++g;
        }
        // Overlapping grains are in phase: divide by the window sum (exact level
        // for upward shifts); where they do not overlap, leave the window shape.
        return acc / std::max (wsum, 1.0f);
    }

    //==========================================================================
    void process (const float* input, float* left, float* right, int numSamples, const Settings& s) noexcept
    {
        if (numSamples <= 0)
            return;

        auto clampf = [] (float x, float lo, float hi) { return std::isfinite (x) ? std::min (hi, std::max (lo, x)) : lo; };
        ensemble  = (double) clampf (s.ensemble, 0.0f, 1.0f);
        overlapMs = (double) clampf (s.overlapMs, 0.0f, 1500.0f);
        releaseMs = (double) clampf (s.releaseMs, 0.0f, 3000.0f);
        const float spread = clampf (s.stereoSpread, 0.0f, 1.0f);
        const float gainDb = clampf (s.harmonyGainDb, -200.0f, 6.0f);
        const float gainTarget = gainDb <= -60.0f ? 0.0f : (float) std::pow (10.0, (double) gainDb / 20.0);
        const float dryTarget = s.dryKill ? 0.0f : 1.0f;

        // Key tracking: the played tone (the one the voices are made from), as a MIDI note.
        // Before the first tone the curves are read at their low end.
        const float keyNote = framePeriod > 0.0 ? (float) (69.0 + 12.0 * std::log2 (fs / framePeriod / kReferenceA4))
                                                : curve::kLowNote;
        const Curve formant = curve::sanitised (s.formantCurve, curve::kFormantLow, curve::kFormantHigh, curve::defaultFormant());
        const Curve hiCut = curve::sanitised (s.hiCutCurve, curve::kHiCutLowHz, curve::kHiCutHighHz, curve::defaultHiCut());
        bodyKeep = (double) clampf (curve::value (formant, keyNote), 0.0f, 1.0f);

        const double hzTarget = std::min ((double) clampf (curve::value (hiCut, keyNote, true), curve::kHiCutLowHz, curve::kHiCutHighHz),
                                          0.45 * fs);
        if (cutHz <= 0.0)
            cutHz = hzTarget;
        else
            cutHz *= std::pow (hzTarget / cutHz, 1.0 - std::exp (-(double) numSamples / (kHiCutSmoothMs * 0.001 * fs)));

        if (cutSetHz <= 0.0 || std::abs (cutHz / cutSetHz - 1.0) > 1.0e-3)
        {
            cutSetHz = cutHz;
            cutL.setLowpass (fs, cutHz, 0.70710678);   // 12 dB per octave, no peak
            cutR.setLowpass (fs, cutHz, 0.70710678);
        }
        if (firstBlock)
        {
            gain = gainTarget;
            dryGain = dryTarget;
            firstBlock = false;
        }

        // Stereo positions: alternating sides, the outer voices furthest out,
        // constant power with unity in the centre (like the dry violin).
        const float inv = 1.0f / (float) numSamples;
        for (Layer& l : layers)
        {
            if (l.state == State::Off)
                continue;
            const int n = l.count;
            const int pairs = (n + 1) / 2;
            for (int i = 0; i < n; ++i)
            {
                const float width = n == 1 ? 0.5f : (float) ((i >> 1) + 1) / (float) pairs;
                const float pan = spread * width * ((i & 1) == 0 ? -1.0f : 1.0f);
                const double theta = ((double) pan + 1.0) * 0.25 * kPi;
                const float tl = (float) (std::sqrt (2.0) * std::cos (theta));
                const float tr = (float) (std::sqrt (2.0) * std::sin (theta));
                Voice& v = l.voices[i];
                v.dL = (tl - v.gL) * inv;
                v.dR = (tr - v.gR) * inv;
            }
        }

        for (int i = 0; i < numSamples; ++i)
        {
            const float raw = input[i];
            const float dry = std::isfinite (raw) ? raw : 0.0f;
            float x = std::min (16.0f, std::max (-16.0f, dry));
            if (std::abs (x) < 1.0e-20f)
                x = 0.0f;   // no denormals into the grains
            ring[(size_t) ((uint64_t) written & ringMask)] = x;
            ++written;
            if (tracking && (double) written >= nextRefine)
                refine();

            const double t = (double) (written - 1);
            const uint32_t writeAt = (uint32_t) ((uint64_t) (written - 1) & delayMask);
            float busL = 0.0f, busR = 0.0f;
            for (int li = 0; li < kLayers; ++li)
            {
                Layer& l = layers[li];
                if (l.state == State::Off)
                    continue;
                if (l.state == State::Active)
                {
                    if (l.env < 1.0f)
                        l.env = std::min (1.0f, l.env + l.fadeInInc);
                }
                else if (l.state == State::Fading)
                {
                    l.fadeX += l.fadeInc;
                    if (l.fadeX >= 1.0)
                    {
                        // never the live layer: that one is Active
                        if (l.maxDelay <= 0)
                        {
                            release (l);
                            continue;
                        }
                        l.state = State::Draining;
                        l.drain = l.maxDelay + 1;
                        l.env = 0.0f;
                    }
                    else
                    {
                        l.env = l.fadeFrom * (1.0f - smoothstep (l.fadeX));
                    }
                }
                else if (--l.drain <= 0)
                {
                    release (l);   // its delay lines have played out
                    continue;
                }

                const bool sounding = l.state != State::Draining;
                for (int vi = 0; vi < l.count; ++vi)
                {
                    Voice& v = l.voices[vi];
                    float y = 0.0f;
                    if (sounding)
                    {
                        for (int guard = 0; guard < 4 && t >= v.nextStart; ++guard)
                            if (! launch (l, v, t))
                                break;
                        y = voiceSample (v) * l.env;
                    }

                    if (v.delay > 0)
                    {
                        float* line = delayLine (li, vi);
                        line[writeAt] = y;
                        y = line[(writeAt - (uint32_t) v.delay) & delayMask];
                    }

                    v.gL += v.dL;
                    v.gR += v.dR;
                    busL += y * v.gL;
                    busR += y * v.gR;
                }
            }

            double outL = cutL.process ((double) busL);
            double outR = cutR.process ((double) busR);
            outL = std::min (kBusLimit, std::max (-kBusLimit, outL));
            outR = std::min (kBusLimit, std::max (-kBusLimit, outR));

            if (gain != gainTarget)
            {
                gain += (gainTarget - gain) * (float) gainCoeff;
                if (std::abs (gain - gainTarget) < 1.0e-6f)
                    gain = gainTarget;
            }
            if (dryGain != dryTarget)
                dryGain = dryTarget > dryGain ? std::min (dryTarget, dryGain + (float) dryStep)
                                              : std::max (dryTarget, dryGain - (float) dryStep);

            const float d = dryGain == 1.0f ? dry : dry * dryGain;
            if (gain == 0.0f)
            {
                left[i] = d;
                right[i] = d;
            }
            else
            {
                left[i] = d + (float) outL * gain;
                right[i] = d + (float) outR * gain;
            }
        }
    }

    int latencySamples (double period) const noexcept
    {
        if (! (period > 0.0) || ! std::isfinite (period))
            return (int) kReadMargin;
        period = std::min (pMax, std::max (pMin, period));
        return (int) std::lround (kReadMargin + 0.5 * period);
    }
};

//==============================================================================
Harmonizer::Harmonizer() : impl (std::make_unique<Impl>()) {}
Harmonizer::~Harmonizer() = default;

void Harmonizer::prepare (double sampleRate)                  { impl->prepare (sampleRate); }
void Harmonizer::reset() noexcept                             { impl->reset(); }
void Harmonizer::setFrame (const Frame& f) noexcept           { impl->setFrame (f); }
int  Harmonizer::latencySamples (float p) const noexcept      { return impl->latencySamples ((double) p); }

void Harmonizer::process (const float* input, float* left, float* right, int numSamples, const Settings& s) noexcept
{
    impl->process (input, left, right, numSamples, s);
}

} // namespace vm
