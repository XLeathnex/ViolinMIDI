#include "SettingsFields.h"

#include "Scale.h"
#include "SettingsStore.h"

#include <functional>
#include <vector>

namespace
{
    using E = vm::Settings;

    struct Field
    {
        enum class Type { Flag, Whole, Number };

        const char* key = "";   // never changes: it is the name in the settings file and in the presets
        Type type = Type::Number;
        double low = 0.0, high = 1.0;
        std::function<double (const E&)> get;
        std::function<void (E&, double)> set;
    };

    Field flag (const char* key, bool E::* member)
    {
        Field f;
        f.key = key;
        f.type = Field::Type::Flag;
        f.get = [member] (const E& e) { return e.*member ? 1.0 : 0.0; };
        f.set = [member] (E& e, double v) { e.*member = v >= 0.5; };
        return f;
    }

    Field whole (const char* key, int E::* member, int low, int high)
    {
        Field f;
        f.key = key;
        f.type = Field::Type::Whole;
        f.low = low;
        f.high = high;
        f.get = [member] (const E& e) { return (double) (e.*member); };
        f.set = [member] (E& e, double v) { e.*member = juce::roundToInt (v); };
        return f;
    }

    template <typename Enum>
    Field choice (const char* key, Enum E::* member, int count)
    {
        Field f;
        f.key = key;
        f.type = Field::Type::Whole;
        f.high = count - 1;
        f.get = [member] (const E& e) { return (double) (int) (e.*member); };
        f.set = [member] (E& e, double v) { e.*member = (Enum) juce::roundToInt (v); };
        return f;
    }

    Field number (const char* key, float E::* member, double low, double high)
    {
        Field f;
        f.key = key;
        f.low = low;
        f.high = high;
        f.get = [member] (const E& e) { return (double) (e.*member); };
        f.set = [member] (E& e, double v) { e.*member = (float) v; };
        return f;
    }

    // The table. The master switch and the freeze buttons are not in it: the
    // app always starts listening, with nothing frozen.
    const std::vector<Field>& table()
    {
        static const std::vector<Field> list = {
            // since 1.0
            choice ("bendMode",      &E::bendMode, 3),
            number ("bendRange",     &E::bendRangeSemis, 0.1, 96.0),
            choice ("dynamics",      &E::dynamics, 3),
            choice ("response",      &E::response, 3),
            whole  ("lowestNote",    &E::lowestNote, vm::kAbsLowestNote, 60),
            number ("gainDb",        &E::inputGainDb, -12.0, 36.0),
            number ("gateDb",        &E::gateDb, -70.0, -20.0),
            whole  ("fixedVelocity", &E::fixedVelocity, 1, 127),
            number ("a4Hz",          &E::a4Hz, 430.0, 450.0),
            whole  ("midiChannel",   &E::midiChannel, 1, 16),
            // since 1.3
            number ("vibratoSmoothing", &E::vibratoSmoothing, 0.0, 1.0),
            flag   ("keyFilter",     &E::keyFilter),
            whole  ("keyRoot",       &E::keyRoot, 0, 11),
            whole  ("scaleMode",     &E::scaleMode, 0, vm::scale::kScaleCount - 1),
            flag   ("pentatonic",    &E::pentatonic),
            flag   ("pentaSustain",  &E::pentaSustain),
            flag   ("triad",         &E::triad),
            whole  ("triadChannel",  &E::triadChannel, 1, 16),
            // since 1.4
            number ("gateOffDb",     &E::gateOffDb, -90.0, -20.0),
            flag   ("octaveAttackGuard",  &E::octaveAttackGuard),
            number ("octaveAttackMs",     &E::octaveAttackMs, 0.0, 60.0),
            flag   ("octaveSustainGuard", &E::octaveSustainGuard),
            number ("octaveSustainMs",    &E::octaveSustainMs, 0.0, 100.0),
            choice ("articulation",  &E::articulation, 3),
            choice ("dynamicsTarget", &E::dynamicsTarget, 4),
            number ("dynamicsSmoothingMs", &E::dynamicsSmoothingMs, 0.0, 150.0),
            whole  ("voicing",       &E::voicing, 0, vm::scale::kVoicingCount - 1),
            choice ("harmonyMode",   &E::harmonyMode, 2),
            flag   ("mpe",           &E::mpe),
            // since 1.5
            flag   ("jumpGuard",     &E::jumpGuard),
            whole  ("jumpIntervalSemis", &E::jumpIntervalSemis, 5, 24),
            number ("jumpWindowMs",  &E::jumpWindowMs, 10.0, 200.0),
            number ("jumpLockMs",    &E::jumpLockMs, 20.0, 150.0),
            number ("bridgeMs",      &E::bridgeMs, 0.0, 200.0),
            number ("overlapMs",     &E::overlapMs, 0.0, 1500.0),
            number ("releaseMs",     &E::releaseMs, 0.0, 3000.0),
            number ("harmonyOffset", &E::harmonyOffset, -0.5, 0.0),
            flag   ("audioHarmony",  &E::audioHarmony),
            number ("vibratoCoupling", &E::vibratoCoupling, 0.0, 1.0),
            number ("stereoSpread",  &E::stereoSpread, 0.0, 1.0),
            number ("harmonyGainDb", &E::harmonyGainDb, -60.0, 6.0),
            flag   ("dryKill",       &E::dryKill),
            // since 2.0 (1.5 wrote "bodyKeep", "humanize" and "hiCutHz", now the two
            // curves and "ensemble"; the old keys stay in the file for 1.5)
            flag   ("dropVoicing",   &E::dropVoicing),
            number ("ensemble",      &E::ensemble, 0.0, 1.0),
            flag   ("voicingEdited", &E::voicingEdited)
        };
        return list;
    }

