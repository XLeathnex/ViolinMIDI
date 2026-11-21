#include "SettingsStore.h"

#include "Brand.h"

#include <cmath>

namespace key
{
    // Keys from version 1.0/1.1 keep their names, so every version reads the same file.
    constexpr const char* bendMode      = "bendMode";
    constexpr const char* bendRange     = "bendRange";
    constexpr const char* dynamics      = "dynamics";
    constexpr const char* response      = "response";
    constexpr const char* lowestNote    = "lowestNote";
    constexpr const char* gainDb        = "gainDb";
    constexpr const char* gateDb        = "gateDb";
    constexpr const char* fixedVelocity = "fixedVelocity";
    constexpr const char* a4Hz          = "a4Hz";
    constexpr const char* midiChannel   = "midiChannel";
    constexpr const char* audioState    = "audioState";
    // added in 1.2
    constexpr const char* schema        = "settingsSchema";
    constexpr const char* lastVersion   = "lastVersion";
    constexpr const char* midiPortName  = "midiPortName";
    constexpr const char* windowX       = "windowX";
    constexpr const char* windowY       = "windowY";
    // added in 1.3. The master switch is not stored: the app always starts listening.
    constexpr const char* vibratoSmoothing = "vibratoSmoothing";
    constexpr const char* keyFilter     = "keyFilter";
    constexpr const char* keyRoot       = "keyRoot";
    constexpr const char* scaleMode     = "scaleMode";
    constexpr const char* pentatonic    = "pentatonic";
    constexpr const char* pentaSustain  = "pentaSustain";
    constexpr const char* triad         = "triad";
    constexpr const char* triadChannel  = "triadChannel";
    constexpr const char* triadWithRoot = "triadWithRoot";
    constexpr const char* midiInput     = "midiInput";
    constexpr const char* midiMap       = "midiMap";
}

SettingsStore::SettingsStore (juce::PropertiesFile& file) : props (file)
{
    migrate();
}

int SettingsStore::bendRangeIndex (float semitones)
{
    int best = 0;
    for (int i = 1; i < kNumBendRanges; ++i)
        if (std::abs (kBendRanges[i] - semitones) < std::abs (kBendRanges[best] - semitones))
            best = i;
    return best;
}

void SettingsStore::migrate()
{
    if (props.getIntValue (key::schema, 0) >= kSchema)
        return;

    // Files from 1.0 and 1.1 have no schema number. Someone who used those
    // versions has set up MainStage for the port called "ViolinMIDI": keep it.
    const bool earlierInstall = props.containsKey (key::bendMode) || props.containsKey (key::audioState);
    if (! props.containsKey (key::midiPortName))
        props.setValue (key::midiPortName, earlierInstall ? brand::kLegacyName : brand::kName);

    props.setValue (key::schema, kSchema);
}

