#include "SettingsText.h"

#include "Scale.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>

namespace vm
{
namespace settingsfile
{

namespace
{
    using E = Settings;

    /** Rounds as juce::roundToInt did in 2.0: to the nearest, a half to the even one. */
    int whole (double v) noexcept   { return static_cast<int> (std::lrint (v)); }

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
        f.get = [member] (const E& e) { return static_cast<double> (e.*member); };
        f.set = [member] (E& e, double v) { e.*member = whole (v); };
        return f;
    }

    template <typename Enum>
    Field choice (const char* key, Enum E::* member, int count)
    {
        Field f;
        f.key = key;
        f.type = Field::Type::Whole;
        f.high = count - 1;
        f.get = [member] (const E& e) { return static_cast<double> (static_cast<int> (e.*member)); };
        f.set = [member] (E& e, double v) { e.*member = static_cast<Enum> (whole (v)); };
        return f;
    }

    Field number (const char* key, float E::* member, double low, double high)
    {
        Field f;
        f.key = key;
        f.low = low;
        f.high = high;
        f.get = [member] (const E& e) { return static_cast<double> (e.*member); };
        f.set = [member] (E& e, double v) { e.*member = static_cast<float> (v); };
        return f;
    }

    Field inProfile (Field f)   { f.scope = Field::Scope::profile; return f; }
    Field inWiring (Field f)    { f.scope = Field::Scope::wiring; return f; }

    std::string trim (const std::string& s)
    {
        const auto first = s.find_first_not_of (" \t\r");
        if (first == std::string::npos)
            return {};
        const auto last = s.find_last_not_of (" \t\r");
        return s.substr (first, last - first + 1);
    }

    std::vector<std::string> tokens (const std::string& s)
    {
        std::vector<std::string> list;
        std::istringstream in (s);
        std::string token;
        while (in >> token)
            list.push_back (token);
        return list;
    }

    /** A number in the C locale; the whole text must be the number. */
    template <typename Number>
    bool parse (const std::string& text, Number& value)
    {
        std::istringstream in (text);
        in.imbue (std::locale::classic());
        Number v {};
        if (! (in >> v))
            return false;
        in >> std::ws;
        if (! in.eof())
            return false;
        value = v;
        return true;
    }

    std::string fixed (double v, int places)
    {
        std::ostringstream out;
        out.imbue (std::locale::classic());
        out << std::fixed << std::setprecision (places) << v;
        return out.str();
    }