    // Masks of voices and curves are not plain numbers: written by hand below.
    constexpr const char* kFreeVoicing = "freeVoicing";       // the semitone distances, e.g. "-12 0 7"
    constexpr const char* kVoicingVoices = "voicingVoices";   // since 2.0, the same for the preset voicing as played
    constexpr const char* kFormantCurve = "formantCurve";     // since 2.0, "note:value" pairs, e.g. "55:0.05 69:0.2"
    constexpr const char* kHiCutCurve = "hiCutCurve";         // since 2.0, "note:Hz" pairs

    struct CurveField
    {
        const char* key;
        vm::Curve E::* member;
        float low, high;
        vm::Curve (*fallback)();
    };

    const CurveField kCurves[] = {
        { kFormantCurve, &E::formantCurve, vm::curve::kFormantLow, vm::curve::kFormantHigh, &vm::curve::defaultFormant },
        { kHiCutCurve,   &E::hiCutCurve,   vm::curve::kHiCutLowHz, vm::curve::kHiCutHighHz, &vm::curve::defaultHiCut }
    };

    juce::String curveText (const vm::Curve& c)
    {
        juce::StringArray points;
        for (int i = 0; i < c.count; ++i)
            points.add (juce::String (c.x[i], 2) + ":" + juce::String (c.y[i], 4));
        return points.joinIntoString (" ");
    }

    vm::Curve curveFromText (const juce::String& text, const CurveField& f)
    {
        vm::Curve c;
        for (const auto& token : juce::StringArray::fromTokens (text, " ", ""))
        {
            if (c.count >= vm::Curve::kMaxPoints || ! token.containsChar (':'))
                continue;
            c.x[c.count] = token.upToFirstOccurrenceOf (":", false, false).getFloatValue();
            c.y[c.count] = token.fromFirstOccurrenceOf (":", false, false).getFloatValue();
            ++c.count;
        }
        return vm::curve::sanitised (c, f.low, f.high, f.fallback());
    }

    juce::var curveVar (const vm::Curve& c)
    {
        juce::Array<juce::var> points;
        for (int i = 0; i < c.count; ++i)
            points.add (juce::Array<juce::var> { juce::var ((double) c.x[i]), juce::var ((double) c.y[i]) });
        return points;
    }

    bool isNumber (const juce::var& v) { return v.isInt() || v.isInt64() || v.isDouble(); }

    vm::Curve curveFromVar (const juce::var& value, const CurveField& f)
    {
        vm::Curve c;
        if (const auto* points = value.getArray())
            for (const auto& point : *points)
                if (const auto* pair = point.getArray(); pair != nullptr && pair->size() == 2 && c.count < vm::Curve::kMaxPoints
                                                         && isNumber ((*pair)[0]) && isNumber ((*pair)[1]))
                {
                    c.x[c.count] = (float) (double) (*pair)[0];
                    c.y[c.count] = (float) (double) (*pair)[1];
                    ++c.count;
                }
        return vm::curve::sanitised (c, f.low, f.high, f.fallback());
    }

    juce::Array<int> freeVoices (uint64_t mask)
    {
        juce::Array<int> list;
        for (int i = 0; i < 49; ++i)
            if ((mask >> i) & 1ull)
                list.add (i - 24);
        return list;
    }

    uint64_t freeMask (const juce::Array<int>& semitones)
    {
        uint64_t mask = 0;
        for (int s : semitones)
            if (s >= -24 && s <= 24)
                mask |= 1ull << (s + 24);
        return vm::scale::cleanFreeMask (mask);
    }

