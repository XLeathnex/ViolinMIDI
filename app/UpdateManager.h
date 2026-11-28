// Runs the slow update steps (looking up versions, downloading, unpacking) on a
// background thread and tells the window what happened.
#pragma once

#include "Updater.h"

#include <atomic>
#include <functional>

class UpdateManager final : public juce::ChangeBroadcaster,
                            private juce::Thread
{
public:
    UpdateManager();
    ~UpdateManager() override;

    /** Looks up the published versions. */
    void check();

    /** 2.1, once the app is up: puts this version into the Programs folder if it is due
        (updater::promoteIfDue), then looks up the versions if `checkForVersions`. */
    void startUp (bool checkForVersions);

    /** 2.1, "Abbrechen": stops a running download or switch; nothing changes (state.json stays). */
    void cancel();

    /** 2.1: a switch with a download is running (the update buttons show "Abbrechen"). */
    bool isDownloading() const noexcept;

    /** Loads the version if necessary, makes it the one to run and restarts into it. */
    void switchTo (const juce::String& version);

    bool isBusy() const noexcept                                    { return busy; }
    const juce::Array<updater::Release>& releases() const noexcept  { return list; }
    bool hasChecked() const noexcept                                { return checked; }

    /** The newest published version, if it is newer than the one running. Otherwise empty. */
    juce::String availableUpdate() const;

    /** One line for the window: what is going on, or what went wrong. */
    juce::String statusText() const;

    void setNotice (const juce::String& text);

    /** Called on the message thread when the app has to quit so the other version can start. */
    std::function<void()> onRestart;

private:
    enum class Job { none, check, switchVersion };

    void run() override;
    void finishCheck (const juce::Array<updater::Release>& found, const juce::Result& result);
    void finishSwitch (const juce::String& version, const juce::Result& result);

    // message thread only
    bool busy = false, checked = false;
    juce::Array<updater::Release> list;
    juce::String message;

    // set before the thread starts, read by the thread
    Job job = Job::none;
    juce::String jobVersion;
    updater::Release jobRelease;
    bool jobNeedsDownload = false;

    std::atomic<int> percent { -1 };

    JUCE_DECLARE_WEAK_REFERENCEABLE (UpdateManager)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UpdateManager)
};
