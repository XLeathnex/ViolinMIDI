// The window "Ueber VioLogic" (About): the emblem large, the word mark, the version, what
// the app does and its earlier names, the licence and the project's address.
// Calm and centred, like the About window of any Mac app.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class AboutPanel final : public juce::Component,
                         public juce::TooltipClient,
                         private juce::ComponentListener
{
public:
    /** `version` as "x.y.z"; the panel writes "Version x.y.z". */
    explicit AboutPanel (const juce::String& version);
    ~AboutPanel() override;

    /** Opens the window (not modal: the main window stays usable), centred on
        `centreAround`, or on the screen if that is nullptr. Only one at a time:
        if it is already open, it comes to the front. Closing it (close button or
        Escape) deletes it. */
    static void show (const juce::String& version, juce::Component* centreAround);

    /** Deletes the window if it is open. For the app's shutdown(). */
    static void close();

    void paint (juce::Graphics&) override;
    void resized() override;
    void parentHierarchyChanged() override;

    /** Explains the part under the mouse: the app, the version, the names, the licence. */
    juce::String getTooltip() override;

    static constexpr int kWidth = 420, kHeight = 366;

private:
    /** The project address in crimson, underlined while the mouse is over it. */
    struct Link final : public juce::HyperlinkButton
    {
        Link();
        void paintButton (juce::Graphics&, bool highlighted, bool down) override;
        int textWidth() const;
    };

    /** The window this panel sits in: once it is hidden, it is deleted. */
    void componentVisibilityChanged (juce::Component&) override;

    juce::String versionText;
    Link link;
    juce::Component::SafePointer<juce::Component> watchedWindow;

    // A window of its own, so it brings its own tooltips.
    juce::TooltipWindow tooltips { this, 700 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AboutPanel)
};
