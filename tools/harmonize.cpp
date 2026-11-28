// Offline tool: plays the harmony voices of a recording with its own sound.
//
//   violinmidi_harmonize in.wav|in.aiff out.wav [--voicing N] [--key ROOT] [--scale N]
//        [--body 0..1] [--ensemble 0..1] [--spread 0..1] [--hicut Hz] [--nodrop] [--gain dB]
//        [--drykill] [--release ms] [--overlap ms] [--coupling 0..1] [--chromatic] [--harmony dB]
//        [--start s] [--length s] [--report]
//
// --body and --hicut set the formant and high-cut curves flat at that value;
// without them the standard key-tracked curves of the app are used.
//
// The recording runs through vm::Engine with the harmony on (as violinmidi_wav
// does); at every analysis hop a Harmonizer::Frame is built from the engine's
// meter and the same input goes through vm::Harmonizer. Output: 16-bit stereo
// WAV. --report measures the voices (pitch, resonance, clicks) note by note.
// The WAV/AIFF readers are copied from wav2midi.cpp.

#include "../core/Engine.h"
#include "../core/Harmonizer.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace
{

struct Wav
{
    double sampleRate = 0.0;
    std::vector<float> mono;
};

uint32_t readU32 (const unsigned char* p) { return (uint32_t) p[0] | ((uint32_t) p[1] << 8) | ((uint32_t) p[2] << 16) | ((uint32_t) p[3] << 24); }
uint16_t readU16 (const unsigned char* p) { return (uint16_t) (p[0] | (p[1] << 8)); }

bool loadWav (const std::string& path, Wav& wav, std::string& error)
{
    std::ifstream file (path, std::ios::binary);
    if (! file)
    {
        error = "cannot open file";
        return false;
    }
    std::vector<unsigned char> data ((std::istreambuf_iterator<char> (file)), std::istreambuf_iterator<char>());
    if (data.size() < 44 || std::memcmp (data.data(), "RIFF", 4) != 0 || std::memcmp (data.data() + 8, "WAVE", 4) != 0)
    {
        error = "not a RIFF/WAVE file";
        return false;
    }

    int format = 0, channels = 0, bits = 0;
    size_t pos = 12;
    const unsigned char* audio = nullptr;
    size_t audioBytes = 0;

    while (pos + 8 <= data.size())
    {
        const unsigned char* chunk = data.data() + pos;
        const size_t size = readU32 (chunk + 4);
        const size_t body = pos + 8;
        if (body + size > data.size() && std::memcmp (chunk, "data", 4) != 0)
            break;

        if (std::memcmp (chunk, "fmt ", 4) == 0 && size >= 16)
        {
            format = readU16 (data.data() + body);
            channels = readU16 (data.data() + body + 2);
            wav.sampleRate = (double) readU32 (data.data() + body + 4);
            bits = readU16 (data.data() + body + 14);
            if (format == 0xFFFE && size >= 26) // WAVE_FORMAT_EXTENSIBLE: real format is in the sub-format GUID
                format = readU16 (data.data() + body + 24);
        }
        else if (std::memcmp (chunk, "data", 4) == 0)
        {
            audio = data.data() + body;
            audioBytes = std::min (size, data.size() - body);
            break;
        }
        pos = body + size + (size & 1);
    }

    if (audio == nullptr || channels < 1 || wav.sampleRate <= 0.0)
    {
        error = "missing fmt or data chunk";
        return false;
    }
    if (! ((format == 1 && (bits == 16 || bits == 24 || bits == 32)) || (format == 3 && bits == 32)))
    {
        error = "unsupported sample format (need 16/24/32-bit PCM or 32-bit float)";
        return false;
    }

    const size_t bytesPerSample = (size_t) bits / 8;
    const size_t frames = audioBytes / (bytesPerSample * (size_t) channels);
    wav.mono.resize (frames);

    for (size_t f = 0; f < frames; ++f)
    {
        double sum = 0.0;
        for (int c = 0; c < channels; ++c)
        {
            const unsigned char* p = audio + (f * (size_t) channels + (size_t) c) * bytesPerSample;
            double v = 0.0;
            if (format == 3)
            {
                float x;
                std::memcpy (&x, p, 4);
                v = x;
            }
            else if (bits == 16)
            {
                v = (double) (int16_t) readU16 (p) / 32768.0;
            }
            else if (bits == 24)
            {
                int32_t x = (int32_t) ((uint32_t) p[0] | ((uint32_t) p[1] << 8) | ((uint32_t) p[2] << 16));
                if (x & 0x800000) x |= ~0xFFFFFF;
                v = (double) x / 8388608.0;
            }
            else
            {
                v = (double) (int32_t) readU32 (p) / 2147483648.0;
            }
            sum += v;
        }
        wav.mono[f] = (float) (sum / (double) channels);
    }
    return true;
}

uint32_t readU32BE (const unsigned char* p) { return ((uint32_t) p[0] << 24) | ((uint32_t) p[1] << 16) | ((uint32_t) p[2] << 8) | (uint32_t) p[3]; }
uint16_t readU16BE (const unsigned char* p) { return (uint16_t) ((p[0] << 8) | p[1]); }

// AIFF: 16- or 24-bit big-endian PCM, as used by many sample libraries.
bool loadAiff (const std::string& path, Wav& wav, std::string& error)
{
    std::ifstream file (path, std::ios::binary);
    if (! file)
    {
        error = "cannot open file";
        return false;
    }
    std::vector<unsigned char> data ((std::istreambuf_iterator<char> (file)), std::istreambuf_iterator<char>());
    if (data.size() < 12 || std::memcmp (data.data(), "FORM", 4) != 0 || std::memcmp (data.data() + 8, "AIFF", 4) != 0)
    {
        error = "not an uncompressed AIFF file";
        return false;
    }

    int channels = 0, bits = 0;
    size_t frames = 0, pos = 12;
    const unsigned char* audio = nullptr;
    size_t audioBytes = 0;

    while (pos + 8 <= data.size())
    {
        const unsigned char* chunk = data.data() + pos;
        const size_t size = readU32BE (chunk + 4);
        const size_t body = pos + 8;
        if (std::memcmp (chunk, "COMM", 4) == 0 && size >= 18 && body + 18 <= data.size())
        {
            channels = readU16BE (data.data() + body);
            frames = readU32BE (data.data() + body + 2);
            bits = readU16BE (data.data() + body + 6);
            // The sample rate is stored as an 80-bit extended float.
            const unsigned char* e = data.data() + body + 8;
            const int exponent = ((e[0] & 0x7F) << 8) | e[1];
            double mantissa = 0.0;
            for (int i = 0; i < 8; ++i)
                mantissa = mantissa * 256.0 + (double) e[2 + i];
            wav.sampleRate = mantissa * std::pow (2.0, (double) (exponent - 16383 - 63));
        }
        else if (std::memcmp (chunk, "SSND", 4) == 0 && body + 8 <= data.size())
        {
            const size_t offset = readU32BE (data.data() + body);
            const size_t start = body + 8 + offset;
            if (start <= data.size())
            {
                audio = data.data() + start;
                audioBytes = std::min (size >= 8 + offset ? size - 8 - offset : (size_t) 0, data.size() - start);
            }
        }
        pos = body + size + (size & 1);
    }

    if (audio == nullptr || channels < 1 || wav.sampleRate <= 0.0 || (bits != 16 && bits != 24))
    {
        error = "unsupported AIFF file (need 16- or 24-bit PCM)";
        return false;
    }

    const size_t bytesPerSample = (size_t) bits / 8;
    frames = std::min (frames, audioBytes / (bytesPerSample * (size_t) channels));
    wav.mono.resize (frames);
    for (size_t f = 0; f < frames; ++f)
    {
        double sum = 0.0;
        for (int c = 0; c < channels; ++c)
        {
            const unsigned char* p = audio + (f * (size_t) channels + (size_t) c) * bytesPerSample;
            if (bits == 16)
            {
                sum += (double) (int16_t) readU16BE (p) / 32768.0;
            }
            else
            {
                int32_t x = (int32_t) (((uint32_t) p[0] << 16) | ((uint32_t) p[1] << 8) | (uint32_t) p[2]);
                if (x & 0x800000) x |= ~0xFFFFFF;
                sum += (double) x / 8388608.0;
            }
        }
        wav.mono[f] = (float) (sum / (double) channels);
    }
    return true;
}

bool hasSuffix (const std::string& s, const char* suffix)
{
    const size_t n = std::strlen (suffix);
    if (s.size() < n)
        return false;
    for (size_t i = 0; i < n; ++i)
        if (std::tolower ((unsigned char) s[s.size() - n + i]) != suffix[i])
            return false;
    return true;
}


void writeWav16 (const std::string& path, const std::vector<float>& left, const std::vector<float>& right, double fs)
{
    std::ofstream f (path, std::ios::binary);
    auto u32 = [&] (uint32_t v) { unsigned char b[4] = { (unsigned char) v, (unsigned char) (v >> 8), (unsigned char) (v >> 16), (unsigned char) (v >> 24) }; f.write ((const char*) b, 4); };
    auto u16 = [&] (uint16_t v) { unsigned char b[2] = { (unsigned char) v, (unsigned char) (v >> 8) }; f.write ((const char*) b, 2); };
    const uint32_t frames = (uint32_t) left.size();
    const uint32_t rate = (uint32_t) std::lround (fs);
    f.write ("RIFF", 4); u32 (36 + frames * 4); f.write ("WAVE", 4);
    f.write ("fmt ", 4); u32 (16); u16 (1); u16 (2); u32 (rate); u32 (rate * 4); u16 (4); u16 (16);
    f.write ("data", 4); u32 (frames * 4);
    for (uint32_t i = 0; i < frames; ++i)
    {
        for (float v : { left[i], right[i] })
        {
            const long q = std::lround (std::max (-1.0f, std::min (1.0f, v)) * 32767.0f);
            u16 ((uint16_t) (int16_t) q);
        }
    }
}

//==============================================================================
// --- Glue: engine meter -> harmonizer frame. This moves into the engine later.
//
// The harmony root is the nearest semitone of the detected pitch, taken when
// the harmony notes change and kept while they stay: that is how the output
// stage picks it (OutputStage::beginHarmony), and re-rounding at every hop
// would move every voice by a semitone whenever a wide vibrato or an
// in-between intonation crosses a quarter tone. Short unvoiced hops inside a
// note keep the last pitch (up to bridgeMs), so a scratchy hop does not end
// the voices.
struct FrameBuilder
{
    float lastPitch = 0.0f;
    int   unvoicedHops = 0;
    int   root = -1;
    int   triad[vm::scale::kMaxVoices] = {};
    int   triadCount = -1;

    vm::Harmonizer::Frame build (const vm::Engine::Meter& m, double fs, double hopSec, const vm::Settings& s, bool frozen)
    {
        vm::Harmonizer::Frame f;
        f.frozen = frozen;
        if (m.active && m.pitch > 0.0f)
        {
            lastPitch = m.pitch;
            unvoicedHops = 0;
        }
        else if (m.active && lastPitch > 0.0f && (double) (++unvoicedHops) * hopSec * 1000.0 <= (double) s.bridgeMs)
        {
            // keep lastPitch
        }
        else
        {
            lastPitch = 0.0f;
            triadCount = -1;
            return f;
        }

        const double hz = (double) s.a4Hz * std::pow (2.0, ((double) lastPitch - 69.0) / 12.0);
        f.sounding = true;
        f.periodSamples = (float) (fs / hz);
        bool sameHarmony = m.triadCount == triadCount && root >= 0;
        for (int i = 0; i < m.triadCount && sameHarmony; ++i)
            sameHarmony = m.triad[i] == triad[i];
        if (! sameHarmony)
        {
            root = (int) std::lround (lastPitch);
            triadCount = m.triadCount;
            for (int i = 0; i < m.triadCount && i < vm::scale::kMaxVoices; ++i)
                triad[i] = m.triad[i];
        }
        const float coupling = std::min (1.0f, std::max (0.0f, s.vibratoCoupling));
        for (int i = 0; i < m.triadCount && i < vm::scale::kMaxVoices; ++i)
        {
            if (m.triad[i] < 0 || m.triad[i] == root)
                continue;   // the unison is the violin itself
            const float target = (float) m.triad[i] + coupling * (lastPitch - (float) root);
            f.semitones[f.voiceCount++] = target - lastPitch;
        }
        vm::Engine::applyDropVoicing (f, m.triadCount, root, s);
        return f;
    }
};

//==============================================================================
// --report: measurements on the rendered voices.

struct HopRow
{
    double time;
    vm::Harmonizer::Frame frame;
    float pitch;
    int note;
};

double rmsOf (const std::vector<float>& y, size_t a, size_t b)
{
    double s = 0.0;
    for (size_t i = a; i < b && i < y.size(); ++i)
        s += (double) y[i] * y[i];
    return b > a ? std::sqrt (s / (double) (b - a)) : 0.0;
}

double autocorrAt (const std::vector<float>& y, size_t a, size_t b, size_t lag)
{
    double xy = 0.0, xx = 0.0, yy = 0.0;
    for (size_t i = a; i + lag < b; ++i)
    {
        xy += (double) y[i] * y[i + lag];
        xx += (double) y[i] * y[i];
        yy += (double) y[i + lag] * y[i + lag];
    }
    return xx > 0.0 && yy > 0.0 ? xy / std::sqrt (xx * yy) : 0.0;
}

/** Fundamental within +-40 cents of `expected` (lag of K periods, see the tests). */
double measureF0 (const std::vector<float>& y, size_t a, size_t b, double fs, double expected)
{
    const double t = fs / expected;
    const int k = std::max (1, std::min (16, (int) (0.02 * fs / t)));
    const size_t lo = (size_t) std::floor (k * t * std::pow (2.0, -40.0 / 1200.0));
    const size_t hi = (size_t) std::ceil (k * t * std::pow (2.0, 40.0 / 1200.0));
    double best = -2.0;
    size_t lag = lo;
    for (size_t l = lo; l <= hi; ++l)
    {
        const double r = autocorrAt (y, a, b, l);
        if (r > best) { best = r; lag = l; }
    }
    const double rm = autocorrAt (y, a, b, lag - 1), rp = autocorrAt (y, a, b, lag + 1);
    const double d = rm - 2.0 * best + rp;
    const double frac = d < 0.0 ? std::max (-0.5, std::min (0.5, 0.5 * (rm - rp) / d)) : 0.0;
    return best > 0.5 ? k * fs / ((double) lag + frac) : 0.0;
}

double amplitudeAt (const std::vector<float>& y, size_t a, size_t b, double fs, double hz)
{
    const double w = 2.0 * 3.14159265358979323846 * hz / fs;
    double re = 0.0, im = 0.0, ws = 0.0;
    for (size_t i = a; i < b; ++i)
    {
        const double win = 0.5 - 0.5 * std::cos (2.0 * 3.14159265358979323846 * (double) (i - a) / (double) (b - a));
        re += win * y[i] * std::cos (w * (double) i);
        im -= win * y[i] * std::sin (w * (double) i);
        ws += win;
    }
    return 2.0 * std::sqrt (re * re + im * im) / ws;
}

/** Spectral centroid of the harmonics of f0 below 8 kHz. */
double harmonicCentroid (const std::vector<float>& y, size_t a, size_t b, double fs, double f0)
{
    double num = 0.0, den = 0.0;
    for (int h = 1; h * f0 < std::min (8000.0, 0.45 * fs); ++h)
    {
        const double amp = amplitudeAt (y, a, b, fs, h * f0);
        num += h * f0 * amp * amp;
        den += amp * amp;
    }
    return den > 0.0 ? num / den : 0.0;
}

/** Largest sample step relative to the RMS around it, worst 10 ms window in [a, b). */
double jumpRatio (const std::vector<float>& y, size_t a, size_t b, double fs, double floorRms)
{
    const size_t w = (size_t) (0.01 * fs);
    double worst = 0.0;
    for (size_t s = a; s + w <= b && s + 2 * w < y.size(); s += w)
    {
        const double rms = rmsOf (y, s >= w ? s - w : 0, s + 2 * w);
        if (rms < floorRms)
            continue;
        double j = 0.0;
        for (size_t i = std::max<size_t> (s, 1); i < s + w; ++i)
            j = std::max (j, (double) std::abs (y[i] - y[i - 1]));
        worst = std::max (worst, j / rms);
    }
    return worst;
}

float medianPeriod (const std::vector<HopRow>& hops)
{
    std::vector<float> p;
    for (const HopRow& h : hops)
        if (h.frame.sounding) p.push_back (h.frame.periodSamples);
    if (p.empty()) return 0.0f;
    std::nth_element (p.begin(), p.begin() + (long) p.size() / 2, p.end());
    return p[p.size() / 2];
}

void report (const std::vector<float>& in, const std::vector<HopRow>& hops, double fs, int block, const vm::Settings& s)
{
    // One voice at a time (dry left out, no ensemble delay or detune, centre), so each can be measured alone.
    constexpr int kVoices = 4;
    std::vector<float> voice[kVoices], sum (in.size(), 0.0f);
    vm::Settings vs = s;
    vs.dryKill = true;
    vs.ensemble = 0.0f;
    vs.stereoSpread = 0.0f;
    vs.harmonyGainDb = 0.0f;
    std::vector<float> l ((size_t) block), r ((size_t) block);
    for (int v = 0; v < kVoices; ++v)
    {
        vm::Harmonizer h;
        h.prepare (fs);
        voice[v].assign (in.size(), 0.0f);
        size_t hop = 0;
        for (size_t pos = 0; pos < in.size(); pos += (size_t) block, ++hop)
        {
            const int n = (int) std::min<size_t> ((size_t) block, in.size() - pos);
            vm::Harmonizer::Frame f = hops[std::min (hop, hops.size() - 1)].frame;
            const bool has = v < f.voiceCount;
            f.voiceCount = has ? 1 : 0;
            f.semitones[0] = has ? f.semitones[v] : 0.0f;
            h.setFrame (f);
            h.process (in.data() + pos, l.data(), r.data(), n, vs);
            for (int i = 0; i < n; ++i)
            {
                voice[v][pos + (size_t) i] = l[(size_t) i];
                sum[pos + (size_t) i] += l[(size_t) i];
            }
        }
    }

    // Notes: runs of hops with the same engine note.
    std::printf ("# report: per note, middle of the note; cents = voice pitch against the pitch it should have\n");
    std::printf ("#  time   note  voice  semis  cents   centroid in -> voice (ratio, expected %s)\n", "r^(1-body), body from the formant curve");
    std::vector<double> allCents;
    double worstSteadyJump = 0.0, worstEndJump = 0.0;
    size_t i = 0;
    while (i < hops.size())
    {
        if (! hops[i].frame.sounding) { ++i; continue; }
        size_t j = i;
        while (j + 1 < hops.size() && hops[j + 1].frame.sounding && hops[j + 1].note == hops[i].note) ++j;
        const double t0 = hops[i].time, t1 = hops[j].time;
        const size_t endSample = std::min (in.size(), (size_t) ((t1 + (double) s.releaseMs * 0.001 + 0.05) * fs));
        if (t1 - t0 > 0.45)
        {
            const double a = t0 + 0.2, b = std::min (t1 - 0.1, a + 0.6);
            const size_t sa = (size_t) (a * fs), sb = (size_t) (b * fs);
            // Pitch of the input over the window, from the engine's per-hop pitch.
            std::vector<float> p;
            for (size_t k = i; k <= j; ++k)
                if (hops[k].time >= a && hops[k].time <= b && hops[k].pitch > 0.0f) p.push_back (hops[k].pitch);
            if (! p.empty())
            {
                std::nth_element (p.begin(), p.begin() + (long) p.size() / 2, p.end());
                const double pitch = p[p.size() / 2];
                const double inHz = (double) s.a4Hz * std::pow (2.0, (pitch - 69.0) / 12.0);
                const double inMeasured = measureF0 (in, sa, sb, fs, inHz);
                const double inCentroid = harmonicCentroid (in, sa, sb, fs, inMeasured > 0.0 ? inMeasured : inHz);
                const vm::Harmonizer::Frame& f = hops[(i + j) / 2].frame;
                for (int v = 0; v < std::min (f.voiceCount, kVoices); ++v)
                {
                    const double semis = f.semitones[v];
                    // Expected: the input's own measured pitch shifted by the voice's interval.
                    const double ref = (inMeasured > 0.0 ? inMeasured : inHz) * std::pow (2.0, semis / 12.0);
                    const double hz = measureF0 (voice[v], sa, sb, fs, ref);
                    const double cents = hz > 0.0 ? 1200.0 * std::log2 (hz / ref) : 999.0;
                    if (hz > 0.0) allCents.push_back (cents);
                    const double c = harmonicCentroid (voice[v], sa, sb, fs, hz > 0.0 ? hz : ref);
                    const double body = (double) vm::curve::value (vm::curve::sanitised (s.formantCurve, vm::curve::kFormantLow,
                                                                                          vm::curve::kFormantHigh, vm::curve::defaultFormant()),
                                                                   (float) pitch);
                    std::printf ("%7.2f  %5.1f  %d  %+6.2f  %+6.1f   %5.0f -> %5.0f Hz (x%.2f, expected x%.2f)\n",
                                 a, pitch, v, semis, cents, inCentroid, c, c / inCentroid,
                                 std::pow (std::pow (2.0, semis / 12.0), 1.0 - body));
                }
                worstSteadyJump = std::max (worstSteadyJump, jumpRatio (sum, sa, sb, fs, 0.0));
            }
        }
        // End of the note: the release or the overlap into the next chord.
        const size_t ea = (size_t) (t1 * fs) > (size_t) (0.02 * fs) ? (size_t) ((t1 - 0.02) * fs) : 0;
        const double ref = rmsOf (sum, ea > (size_t) (0.2 * fs) ? ea - (size_t) (0.2 * fs) : 0, ea);
        worstEndJump = std::max (worstEndJump, jumpRatio (sum, ea, endSample, fs, ref * 0.03));
        if (t1 - t0 > 0.8)
        {
            // Where the release starts: voice level 0.6 s before the end, just before it, and over the release.
            const size_t e = (size_t) (t1 * fs), w = (size_t) (0.05 * fs);
            const double sustain = rmsOf (sum, e - (size_t) (0.6 * fs), e - (size_t) (0.6 * fs) + 2 * w);
            const double rel = (double) s.releaseMs * 0.001;
            std::printf ("# note end %.2f s: voices %.1f dB at -0.6 s, %.1f dB at -0.1 s, %.1f dB at the end, "
                         "%.1f dB at +%.2f s (half the release)\n", t1,
                         20.0 * std::log10 (sustain + 1e-9), 20.0 * std::log10 (rmsOf (sum, e - 2 * w, e - w) + 1e-9),
                         20.0 * std::log10 (rmsOf (sum, e - w / 2, e + w / 2) + 1e-9),
                         20.0 * std::log10 (rmsOf (sum, e + (size_t) (rel * 0.5 * fs) - w / 2, e + (size_t) (rel * 0.5 * fs) + w / 2) + 1e-9),
                         rel * 0.5);
        }
        i = j + 1;
    }
    double meanAbs = 0.0, worst = 0.0;
    for (double c : allCents) { meanAbs += std::abs (c); worst = std::max (worst, std::abs (c)); }
    if (! allCents.empty()) meanAbs /= (double) allCents.size();
    std::printf ("# voice pitch: %zu measurements, mean |error| %.1f cents, worst %.1f cents\n", allCents.size(), meanAbs, worst);
    std::printf ("# click measure (largest step / local RMS): steady parts up to %.2f, note ends (release/overlap) up to %.2f\n",
                 worstSteadyJump, worstEndJump);
}

} // namespace

