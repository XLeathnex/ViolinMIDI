// Updates and version switching.
//
// The app that sits in the Programs folder is never modified. Every other
// version is downloaded from the project's releases page and unpacked next to
// the app's settings:
//
//     ~/Library/Application Support/ViolinMIDI/Versions/1.2.0/ViolinMIDI.app
//
// A small state file says which version should run. When the installed app
// starts and another version is selected, it starts that copy and quits.
// Nothing is replaced or deleted in place, so an update can not leave a
// half-written app behind, and going back to any version is the same step as
// going forward.
#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "UpdatePolicy.h"

#include <functional>

namespace updater
{

struct Release
{
    juce::String version;      ///< "1.2.0"
    juce::String date;         ///< "2026-11-21"
    juce::String notes;
    juce::String downloadUrl;
    juce::String sha256;       ///< lower-case hex, empty if the server gave none
    juce::int64 size = 0;
};

/** Which version runs, and where the app the user starts lives. */
struct State
{
    juce::String selected;           ///< version to run; empty = the installed app itself
    juce::String chosenWithInstalled;///< version of the installed app when `selected` was chosen
    juce::String installedPath;      ///< the app the user starts (Programs folder)
    juce::String installedVersion;
    juce::String pending;            ///< version that was started and has not reported back yet
    juce::int64 pendingSince = 0;    ///< when that start happened, seconds since 1970
};

struct Startup
{
    bool quitNow = false;      ///< another version has been started in our place
    juce::String notice;       ///< something the user should know, or empty
};

//==============================================================================
bool isValidVersion (const juce::String& version);
/** Negative if a is older than b, zero if equal, positive if newer. */
int compareVersions (const juce::String& a, const juce::String& b);

/** Version of the app that is running (from its own bundle on macOS). */
juce::String runningVersion();
juce::File runningBundle();
/** True if this process runs from the downloaded-versions folder. */
bool runningDownloadedCopy();

juce::File versionsFolder();
juce::File bundleFor (const juce::String& version);
bool isDownloaded (const juce::String& version);

State readState();
bool writeState (const State&);

//==============================================================================
/** Call first thing at start-up. May start the selected version instead. */
Startup onStartup (const juce::StringArray& commandLine);

/** Call once the app is up: confirms that this version starts properly. */
void startedSuccessfully();

/** 2.1: the app quits before its start completed (during the start animation): the pending start
    is cleared without counting as a good one (no promotion), and not as a failed one either. */
void startEndedEarly();

/** 2.1: what identifies an installed app beyond its version: path, version, build, executable
    time and size. */
juce::String bundleIdentity (const juce::File& bundle);

/** 2.1: a process (other than this one) runs from that bundle. False where it cannot be known. */
bool processRunsFrom (const juce::File& bundle);

/** 2.1: a downloaded copy that started well replaces the app in the Programs folder (copied
    beside it, verified, swapped atomically). Only if newer than the installed one and nothing
    runs from it. Blocks for 1-3 s: never on the message thread. `shouldStop` cancels the copy. */
struct PromotionResult
{
    updpolicy::Promotion outcome = updpolicy::Promotion::notDue;
    bool done = false;              ///< due, and the installed app is this version now
    juce::String message;           ///< for the status line, empty if nothing to say
};
PromotionResult promoteIfDue (const std::function<bool()>& shouldStop = nullptr);

/** Chooses the version to run from now on. Empty = the installed app. */
juce::Result select (const juce::String& version);

/** Starts the chosen version as soon as this process has ended. The caller quits. */
juce::Result relaunchInto (const juce::String& version, const juce::StringArray& commandLine);

//==============================================================================
/** Reads the list of published versions, newest first. Blocks. */
juce::Result fetchReleases (juce::Array<Release>& releases);

/** Downloads one version. `progress` gets 0..1 and returns false to cancel. Blocks. */
juce::Result download (const Release& release, const juce::File& destination,
                       const std::function<bool (double)>& progress);

/** 2.1: the whole job of the update button except the restart: download (with progress and
    cancel), install, and only then select. On any failure or cancel nothing is left behind and
    state.json is as it was. Blocks. */
juce::Result prepareSwitch (const Release& release, const std::function<bool (double)>& progress);

/** Unpacks a downloaded version, checks it and puts it into the versions folder.
    @param expectedVersion  version the archive must contain, or empty to accept what is inside
    @param installedVersion receives the version that was installed */
juce::Result installZip (const juce::File& zip, const juce::String& expectedVersion,
                         juce::String& installedVersion);

} // namespace updater
