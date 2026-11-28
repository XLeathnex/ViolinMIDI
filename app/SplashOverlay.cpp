#include "SplashOverlay.h"

#include "Brand.h"

#include <algorithm>
#include <cmath>

using namespace brand;

namespace
{

    constexpr float kEmblemSide = 220.0f;        // the emblem's own animation
    constexpr float kFrameBoxWidth = 640.0f;     // frames are scaled to fit this box: a 16:9 animation
    constexpr float kFrameBoxHeight = 360.0f;    // (the logo GIF) fills it, a square one gets 360 x 360
    constexpr float kGlowReach = 1.06f;          // the glow's largest radius, in emblem sides
    constexpr float kWordMarkHeight = 40.0f;     // nominal font height of "VioLogic"
    constexpr float kWordMarkBox = 52.0f, kWordMarkGap = 18.0f, kWordMarkRise = 8.0f;
    constexpr float kWordMarkTracking = 0.035f;
    constexpr float kOpticalLift = 24.0f;        // the group sits a little above the centre, where the eye expects it
    constexpr float kProgressWidth = 168.0f, kProgressHeight = 2.0f, kProgressGap = 30.0f;
    constexpr float kProgressBelowInk = 34.0f;   // with frames: from the bottom of their logo, as far as below our word mark
    constexpr float kStatusFromBottom = 70.0f, kStatusHeight = 18.0f;
    constexpr float kVersionFromBottom = 48.0f, kVersionHeight = 16.0f;

    constexpr int kTimerHz = 60;
    constexpr double kNoPaintGraceMs = 1000.0;   // never drawn (a hidden window): the clock starts anyway
    constexpr double kShownGraceSeconds = 0.25;  // onShown comes this long after the reveal even if no picture was drawn
    constexpr double kHoldSeconds = 0.25;        // the finished logo stays at least this long before the fade
    constexpr double kBreathSeconds = 2.4;       // one breath of the glow
    constexpr double kWordMarkFrom = 0.18, kFooterFrom = 0.3, kFramesFadeIn = 0.3;
    constexpr double kProgressSeconds = 0.3;     // the hairline runs to its end once the app is ready
    constexpr float kFadeGrow = 0.035f;          // the logo grows by this much while the overlay fades

    // Keying out the dark background of opaque frames.
    constexpr int kDarkBackground = 56;          // a darker border counts as a background to remove; no background is lighter
    constexpr float kBlackMargin = 4.0f;         // above the background's level, for the JPEG's noise...
    constexpr float kKnee = 24.0f;               // ...and a soft start for the light above it
    constexpr float kPlateBlur = 1.0f / 45.0f;   // the background is smoothed over this share of the shorter side
    constexpr float kFrameEdgeBand = 0.1f;       // the frame's edges fade out over this share of its shorter side
    constexpr int kInkAlpha = 96;                // pixels at least this opaque belong to the logo itself

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

    //==============================================================================
    constexpr float kLift = 0.025f;              // the vignette: the middle this much lighter...
    constexpr float kDarken = 0.30f;             // ...the corners this much darker...
    constexpr float kVignetteInner = 0.45f;      // ...and in between, from here on, neither
    constexpr float kGlowPeak = 0.24f;           // the glow's strongest alpha, at the top of a breath

    /** A different value in -0.5 .. 0.5 for every pixel. Rounding with it turns the
        steps of a dark 8-bit gradient into a fine grain instead of visible rings. */
    float dither (int x, int y)
    {
        auto h = (juce::uint32) x * 374761393u + (juce::uint32) y * 668265263u;
        h = (h ^ (h >> 13)) * 1274126177u;
        h ^= h >> 16;
        return (float) (h & 0xffffu) / 65535.0f - 0.5f;
    }

    juce::uint8 toByte (float value)
    {
        return (juce::uint8) juce::jlimit (0, 255, juce::roundToInt (value));
    }