AppSettings SettingsStore::load()
{
    AppSettings s;
    const vm::Settings d;
    auto& e = s.engine;

    e.bendMode      = (vm::BendMode) juce::jlimit (0, 2, props.getIntValue (key::bendMode, (int) d.bendMode));
    e.dynamics      = (vm::DynamicsMode) juce::jlimit (0, 2, props.getIntValue (key::dynamics, (int) d.dynamics));
    e.response      = (vm::Response) juce::jlimit (0, 2, props.getIntValue (key::response, (int) d.response));
    e.lowestNote    = juce::jlimit (vm::kAbsLowestNote, 60, props.getIntValue (key::lowestNote, d.lowestNote));
    e.inputGainDb   = (float) juce::jlimit (-12.0, 36.0, props.getDoubleValue (key::gainDb, d.inputGainDb));
    e.gateDb        = (float) juce::jlimit (-70.0, -20.0, props.getDoubleValue (key::gateDb, d.gateDb));
    e.fixedVelocity = juce::jlimit (1, 127, props.getIntValue (key::fixedVelocity, d.fixedVelocity));
    e.a4Hz          = (float) juce::jlimit (430.0, 450.0, props.getDoubleValue (key::a4Hz, d.a4Hz));
    e.midiChannel   = juce::jlimit (1, 16, props.getIntValue (key::midiChannel, d.midiChannel));

    // Versions before 1.3 read this as a whole number and fall back to 2 for the small ranges.
    e.bendRangeSemis = kBendRanges[bendRangeIndex ((float) props.getDoubleValue (key::bendRange, d.bendRangeSemis))];

    e.vibratoSmoothing = (float) juce::jlimit (0.0, 1.0, props.getDoubleValue (key::vibratoSmoothing, d.vibratoSmoothing));
    e.keyFilter     = props.getBoolValue (key::keyFilter, d.keyFilter);
    e.keyRoot       = juce::jlimit (0, 11, props.getIntValue (key::keyRoot, d.keyRoot));
    e.scaleMode     = juce::jlimit (0, 6, props.getIntValue (key::scaleMode, d.scaleMode));
    e.pentatonic    = props.getBoolValue (key::pentatonic, d.pentatonic);
    e.pentaSustain  = props.getBoolValue (key::pentaSustain, d.pentaSustain);
    e.triad         = props.getBoolValue (key::triad, d.triad);
    e.triadChannel  = juce::jlimit (1, 16, props.getIntValue (key::triadChannel, d.triadChannel));
    e.triadWithRoot = props.getBoolValue (key::triadWithRoot, d.triadWithRoot);

    s.midiInputName = props.getValue (key::midiInput);
    s.midiMapJson   = props.getValue (key::midiMap);

    s.midiPortName = props.getValue (key::midiPortName, brand::kName).trim();
    if (s.midiPortName.isEmpty())
        s.midiPortName = brand::kName;

    s.hasWindowPosition = props.containsKey (key::windowX) && props.containsKey (key::windowY);
    s.windowX = props.getIntValue (key::windowX);
    s.windowY = props.getIntValue (key::windowY);
    return s;
}

void SettingsStore::save (const AppSettings& s)
{
    const auto& e = s.engine;
    props.setValue (key::bendMode, (int) e.bendMode);
    props.setValue (key::bendRange, (double) e.bendRangeSemis);
    props.setValue (key::vibratoSmoothing, (double) e.vibratoSmoothing);
    props.setValue (key::keyFilter, e.keyFilter);
    props.setValue (key::keyRoot, e.keyRoot);
    props.setValue (key::scaleMode, e.scaleMode);
    props.setValue (key::pentatonic, e.pentatonic);
    props.setValue (key::pentaSustain, e.pentaSustain);
    props.setValue (key::triad, e.triad);
    props.setValue (key::triadChannel, e.triadChannel);
    props.setValue (key::triadWithRoot, e.triadWithRoot);
    props.setValue (key::midiInput, s.midiInputName);
    props.setValue (key::midiMap, s.midiMapJson);
    props.setValue (key::dynamics, (int) e.dynamics);
    props.setValue (key::response, (int) e.response);
    props.setValue (key::lowestNote, e.lowestNote);
    props.setValue (key::gainDb, (double) e.inputGainDb);
    props.setValue (key::gateDb, (double) e.gateDb);
    props.setValue (key::fixedVelocity, e.fixedVelocity);
    props.setValue (key::a4Hz, (double) e.a4Hz);
    props.setValue (key::midiChannel, e.midiChannel);
    props.setValue (key::midiPortName, s.midiPortName);
    props.setValue (key::schema, kSchema);
    props.setValue (key::lastVersion, JUCE_APPLICATION_VERSION_STRING);

    if (s.hasWindowPosition)
    {
        props.setValue (key::windowX, s.windowX);
        props.setValue (key::windowY, s.windowY);
    }
}

std::unique_ptr<juce::XmlElement> SettingsStore::audioState() const
{
    auto state = props.getXmlValue (key::audioState);
    if (state != nullptr)
        state->removeAttribute ("audioDeviceRate"); // never change the rate other programs are using
    return state;
}

void SettingsStore::saveAudioState (const juce::XmlElement& state)
{
    juce::XmlElement copy (state);
    copy.removeAttribute ("audioDeviceRate");
    props.setValue (key::audioState, &copy);
}

void SettingsStore::flush()
{
    props.saveIfNeeded();
}
