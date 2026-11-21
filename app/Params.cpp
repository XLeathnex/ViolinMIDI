#include "Params.h"

#include <cmath>

namespace
{
    juce::String text (const char* utf8) { return juce::String::fromUTF8 (utf8); }

    using E = vm::Settings;

    Param toggle (const char* id, const char* name, bool E::* field)
    {
        Param p;
        p.id = id;
        p.name = text (name);
        p.kind = Param::Kind::Toggle;
        p.get = [field] (const AppSettings& s) { return s.engine.*field ? 1.0 : 0.0; };
        p.set = [field] (AppSettings& s, double v) { s.engine.*field = v >= 0.5; };
        return p;
    }

    /** An int field holding first, first + 1, ... first + count - 1. */
    Param choice (const char* id, const char* name, int E::* field, int first, int count)
    {
        Param p;
        p.id = id;
        p.name = text (name);
        p.kind = Param::Kind::Choice;
        p.high = count - 1;
        p.get = [field, first] (const AppSettings& s) { return (double) (s.engine.*field - first); };
        p.set = [field, first, count] (AppSettings& s, double v) { s.engine.*field = first + juce::jlimit (0, count - 1, juce::roundToInt (v)); };
        return p;
    }

    template <typename Enum>
    Param enumChoice (const char* id, const char* name, Enum E::* field, int count)
    {
        Param p;
        p.id = id;
        p.name = text (name);
        p.kind = Param::Kind::Choice;
        p.high = count - 1;
        p.get = [field] (const AppSettings& s) { return (double) (int) (s.engine.*field); };
        p.set = [field, count] (AppSettings& s, double v) { s.engine.*field = (Enum) juce::jlimit (0, count - 1, juce::roundToInt (v)); };
        return p;
    }

    Param range (const char* id, const char* name, float E::* field, double low, double high, double step)
    {
        Param p;
        p.id = id;
        p.name = text (name);
        p.kind = Param::Kind::Range;
        p.low = low;
        p.high = high;
        p.get = [field] (const AppSettings& s) { return (double) (s.engine.*field); };
        p.set = [field, low, high, step] (AppSettings& s, double v)
        {
            v = juce::jlimit (low, high, v);
            s.engine.*field = (float) (step > 0.0 ? low + std::round ((v - low) / step) * step : v);
        };
        return p;
    }

    std::vector<Param> build()
    {
        std::vector<Param> list;

        list.push_back (toggle ("enabled", "Tonh\xc3\xb6henerkennung", &E::enabled));
        list.push_back (toggle ("keyFilter", "Tonart ber\xc3\xbc" "cksichtigen", &E::keyFilter));
        list.push_back (choice ("keyRoot", "Grundton", &E::keyRoot, 0, 12));
        list.push_back (choice ("scaleMode", "Kirchentonart", &E::scaleMode, 0, 7));
        list.push_back (toggle ("pentatonic", "Nur Pentatonik", &E::pentatonic));
        list.push_back (toggle ("pentaSustain", "Pentatonik-Ton halten", &E::pentaSustain));
        list.push_back (toggle ("triad", "Dreiklang", &E::triad));
        list.push_back (toggle ("triadWithRoot", "Dreiklang mit Grundton", &E::triadWithRoot));
        list.push_back (choice ("triadChannel", "Dreiklang-Kanal", &E::triadChannel, 1, 16));

        {
            // The switch the foot pedal wants: bends on or off.
            Param p;
            p.id = "continuous";
            p.name = "Stufenlos";
            p.kind = Param::Kind::Toggle;
            p.get = [] (const AppSettings& s) { return s.engine.bendMode == vm::BendMode::Continuous ? 1.0 : 0.0; };
            p.set = [] (AppSettings& s, double v) { s.engine.bendMode = v >= 0.5 ? vm::BendMode::Continuous : vm::BendMode::Quantized; };
            list.push_back (p);
        }

        list.push_back (enumChoice ("bendMode", "Bewegung", &E::bendMode, 3));
        list.push_back (range ("vibratoSmoothing", "Vibrato-Gl\xc3\xa4ttung", &E::vibratoSmoothing, 0.0, 1.0, 0.01));

        {
            Param p;
            p.id = "bendRange";
            p.name = "Pitch-Bend-Bereich";
            p.kind = Param::Kind::Choice;
            p.high = SettingsStore::kNumBendRanges - 1;
            p.get = [] (const AppSettings& s) { return (double) SettingsStore::bendRangeIndex (s.engine.bendRangeSemis); };
            p.set = [] (AppSettings& s, double v)
            {
                s.engine.bendRangeSemis = SettingsStore::kBendRanges[juce::jlimit (0, SettingsStore::kNumBendRanges - 1, juce::roundToInt (v))];
            };
            list.push_back (p);
        }

        list.push_back (enumChoice ("response", "Ansprache", &E::response, 3));
        list.push_back (range ("gate", "Schwelle", &E::gateDb, -70.0, -20.0, 0.5));
        list.push_back (range ("gain", "Verst\xc3\xa4rkung", &E::inputGainDb, -12.0, 36.0, 0.5));
        list.push_back (enumChoice ("dynamics", "Dynamik", &E::dynamics, 3));

        {
            Param p;
            p.id = "fixedVelocity";
            p.name = text ("Feste Anschlagst\xc3\xa4rke");
            p.kind = Param::Kind::Range;
            p.low = 1.0;
            p.high = 127.0;
            p.get = [] (const AppSettings& s) { return (double) s.engine.fixedVelocity; };
            p.set = [] (AppSettings& s, double v) { s.engine.fixedVelocity = juce::jlimit (1, 127, juce::roundToInt (v)); };
            list.push_back (p);
        }

        list.push_back (choice ("midiChannel", "MIDI-Kanal", &E::midiChannel, 1, 16));
        list.push_back (choice ("lowestNote", "Tiefster Ton", &E::lowestNote, vm::kAbsLowestNote, 60 - vm::kAbsLowestNote + 1));
        list.push_back (range ("tuning", "Kammerton", &E::a4Hz, 430.0, 450.0, 0.5));

        {
            Param p;
            p.id = "panic";
            p.name = "Alle Noten aus";
            p.kind = Param::Kind::Trigger;
            p.get = [] (const AppSettings&) { return 0.0; };
            p.set = [] (AppSettings&, double) {};
            list.push_back (p);
        }

        return list;
    }
}

namespace params
{

const std::vector<Param>& all()
{
    static const std::vector<Param> list = build();
    return list;
}

int indexOf (const juce::String& id)
{
    const auto& list = all();
    for (size_t i = 0; i < list.size(); ++i)
        if (list[i].id == id)
            return (int) i;
    return -1;
}

bool applyMidi (const Param& p, AppSettings& settings, bool isNote, int value)
{
    value = juce::jlimit (0, 127, value);
    const double before = p.get (settings);

    switch (p.kind)
    {
        case Param::Kind::Trigger:
            return isNote || value >= 64;

        case Param::Kind::Toggle:
            p.set (settings, isNote ? (before >= 0.5 ? 0.0 : 1.0) : (value >= 64 ? 1.0 : 0.0));
            break;

        case Param::Kind::Choice:
        {
            const int count = juce::roundToInt (p.high) + 1;
            const int index = isNote ? (juce::roundToInt (before) + 1) % count
                                     : juce::jmin (count - 1, value * count / 128);
            p.set (settings, (double) index);
            break;
        }

        case Param::Kind::Range:
            p.set (settings, p.low + (p.high - p.low) * (double) value / 127.0);
            break;
    }

    return p.get (settings) != before;
}

} // namespace params
