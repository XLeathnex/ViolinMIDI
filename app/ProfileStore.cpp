// The instrument profiles in the settings file (see ProfileStore.h).
#include "ProfileStore.h"

ProfileStore::Opened ProfileStore::open (const juce::String&, vm::Settings&, float&, const juce::String&, const juce::String&)
{
    return {};
}

juce::String ProfileStore::toJson() const
{
    return juce::JSON::toString (stored, true);
}

void ProfileStore::markChanged() noexcept {}

juce::String ProfileStore::cleanName (const juce::String& name)
{
    return name.trim().substring (0, kMaxNameLength);
}

juce::String ProfileStore::firstProfileName()
{
    return juce::String::fromUTF8 ("Meine Geige");
}

juce::StringArray ProfileStore::selfTest()
{
    return {};
}
