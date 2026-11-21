#include "Updater.h"

#include <juce_cryptography/juce_cryptography.h>

#include <cstdlib>

#if JUCE_WINDOWS
 #include <process.h>
#else
 #include <unistd.h>
#endif

namespace updater
{

namespace
{
    constexpr const char* kRepository = "XLeathnex/ViolinMIDI";
    constexpr const char* kAssetName  = "ViolinMIDI-macOS.zip";
    constexpr const char* kBundleId   = "app.violinmidi.ViolinMIDI";
    constexpr const char* kBundleName = "ViolinMIDI.app";

    // A start that has not reported back within this time counts as failed.
    constexpr juce::int64 kPendingGraceSeconds = 20;

    // Downloaded versions are app bundles started with the system's own tools,
    // which only exist on the Mac. Everything still compiles everywhere.
    bool supported() noexcept
    {
       #if JUCE_MAC
        return true;
       #else
        return false;
       #endif
    }

    int processId()
    {
       #if JUCE_WINDOWS
        return (int) _getpid();
       #else
        return (int) getpid();
       #endif
    }

    juce::String text (const char* utf8) { return juce::String::fromUTF8 (utf8); }

    juce::File infoPlist (const juce::File& bundle)    { return bundle.getChildFile ("Contents/Info.plist"); }
    juce::File executableIn (const juce::File& bundle) { return bundle.getChildFile ("Contents/MacOS/ViolinMIDI"); }
    juce::File stateFile()                              { return versionsFolder().getChildFile ("state.json"); }
    juce::int64 nowSeconds()                            { return juce::Time::currentTimeMillis() / 1000; }

    /** Reads one string value from an Info.plist. */
    juce::String plistValue (const juce::File& plist, const juce::String& key)
    {
        if (auto xml = juce::parseXML (plist))
            if (auto* dict = xml->getChildByName ("dict"))
                for (auto* e = dict->getFirstChildElement(); e != nullptr; e = e->getNextElement())
                    if (e->hasTagName ("key") && e->getAllSubText().trim() == key)
                        if (auto* value = e->getNextElement())
                            return value->getAllSubText().trim();
        return {};
    }

    juce::String shellQuote (const juce::String& s)
    {
        return "'" + s.replace ("'", "'\\''") + "'";
    }

    /** Runs a system tool and waits for it. True if it reports success. */
    bool runTool (const juce::String& tool, const juce::StringArray& parameters)
    {
        juce::StringArray arguments;
        arguments.add (tool);
        arguments.addArray (parameters);

        juce::ChildProcess process;
        if (! process.start (arguments))
            return false;

        process.readAllProcessOutput(); // drain, so the tool can not block on a full pipe
        if (! process.waitForProcessToFinish (120000))
        {
            process.kill();
            return false;
        }
        return process.getExitCode() == 0;
    }

