// Archet (formerly ViolinMIDI) - plays virtual instruments from the pitch of a violin.
//
// The app listens to an audio input, follows the pitch of the violin and
// sends it as MIDI through a virtual MIDI port.

#include "Brand.h"
#include "MainComponent.h"
#include "SelfTest.h"
#include "Theme.h"
#include "Updater.h"

#include <cstdio>

namespace
{
    void say (const juce::String& text)
    {
        std::printf ("%s\n", text.toRawUTF8());
        std::fflush (stdout);
    }

    /** The word that follows an option on the command line, or empty. */
    juce::String valueAfter (const juce::StringArray& arguments, const char* option)
    {
        const int index = arguments.indexOf (option);
        return index >= 0 && index + 1 < arguments.size() ? arguments[index + 1] : juce::String();
    }

    /** Commands for looking after downloaded versions without the window.
        The build's update test uses them, and they help when something is stuck.
        Returns the exit code, or -1 if the command line asks for none of them. */
    int runMaintenanceCommand (const juce::StringArray& arguments)
    {
        if (arguments.contains ("--render-icon"))
        {
            // "--render-icon file.png [size]": writes the app icon (assets/AppIcon.png comes from here)
            const auto file = juce::File::getCurrentWorkingDirectory().getChildFile (valueAfter (arguments, "--render-icon"));
            const int index = arguments.indexOf ("--render-icon");
            const int size = index + 2 < arguments.size() ? juce::jlimit (16, 2048, arguments[index + 2].getIntValue()) : 1024;
            const bool ok = brand::renderIconPng (file, size);
            say (ok ? "icon written: " + file.getFullPathName() : juce::String ("could not write the icon"));
            return ok ? 0 : 1;
        }

        if (arguments.contains ("--list-versions"))
        {
            juce::Array<updater::Release> releases;
            const auto result = updater::fetchReleases (releases);
            if (result.failed())
            {
                say (result.getErrorMessage());
                return 1;
            }
            say ("running: " + updater::runningVersion());
            for (const auto& release : releases)
                say (release.version + "  " + release.date + "  " + juce::String (release.size) + " bytes  "
                     + (release.sha256.isNotEmpty() ? "sha256 " + release.sha256.substring (0, 16) : juce::String ("no checksum")));
            return 0;
        }

        if (arguments.contains ("--fetch-version"))
        {
            // "--fetch-version 1.2.0 --to file.zip" (or "latest"): the download the update button does
            const auto wanted = valueAfter (arguments, "--fetch-version");
            const auto target = juce::File::getCurrentWorkingDirectory().getChildFile (valueAfter (arguments, "--to"));

            juce::Array<updater::Release> releases;
            auto result = updater::fetchReleases (releases);
            if (result.wasOk() && releases.isEmpty())
                result = juce::Result::fail ("no published versions");

            if (result.wasOk())
            {
                updater::Release chosen;
                for (const auto& release : releases)
                    if (release.version == wanted || (wanted == "latest" && chosen.version.isEmpty()))
                        chosen = release;

                result = chosen.version.isEmpty() ? juce::Result::fail ("version " + wanted + " is not published")
                                                  : updater::download (chosen, target, nullptr);
                if (result.wasOk())
                    say ("fetched " + chosen.version + "  " + juce::String (target.getSize()) + " bytes  "
                         + (chosen.sha256.isNotEmpty() ? "checksum verified" : "no checksum published"));
            }

            if (result.failed())
                say (result.getErrorMessage());
            return result.wasOk() ? 0 : 1;
        }

        if (arguments.contains ("--install-zip"))
        {
            juce::String installed;
            const auto zip = juce::File::getCurrentWorkingDirectory().getChildFile (valueAfter (arguments, "--install-zip"));
            auto result = updater::installZip (zip, {}, installed);
            if (result.wasOk() && arguments.contains ("--select"))
            {
                // the app this command runs from is the installed one
                auto state = updater::readState();
                state.installedPath = updater::runningBundle().getFullPathName();
                state.installedVersion = updater::runningVersion();
                updater::writeState (state);
                result = updater::select (installed);
            }
            say (result.wasOk() ? "installed " + installed : result.getErrorMessage());
            return result.wasOk() ? 0 : 1;
        }

        if (arguments.contains ("--select-version"))
        {
            const auto version = valueAfter (arguments, "--select-version");
            const auto result = updater::select (version == "none" ? juce::String() : version);
            say (result.wasOk() ? "selected " + version : result.getErrorMessage());
            return result.wasOk() ? 0 : 1;
        }

        return -1;
    }

