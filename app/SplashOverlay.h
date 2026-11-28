// The start animation. It covers the whole main window while the audio engine,
// the MIDI ports and the camera come up, then fades away and reveals the app.
//
// Without pictures of its own (setFrames), the emblem animates itself: it fades
// in and grows slightly, a soft crimson glow breathes behind it, the word mark
// rises below it. With the animated logo as a sequence of pictures, that
// sequence plays once instead and its last picture stays until the app is ready.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

class SplashOverlay final : public juce::Component,
                            private juce::Timer
{
public:
    SplashOverlay();
    ~SplashOverlay() override;

    /** The animated logo as single pictures, played instead of the emblem's own
        animation: once, centred, scaled to fit 640 x 360 keeping their aspect (a
        square animation: 360 x 360); the last picture then stays. The pictures
        bring their own word mark, so the overlay draws none while they play.
        Pictures on an opaque dark background (a GIF saved as JPEGs) have that
        background keyed out and their edges softened, so they sit on the overlay
        without a box. Call it before the overlay is first shown; it does the
        keying right away. Empty: the emblem is animated again. */
    void setFrames (std::vector<juce::Image> frames, double framesPerSecond);

    /** The quiet line at the bottom, e.g. "Audio und MIDI werden gestartet ...". */
    void setStatusText (const juce::String&);

    /** The number shown as "Version x.y.z". Taken from the application by default. */
    void setVersion (const juce::String&);

    /** The app is ready. The overlay fades out once it has been on screen for
        kMinimumSeconds and the logo has fully appeared (so it never just flashes
        and the animation is seen to its end), then calls onFinished. Without
        this call it fades out by itself after kTimeoutSeconds. */
    void finish();

    /** True once it has faded out completely. */
    bool isFinished() const noexcept   { return done; }

    /** Called once, from a fresh message, when the logo has fully appeared and
        that picture is on the screen (about kIntroSeconds after the overlay was
        first drawn; with frames, when the last one is shown), or at once when a
        click asks to skip the start screen. The moment for work that blocks the
        message thread, such as opening the audio device: the animation cannot
        move while it runs, and a still picture of the finished logo is then all
        it needs to show. Always comes before onFinished. */
    std::function<void()> onShown;

    /** Called once, from a fresh message, after the overlay has faded out. The
        owner may delete the overlay inside it. */
    std::function<void()> onFinished;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

    static constexpr double kIntroSeconds = 0.8;     // the emblem appears
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
    double revealSeconds() const;    // when the logo has fully appeared, on the animation's clock
    double minimumSeconds() const;   // the earliest moment the fade may begin
    void timerCallback() override;

    void paintBackground (juce::Graphics&);
    void paintGlow (juce::Graphics&, const Layout&, double t);
    void paintLogo (juce::Graphics&, const Layout&, double t) const;
    void paintWordMark (juce::Graphics&, const Layout&, double t) const;
    void paintFooter (juce::Graphics&, const Layout&, double t) const;
    float progressAt (double t) const;

    std::vector<juce::Image> frameImages;
    juce::Rectangle<float> frameInk;   // the finished logo inside the last frame, in its pixels
    double frameRate = 25.0;
    double framesFrom = 0.0;           // seconds on the clock when the frames were given

    // The vignette and the glow, drawn once per size and screen scale with a fine
    // dither: smooth gradients this dark would otherwise show rings.
    juce::Image background, glowSprite;

    juce::String statusText, versionText;

    double createdMs = 0.0;          // when the overlay was made
    double startMs = 0.0;            // when it was first drawn: the animation's clock starts here
    double finishAt = -1.0;          // seconds on the clock when finish() came, -1 before
    double fadeStartMs = 0.0;        // 0 until the fade-out has begun
    bool introPainted = false;       // the finished logo has been drawn once
    bool skipRequested = false;      // a click: no need to wait for the animation's end
    bool shownSent = false, done = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SplashOverlay)
};
