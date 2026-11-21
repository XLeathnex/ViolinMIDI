// Test signal generator: a simple bowed/plucked string model with known pitch.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <vector>

namespace vmtest
{

constexpr double kPi = 3.14159265358979323846;

inline double noteToHz (double note, double a4 = 440.0) { return a4 * std::pow (2.0, (note - 69.0) / 12.0); }
inline double hzToNote (double hz, double a4 = 440.0)   { return 69.0 + 12.0 * std::log2 (hz / a4); }
inline double dbToLin (double db)                        { return std::pow (10.0, db / 20.0); }

struct Timbre
{
    double rolloff = 1.0;          ///< harmonic k has amplitude 1 / k^rolloff (1 = sawtooth, bowed string)
    double fundamentalGain = 1.0;
    double oddGain = 1.0;          ///< odd harmonics above the fundamental
    double evenGain = 1.0;
    double bowNoiseDb = -34.0;     ///< bow noise relative to the tone
    double jitterCents = 3.0;      ///< slow random pitch wobble
    bool   formants = true;        ///< bridge / pickup resonances
    bool   randomPhases = false;
};

struct Segment
{
    double duration = 0.5;
    double pitchStart = 69.0, pitchEnd = 69.0;        ///< fractional MIDI note
    double levelStartDb = -20.0, levelEndDb = -20.0;  ///< RMS level of the tone
    bool   silent = false;
    double vibratoDepth = 0.0;     ///< semitones, peak
    double vibratoRateHz = 6.0;
    double vibratoAmDb = 0.0;      ///< loudness change that comes with the vibrato
    double attackSec = 0.0;        ///< fade-in with a noisy bow transient
    double decayDbPerSec = 0.0;    ///< plucked or ringing notes
};

struct Signal
{
    double sampleRate = 48000.0;
    std::vector<float> samples;
    std::vector<float> truePitch;  ///< per sample, negative while silent
};

inline Segment tone (double note, double seconds, double levelDb = -20.0, double attackSec = 0.02)
{
    Segment s;
    s.duration = seconds;
    s.pitchStart = s.pitchEnd = note;
    s.levelStartDb = s.levelEndDb = levelDb;
    s.attackSec = attackSec;
    return s;
}

inline Segment rest (double seconds)
{
    Segment s;
    s.duration = seconds;
    s.silent = true;
    return s;
}

inline Segment slide (double from, double to, double seconds, double levelDb = -20.0)
{
    Segment s;
    s.duration = seconds;
    s.pitchStart = from;
    s.pitchEnd = to;
    s.levelStartDb = s.levelEndDb = levelDb;
    return s;
}

class Synth
{
public:
    Synth (double sampleRate, unsigned seed) : fs (sampleRate), rng (seed) {}