    /** Starts an app bundle once this process has ended. Two copies running
        at the same moment would fight over the MIDI port and the single-instance lock. */
    bool launchAfterExit (const juce::File& bundle, const juce::StringArray& commandLine)
    {
        if (! supported())
            return false;

        juce::String command;
        command << "(while kill -0 " << processId() << " 2>/dev/null; do /bin/sleep 0.2; done; "
                << "/usr/bin/open -n " << shellQuote (bundle.getFullPathName());

        if (! commandLine.isEmpty())
        {
            command << " --args";
            for (const auto& argument : commandLine)
                command << " " << shellQuote (argument);
        }

        command << ") >/dev/null 2>&1 &";
        return std::system (command.toRawUTF8()) == 0;
    }
}

//==============================================================================
bool isValidVersion (const juce::String& version)
{
    // Version names become folder names, so nothing but digits and dots gets through.
    return version.isNotEmpty()
        && version.length() <= 24
        && version.containsOnly ("0123456789.")
        && juce::CharacterFunctions::isDigit (version[0])
        && ! version.contains ("..")
        && ! version.endsWithChar ('.');
}

int compareVersions (const juce::String& a, const juce::String& b)
{
    const auto partsA = juce::StringArray::fromTokens (a, ".", "");
    const auto partsB = juce::StringArray::fromTokens (b, ".", "");

    for (int i = 0; i < juce::jmax (partsA.size(), partsB.size()); ++i)
    {
        const int x = i < partsA.size() ? partsA[i].getIntValue() : 0;
        const int y = i < partsB.size() ? partsB[i].getIntValue() : 0;
        if (x != y)
            return x < y ? -1 : 1;
    }
    return 0;
}

juce::File runningBundle()
{
    return juce::File::getSpecialLocation (juce::File::currentApplicationFile);
}

juce::String runningVersion()
{
    // On the Mac the bundle says which version it is; elsewhere there is no bundle.
    const auto fromBundle = plistValue (infoPlist (runningBundle()), "CFBundleShortVersionString");
    if (isValidVersion (fromBundle))
        return fromBundle;

    return JUCE_APPLICATION_VERSION_STRING;
}

juce::File versionsFolder()
{
    const auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
   #if JUCE_MAC
    return base.getChildFile ("Application Support/ViolinMIDI/Versions");
   #else
    return base.getChildFile ("ViolinMIDI/Versions");
   #endif
}

juce::File bundleFor (const juce::String& version)
{
    return versionsFolder().getChildFile (version).getChildFile (kBundleName);
}

bool isDownloaded (const juce::String& version)
{
    return isValidVersion (version) && executableIn (bundleFor (version)).existsAsFile();
}

bool runningDownloadedCopy()
{
    return runningBundle().isAChildOf (versionsFolder());
}

//==============================================================================
State readState()
{
    State s;
    const auto json = juce::JSON::parse (stateFile());

    if (auto* object = json.getDynamicObject())
    {
        s.selected            = object->getProperty ("selected").toString();
        s.chosenWithInstalled = object->getProperty ("chosenWithInstalled").toString();
        s.installedPath       = object->getProperty ("installedPath").toString();
        s.installedVersion    = object->getProperty ("installedVersion").toString();
        s.pending             = object->getProperty ("pending").toString();
        s.pendingSince        = (juce::int64) object->getProperty ("pendingSince");
    }

    if (! isValidVersion (s.selected)) s.selected.clear();
    if (! isValidVersion (s.pending))  s.pending.clear();
    return s;
}

bool writeState (const State& s)
{
    auto* object = new juce::DynamicObject();
    object->setProperty ("selected", s.selected);
    object->setProperty ("chosenWithInstalled", s.chosenWithInstalled);
    object->setProperty ("installedPath", s.installedPath);
    object->setProperty ("installedVersion", s.installedVersion);
    object->setProperty ("pending", s.pending);
    object->setProperty ("pendingSince", s.pendingSince);
    const juce::var value (object);

    const auto file = stateFile();
    file.getParentDirectory().createDirectory();
    return file.replaceWithText (juce::JSON::toString (value));
}

//==============================================================================
Startup onStartup (const juce::StringArray& commandLine)
{
    Startup result;

    if (! supported() || runningDownloadedCopy())
        return result; // nothing to hand over to, or we are a downloaded version: just run

    // We are the app the user started. Note where it lives, then see whether
    // a different version is meant to run.
    State state = readState();
    const State before = state;
    const auto me = runningVersion();

    state.installedPath = runningBundle().getFullPathName();
    state.installedVersion = me;

    // The installed app was replaced by hand since the choice was made: it wins.
    if (state.selected.isNotEmpty() && state.chosenWithInstalled != me)
        state.selected.clear();

    if (state.selected.isNotEmpty() && (state.selected == me || ! isDownloaded (state.selected)))
        state.selected.clear();

    if (state.selected.isNotEmpty() && state.pending == state.selected)
    {
        if (nowSeconds() - state.pendingSince < kPendingGraceSeconds)
        {
            result.quitNow = true; // that version is being started right now
            return result;
        }

        // It was started before and never came up. Fall back to this app.
        result.notice = "Version " + state.selected + text (" lie\xc3\x9f sich nicht starten. Es l\xc3\xa4uft wieder Version ") + me + ".";
        state.selected.clear();
    }

    if (state.selected.isEmpty())
    {
        state.pending.clear();
        state.pendingSince = 0;
    }
    else
    {
        state.pending = state.selected;
        state.pendingSince = nowSeconds();
    }

    const bool changed = state.selected != before.selected || state.pending != before.pending
                      || state.pendingSince != before.pendingSince
                      || state.installedPath != before.installedPath || state.installedVersion != before.installedVersion;
    if (changed && ! writeState (state) && state.selected.isNotEmpty())
        return result; // can not keep track of the hand-over, so stay with this app

    if (state.selected.isNotEmpty())
    {
        if (launchAfterExit (bundleFor (state.selected), commandLine))
        {
            result.quitNow = true;
        }
        else
        {
            state.selected.clear();
            state.pending.clear();
            state.pendingSince = 0;
            writeState (state);
        }
    }

    return result;
}

void startedSuccessfully()
{
    State state = readState();
    if (state.pending.isNotEmpty())
    {
        state.pending.clear();
        state.pendingSince = 0;
        writeState (state);
    }
}

juce::Result select (const juce::String& version)
{
    if (version.isNotEmpty() && ! isDownloaded (version))
        return juce::Result::fail ("Version " + version + " ist nicht geladen.");

    State state = readState();
    const auto installed = runningDownloadedCopy() ? state.installedVersion : runningVersion();

    state.selected = version == installed ? juce::String() : version;
    state.chosenWithInstalled = installed;
    state.pending.clear();
    state.pendingSince = 0;

    return writeState (state) ? juce::Result::ok()
                              : juce::Result::fail ("Die Auswahl konnte nicht gespeichert werden.");
}

juce::Result relaunchInto (const juce::String& version, const juce::StringArray& commandLine)
{
    State state = readState();
    const auto installed = runningDownloadedCopy() ? state.installedVersion : runningVersion();
    juce::File target;

    if (version.isEmpty() || version == installed)
    {
        target = runningDownloadedCopy() ? juce::File (state.installedPath) : runningBundle();
    }
    else
    {
        target = bundleFor (version);
        state.pending = version;
        state.pendingSince = nowSeconds();
        writeState (state);
    }

    if (! target.exists())
        return juce::Result::fail (text ("Die App wurde nicht gefunden: ") + target.getFullPathName());

    return launchAfterExit (target, commandLine)
               ? juce::Result::ok()
               : juce::Result::fail (text ("Der Neustart lie\xc3\x9f sich nicht ausl\xc3\xb6sen."));
}

//==============================================================================
juce::Result fetchReleases (juce::Array<Release>& releases)
{
    releases.clear();

    const juce::URL url ("https://api.github.com/repos/" + juce::String (kRepository) + "/releases?per_page=100");
    int status = 0;
    auto stream = url.createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                                             .withConnectionTimeoutMs (10000)
                                             .withStatusCode (&status)
                                             .withExtraHeaders ("Accept: application/vnd.github+json\r\nUser-Agent: ViolinMIDI"));
    if (stream == nullptr)
        return juce::Result::fail ("Keine Verbindung zum Update-Server.");