    /** German texts for the standard audio settings. */
    void installTranslations()
    {
        const char* text =
            "language: German\n"
            "countries: de at ch\n"
            "\"Audio device type:\" = \"Treiber:\"\n"
            "\"Input:\" = \"Eingang:\"\n"
            "\"Output:\" = \"Ausgang:\"\n"
            "\"Device:\" = \"Ger\xc3\xa4t:\"\n"
            "\"Sample rate:\" = \"Abtastrate:\"\n"
            "\"Audio buffer size:\" = \"Puffergr\xc3\xb6\xc3\x9f" "e:\"\n"
            "\"Active input channels:\" = \"Aktive Eing\xc3\xa4nge:\"\n"
            "\"(no audio input channels found)\" = \"(keine Eing\xc3\xa4nge gefunden)\"\n"
            "\"Control Panel\" = \"Treiber-Einstellungen\"\n"
            "\"Opens the device's own control panel\" = \"\xc3\x96" "ffnet die Einstellungen des Ger\xc3\xa4ts\"\n"
            "\"Reset Device\" = \"Ger\xc3\xa4t zur\xc3\xbc" "cksetzen\"\n"
            "\"Show advanced settings...\" = \"Erweiterte Einstellungen \xe2\x80\xa6\"\n"
            "\"Error when trying to open audio device!\" = \"Das Audio-Ger\xc3\xa4t lie\xc3\x9f sich nicht \xc3\xb6" "ffnen.\"\n"
            "\"none\" = \"keiner\"\n"
            "\"Test\" = \"Test\"\n"
            "\"Plays a test tone\" = \"Spielt einen Testton\"\n";

        juce::LocalisedStrings::setCurrentMappings (new juce::LocalisedStrings (juce::String::fromUTF8 (text), false));
    }
}

class ViolinMidiApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override       { return brand::kName; }
    const juce::String getApplicationVersion() override    { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override             { return false; } // two would fight over the MIDI port

    void initialise (const juce::String&) override
    {
        const auto arguments = getCommandLineParameterArray();
        juce::LookAndFeel::setDefaultLookAndFeel (&theme);
        installTranslations();

        if (arguments.contains ("--selftest"))
        {
            selfTest = std::make_unique<SelfTest> ([this] (int code) { quitWith (code); });
            selfTest->start();
            return;
        }

        if (const int code = runMaintenanceCommand (arguments); code >= 0)
        {
            quitWith (code);
            return;
        }

        // If another version has been chosen, it is started in our place.
        const auto startup = updater::onStartup (arguments);
        if (startup.quitNow)
        {
            quitWith (0);
            return;
        }

        // "--write-version-to <file>": report which version came up, then leave.
        if (arguments.contains ("--write-version-to"))
        {
            const auto file = juce::File::getCurrentWorkingDirectory().getChildFile (valueAfter (arguments, "--write-version-to"));
            const bool written = file.replaceWithText (updater::runningVersion() + "\n" + updater::runningBundle().getFullPathName() + "\n",
                                                       false, false, "\n");
            updater::startedSuccessfully();
            quitWith (written ? 0 : 1);
            return;
        }

        // "--no-audio" shows the window without opening an input, "--show-settings"
        // opens the settings window, "--demo" plays a melody into the displays
        // (all three for looking at the layout).
        MainComponent::Options options;
        options.openAudio = ! arguments.contains ("--no-audio");
        options.demo = arguments.contains ("--demo");
        options.showSettingsAtStart = arguments.contains ("--show-settings") || arguments.contains ("--show-mapping");
        options.settingsPage = arguments.contains ("--show-mapping") ? 1 : 0;
        options.showVoicingAtStart = arguments.contains ("--show-voicing");
        options.showPanelAtStart = arguments.contains ("--show-tracking") ? 1
                                 : (arguments.contains ("--show-audio") ? 2 : (arguments.contains ("--show-camera") ? 3 : 0));
        options.showMenuAtStart = arguments.contains ("--show-scales") ? 1 : (arguments.contains ("--show-voicings") ? 2 : 0);
        options.notice = startup.notice;
        mainWindow = std::make_unique<MainWindow> (getApplicationName(), options);
        updater::startedSuccessfully();
    }

    void shutdown() override
    {
        mainWindow = nullptr;
        selfTest = nullptr;
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
        juce::LocalisedStrings::setCurrentMappings (nullptr);
    }

    void systemRequestedQuit() override                     { quit(); }
    void anotherInstanceStarted (const juce::String&) override {}

private:
    /** Ends the app from a fresh message, which also works from inside initialise(). */
    void quitWith (int code)
    {
        juce::MessageManager::callAsync ([this, code]
        {
            setApplicationReturnValue (code);
            quit();
        });
    }

    class MainWindow final : public juce::DocumentWindow
    {
    public:
        MainWindow (const juce::String& name, const MainComponent::Options& options)
            : DocumentWindow (name, brand::colour::window,
                              juce::DocumentWindow::closeButton | juce::DocumentWindow::minimiseButton)
        {
            setUsingNativeTitleBar (true);
            auto* content = new MainComponent (options);
            setContentOwned (content, true);
            setResizable (false, false);

            juce::Point<int> topLeft;
            if (content->savedWindowPosition (topLeft))
                setTopLeftPosition (topLeft);
            else
                centreWithSize (getWidth(), getHeight());

            setVisible (true);
        }

        ~MainWindow() override
        {
            // The content is still alive here; it saves the position with the other settings.
            if (auto* content = dynamic_cast<MainComponent*> (getContentComponent()))
                content->rememberWindowPosition (getPosition());
        }

        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

    Theme theme;   // declared first, so it outlives the windows that use it
    std::unique_ptr<MainWindow> mainWindow;
    std::unique_ptr<SelfTest> selfTest;
};

START_JUCE_APPLICATION (ViolinMidiApplication)