    void setPixel (const juce::Image::BitmapData& pixels, int x, int y, juce::uint8 a, juce::uint8 r, juce::uint8 g, juce::uint8 b)
    {
        juce::unalignedPointerCast<juce::PixelARGB*> (pixels.getPixelPointer (x, y))->setARGB (a, r, g, b);
    }

    /** The display colour with a very soft vignette. Symmetric, so one quarter is
        worked out and mirrored; each pixel still gets its own grain. */
    juce::Image makeBackground (int width, int height)
    {
        juce::Image image (juce::Image::ARGB, width, height, false);
        const juce::Image::BitmapData pixels (image, juce::Image::BitmapData::writeOnly);

        const float cx = (float) width * 0.5f, cy = (float) height * 0.5f;
        const float reach = juce::jmax (1.0f, std::sqrt (cx * cx + cy * cy));
        const float base[] = { (float) colour::display.getRed(), (float) colour::display.getGreen(), (float) colour::display.getBlue() };

        for (int y = 0; y < (height + 1) / 2; ++y)
        {
            for (int x = 0; x < (width + 1) / 2; ++x)
            {
                const float dx = cx - ((float) x + 0.5f), dy = cy - ((float) y + 0.5f);
                const float d = std::sqrt (dx * dx + dy * dy) / reach;
                const float lift = kLift * (1.0f - easeInOut (d / kVignetteInner));
                const float keep = 1.0f - kDarken * easeInOut ((d - kVignetteInner) / (1.0f - kVignetteInner));

                float shade[3];
                for (int c = 0; c < 3; ++c)
                    shade[c] = (base[c] + (255.0f - base[c]) * lift) * keep;

                for (const auto& p : { juce::Point<int> (x, y), juce::Point<int> (width - 1 - x, y),
                                       juce::Point<int> (x, height - 1 - y), juce::Point<int> (width - 1 - x, height - 1 - y) })
                {
                    const float n = dither (p.x, p.y);
                    setPixel (pixels, p.x, p.y, 255, toByte (shade[0] + n), toByte (shade[1] + n), toByte (shade[2] + n));
                }
            }
        }

        return image;
    }

    /** The glow at the top of a breath: crimson, strongest in the middle, fading
        softly to nothing at the edge of the square. */
    juce::Image makeGlow (int side)
    {
        juce::Image image (juce::Image::ARGB, side, side, true);
        const juce::Image::BitmapData pixels (image, juce::Image::BitmapData::readWrite);

        const float centre = (float) side * 0.5f;
        const float red = (float) colour::crimson.getRed(), green = (float) colour::crimson.getGreen(), blue = (float) colour::crimson.getBlue();

        for (int y = 0; y < side; ++y)
        {
            for (int x = 0; x < side; ++x)
            {
                const float dx = ((float) x + 0.5f - centre) / centre, dy = ((float) y + 0.5f - centre) / centre;
                const float d2 = dx * dx + dy * dy;
                if (d2 >= 1.0f)
                    continue;

                // Bell-shaped, reaching exactly zero at the edge; stored premultiplied.
                const float alpha = juce::jlimit (0.0f, 1.0f, kGlowPeak * std::exp (-3.2f * d2) * (1.0f - d2) + dither (x, y) * (1.5f / 255.0f));
                setPixel (pixels, x, y, toByte (255.0f * alpha), toByte (red * alpha), toByte (green * alpha), toByte (blue * alpha));
            }
        }

        return image;
    }