//==============================================================================
int main (int argc, char** argv)
{
    if (argc < 3)
    {
        std::printf ("usage: violinmidi_harmonize in.wav|in.aiff out.wav [--voicing N] [--key ROOT] [--scale N]\n"
                     "       [--body 0..1] [--ensemble 0..1] [--spread 0..1] [--hicut Hz] [--nodrop] [--gain dB] [--drykill]\n"
                     "       [--release ms] [--overlap ms] [--coupling 0..1] [--chromatic] [--harmony dB]\n"
                     "       [--start s] [--length s] [--report]\n"
                     "  --gain is the engine's input gain (analysis only, as in violinmidi_wav);\n"
                     "  --harmony is the level of the voices (Settings::harmonyGainDb).\n");
        return 2;
    }

    vm::Settings s;
    s.triad = true;
    s.audioHarmony = true;
    bool wantReport = false, throughEngine = false, normalise = false;
    double start = 0.0, length = 0.0;
    for (int i = 3; i < argc; ++i)
    {
        const std::string a = argv[i];
        const char* value = i + 1 < argc ? argv[i + 1] : "0";
        auto num = [&] { ++i; return std::atof (value); };
        if (a == "--voicing")        s.voicing = (int) num();
        else if (a == "--key")       s.keyRoot = (int) num();
        else if (a == "--scale")     s.scaleMode = (int) num();
        else if (a == "--body")      s.formantCurve = vm::curve::flat ((float) num());
        else if (a == "--ensemble")  s.ensemble = (float) num();
        else if (a == "--spread")    s.stereoSpread = (float) num();
        else if (a == "--hicut")     s.hiCutCurve = vm::curve::flat ((float) num());
        else if (a == "--nodrop")    s.dropVoicing = false;
        else if (a == "--gain")      s.inputGainDb = (float) num();
        else if (a == "--harmony")   s.harmonyGainDb = (float) num();
        else if (a == "--release")   s.releaseMs = (float) num();
        else if (a == "--overlap")   s.overlapMs = (float) num();
        else if (a == "--coupling")  s.vibratoCoupling = (float) num();
        else if (a == "--start")     start = num();
        else if (a == "--length")    length = num();
        else if (a == "--drykill")   s.dryKill = true;
        else if (a == "--chromatic") s.harmonyMode = vm::HarmonyMode::Chromatic;
        else if (a == "--report")    wantReport = true;
        else if (a == "--engine")    throughEngine = true;   // the sound path the app uses: Engine::process with outputs
        else if (a == "--normalise") normalise = true;       // bring the output to -1 dBFS (quiet recordings)
        else { std::printf ("unknown option %s\n", a.c_str()); return 2; }
    }

    Wav wav;
    std::string error;
    const std::string inputPath = argv[1];
    const bool isAiff = hasSuffix (inputPath, ".aiff") || hasSuffix (inputPath, ".aif");
    if (! (isAiff ? loadAiff (inputPath, wav, error) : loadWav (inputPath, wav, error)))
    {
        std::printf ("error: %s\n", error.c_str());
        return 1;
    }
    const double fs = wav.sampleRate;
    {
        const size_t a = std::min (wav.mono.size(), (size_t) (std::max (0.0, start) * fs));
        const size_t b = length > 0.0 ? std::min (wav.mono.size(), a + (size_t) (length * fs)) : wav.mono.size();
        wav.mono = std::vector<float> (wav.mono.begin() + (long) a, wav.mono.begin() + (long) b);
    }

    vm::Engine engine;
    engine.prepare (fs);
    vm::Harmonizer harmonizer;
    harmonizer.prepare (fs);
    FrameBuilder builder;

    vm::MidiEventList events;
    events.reserve (4096);
    // One analysis hop per block, so a frame is built at every hop.
    const int block = throughEngine ? 256 : engine.hopSize() * engine.decimation();
    const double hopSec = (double) block / fs;

    std::vector<float> left (wav.mono.size()), right (wav.mono.size());
    std::vector<HopRow> hops;
    int notes = 0;
    for (size_t pos = 0; pos < wav.mono.size(); pos += (size_t) block)
    {
        const int n = (int) std::min<size_t> ((size_t) block, wav.mono.size() - pos);
        events.clear();
        if (throughEngine)
            engine.process (wav.mono.data() + pos, n, s, events, left.data() + pos, right.data() + pos);
        else
            engine.process (wav.mono.data() + pos, n, s, events);
        for (const vm::MidiEvent& e : events)
            if ((e.bytes[0] & 0xF0) == 0x90 && e.bytes[2] > 0 && (e.bytes[0] & 0x0F) == s.midiChannel - 1)
                ++notes;
        if (throughEngine)
            continue;
        const vm::Engine::Meter& m = engine.meter();
        const vm::Harmonizer::Frame f = builder.build (m, fs, hopSec, s, false);
        harmonizer.setFrame (f);
        harmonizer.process (wav.mono.data() + pos, left.data() + pos, right.data() + pos, n, s);
        hops.push_back ({ (double) pos / fs, f, m.pitch, m.note });
    }

    double peak = 0.0, inPeak = 0.0;
    for (size_t i = 0; i < left.size(); ++i)
    {
        peak = std::max (peak, (double) std::max (std::abs (left[i]), std::abs (right[i])));
        inPeak = std::max (inPeak, (double) std::abs (wav.mono[i]));
    }
    double scale = 1.0;
    if (peak > 0.99 || (normalise && peak > 0.0))
    {
        scale = (normalise ? 0.89 : 0.99) / peak;
        for (size_t i = 0; i < left.size(); ++i) { left[i] *= (float) scale; right[i] *= (float) scale; }
    }
    writeWav16 (argv[2], left, right, fs);
    std::printf ("# %s -> %s: %.0f Hz, %.2f s, %d notes, input peak %.1f dBFS, output peak %.1f dBFS%s\n",
                 argv[1], argv[2], fs, (double) left.size() / fs, notes, 20.0 * std::log10 (inPeak + 1e-12),
                 20.0 * std::log10 (peak + 1e-12), scale < 1.0 ? " (scaled down to -0.1 dBFS)" : "");
    std::printf ("# voice latency at the median note (period %.0f samples): %d samples\n", medianPeriod (hops),
                 harmonizer.latencySamples (medianPeriod (hops)));

    if (wantReport && ! throughEngine)
        report (wav.mono, hops, fs, block, s);
    return 0;
}
