// Which version runs at start-up, and when a downloaded version replaces the
// installed app (2.1: no fallback to an old version). The rules only: the
// updater (Updater.cpp) gathers the facts from disk and the process list, asks
// here, and writes back what the decision says. Plain C++, so every rule is
// tested in violinmidi_tests.
#pragma once

#include <map>
#include <string>

namespace updpolicy
{

constexpr long long kLaunchGraceSeconds = 20;   ///< a hand-over that has not come up yet may still be on its way
constexpr long long kHandOverSeconds    = 60;   ///< a downloaded copy started within this time of a hand-over was handed over
constexpr int       kStrikes            = 2;    ///< consecutive unclean ends of a start before it counts as failed

/** Negative if a is older than b, zero if equal ("2.1" == "2.1.0"), positive if newer. */
int compareVersions (const std::string& a, const std::string& b);

/** The keys of state.json (2.0's six and 2.1's new ones). */
struct State
{
    std::string selected, chosenWithInstalled, installedPath, installedVersion, pending;
    long long   pendingSince = 0;
    std::string chosenWithInstalledId, installedId, launchedVersion, notice;
    long long   launchedAt = 0;
    int         failedStarts = 0;
};

/** started.json: when each version last started well, and when a start of it last failed. */
struct History
{
    std::map<std::string, long long> good, failed;
};

struct Facts
{
    State state;
    History history;
    std::string me, myIdentity;              ///< the running app
    long long now = 0;
    std::map<std::string, bool> downloaded;  ///< version -> its copy is complete in the versions folder
    bool selectedRunning = false;            ///< a process runs from the selected version's bundle
    bool installedExists = true;             ///< (downloaded start) the app at state.installedPath is there
    std::string installedIdentityOnDisk;     ///< (downloaded start) bundleIdentity of that app now
    std::string installedVersionOnDisk;
};

struct Decision
{
    enum class Action { runSelf, handOver, quitNow } action = Action::runSelf;
    std::string target;        ///< handOver: a version, or empty for the installed app
    State state;               ///< write this back if it differs from what was read
    History history;
    std::string notice;        ///< show it (runSelf only)
};

/** The app in the Programs folder starts. */
Decision onInstalledStart (const Facts&);
/** A copy in the versions folder starts (handed over, or started directly from the Dock or Finder). */
Decision onDownloadedStart (const Facts&);

struct PromotionFacts
{
    std::string me, installedVersionOnDisk;
    bool runningDownloadedCopy = false, startedWell = false, installedExists = false,
         installedInsideVersions = false, translocated = false, writable = false, installedRunning = false;
};
enum class Promotion { due, notDue, notWritable, installedRunning };
/** Whether this downloaded copy should replace the installed app now. */
Promotion promotion (const PromotionFacts&);

} // namespace updpolicy