    /** What has to hold between fields, whatever the file said. */
    void tidy (E& e)
    {
        e.voicing = vm::scale::clampVoicing (e.voicing);
        e.freeVoicing = vm::scale::cleanFreeMask (e.freeVoicing);
        // Edited voices without the flag (or a file from before 2.0): the preset's own.
        e.voicingVoices = e.voicingEdited ? vm::scale::cleanFreeMask (e.voicingVoices) : vm::scale::presetMask (e.voicing);
        for (const auto& f : kCurves)
            e.*(f.member) = vm::curve::sanitised (e.*(f.member), f.low, f.high, f.fallback());

        e.bendRangeSemis = SettingsStore::kBendRanges[SettingsStore::bendRangeIndex (e.bendRangeSemis)];
        e.gateOffDb = juce::jmin (e.gateOffDb, e.gateDb);
        e.highestNote = E().highestNote;

        // Until 1.3 "only the pentatonic" was a switch next to the church modes.
        // The pentatonic scales are in the list of scales now.
        if (e.pentatonic)
        {
            e.scaleMode = vm::scale::usesMajorPentatonic (e.scaleMode) ? 10 : 11;
            e.pentatonic = false;
        }
    }
}

namespace fields
{

juce::var toVar (const vm::Settings& e)
{
    auto* object = new juce::DynamicObject();

    for (const Field& f : table())
    {
        const double v = f.get (e);
        if (f.type == Field::Type::Flag)        object->setProperty (f.key, v >= 0.5);
        else if (f.type == Field::Type::Whole)  object->setProperty (f.key, juce::roundToInt (v));
        else                                    object->setProperty (f.key, v);
    }

    juce::Array<juce::var> voices;
    for (int s : freeVoices (e.freeVoicing))
        voices.add (s);
    object->setProperty (kFreeVoicing, voices);

    juce::Array<juce::var> played;
    for (int s : freeVoices (e.voicingVoices))
        played.add (s);
    object->setProperty (kVoicingVoices, played);

    for (const auto& f : kCurves)
        object->setProperty (f.key, curveVar (e.*(f.member)));

    return juce::var (object);
}

void fromVar (const juce::var& object, vm::Settings& into)
{
    if (! object.isObject())
        return;

    for (const Field& f : table())
    {
        const juce::var value = object.getProperty (f.key, juce::var());
        if (value.isBool())
            f.set (into, value ? 1.0 : 0.0);
        else if (value.isInt() || value.isInt64() || value.isDouble())
            f.set (into, juce::jlimit (f.low, f.high, (double) value));
    }

    const auto readVoices = [&object] (const char* key, uint64_t& mask)
    {
        const auto* voices = object.getProperty (key, juce::var()).getArray();
        if (voices == nullptr)
            return false;

        juce::Array<int> semitones;
        for (const juce::var& v : *voices)
            if (isNumber (v))
                semitones.add (juce::roundToInt ((double) v));
        mask = freeMask (semitones);
        return true;
    };

    readVoices (kFreeVoicing, into.freeVoicing);
    if (! readVoices (kVoicingVoices, into.voicingVoices))
        into.voicingEdited = false;

    for (const auto& f : kCurves)
        if (object.hasProperty (f.key))
            into.*(f.member) = curveFromVar (object.getProperty (f.key, juce::var()), f);

    tidy (into);
}

void load (juce::PropertiesFile& props, vm::Settings& into)
{
    for (const Field& f : table())
    {
        if (! props.containsKey (f.key))
            continue;

        if (f.type == Field::Type::Flag)
            f.set (into, props.getBoolValue (f.key) ? 1.0 : 0.0);
        else
            f.set (into, juce::jlimit (f.low, f.high, props.getDoubleValue (f.key)));
    }

    const auto readVoices = [&props] (const char* key, uint64_t& mask)
    {
        if (! props.containsKey (key))
            return false;

        juce::Array<int> semitones;
        for (const auto& token : juce::StringArray::fromTokens (props.getValue (key), " ", ""))
            if (token.isNotEmpty())
                semitones.add (token.getIntValue());
        mask = freeMask (semitones);
        return true;
    };

    readVoices (kFreeVoicing, into.freeVoicing);
    if (! readVoices (kVoicingVoices, into.voicingVoices))
        into.voicingEdited = false;

    for (const auto& f : kCurves)
        if (props.containsKey (f.key))
            into.*(f.member) = curveFromText (props.getValue (f.key), f);

    // Until 1.3 a note ended 6 dB below the one threshold there was.
    if (! props.containsKey ("gateOffDb"))
        into.gateOffDb = into.gateDb - 6.0f;

    tidy (into);
}

void save (juce::PropertiesFile& props, const vm::Settings& e)
{
    for (const Field& f : table())
    {
        const double v = f.get (e);
        if (f.type == Field::Type::Flag)        props.setValue (f.key, v >= 0.5);
        else if (f.type == Field::Type::Whole)  props.setValue (f.key, juce::roundToInt (v));
        else                                    props.setValue (f.key, v);
    }

    const auto writeVoices = [&props] (const char* key, uint64_t mask)
    {
        juce::StringArray voices;
        for (int s : freeVoices (mask))
            voices.add (juce::String (s));
        props.setValue (key, voices.joinIntoString (" "));
    };

    writeVoices (kFreeVoicing, e.freeVoicing);
    writeVoices (kVoicingVoices, e.voicingVoices);

    for (const auto& f : kCurves)
        props.setValue (f.key, curveText (e.*(f.member)));
}

} // namespace fields