    const auto body = stream->readEntireStreamAsString();
    if (status != 200)
        return juce::Result::fail ("Der Update-Server antwortet mit Fehler " + juce::String (status) + ".");

    juce::var json;
    if (juce::JSON::parse (body, json).failed() || ! json.isArray())
        return juce::Result::fail (text ("Die Versionsliste war nicht lesbar."));

    for (const auto& item : *json.getArray())
    {
        if ((bool) item.getProperty ("draft", false) || (bool) item.getProperty ("prerelease", false))
            continue;

        Release release;
        release.version = item.getProperty ("tag_name", {}).toString().trimCharactersAtStart ("v");
        if (! isValidVersion (release.version))
            continue;

        release.date = item.getProperty ("published_at", {}).toString().substring (0, 10);
        release.notes = item.getProperty ("body", {}).toString().trim();

        if (const auto* assets = item.getProperty ("assets", {}).getArray())
        {
            for (const auto& asset : *assets)
            {
                if (asset.getProperty ("name", {}).toString() != kAssetName)
                    continue;
                release.downloadUrl = asset.getProperty ("browser_download_url", {}).toString();
                release.size = (juce::int64) asset.getProperty ("size", 0);
                const auto digest = asset.getProperty ("digest", {}).toString();
                if (digest.startsWith ("sha256:"))
                    release.sha256 = digest.fromFirstOccurrenceOf (":", false, false).toLowerCase();
            }
        }

        if (release.downloadUrl.startsWith ("https://"))
            releases.add (release);
    }

    std::sort (releases.begin(), releases.end(),
               [] (const Release& a, const Release& b) { return compareVersions (a.version, b.version) > 0; });
    return juce::Result::ok();
}