    /** The scale the screen draws at (2 on a Retina display); the images are made for it. */
    float screenScale (juce::Graphics& g)
    {
        return juce::jlimit (1.0f, 2.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
    }

    //==============================================================================
    /** A pixel of an RGB or ARGB picture (premultiplied: as it looks on black). Read
        straight from the picture's own format, which saves a converted copy of each frame. */
    juce::PixelARGB pixelAt (const juce::Image::BitmapData& pixels, int x, int y)
    {
        const auto* p = pixels.getPixelPointer (x, y);
        if (pixels.pixelFormat == juce::Image::RGB)
        {
            const auto& rgb = *juce::unalignedPointerCast<const juce::PixelRGB*> (p);
            return juce::PixelARGB (255, rgb.getRed(), rgb.getGreen(), rgb.getBlue());
        }

        return *juce::unalignedPointerCast<const juce::PixelARGB*> (p);
    }

    int brightest (const juce::PixelARGB& p)
    {
        return juce::jmax ((int) p.getRed(), (int) p.getGreen(), (int) p.getBlue());
    }

    /** True if a frame sits on an opaque dark background, judged by its border. Not if
        the border is transparent (the frames bring their own alpha) or light. */
    bool hasDarkBackground (const juce::Image& frame)
    {
        const juce::Image::BitmapData pixels (frame, juce::Image::BitmapData::readOnly);
        const int w = frame.getWidth(), h = frame.getHeight();

        std::vector<int> levels;
        int opaque = 0;
        const auto sample = [&] (int x, int y)
        {
            const auto p = pixelAt (pixels, x, y);
            opaque += p.getAlpha() >= 250 ? 1 : 0;
            levels.push_back (brightest (p));
        };

        for (int x = 0; x < w; ++x)
        {
            sample (x, 0);
            sample (x, h - 1);
        }
        for (int y = 1; y < h - 1; ++y)
        {
            sample (0, y);
            sample (w - 1, y);
        }

        if (opaque * 10 < (int) levels.size() * 9)
            return false;

        // The median: the logo may cross the border in a frame, the background fills most of it.
        const auto middle = levels.begin() + (std::ptrdiff_t) (levels.size() / 2);
        std::nth_element (levels.begin(), middle, levels.end());
        return *middle <= kDarkBackground;
    }

    /** Blurs a single-channel picture of w x h with a box of 2 * radius + 1, across and down. */
    void boxBlur (std::vector<float>& values, int w, int h, int radius)
    {
        std::vector<float> line;
        const auto pass = [&] (int count, int length, auto&& at)
        {
            line.resize ((size_t) length);
            for (int i = 0; i < count; ++i)
            {
                for (int j = 0; j < length; ++j)
                    line[(size_t) j] = at (i, j);

                // A running sum over the window; beyond the ends the edge pixel repeats.
                const auto value = [&] (int k) { return line[(size_t) juce::jlimit (0, length - 1, k)]; };
                float sum = 0.0f;
                for (int k = -radius; k <= radius; ++k)
                    sum += value (k);

                for (int j = 0; j < length; ++j)
                {
                    at (i, j) = sum / (float) (2 * radius + 1);
                    sum += value (j + radius + 1) - value (j - radius);
                }
            }
        };

        pass (h, w, [&] (int y, int x) -> float& { return values[(size_t) (y * w + x)]; });
        pass (w, h, [&] (int x, int y) -> float& { return values[(size_t) (y * w + x)]; });
    }

    /** The background behind the logo, pixel by pixel, for frames of `size`:
        the darkest frame (where the logo has not come in yet), never lighter than a
        dark background, and smoothed. The GIF's spotlight is part of it, so it goes
        too; a logo that is there in every frame keeps most of its colour. */
    std::vector<float> backgroundPlate (const std::vector<juce::Image>& frames, juce::Rectangle<int> size)
    {
        const int w = size.getWidth(), h = size.getHeight();

        const juce::Image* darkest = nullptr;
        double least = 0.0;
        for (const auto& frame : frames)
        {
            if (frame.getBounds() != size)
                continue;

            // Every fourth pixel is enough to tell how much logo a frame shows.
            const juce::Image::BitmapData pixels (frame, juce::Image::BitmapData::readOnly);
            double sum = 0.0;
            for (int y = 0; y < h; y += 4)
                for (int x = 0; x < w; x += 4)
                    sum += brightest (pixelAt (pixels, x, y));

            if (darkest == nullptr || sum < least)
            {
                darkest = &frame;
                least = sum;
            }
        }

        std::vector<float> plate ((size_t) (w * h), (float) kDarkBackground);
        if (darkest == nullptr)
            return plate;

        const juce::Image::BitmapData pixels (*darkest, juce::Image::BitmapData::readOnly);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
                plate[(size_t) (y * w + x)] = (float) juce::jmin (kDarkBackground, brightest (pixelAt (pixels, x, y)));

        // Twice, for a soft bell: the JPEG's noise and blocks go, the spotlight's shape stays.
        const int radius = juce::jmax (1, juce::roundToInt ((float) juce::jmin (w, h) * kPlateBlur));
        boxBlur (plate, w, h, radius);
        boxBlur (plate, w, h, radius);
        return plate;
    }

    /** A frame on an opaque dark background, made to sit on the overlay without
        a box: what is no brighter than the background there becomes transparent,
        everything brighter keeps its colour (as if the frame were laid on with
        "screen"), and the edges fade out softly, so a logo entering from outside the
        frame comes in without a line. */
    juce::Image keyedFrame (const juce::Image& frame, const std::vector<float>& plate)
    {
        const int w = frame.getWidth(), h = frame.getHeight();
        juce::Image keyed (juce::Image::ARGB, w, h, false);

        // The edge fade, as the product of a ramp across and a ramp down.
        const float band = juce::jmax (1.0f, kFrameEdgeBand * (float) juce::jmin (w, h));
        const auto ramp = [band] (int i, int n)
        {
            return easeInOut (juce::jmin ((float) i + 0.5f, (float) (n - i) - 0.5f) / band);
        };

        std::vector<float> across ((size_t) w), down ((size_t) h);
        for (int x = 0; x < w; ++x)
            across[(size_t) x] = ramp (x, w);
        for (int y = 0; y < h; ++y)
            down[(size_t) y] = ramp (y, h);

        // Just above the background, light comes in gently (a soft knee), so what is left
        // of the GIF's lighting and its JPEG noise stays invisible; above that, linear.
        const auto aboveBackground = [] (float d)
        {
            return d < kKnee ? d * d / (2.0f * kKnee) : d - 0.5f * kKnee;
        };

        const juce::Image::BitmapData in (frame, juce::Image::BitmapData::readOnly);
        const juce::Image::BitmapData out (keyed, juce::Image::BitmapData::writeOnly);

        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < w; ++x)
            {
                // Stretched back to the full range; premultiplied, so the colour is what is left
                // of the pixel and its brightest channel the opacity.
                const float black = plate[(size_t) (y * w + x)] + kBlackMargin;
                const float gain = 255.0f / (255.0f - black - 0.5f * kKnee) * across[(size_t) x] * down[(size_t) y];
                const auto p = pixelAt (in, x, y);
                const float r = aboveBackground (juce::jmax (0.0f, (float) p.getRed() - black)) * gain;
                const float g = aboveBackground (juce::jmax (0.0f, (float) p.getGreen() - black)) * gain;
                const float b = aboveBackground (juce::jmax (0.0f, (float) p.getBlue() - black)) * gain;
                setPixel (out, x, y, toByte (juce::jmax (r, g, b)), toByte (r), toByte (g), toByte (b));
            }
        }

