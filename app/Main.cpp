// ViolinMIDI - plays virtual instruments from the pitch of a violin.
//
// The app listens to an audio input, follows the pitch of the violin and
// sends it as MIDI through a virtual port named "ViolinMIDI".

#include "MainComponent.h"
#include "SelfTest.h"
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
}

class ViolinMidiApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override       { return JUCE_APPLICATION_NAME_STRING; }
    const juce::String getApplicationVersion() override    { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override             { return false; } // two would fight over the MIDI port

    void initialise (const juce::String&) override
    {
        const auto arguments = getCommandLineParameterArray();

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
        // opens the settings window as well (both for looking at the layout).
        mainWindow = std::make_unique<MainWindow> (getApplicationName() + " " + updater::runningVersion(),
                                                   ! arguments.contains ("--no-audio"),
                                                   startup.notice,
                                                   arguments.contains ("--show-settings"));
        updater::startedSuccessfully();
    }

    void shutdown() override
    {
        mainWindow = nullptr;
        selfTest = nullptr;
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
        MainWindow (const juce::String& name, bool openAudio, const juce::String& notice, bool showSettings)
            : DocumentWindow (name,
                              juce::Desktop::getInstance().getDefaultLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId),
                              juce::DocumentWindow::closeButton | juce::DocumentWindow::minimiseButton)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new MainComponent (openAudio, notice, showSettings), true);
            setResizable (false, false);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }

        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

    std::unique_ptr<MainWindow> mainWindow;
    std::unique_ptr<SelfTest> selfTest;
};

START_JUCE_APPLICATION (ViolinMidiApplication)
