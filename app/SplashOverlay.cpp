#include "SplashOverlay.h"

#include "Brand.h"

#include <algorithm>
#include <cmath>

using namespace brand;

namespace
{
    juce::String U (const char* utf8) { return juce::String::fromUTF8 (utf8); }

    constexpr float kEmblemSide = 220.0f;        // the emblem's own animation
    constexpr float kFrameBox = 360.0f;          // frames are scaled to fit this square
    constexpr float kGlowReach = 1.06f;          // the glow's largest radius, in emblem sides
    constexpr float kWordMarkHeight = 40.0f;     // nominal font height of "VioLogic"
    constexpr float kWordMarkBox = 52.0f, kWordMarkGap = 18.0f, kWordMarkRise = 8.0f;
    constexpr float kWordMarkTracking = 0.035f;
    constexpr float kOpticalLift = 24.0f;        // the group sits a little above the centre, where the eye expects it
    constexpr float kProgressWidth = 168.0f, kProgressHeight = 2.0f, kProgressGap = 30.0f;
    constexpr float kStatusFromBottom = 70.0f, kStatusHeight = 18.0f;
    constexpr float kVersionFromBottom = 48.0f, kVersionHeight = 16.0f;

    constexpr int kTimerHz = 60;
    constexpr double kNoPaintGraceMs = 1000.0;   // never drawn (a hidden window): the clock starts anyway
    constexpr double kShownFallbackSeconds = SplashOverlay::kIntroSeconds + 0.25;
    constexpr double kBreathSeconds = 2.4;       // one breath of the glow
    constexpr double kWordMarkFrom = 0.18, kFooterFrom = 0.3, kFramesFadeIn = 0.3;
    constexpr double kProgressSeconds = 0.3;     // the hairline runs to its end once the app is ready
    constexpr float kFadeGrow = 0.035f;          // the logo grows by this much while the overlay fades

    double nowMs()   { return juce::Time::getMillisecondCounterHiRes(); }

    /** 0 before `from`, 1 after `to`, linear in between. */
    float phase (double t, double from, double to)
    {
        return (float) juce::jlimit (0.0, 1.0, (t - from) / (to - from));
    }

    /** A quick start and a soft landing, for everything that appears. */
    float easeOut (float x)
    {
        const float r = 1.0f - juce::jlimit (0.0f, 1.0f, x);
        return 1.0f - r * r * r;
    }

    float easeInOut (float x)
    {
        const float c = juce::jlimit (0.0f, 1.0f, x);
        return c * c * (3.0f - 2.0f * c);
    }

    /** How far the hairline has got while the app is still starting. Nobody can
        say how long the audio device takes, so it only approaches the end. */
    float waitingProgress (double t)
    {
        return 0.82f * (1.0f - (float) std::exp (-t / 0.9));
    }
}

//==============================================================================
SplashOverlay::SplashOverlay()
    : createdMs (nowMs())
{
    // It covers the whole window, so nothing behind it needs drawing until it fades.
    setOpaque (true);
    setAlwaysOnTop (true);   // stays above the window's other parts, even ones brought to the front later

    // Clicks end it early; the keyboard stays with the main window (space bar, number keys).
    setInterceptsMouseClicks (true, false);
    setWantsKeyboardFocus (false);
    setMouseClickGrabsKeyboardFocus (false);

    setTooltip (U ("Startbildschirm: VioLogic bereitet Audio-Eingang, MIDI-Ausgang und Kamera vor. "
                   "Ein Klick blendet ihn aus, sobald er kurz zu sehen war."));

    if (auto* app = juce::JUCEApplicationBase::getInstance())
        versionText = app->getApplicationVersion();

    startTimerHz (kTimerHz);
}

SplashOverlay::~SplashOverlay()
{
    stopTimer();
}

//==============================================================================
void SplashOverlay::setFrames (std::vector<juce::Image> newFrames, double fps)
{
    newFrames.erase (std::remove_if (newFrames.begin(), newFrames.end(),
                                     [] (const juce::Image& image) { return ! image.isValid(); }),
                     newFrames.end());
    frameImages = std::move (newFrames);
    frameRate = fps > 0.0 ? juce::jlimit (1.0, 60.0, fps) : 25.0;
    repaint();
}

void SplashOverlay::setStatusText (const juce::String& text)
{
    if (text == statusText)
        return;

    statusText = text;
    repaint (layout().status.expanded (0.0f, 2.0f).getSmallestIntegerContainer());
}

void SplashOverlay::setVersion (const juce::String& version)
{
    if (version == versionText)
        return;

    versionText = version;
    repaint (layout().version.expanded (0.0f, 2.0f).getSmallestIntegerContainer());
}

void SplashOverlay::finish()
{
    if (finishAt < 0.0)
        finishAt = secondsShown();
}

void SplashOverlay::mouseDown (const juce::MouseEvent&)
{
    finish();
}

double SplashOverlay::secondsShown() const
{
    return startMs > 0.0 ? (nowMs() - startMs) / 1000.0 : 0.0;
}

