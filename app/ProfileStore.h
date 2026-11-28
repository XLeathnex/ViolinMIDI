// The instrument profiles in the settings file (2.1): one JSON text under the key
// "instrumentProfiles", written together with the plain keys in one save, so
// the two can never disagree after a crash. Keys of later versions (at the top
// level, per profile and per value set) are carried over unchanged.
//
// open() is the start-up step: the first start of 2.1 makes one profile from
// the plain keys (the migration from 2.0); later, a difference between the
// active profile and the plain keys means another version changed them - the
// profile wins and the difference is reported for a notice.
//
// Message thread only.
#pragma once

#include <juce_core/juce_core.h>

#include "Profiles.h"

#include <optional>

class ProfileStore
{
public:
    vm::ProfileList list;

    struct Opened
    {
        bool created = false;                          ///< no (readable) profiles: one was made from the plain keys
        std::optional<vm::ProfileValues> outside;      ///< another version changed the plain keys; the profile was kept
        juce::String outsideVersion;                   ///< "2.0.0", from the key lastVersion
        juce::StringArray outsideLabels;               ///< German names of the changed values
    };

    /** Reads the profiles and puts the active one's values into `engine` and `deadZone`, which hold
        the plain keys as loaded. */
    Opened open (const juce::String& json, vm::Settings& engine, float& deadZone,
                 const juce::String& lastVersion, const juce::String& runningVersion);

    /** For the settings file; cached, rebuilt only after a change. */
    juce::String toJson() const;
    /** After list operations done from outside. */
    void markChanged() noexcept;
    /** Non-empty once open() found text it could not read (kept once under "instrumentProfilesUnreadable"). */
    juce::String unreadableText;

    static constexpr int kMaxNameLength = 40;
    /** Trimmed, without line breaks and tabs, at most kMaxNameLength characters. */
    static juce::String cleanName (const juce::String&);
    /** "Meine Geige". */
    static juce::String firstProfileName();

    /** The storage checks of "--selftest" (profiles A.12): empty if all pass, else one line per failure. */
    static juce::StringArray selfTest();

private:
    juce::var stored;
};