        return keyed;
    }

    /** Where the logo itself is inside a frame: the box around its clearly visible
        pixels, without a soft background glow. The whole frame if it has none. */
    juce::Rectangle<float> inkBounds (const juce::Image& frame)
    {
        const juce::Image::BitmapData pixels (frame, juce::Image::BitmapData::readOnly);
        const int w = frame.getWidth(), h = frame.getHeight();

        int left = w, right = -1, top = h, bottom = -1;
        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < w; ++x)
            {
                if (pixelAt (pixels, x, y).getAlpha() < kInkAlpha)
                    continue;

                left = juce::jmin (left, x);
                right = juce::jmax (right, x);
                top = juce::jmin (top, y);
                bottom = juce::jmax (bottom, y);
            }
        }

        if (right < left)
            return frame.getBounds().toFloat();

        return juce::Rectangle<int>::leftTopRightBottom (left, top, right + 1, bottom + 1).toFloat();
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

    // No tooltip: it is not a control, and a tip popping up over the logo where the mouse
    // happens to rest at start would spoil it.

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

    // Read as RGB or ARGB below; a mask-only picture becomes ARGB first.
    for (auto& frame : newFrames)
        if (frame.getFormat() != juce::Image::RGB && frame.getFormat() != juce::Image::ARGB)
            frame = frame.convertedToFormat (juce::Image::ARGB);

    // An opaque dark background is judged by the first frame and removed from all of the
    // same size (the frames of one animation are; any other is drawn as it is).
    if (! newFrames.empty() && hasDarkBackground (newFrames.front()))
    {
        const auto size = newFrames.front().getBounds();
        const auto plate = backgroundPlate (newFrames, size);
        for (auto& frame : newFrames)
            if (frame.getBounds() == size)
                frame = keyedFrame (frame, plate);
    }

    frameImages = std::move (newFrames);
    frameInk = frameImages.empty() ? juce::Rectangle<float>() : inkBounds (frameImages.back());
    frameRate = fps > 0.0 ? juce::jlimit (1.0, 60.0, fps) : 25.0;

    // They play from their first picture on, even when they come while the overlay is already up.
    framesFrom = secondsShown();
    if (! shownSent)
        introPainted = false;

    // The time spent here (the keying) is not time spent waiting for the first picture.
    if (startMs <= 0.0)
        createdMs = nowMs();

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
    // Whoever clicks wants the app: no waiting for the animation's end, only for the minimum.
    skipRequested = true;
    finish();
}