float SplashOverlay::progressAt (double t) const
{
    if (finishAt < 0.0)
        return waitingProgress (t);

    const float from = waitingProgress (finishAt);
    return from + (1.0f - from) * easeOut (phase (t, finishAt, finishAt + kProgressSeconds));
}

//==============================================================================
void SplashOverlay::timerCallback()
{
    const double now = nowMs();

    if (startMs <= 0.0)
    {
        // Not drawn yet: the window is still coming up. The clock starts with the
        // first picture, so the animation is seen from its beginning.
        if (now - createdMs < kNoPaintGraceMs)
            return;
        startMs = now;
    }

    const double t = (now - startMs) / 1000.0;
    juce::Component::SafePointer<SplashOverlay> safeThis (this);

    // The finished logo is on the screen: now the owner may block the message thread.
    // From a fresh message, so this frame has reached the screen first.
    if (! shownSent && (introPainted || t >= kShownFallbackSeconds))
    {
        shownSent = true;
        juce::MessageManager::callAsync ([safeThis]
        {
            if (safeThis != nullptr && safeThis->onShown != nullptr)
            {
                const auto callback = safeThis->onShown;   // a copy: the owner may replace it inside
                callback();
            }
        });
    }

    if (fadeStartMs <= 0.0)
    {
        const bool wanted = finishAt >= 0.0 || t >= kTimeoutSeconds;
        if (! (wanted && shownSent && t >= kMinimumSeconds))
        {
            // While the logo comes in, everything moves. After that only the glow
            // (or the frames) and the hairline do.
            if (t < kIntroSeconds + 0.1)
            {
                repaint();
            }
            else
            {
                const auto l = layout();
                repaint ((frameImages.empty() ? l.glow : l.logo).getSmallestIntegerContainer());
                repaint (l.progress.expanded (1.0f).getSmallestIntegerContainer());
            }
            return;
        }

        if (finishAt < 0.0)
            finishAt = t;   // the hairline runs to its end with the fade
        fadeStartMs = now;
        setInterceptsMouseClicks (false, false);   // the app can be used while the overlay fades
    }

    const float u = (float) ((now - fadeStartMs) / (kFadeSeconds * 1000.0));
    if (u >= 1.0f)
    {
        stopTimer();
        done = true;
        setVisible (false);

        // From a fresh message, so the owner can delete the overlay inside the callback.
        juce::MessageManager::callAsync ([safeThis]
        {
            if (safeThis != nullptr && safeThis->onFinished != nullptr)
            {
                const auto callback = safeThis->onFinished;   // a copy: deleting the overlay deletes the original
                callback();
            }
        });
        return;
    }

    setAlpha (1.0f - easeInOut (u));
    repaint();   // the logo grows while it fades
}

//==============================================================================
SplashOverlay::Layout SplashOverlay::layout() const
{
    const auto bounds = getLocalBounds().toFloat();

    // The logo's box: the emblem's square, or the first frame fitted into the frame box.
    float logoW = kEmblemSide, logoH = kEmblemSide;
    if (! frameImages.empty())
    {
        const auto& first = frameImages.front();
        const float scale = juce::jmin (kFrameBox / (float) first.getWidth(), kFrameBox / (float) first.getHeight());
        logoW = (float) first.getWidth() * scale;
        logoH = (float) first.getHeight() * scale;
    }

    const float groupH = logoH + kWordMarkGap + kWordMarkBox;
    const float top = bounds.getCentreY() - groupH * 0.5f - kOpticalLift;

    Layout l;
    l.logo = juce::Rectangle<float> (logoW, logoH).withCentre ({ bounds.getCentreX(), top + logoH * 0.5f });

    const float glowSide = 2.0f * kEmblemSide * kGlowReach + 4.0f;
    l.glow = juce::Rectangle<float> (glowSide, glowSide).withCentre (l.logo.getCentre());

    l.wordMark = { bounds.getX(), l.logo.getBottom() + kWordMarkGap, bounds.getWidth(), kWordMarkBox };
    l.progress = juce::Rectangle<float> (kProgressWidth, kProgressHeight)
                     .withCentre ({ bounds.getCentreX(), l.wordMark.getBottom() + kProgressGap });
    l.status = { bounds.getX(), bounds.getBottom() - kStatusFromBottom, bounds.getWidth(), kStatusHeight };
    l.version = { bounds.getX(), bounds.getBottom() - kVersionFromBottom, bounds.getWidth(), kVersionHeight };
    return l;
}

void SplashOverlay::paint (juce::Graphics& g)
{
    // The animation's clock starts with the first picture on the screen.
    if (startMs <= 0.0)
        startMs = nowMs();

    const double t = secondsShown();
    const auto l = layout();

    paintBackground (g);
    if (frameImages.empty())
        paintGlow (g, l, t);
    paintLogo (g, l, t);
    paintWordMark (g, l, t);
    paintFooter (g, l, t);

    if (t >= kIntroSeconds)
        introPainted = true;
}

