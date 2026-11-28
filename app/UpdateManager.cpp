#include "UpdateManager.h"

namespace
{
    juce::String text (const char* utf8) { return juce::String::fromUTF8 (utf8); }

    /** Version of the app the user starts, whichever copy is running right now. */
    juce::String installedVersion()
    {
        return updater::runningDownloadedCopy() ? updater::readState().installedVersion
                                                : updater::runningVersion();
    }
}

UpdateManager::UpdateManager() : juce::Thread ("ViolinMIDI updates")
{
}

UpdateManager::~UpdateManager()
{
    stopThread (5000);
}

//==============================================================================
void UpdateManager::check()
{
    if (busy)
        return;

    stopThread (2000); // the previous run has reported back; make sure it has fully ended
    busy = true;
    job = Job::check;
    percent = -1;
    message.clear();
    sendChangeMessage();
    startThread();
}

void UpdateManager::startUp (bool checkForVersions)
{
    if (checkForVersions)
        check();
}

void UpdateManager::cancel() {}

bool UpdateManager::isDownloading() const noexcept
{
    return false;
}

void UpdateManager::switchTo (const juce::String& version)
{
    if (busy)
        return;

    // Going back to the app in the Programs folder needs no download.
    const bool toInstalled = version.isEmpty() || version == installedVersion();
    jobVersion = toInstalled ? juce::String() : version;
    jobNeedsDownload = ! toInstalled && ! updater::isDownloaded (version);
    jobRelease = {};

    if (jobNeedsDownload)
    {
        for (const auto& release : list)
            if (release.version == version)
                jobRelease = release;

        if (jobRelease.downloadUrl.isEmpty())
        {
            message = "Version " + version + text (" ist nicht verf\xc3\xbcgbar.");
            sendChangeMessage();
            return;
        }
    }

    stopThread (2000);
    busy = true;
    job = Job::switchVersion;
    percent = -1;
    message.clear();
    sendChangeMessage();
    startThread();
}

juce::String UpdateManager::availableUpdate() const
{
    if (list.isEmpty())
        return {};

    const auto& newest = list.getReference (0).version;
    return updater::compareVersions (newest, updater::runningVersion()) > 0 ? newest : juce::String();
}

juce::String UpdateManager::statusText() const
{
    if (! busy)
        return message;

    if (job == Job::check)
        return text ("Suche nach neuen Versionen \xe2\x80\xa6");

    const int p = percent.load();
    if (jobVersion.isEmpty())
        return text ("Wechsle zur installierten Version \xe2\x80\xa6");
    if (p >= 0)
        return "Lade Version " + jobVersion + text (" \xe2\x80\xa6 ") + juce::String (p) + " %";
    return "Bereite Version " + jobVersion + text (" vor \xe2\x80\xa6");
}

void UpdateManager::setNotice (const juce::String& notice)
{
    message = notice;
    sendChangeMessage();
}

//==============================================================================
void UpdateManager::run()
{
    const juce::WeakReference<UpdateManager> weak (this);

    if (job == Job::check)
    {
        juce::Array<updater::Release> found;
        const auto result = updater::fetchReleases (found);

        juce::MessageManager::callAsync ([weak, found, result]
        {
            if (auto* self = weak.get())
                self->finishCheck (found, result);
        });
        return;
    }

    auto result = juce::Result::ok();
    const auto version = jobVersion;

    if (jobNeedsDownload)
    {
        const auto zip = updater::versionsFolder().getChildFile ("download-" + version + ".zip");

        result = updater::download (jobRelease, zip, [this] (double fraction)
        {
            percent = juce::jlimit (0, 100, juce::roundToInt (fraction * 100.0));
            return ! threadShouldExit();
        });

        percent = -1;
        if (result.wasOk())
        {
            juce::String installed;
            result = updater::installZip (zip, version, installed);
        }
        zip.deleteFile();
    }

    juce::MessageManager::callAsync ([weak, version, result]
    {
        if (auto* self = weak.get())
            self->finishSwitch (version, result);
    });
}

void UpdateManager::finishCheck (const juce::Array<updater::Release>& found, const juce::Result& result)
{
    busy = false;
    checked = true;

    if (result.wasOk())
        list = found;
    else
        message = result.getErrorMessage();

    sendChangeMessage();
}

void UpdateManager::finishSwitch (const juce::String& version, const juce::Result& result)
{
    busy = false;
    auto outcome = result;

    if (outcome.wasOk())
        outcome = updater::select (version);
    if (outcome.wasOk())
        outcome = updater::relaunchInto (version, {});

    if (outcome.failed())
    {
        message = outcome.getErrorMessage();
        sendChangeMessage();
        return;
    }

    message = version.isEmpty() ? text ("Starte die installierte Version \xe2\x80\xa6")
                                : "Starte Version " + version + text (" \xe2\x80\xa6");
    sendChangeMessage();

    if (onRestart != nullptr)
        onRestart();
}
