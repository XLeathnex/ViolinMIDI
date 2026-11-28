#include "SettingsFields.h"

#include "Scale.h"
#include "SettingsStore.h"

#include <cmath>
#include <vector>

namespace
{
    using E = vm::Settings;
    using Field = vm::settingsfile::Field;
    using CurveField = vm::settingsfile::CurveField;

    // Masks of voices and curves are not plain numbers: written by hand below.
    using vm::settingsfile::kFreeVoicing;
    using vm::settingsfile::kVoicingVoices;
    using vm::settingsfile::kVoicingVoicesOf;

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

    std::vector<int> freeVoices (uint64_t mask)       { return vm::settingsfile::voiceList (mask); }
    uint64_t freeMask (const std::vector<int>& semis) { return vm::settingsfile::voiceMask (semis); }

    /** What has to hold between fields, whatever the file said. */
    void tidy (E& e)
    {
        vm::settingsfile::tidy (e);
        e.bendRangeSemis = SettingsStore::kBendRanges[SettingsStore::bendRangeIndex (e.bendRangeSemis)];
    }
}

namespace fields
{

juce::var toVar (const vm::Settings& e, Part part)
{
    auto* object = new juce::DynamicObject();

    for (const Field& f : vm::settingsfile::table())
    {
        if (part == Part::presetImage && f.scope != Field::Scope::preset)
            continue;

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

    for (const auto& f : vm::settingsfile::curves())
        object->setProperty (f.key, curveVar (e.*(f.member)));

    return juce::var (object);
}

void fromVar (const juce::var& object, vm::Settings& into)
{
    if (! object.isObject())
        return;

    for (const Field& f : vm::settingsfile::table())
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

        std::vector<int> semitones;
        for (const juce::var& v : *voices)
            if (isNumber (v))
                semitones.push_back (juce::roundToInt ((double) v));
        mask = freeMask (semitones);
        return true;
    };

    readVoices (kFreeVoicing, into.freeVoicing);
    if (! readVoices (kVoicingVoices, into.voicingVoices))
        into.voicingEdited = false;

    for (const auto& f : vm::settingsfile::curves())
        if (object.hasProperty (f.key))
            into.*(f.member) = curveFromVar (object.getProperty (f.key, juce::var()), f);

    // A preset from before 2.0 ("ensemble" is new) at the old default spread of 50 %
    // gets the new default, as the settings file does (SettingsStore::migrate).
    if (! object.hasProperty ("ensemble") && std::abs ((double) object.getProperty ("stereoSpread", 0.0) - 0.5) < 1.0e-6)
        into.stereoSpread = vm::Settings().stereoSpread;

    tidy (into);
    vm::settingsfile::migrateHold (! object.hasProperty ("harmonySource"), into);
}

void load (juce::PropertiesFile& props, vm::Settings& into)
{
    for (const Field& f : vm::settingsfile::table())
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

        std::vector<int> semitones;
        for (const auto& token : juce::StringArray::fromTokens (props.getValue (key), " ", ""))
            if (token.isNotEmpty())
                semitones.push_back (token.getIntValue());
        mask = freeMask (semitones);
        return true;
    };

    readVoices (kFreeVoicing, into.freeVoicing);
    if (! readVoices (kVoicingVoices, into.voicingVoices))
        into.voicingEdited = false;

    // 1.5 knows neither key: after a visit there the voicing may have changed while the
    // edited voices stayed. They belong to the voicing they were made for, or to none.
    if (into.voicingEdited && props.getIntValue (kVoicingVoicesOf, -1) != into.voicing)
        into.voicingEdited = false;

    for (const auto& f : vm::settingsfile::curves())
        if (props.containsKey (f.key))
            into.*(f.member) = curveFromText (props.getValue (f.key), f);

    // Until 1.3 a note ended 6 dB below the one threshold there was.
    if (! props.containsKey ("gateOffDb"))
        into.gateOffDb = into.gateDb - 6.0f;

    tidy (into);
    vm::settingsfile::migrateHold (! props.containsKey ("harmonySource"), into);
}

void save (juce::PropertiesFile& props, const vm::Settings& e)
{
    for (const Field& f : vm::settingsfile::table())
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
    props.setValue (kVoicingVoicesOf, e.voicing);

    for (const auto& f : vm::settingsfile::curves())
        props.setValue (f.key, curveText (e.*(f.member)));
}

juce::StringArray profileKeys()
{
    juce::StringArray keys;
    for (const auto& key : vm::settingsfile::keysOf (Field::Scope::profile))
        keys.add (key);
    return keys;
}

juce::NamedValueSet subset (const vm::Settings& e, std::initializer_list<const char*> keys)
{
    const auto all = toVar (e);
    juce::NamedValueSet result;
    for (const char* key : keys)
        if (all.hasProperty (key))
            result.set (key, all.getProperty (key, juce::var()));
    return result;
}

} // namespace fields