double SplashOverlay::secondsShown() const
{
    return startMs > 0.0 ? (nowMs() - startMs) / 1000.0 : 0.0;
}

double SplashOverlay::revealSeconds() const
{
    if (frameImages.empty())
        return kIntroSeconds;

    // The moment the last frame comes up (and the frames' own fade-in is over).
    return framesFrom + juce::jmax (kFramesFadeIn, (double) (frameImages.size() - 1) / frameRate);
}

double SplashOverlay::minimumSeconds() const
{
    return skipRequested ? kMinimumSeconds : juce::jmax (kMinimumSeconds, revealSeconds() + kHoldSeconds);
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
    if (! shownSent && (introPainted || skipRequested || t >= revealSeconds() + kShownGraceSeconds))
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
        const double minimum = minimumSeconds();
        const bool wanted = finishAt >= 0.0 || t >= juce::jmax (kTimeoutSeconds, minimum + 1.0);
        if (! (wanted && shownSent && t >= minimum))
        {
            // While the emblem, the word mark and the footer come in, everything moves.
            // After that only the glow (or the frames, until the last one) and the hairline do.
            if (t < kIntroSeconds + 0.1)
            {
                repaint();
            }
            else
            {
                const auto l = layout();
                if (frameImages.empty())
                    repaint (l.glow.getSmallestIntegerContainer());
                else if (t < revealSeconds() + 0.1)
                    repaint (l.logo.getSmallestIntegerContainer());

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
    Layout l;

    if (frameImages.empty())
    {
        // The emblem with the word mark below it, as one group.
        const float groupH = kEmblemSide + kWordMarkGap + kWordMarkBox;
        const float top = bounds.getCentreY() - groupH * 0.5f - kOpticalLift;

        l.logo = juce::Rectangle<float> (kEmblemSide, kEmblemSide).withCentre ({ bounds.getCentreX(), top + kEmblemSide * 0.5f });
        l.wordMark = { bounds.getX(), l.logo.getBottom() + kWordMarkGap, bounds.getWidth(), kWordMarkBox };
        l.progress = juce::Rectangle<float> (kProgressWidth, kProgressHeight)
                         .withCentre ({ bounds.getCentreX(), l.wordMark.getBottom() + kProgressGap });
    }
    else
    {
        // The frames, fitted into their box. What is placed is the logo inside them (with
        // its word mark), so it sits where the emblem and our word mark would, whatever
        // margins the animation has around it.
        const auto& first = frameImages.front();
        const float frameW = (float) first.getWidth(), frameH = (float) first.getHeight();
        const float scale = juce::jmin (kFrameBoxWidth / frameW, kFrameBoxHeight / frameH);
        const auto ink = frameInk * scale;
        const float top = bounds.getCentreY() - ink.getHeight() * 0.5f - kOpticalLift;

        l.logo = { bounds.getCentreX() - ink.getCentreX(), top - ink.getY(), frameW * scale, frameH * scale };
        l.progress = juce::Rectangle<float> (kProgressWidth, kProgressHeight)
                         .withCentre ({ bounds.getCentreX(), top + ink.getHeight() + kProgressBelowInk });
    }

    const float glowSide = 2.0f * kEmblemSide * kGlowReach + 4.0f;
    l.glow = juce::Rectangle<float> (glowSide, glowSide).withCentre (l.logo.getCentre());
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
    if (frameImages.empty())
        paintWordMark (g, l, t);   // the frames bring their own
    paintFooter (g, l, t);

    if (t >= revealSeconds())
        introPainted = true;
}

void SplashOverlay::paintBackground (juce::Graphics& g)
{
    const float scale = screenScale (g);
    const int w = juce::roundToInt ((float) getWidth() * scale), h = juce::roundToInt ((float) getHeight() * scale);
    if (w <= 0 || h <= 0)
        return;

    if (background.getWidth() != w || background.getHeight() != h)
        background = makeBackground (w, h);

    g.drawImage (background, getLocalBounds().toFloat());
}

void SplashOverlay::paintGlow (juce::Graphics& g, const Layout& l, double t)
{
    // Breathes in and out slowly; it begins with the emblem, at its quietest.
    const float appear = easeOut (phase (t, 0.0, kIntroSeconds));
    const float breath = 0.5f - 0.5f * (float) std::cos (juce::MathConstants<double>::twoPi * t / kBreathSeconds);
    const float alpha = appear * (0.15f + 0.09f * breath);   // at most kGlowPeak
    if (alpha <= 0.0f)
        return;

    const int side = juce::roundToInt (2.0f * kEmblemSide * kGlowReach * screenScale (g));
    if (glowSprite.getWidth() != side)
        glowSprite = makeGlow (side);

    const float radius = kEmblemSide * (kGlowReach - 0.12f + 0.12f * breath);

    juce::Graphics::ScopedSaveState state (g);
    g.setOpacity (alpha / kGlowPeak);
    g.drawImage (glowSprite, juce::Rectangle<float> (2.0f * radius, 2.0f * radius).withCentre (l.logo.getCentre()));
}

void SplashOverlay::paintLogo (juce::Graphics& g, const Layout& l, double t) const
{
    // While the overlay fades, the logo comes a little closer.
    const float out = fadeStartMs > 0.0 ? (float) juce::jlimit (0.0, 1.0, (nowMs() - fadeStartMs) / (kFadeSeconds * 1000.0)) : 0.0f;
    const float grow = 1.0f + kFadeGrow * out * out;

    if (! frameImages.empty())
    {
        // The frames bring their own animation: they only fade in, play once, and the
        // last one stays (a logo reveal started again would look like a hiccup).
        const auto last = (long long) frameImages.size() - 1;
        const auto step = juce::jlimit (0LL, last, (long long) std::floor ((t - framesFrom) * frameRate));
        const auto& frame = frameImages[(size_t) step];
        const auto area = l.logo.withSizeKeepingCentre (l.logo.getWidth() * grow, l.logo.getHeight() * grow);

        juce::Graphics::ScopedSaveState state (g);
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.setOpacity (easeOut (phase (t, framesFrom, framesFrom + kFramesFadeIn)));
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