juce::Result download (const Release& release, const juce::File& destination,
                       const std::function<bool (double)>& progress)
{
    const juce::URL url (release.downloadUrl);
    int status = 0;
    auto stream = url.createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                                             .withConnectionTimeoutMs (15000)
                                             .withStatusCode (&status)
                                             .withNumRedirectsToFollow (8)
                                             .withExtraHeaders ("User-Agent: ViolinMIDI"));
    if (stream == nullptr || status != 200)
        return juce::Result::fail ("Version " + release.version + text (" lie\xc3\x9f sich nicht laden."));

    destination.getParentDirectory().createDirectory();
    destination.deleteFile();
    juce::int64 total = 0;

    {
        juce::FileOutputStream out (destination);
        if (! out.openedOk())
            return juce::Result::fail ("Die Datei konnte nicht geschrieben werden.");

        juce::HeapBlock<char> buffer (65536);
        while (! stream->isExhausted())
        {
            const int count = stream->read (buffer.getData(), 65536);
            if (count < 0)
                return juce::Result::fail (text ("Die Verbindung ist beim Laden abgebrochen."));
            if (count == 0)
                break;
            if (! out.write (buffer.getData(), (size_t) count))
                return juce::Result::fail ("Die Datei konnte nicht geschrieben werden.");

            total += count;
            if (progress != nullptr && ! progress (release.size > 0 ? (double) total / (double) release.size : 0.0))
                return juce::Result::fail ("Abgebrochen.");
        }
        out.flush();
    }

    if (release.size > 0 && total != release.size)
        return juce::Result::fail (text ("Die geladene Datei ist unvollst\xc3\xa4ndig."));

    if (release.sha256.isNotEmpty() && juce::SHA256 (destination).toHexString().toLowerCase() != release.sha256)
        return juce::Result::fail (text ("Die geladene Datei ist besch\xc3\xa4" "digt (Pr\xc3\xbc" "fsumme stimmt nicht)."));

    return juce::Result::ok();
}

juce::Result installZip (const juce::File& zip, const juce::String& expectedVersion, juce::String& installedVersion)
{
    if (! supported())
        return juce::Result::fail (text ("Versionen lassen sich nur auf dem Mac wechseln."));

    const auto folder = versionsFolder();
    folder.createDirectory();

    // Unpack next to the final place, so the last step is a plain rename.
    const auto incoming = folder.getChildFile ("incoming-" + juce::String::toHexString (juce::Random::getSystemRandom().nextInt64()));
    struct Cleanup
    {
        juce::File folderToRemove;
        ~Cleanup() { folderToRemove.deleteRecursively(); }
    } cleanup { incoming };

    if (! incoming.createDirectory())
        return juce::Result::fail ("Der Ordner " + folder.getFullPathName() + text (" ist nicht beschreibbar."));

    if (! runTool ("/usr/bin/ditto", { "-x", "-k", zip.getFullPathName(), incoming.getFullPathName() }))
        return juce::Result::fail (text ("Das Archiv lie\xc3\x9f sich nicht entpacken."));

    const auto bundle = incoming.getChildFile (kBundleName);
    if (! executableIn (bundle).existsAsFile())
        return juce::Result::fail (text ("Das Archiv enth\xc3\xa4lt keine ViolinMIDI-App."));

    if (plistValue (infoPlist (bundle), "CFBundleIdentifier") != kBundleId)
        return juce::Result::fail (text ("Das Archiv enth\xc3\xa4lt eine andere App."));

    const auto version = plistValue (infoPlist (bundle), "CFBundleShortVersionString");
    if (! isValidVersion (version) || (expectedVersion.isNotEmpty() && version != expectedVersion))
        return juce::Result::fail (text ("Das Archiv enth\xc3\xa4lt nicht die erwartete Version."));

    // The copy that is running must stay as it is.
    if (version == runningVersion() && runningDownloadedCopy())
    {
        installedVersion = version;
        return juce::Result::ok();
    }

    runTool ("/usr/bin/xattr", { "-dr", "com.apple.quarantine", bundle.getFullPathName() });

    if (! runTool ("/usr/bin/codesign", { "--verify", "--deep", "--strict", bundle.getFullPathName() }))
        return juce::Result::fail (text ("Die geladene App ist besch\xc3\xa4" "digt (Signatur ung\xc3\xbcltig)."));

    const auto destination = folder.getChildFile (version);
    if (destination.exists() && ! destination.deleteRecursively())
        return juce::Result::fail ("Die vorhandene Kopie von Version " + version + text (" lie\xc3\x9f sich nicht ersetzen."));

    if (! incoming.moveFileTo (destination))
        return juce::Result::fail ("Version " + version + text (" lie\xc3\x9f sich nicht ablegen."));

    installedVersion = version;
    return juce::Result::ok();
}

} // namespace updater
