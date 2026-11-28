// The start animation. It covers the whole main window while the audio engine,
// the MIDI ports and the camera come up, then fades away and reveals the app.
//
// Until the animated logo arrives as a sequence of pictures (setFrames), the
// emblem animates itself: it fades in and grows slightly, a soft crimson glow
// breathes behind it, the word mark rises below it.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

class SplashOverlay final : public juce::Component,
                            public juce::SettableTooltipClient,
                            private juce::Timer
{
public:
    SplashOverlay();
    ~SplashOverlay() override;

    /** The animated logo as single pictures, played in a loop (centred, scaled to
        fit about 360 x 360) instead of the emblem's own animation. The word mark
        stays below it. Empty: the emblem is animated again. */
    void setFrames (std::vector<juce::Image> frames, double framesPerSecond);

    /** The quiet line at the bottom, e.g. "Audio und MIDI werden gestartet ...". */
    void setStatusText (const juce::String&);

    /** The number shown as "Version x.y.z". Taken from the application by default. */
    void setVersion (const juce::String&);

    /** The app is ready. The overlay fades out once it has been on screen for
        kMinimumSeconds (so it never just flashes), then calls onFinished. Without
        this call it fades out by itself after kTimeoutSeconds. */
    void finish();

    /** True once it has faded out completely. */
    bool isFinished() const noexcept   { return done; }

    /** Called once, from a fresh message, when the logo has fully appeared and
        that picture is on the screen (about kIntroSeconds after the overlay was
        first drawn). The moment for work that blocks the message thread, such as
        opening the audio device: the animation cannot move while it runs, and a
        still picture of the finished logo is then all it needs to show. */
    std::function<void()> onShown;

    /** Called once, from a fresh message, after the overlay has faded out. The
        owner may delete the overlay inside it. */
    std::function<void()> onFinished;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

    static constexpr double kIntroSeconds = 0.8;     // the logo appears
    static constexpr double kMinimumSeconds = 1.2;   // never shorter than this on screen
    static constexpr double kTimeoutSeconds = 3.0;   // fades by itself if finish() never comes
    static constexpr double kFadeSeconds = 0.35;     // the fade-out

private:
    /** Where everything goes; derived from the size and whether frames are played. */
    struct Layout
    {
        juce::Rectangle<float> logo;       // the emblem or the frames, at full size
        juce::Rectangle<float> glow;       // what the breathing glow can reach
        juce::Rectangle<float> wordMark;
        juce::Rectangle<float> progress;   // the crimson hairline
        juce::Rectangle<float> status;
        juce::Rectangle<float> version;
    };

    Layout layout() const;
    double secondsShown() const;
    void timerCallback() override;

    void paintBackground (juce::Graphics&) const;
    void paintGlow (juce::Graphics&, const Layout&, double t) const;
    void paintLogo (juce::Graphics&, const Layout&, double t) const;
    void paintWordMark (juce::Graphics&, const Layout&, double t) const;
    void paintFooter (juce::Graphics&, const Layout&, double t) const;
    float progressAt (double t) const;

    std::vector<juce::Image> frameImages;
    double frameRate = 25.0;

    juce::String statusText, versionText;

    double createdMs = 0.0;          // when the overlay was made
    double startMs = 0.0;            // when it was first drawn: the animation's clock starts here
    double finishAt = -1.0;          // seconds on the clock when finish() came, -1 before
    double fadeStartMs = 0.0;        // 0 until the fade-out has begun
    bool introPainted = false;       // the finished logo has been drawn once
    bool shownSent = false, done = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SplashOverlay)
};