    std::string significant (double v)
    {
        std::ostringstream out;
        out.imbue (std::locale::classic());
        out << std::setprecision (9) << v;
        return out.str();
    }
}

//==============================================================================
const std::vector<Field>& table()
{
    static const std::vector<Field> list = {
        // since 1.0
        choice ("bendMode",      &E::bendMode, 3),
        number ("bendRange",     &E::bendRangeSemis, 0.1, 96.0),
        choice ("dynamics",      &E::dynamics, 3),
        choice ("response",      &E::response, 3),
        inProfile (whole  ("lowestNote",    &E::lowestNote, kAbsLowestNote, 60)),
        inProfile (number ("gainDb",        &E::inputGainDb, -12.0, 36.0)),
        inProfile (number ("gateDb",        &E::gateDb, -70.0, -20.0)),
        whole  ("fixedVelocity", &E::fixedVelocity, 1, 127),
        number ("a4Hz",          &E::a4Hz, 430.0, 450.0),
        whole  ("midiChannel",   &E::midiChannel, 1, 16),
        // since 1.3
        number ("vibratoSmoothing", &E::vibratoSmoothing, 0.0, 1.0),
        flag   ("keyFilter",     &E::keyFilter),
        whole  ("keyRoot",       &E::keyRoot, 0, 11),
        whole  ("scaleMode",     &E::scaleMode, 0, scale::kScaleCount - 1),
        flag   ("pentatonic",    &E::pentatonic),
        flag   ("pentaSustain",  &E::pentaSustain),
        flag   ("triad",         &E::triad),
        whole  ("triadChannel",  &E::triadChannel, 1, 16),
        // since 1.4
        inProfile (number ("gateOffDb",     &E::gateOffDb, -90.0, -20.0)),
        inProfile (flag   ("octaveAttackGuard",  &E::octaveAttackGuard)),
        inProfile (number ("octaveAttackMs",     &E::octaveAttackMs, 0.0, 60.0)),
        inProfile (flag   ("octaveSustainGuard", &E::octaveSustainGuard)),
        inProfile (number ("octaveSustainMs",    &E::octaveSustainMs, 0.0, 100.0)),
        choice ("articulation",  &E::articulation, 3),
        choice ("dynamicsTarget", &E::dynamicsTarget, 4),
        number ("dynamicsSmoothingMs", &E::dynamicsSmoothingMs, 0.0, 150.0),
        whole  ("voicing",       &E::voicing, 0, scale::kVoicingCount - 1),
        choice ("harmonyMode",   &E::harmonyMode, 2),
        flag   ("mpe",           &E::mpe),
        // since 1.5
        inProfile (flag   ("jumpGuard",     &E::jumpGuard)),
        inProfile (whole  ("jumpIntervalSemis", &E::jumpIntervalSemis, 5, 24)),
        inProfile (number ("jumpWindowMs",  &E::jumpWindowMs, 10.0, 200.0)),
        inProfile (number ("jumpLockMs",    &E::jumpLockMs, 20.0, 150.0)),
        inProfile (number ("bridgeMs",      &E::bridgeMs, 0.0, 200.0)),
        number ("overlapMs",     &E::overlapMs, 0.0, 1500.0),
        number ("releaseMs",     &E::releaseMs, 0.0, 3000.0),
        number ("harmonyOffset", &E::harmonyOffset, -0.5, 0.0),
        flag   ("audioHarmony",  &E::audioHarmony),
        number ("vibratoCoupling", &E::vibratoCoupling, 0.0, 1.0),
        number ("stereoSpread",  &E::stereoSpread, 0.0, 1.0),
        number ("harmonyGainDb", &E::harmonyGainDb, -60.0, 6.0),
        inWiring (flag ("dryKill", &E::dryKill)),
        // since 2.0 (1.5 wrote "bodyKeep", "humanize" and "hiCutHz", now the two
        // curves and "ensemble"; the old keys stay in the file for 1.5)
        flag   ("dropVoicing",   &E::dropVoicing),
        number ("ensemble",      &E::ensemble, 0.0, 1.0),
        flag   ("voicingEdited", &E::voicingEdited),
        // since 2.1 (unknown to 2.0, which keeps the fixed key and its own filters)
        choice ("harmonySource", &E::harmonySource, 2),
        choice ("chordMode",     &E::chordMode, 4),
        flag   ("pentaFilter",   &E::pentaFilter),
        number ("hybridLockMs",  &E::hybridLockMs, 150.0, 1000.0)
    };
    return list;
}

const Field* find (const std::string& key)
{
    for (const Field& f : table())
        if (key == f.key)
            return &f;
    return nullptr;
}

std::vector<std::string> keysOf (Field::Scope scope)
{
    std::vector<std::string> keys;
    for (const Field& f : table())
        if (f.scope == scope)
            keys.emplace_back (f.key);
    return keys;
}

const std::vector<CurveField>& curves()
{
    static const std::vector<CurveField> list = {
        { kFormantCurve, &E::formantCurve, curve::kFormantLow, curve::kFormantHigh, &curve::defaultFormant },
        { kHiCutCurve,   &E::hiCutCurve,   curve::kHiCutLowHz, curve::kHiCutHighHz, &curve::defaultHiCut }
    };
    return list;
}

//==============================================================================
std::vector<int> voiceList (uint64_t mask)
{
    std::vector<int> list;
    for (int i = 0; i < 49; ++i)
        if ((mask >> i) & 1ull)
            list.push_back (i - 24);
    return list;
}

uint64_t voiceMask (const std::vector<int>& semitones)
{
    uint64_t mask = 0;
    for (int s : semitones)
        if (s >= -24 && s <= 24)
            mask |= 1ull << (s + 24);
    return scale::cleanFreeMask (mask);
}

std::string voicesText (uint64_t mask)
{
    std::string text;
    for (int s : voiceList (mask))
        text += (text.empty() ? "" : " ") + std::to_string (s);
    return text;
}

uint64_t voicesFromText (const std::string& text)
{
    std::vector<int> semitones;
    for (const auto& token : tokens (text))
    {
        int s = 0;
        if (parse (token, s))
            semitones.push_back (s);
    }
    return voiceMask (semitones);
}

std::string curveText (const Curve& c)
{
    std::string text;
    for (int i = 0; i < c.count; ++i)
        text += (i == 0 ? "" : " ") + fixed (c.x[i], 2) + ":" + fixed (c.y[i], 4);
    return text;
}

Curve curveFromText (const std::string& text, const CurveField& f)
{
    Curve c;
    for (const auto& token : tokens (text))
    {
        const auto colon = token.find (':');
        float x = 0.0f, y = 0.0f;
        if (c.count >= Curve::kMaxPoints || colon == std::string::npos
            || ! parse (token.substr (0, colon), x) || ! parse (token.substr (colon + 1), y))
            continue;
        c.x[c.count] = x;
        c.y[c.count] = y;
        ++c.count;
    }
    return curve::sanitised (c, f.low, f.high, f.fallback());
}

//==============================================================================
void tidy (Settings& e)
{
    e.voicing = scale::clampVoicing (e.voicing);
    e.freeVoicing = scale::cleanFreeMask (e.freeVoicing);
    // Edited voices without the flag (or data from before 2.0): the preset's own.
    e.voicingVoices = e.voicingEdited ? scale::cleanFreeMask (e.voicingVoices) : scale::presetMask (e.voicing);
    for (const auto& f : curves())
        e.*(f.member) = curve::sanitised (e.*(f.member), f.low, f.high, f.fallback());

    e.gateOffDb = std::min (e.gateOffDb, e.gateDb);
    e.highestNote = Settings().highestNote;

    // Until 1.3 "only the pentatonic" was a switch next to the church modes.
    // The pentatonic scales are in the list of scales now.
    if (e.pentatonic)
    {
        e.scaleMode = scale::usesMajorPentatonic (e.scaleMode) ? 10 : 11;
        e.pentatonic = false;
    }
}

void migrateHold (bool writtenBefore21, Settings& e) noexcept
{
    // 2.0 held the last valid note only with a scale of fewer than seven notes.
    if (writtenBefore21 && e.pentaSustain && scale::noteCount (e.scaleMode) >= 7)
        e.pentaSustain = false;
}

//==============================================================================
std::string toText (const Settings& e)
{
    std::string text = "# VioLogic engine settings\n";

    for (const Field& f : table())
    {
        const double v = f.get (e);
        text += f.key;
        text += '=';
        if (f.type == Field::Type::Flag)        text += v >= 0.5 ? "1" : "0";
        else if (f.type == Field::Type::Whole)  text += std::to_string (whole (v));
        else                                    text += significant (v);
        text += '\n';
    }

    text += std::string (kFreeVoicing) + "=" + voicesText (e.freeVoicing) + "\n";
    text += std::string (kVoicingVoices) + "=" + voicesText (e.voicingVoices) + "\n";
    text += std::string (kVoicingVoicesOf) + "=" + std::to_string (e.voicing) + "\n";
    for (const auto& f : curves())
        text += std::string (f.key) + "=" + curveText (e.*(f.member)) + "\n";
    return text;
}

bool fromText (const std::string& text, Settings& into, std::vector<std::string>* unknown)
{
    bool ok = true, hasVoices = false, hasGateOff = false, hasSource = false;
    int voicesOf = -1;   // missing: the voices belong to no voicing (1.5 knows neither key)

    std::istringstream lines (text);
    std::string line;
    while (std::getline (lines, line))
    {
        line = trim (line);
        if (line.empty() || line[0] == '#')
            continue;

        const auto equals = line.find ('=');
        if (equals == std::string::npos)
        {
            ok = false;
            continue;
        }

        const auto key = trim (line.substr (0, equals));
        const auto value = trim (line.substr (equals + 1));

        if (const Field* f = find (key))
        {
            double v = 0.0;
            if (f->type == Field::Type::Flag && (value == "true" || value == "false"))
                v = value == "true" ? 1.0 : 0.0;
            else if (! parse (value, v) || ! std::isfinite (v))
            {
                ok = false;
                continue;
            }

            // A flag as the settings file reads it: a whole number other than 0 is on.
            f->set (into, f->type == Field::Type::Flag ? (std::abs (v) >= 1.0 ? 1.0 : 0.0) : std::clamp (v, f->low, f->high));
            hasGateOff = hasGateOff || key == "gateOffDb";
            hasSource = hasSource || key == "harmonySource";
        }
        else if (key == kFreeVoicing)
        {
            into.freeVoicing = voicesFromText (value);
        }
        else if (key == kVoicingVoices)
        {
            into.voicingVoices = voicesFromText (value);
            hasVoices = true;
        }
        else if (key == kVoicingVoicesOf)
        {
            if (! parse (value, voicesOf))
                ok = false;
        }
        else
        {
            const auto curve = std::find_if (curves().begin(), curves().end(), [&key] (const CurveField& c) { return key == c.key; });
            if (curve != curves().end())
                into.*(curve->member) = curveFromText (value, *curve);
            else if (unknown != nullptr)
                unknown->push_back (key);
        }
    }

    // The rules of the settings file for data of older versions (app/SettingsFields.cpp, load()).
    if (! hasVoices)
        into.voicingEdited = false;
    if (into.voicingEdited && voicesOf != into.voicing)
        into.voicingEdited = false;
    if (! hasGateOff)
        into.gateOffDb = into.gateDb - 6.0f;   // until 1.3 a note ended 6 dB below the one threshold there was

    tidy (into);
    migrateHold (! hasSource, into);
    return ok;
}

} // namespace settingsfile

//==============================================================================
void keepOnPresetLoad (Settings& loaded, const Settings& live)
{
    for (const auto& f : settingsfile::table())
        if (f.scope != settingsfile::Field::Scope::preset)
            f.set (loaded, f.get (live));
}

} // namespace vm