void SplashOverlay::paintBackground (juce::Graphics& g) const
{
    g.fillAll (colour::display);

    // A very soft vignette: the middle a breath lighter, the corners a little darker.
    const auto r = getLocalBounds().toFloat();
    const auto c = r.getCentre();
    const float reach = juce::jmax (1.0f, c.getDistanceFrom (r.getTopLeft()));

    juce::ColourGradient vignette (juce::Colours::white.withAlpha (0.025f), c.x, c.y,
                                   juce::Colours::black.withAlpha (0.30f), c.x + reach, c.y, true);
    vignette.addColour (0.45, juce::Colours::transparentBlack);
    g.setGradientFill (vignette);
    g.fillRect (r);
}

void SplashOverlay::paintGlow (juce::Graphics& g, const Layout& l, double t) const
{
    // Breathes in and out slowly; it begins with the emblem, at its quietest.
    const float appear = easeOut (phase (t, 0.0, kIntroSeconds));
    const float breath = 0.5f - 0.5f * (float) std::cos (juce::MathConstants<double>::twoPi * t / kBreathSeconds);
    const float alpha = appear * (0.15f + 0.09f * breath);
    if (alpha <= 0.0f)
        return;

    const float radius = kEmblemSide * (kGlowReach - 0.12f + 0.12f * breath);
    const auto c = l.logo.getCentre();

    juce::ColourGradient glow (colour::crimson.withAlpha (alpha), c.x, c.y,
                               colour::crimson.withAlpha (0.0f), c.x + radius, c.y, true);
    glow.addColour (0.35, colour::crimson.withAlpha (alpha * 0.7f));
    glow.addColour (0.65, colour::crimson.withAlpha (alpha * 0.25f));
    g.setGradientFill (glow);
    g.fillEllipse (juce::Rectangle<float> (2.0f * radius, 2.0f * radius).withCentre (c));
}

void SplashOverlay::paintLogo (juce::Graphics& g, const Layout& l, double t) const
{
    // While the overlay fades, the logo comes a little closer.
    const float out = fadeStartMs > 0.0 ? (float) juce::jlimit (0.0, 1.0, (nowMs() - fadeStartMs) / (kFadeSeconds * 1000.0)) : 0.0f;
    const float grow = 1.0f + kFadeGrow * out * out;

    if (! frameImages.empty())
    {
        // The frames bring their own animation: they only fade in, then loop.
        const auto step = (long long) std::floor (t * frameRate);
        const auto& frame = frameImages[(size_t) (step % (long long) frameImages.size())];
        const auto area = l.logo.withSizeKeepingCentre (l.logo.getWidth() * grow, l.logo.getHeight() * grow);

        juce::Graphics::ScopedSaveState state (g);
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.setOpacity (easeOut (phase (t, 0.0, kFramesFadeIn)));
        g.drawImage (frame, area, juce::RectanglePlacement::centred);
        return;
    }

    // Fades in while it grows from 92 % to its full size.
    const float appear = easeOut (phase (t, 0.0, kIntroSeconds));
    const float scale = (0.92f + 0.08f * appear) * grow;
    brand::drawEmblem (g, l.logo.withSizeKeepingCentre (l.logo.getWidth() * scale, l.logo.getHeight() * scale), appear);
}

void SplashOverlay::paintWordMark (juce::Graphics& g, const Layout& l, double t) const
{
    // "Vio" in crimson, "Logic" in silver, as the logo writes it; rises a little as it appears.
    const float appear = easeOut (phase (t, kWordMarkFrom, kIntroSeconds));
    if (appear <= 0.0f)
        return;

    const auto wordFont = font (kWordMarkHeight, true).withExtraKerningFactor (kWordMarkTracking);
    juce::AttributedString s;
    s.setJustification (juce::Justification::centred);
    s.append ("Vio", wordFont, colour::crimson.withAlpha (appear));
    s.append ("Logic", wordFont, colour::silver.withAlpha (appear));

    juce::TextLayout text;
    text.createLayout (s, l.wordMark.getWidth());
    text.draw (g, l.wordMark.translated (0.0f, kWordMarkRise * (1.0f - appear)));
}

void SplashOverlay::paintFooter (juce::Graphics& g, const Layout& l, double t) const
{
    const float appear = easeOut (phase (t, kFooterFrom, kIntroSeconds));
    if (appear <= 0.0f)
        return;

    // The hairline under the word mark: a dim track, the crimson part runs towards the end.
    const float radius = kProgressHeight * 0.5f;
    g.setColour (colour::control.withAlpha (appear));
    g.fillRoundedRectangle (l.progress, radius);
    g.setColour (colour::crimson.withAlpha (appear));
    g.fillRoundedRectangle (l.progress.withWidth (l.progress.getWidth() * progressAt (t)), radius);

    g.setFont (font (12.5f));
    g.setColour (colour::secondary.withAlpha (appear));
    g.drawText (statusText, l.status, juce::Justification::centred, false);

    if (versionText.isNotEmpty())
    {
        g.setFont (font (11.5f));
        g.setColour (colour::silver.withAlpha (appear));
        g.drawText ("Version " + versionText, l.version, juce::Justification::centred, false);
    }
}
