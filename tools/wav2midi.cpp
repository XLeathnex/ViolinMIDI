// Offline tool: runs a WAV or AIFF recording through the engine and prints the notes.
//
//   violinmidi_wav recording.wav [--mode continuous|smoothed|quantized]
//                  [--range N] [--lowest N] [--gate dB] [--gain dB]
//                  [--response fast|normal|safe] [--trace file.csv]
//
// Useful for checking real recordings without the app or a Mac.

#include "../core/Engine.h"

#include <algorithm>
#include <cctype>
#include <cmath>
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

const char* noteName (int note, char* buffer, size_t size)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    std::snprintf (buffer, size, "%s%d", names[((note % 12) + 12) % 12], note / 12 - 1);
    return buffer;
}

} // namespace

int main (int argc, char** argv)
{
    if (argc < 2)
    {
        std::printf ("usage: violinmidi_wav file.wav [--mode continuous|smoothed|quantized] [--range N]\n"
                     "       [--lowest N] [--gate dB] [--gain dB] [--response fast|normal|safe] [--trace file.csv]\n");
        return 2;
    }

    vm::Settings settings;
    std::string tracePath;
    for (int i = 2; i < argc; ++i)
    {
        const std::string a = argv[i];
        const char* value = i + 1 < argc ? argv[i + 1] : "";
        if (a == "--mode")
        {
            if (std::strcmp (value, "smoothed") == 0) settings.bendMode = vm::BendMode::Smoothed;
            else if (std::strcmp (value, "quantized") == 0) settings.bendMode = vm::BendMode::Quantized;
            ++i;
        }
        else if (a == "--response")
        {
            if (std::strcmp (value, "fast") == 0) settings.response = vm::Response::Fast;
            else if (std::strcmp (value, "safe") == 0) settings.response = vm::Response::Safe;
            ++i;
        }
        else if (a == "--range")  { settings.bendRangeSemis = std::atoi (value); ++i; }
        else if (a == "--lowest") { settings.lowestNote = std::atoi (value); ++i; }
        else if (a == "--gate")   { settings.gateDb = (float) std::atof (value); ++i; }
        else if (a == "--gain")   { settings.inputGainDb = (float) std::atof (value); ++i; }
        else if (a == "--trace")  { tracePath = value; ++i; }
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

    vm::Engine engine;
    engine.prepare (wav.sampleRate);
    std::vector<vm::Engine::TraceRow> trace;
    if (! tracePath.empty())
        engine.setTrace (&trace);

    vm::MidiEventList out;
    out.reserve (4096);

    double peak = 0.0;
    for (float v : wav.mono)
        peak = std::max (peak, (double) std::abs (v));
    std::printf ("# %s: %.0f Hz, %.2f s, peak %.1f dBFS\n", argv[1], wav.sampleRate,
                 (double) wav.mono.size() / wav.sampleRate, 20.0 * std::log10 (peak + 1.0e-12));
    std::printf ("# time_s  event\n");

    int bend = 8192, notesOn = 0, bends = 0, controllers = 0;
    const int block = 64;
    for (size_t pos = 0; pos < wav.mono.size(); pos += (size_t) block)
    {
        const int n = (int) std::min<size_t> ((size_t) block, wav.mono.size() - pos);
        out.clear();
        engine.process (wav.mono.data() + pos, n, settings, out);
        for (const vm::MidiEvent& e : out)
        {
            const double t = (double) (pos + (size_t) e.sampleOffset) / wav.sampleRate;
            const int status = e.bytes[0] & 0xF0;
            char name[16];
            if (status == 0x90)
            {
                ++notesOn;
                const double cents = (double) (bend - 8192) / 8192.0 * settings.bendRangeSemis * 100.0;
                std::printf ("%8.3f  ON  %3d %-4s vel %3d  bend %+6.1f ct\n", t, e.bytes[1], noteName (e.bytes[1], name, sizeof (name)), e.bytes[2], cents);
            }
            else if (status == 0x80)
            {
                std::printf ("%8.3f  OFF %3d %-4s\n", t, e.bytes[1], noteName (e.bytes[1], name, sizeof (name)));
            }
            else if (status == 0xE0)
            {
                bend = e.bytes[1] | (e.bytes[2] << 7);
                ++bends;
            }
            else if (status == 0xB0)
            {
                ++controllers;
            }
        }
    }

    out.clear();
    engine.shutdown (out);
    std::printf ("# %d note(s), %d pitch-bend and %d controller messages\n", notesOn, bends, controllers);

    if (! tracePath.empty())
    {
        std::ofstream csv (tracePath);
        csv << "time,voiced,pitch,clarity,level_db,active,note\n";
        for (const auto& row : trace)
            csv << row.timeSec << ',' << (row.voiced ? 1 : 0) << ',' << row.pitch << ',' << row.clarity << ','
                << row.levelDb << ',' << (row.active ? 1 : 0) << ',' << row.note << '\n';
    }
    return 0;
}