    Signal render (const std::vector<Segment>& segments, const Timbre& timbre, double floorNoiseDb = -80.0)
    {
        Signal out;
        out.sampleRate = fs;

        std::normal_distribution<double> gauss (0.0, 1.0);
        std::uniform_real_distribution<double> uniform (0.0, 1.0);

        constexpr int kMaxHarmonics = 80;
        double phaseOffset[kMaxHarmonics + 1];
        for (double& p : phaseOffset)
            p = timbre.randomPhases ? uniform (rng) * 2.0 * kPi : 0.0;

        double harmonicAmp[kMaxHarmonics + 1] = {};
        int harmonics = 1;
        double norm = 1.0;

        double phase = 0.0;          // cycles
        double amp = 0.0;            // smoothed amplitude
        double noiseState = 0.0;     // low-passed bow noise
        double jitter = 0.0;         // random-walk pitch wobble in semitones
        double lastPitch = 69.0;
        const double ampCoeff = 1.0 - std::exp (-1.0 / (0.002 * fs));
        const double noiseCoeff = 1.0 - std::exp (-2.0 * kPi * 6000.0 / fs);
        const double jitterCoeff = 1.0 - std::exp (-1.0 / (0.03 * fs));
        const double floorNoise = dbToLin (floorNoiseDb);
        const double bowNoise = dbToLin (timbre.bowNoiseDb);

        for (const Segment& seg : segments)
        {
            const int n = (int) std::lround (seg.duration * fs);
            for (int i = 0; i < n; ++i)
            {
                const double t = (double) i / fs;
                const double u = n > 1 ? (double) i / (double) (n - 1) : 0.0;

                // --- pitch
                jitter += (gauss (rng) * timbre.jitterCents * 0.01 - jitter) * jitterCoeff;
                const double vib = seg.vibratoDepth * std::sin (2.0 * kPi * seg.vibratoRateHz * t);
                // While the bow is off the string, the string keeps its last pitch as it fades.
                const double pitch = seg.silent ? lastPitch
                                                : seg.pitchStart + (seg.pitchEnd - seg.pitchStart) * u + vib;
                lastPitch = pitch;
                const double hz = noteToHz (pitch + jitter * 0.2);

                // --- level
                double levelDb = seg.levelStartDb + (seg.levelEndDb - seg.levelStartDb) * u
                               - seg.decayDbPerSec * t
                               + seg.vibratoAmDb * std::sin (2.0 * kPi * seg.vibratoRateHz * t + 0.7);
                double target = seg.silent ? 0.0 : dbToLin (levelDb);
                double attackNoise = 0.0;
                if (! seg.silent && seg.attackSec > 0.0 && t < seg.attackSec)
                {
                    const double a = t / seg.attackSec;
                    target *= a * a;
                    attackNoise = (1.0 - a) * 6.0; // scratchy start
                }
                amp += (target - amp) * ampCoeff;

                // --- harmonic amplitudes, refreshed every 32 samples
                if ((i & 31) == 0)
                {
                    const double limit = std::min (0.45 * fs, 10000.0);
                    harmonics = std::max (1, std::min (kMaxHarmonics, (int) std::floor (limit / hz)));
                    double power = 0.0;
                    for (int k = 1; k <= harmonics; ++k)
                    {
                        double a = 1.0 / std::pow ((double) k, timbre.rolloff);
                        if (k == 1)            a *= timbre.fundamentalGain;
                        else if ((k & 1) != 0) a *= timbre.oddGain;
                        else                   a *= timbre.evenGain;
                        if (timbre.formants)
                            a *= formant (hz * (double) k);
                        harmonicAmp[k] = a;
                        power += 0.5 * a * a;
                    }
                    norm = power > 0.0 ? 1.0 / std::sqrt (power) : 1.0;
                }

                // --- tone: sum of harmonics by complex rotation
                phase += hz / fs;
                if (phase >= 1.0)
                    phase -= std::floor (phase);
                const double theta = 2.0 * kPi * phase;
                const double c1 = std::cos (theta), s1 = std::sin (theta);
                double ck = 1.0, sk = 0.0, sum = 0.0;
                for (int k = 1; k <= harmonics; ++k)
                {
                    const double cNext = ck * c1 - sk * s1;
                    const double sNext = sk * c1 + ck * s1;
                    ck = cNext;
                    sk = sNext;
                    if (timbre.randomPhases)
                        sum += harmonicAmp[k] * (sk * std::cos (phaseOffset[k]) + ck * std::sin (phaseOffset[k]));
                    else
                        sum += harmonicAmp[k] * sk;
                }

                // --- bow noise
                noiseState += (gauss (rng) - noiseState) * noiseCoeff;
                const double noise = noiseState * 2.0 * amp * bowNoise * (1.0 + attackNoise);

                const double sample = sum * norm * amp + noise + gauss (rng) * floorNoise;
                out.samples.push_back ((float) sample);
                out.truePitch.push_back (seg.silent ? -1.0f : (float) pitch);
            }
        }

        return out;
    }

private:
    static double formant (double hz)
    {
        auto bump = [] (double f, double centre, double width) {
            const double d = (f - centre) / width;
            return std::exp (-d * d);
        };
        return 0.6 + 1.2 * bump (hz, 480.0, 220.0) + 0.9 * bump (hz, 2600.0, 700.0) + 0.5 * bump (hz, 1100.0, 300.0);
    }

    double fs;
    std::mt19937 rng;
};

} // namespace vmtest
