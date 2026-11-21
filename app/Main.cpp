// ViolinMIDI - plays virtual instruments from the pitch of a violin.
//
// The app listens to an audio input, follows the pitch of the violin and
// sends it as MIDI through a virtual port named "ViolinMIDI".

#include "MainComponent.h"
#include "SelfTest.h"

class ViolinMidiApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override       { return JUCE_APPLICATION_NAME_STRING; }
    const juce::String getApplicationVersion() override    { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override             { return false; } // two would fight over the MIDI port

    void initialise (const juce::String& commandLine) override
    {
        if (commandLine.contains ("--selftest"))
        {
            selfTest = std::make_unique<SelfTest> ([this] (int code)
            {
                setApplicationReturnValue (code);
                quit();
            });
            selfTest->start();
            return;
        }

        // "--no-audio" shows the window without opening an input (for looking at the layout).
        mainWindow = std::make_unique<MainWindow> (getApplicationName(), ! commandLine.contains ("--no-audio"));
    }

    void shutdown() override
    {
        mainWindow = nullptr;
        selfTest = nullptr;
    }

    void systemRequestedQuit() override                     { quit(); }
    void anotherInstanceStarted (const juce::String&) override {}

private:
    class MainWindow final : public juce::DocumentWindow
    {
    public:
        MainWindow (const juce::String& name, bool openAudio)
            : DocumentWindow (name,
                              juce::Desktop::getInstance().getDefaultLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId),
                              juce::DocumentWindow::closeButton | juce::DocumentWindow::minimiseButton)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new MainComponent (openAudio), true);
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
