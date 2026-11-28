#include "PadHud.h"

#include "Brand.h"

#include <cmath>

namespace
{
    // Fading: in quickly, so it is there the moment the values move; out
    // slowly and only after a pause, so a short stop in a sweep does not
    // make it blink.
    constexpr double kFadeInMs = 120.0, kHoldMs = 400.0, kFadeOutMs = 500.0;
    constexpr int kFadeHz = 60;

    // Box: padding, the pad square, a gap, one line of text.
    constexpr float kInset = 8.0f, kGap = 4.0f, kTextH = 15.0f;
    constexpr float kSmallestSide = 32.0f;   // below this the pad would be unreadable

    constexpr float kBoxAlpha = 0.80f;       // translucent, the graph or picture shows through a little

    juce::String U (const char* utf8) { return juce::String::fromUTF8 (utf8); }

    /** 0..1 in, 0..1 out, gentle at both ends. */
    float eased (float t) noexcept
    {
        const float c = juce::jlimit (0.0f, 1.0f, t);
        return c * c * (3.0f - 2.0f * c);
    }

    juce::String percent (float v)
    {
        return juce::String (juce::roundToInt (juce::jlimit (0.0f, 1.0f, v) * 100.0f)) + " %";
    }
}

//==============================================================================
PadHud::PadHud()
{
    setInterceptsMouseClicks (false, false);
    setTooltip (U ("Schnecken-Pad: Der rote Punkt zeigt, wo die beiden Werte gerade stehen, seitlich (X) und "
                   "auf und ab (Y), unten in Prozent. Die Anzeige erscheint, sobald die Schnecke das Totzonen-Oval "
                   "verl\xc3\xa4sst und die Werte wandern, und blendet wieder aus, wenn die Geige ruht."));
}

void PadHud::setState (float x, float y, bool active)
{
    const float nx = juce::jlimit (0.0f, 1.0f, x), ny = juce::jlimit (0.0f, 1.0f, y);
    const bool moved = std::abs (nx - valueX) > 0.0005f || std::abs (ny - valueY) > 0.0005f;
    valueX = nx;
    valueY = ny;

    if (active != isActive)
    {
        isActive = active;
        if (! isActive)
            releasedAt = juce::Time::getMillisecondCounterHiRes();
    }

    const bool settled = isActive ? level >= 1.0f : level <= 0.0f;
    if (! settled)
        startFading();

    if (moved && level > 0.0f)
        repaintBox();
}

void PadHud::setAxisNames (const juce::String& x, const juce::String& y)
{
    if (x == nameX && y == nameY)
        return;

    nameX = x;
    nameY = y;
    if (level > 0.0f)
        repaintBox();
}

//==============================================================================
void PadHud::startFading()
{
    if (isTimerRunning())
        return;

    lastTick = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (kFadeHz);
}

void PadHud::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const double step = juce::jmax (0.0, now - lastTick);
    lastTick = now;

    const float before = level;
    if (isActive)
        level = juce::jmin (1.0f, level + (float) (step / kFadeInMs));
    else if (now - releasedAt >= kHoldMs)
        level = juce::jmax (0.0f, level - (float) (step / kFadeOutMs));

    // Clicks (and with them the tooltip) only while it can be seen; hidden, it is not there at all.
    const bool shown = level > 0.0f;
    if (shown != intercepting)
    {
        intercepting = shown;
        setInterceptsMouseClicks (shown, false);
    }

    if (std::abs (level - before) > 0.0f)
        repaintBox();

    if (isActive ? level >= 1.0f : level <= 0.0f)
        stopTimer();
}

//==============================================================================
juce::Rectangle<float> PadHud::boxArea() const
{
    const float w = (float) getWidth(), h = (float) getHeight();
    const float side = std::floor (juce::jmin (juce::jmin ((float) kWidth, w) - 2.0f * kInset,
                                               juce::jmin ((float) kHeight, h) - 2.0f * kInset - kGap - kTextH));
    if (side < kSmallestSide)
        return {};

    const float boxW = side + 2.0f * kInset, boxH = side + 2.0f * kInset + kGap + kTextH;
    return { w - boxW, 0.0f, boxW, boxH };   // top right: the corner the overlays are given
}

juce::Rectangle<float> PadHud::padArea (juce::Rectangle<float> box)
{
    const float side = box.getWidth() - 2.0f * kInset;
    return { box.getX() + kInset, box.getY() + kInset, side, side };
}

void PadHud::repaintBox()
{
    repaint (boxArea().getSmallestIntegerContainer().expanded (1));
}

void PadHud::paint (juce::Graphics& g)
{
    using namespace brand;

    const float alpha = eased (level);
    if (alpha <= 0.0f)
        return;

    const auto box = boxArea();
    if (box.isEmpty())
        return;

    // The box: dark and translucent, one quiet hairline.
    g.setColour (colour::display.withAlpha (kBoxAlpha * alpha));
    g.fillRoundedRectangle (box, kControlRadius);
    g.setColour (colour::silver.withAlpha (0.16f * alpha));
    g.drawRoundedRectangle (box.reduced (0.5f), kControlRadius, 1.0f);

    const auto pad = padArea (box);

    // The pad: a faint frame and the crosshair through the middle (both values at 50 %).
    g.setColour (colour::silver.withAlpha (0.13f * alpha));
    g.drawRect (pad, 1.0f);
    g.setColour (colour::silver.withAlpha (0.42f * alpha));
    g.fillRect (pad.getX(), std::round (pad.getCentreY()) - 0.5f, pad.getWidth(), 1.0f);
    g.fillRect (std::round (pad.getCentreX()) - 0.5f, pad.getY(), 1.0f, pad.getHeight());

    // Each axis is named at its growing end: X above the right end of the horizontal line, Y on top of the
    // vertical one. A long name ("Harmonie-Mix") may run across the other line, so each sits on a small plate
    // of the box's colour that interrupts the line under it.
    const auto nameFont = font (10.0f);
    g.setFont (nameFont);
    const float widest = pad.getWidth() - 6.0f;
    const auto label = [&] (const juce::String& name, bool xAxis)
    {
        const float w = juce::jmin (widest, juce::GlyphArrangement::getStringWidth (nameFont, name) + 6.0f);
        const auto r = xAxis ? juce::Rectangle<float> (pad.getRight() - 3.0f - w, pad.getCentreY() - 13.0f, w, 12.0f)
                             : juce::Rectangle<float> (w, 12.0f).withCentre ({ pad.getCentreX(), pad.getY() + 8.0f });
        g.setColour (colour::display.withAlpha (alpha));
        g.fillRoundedRectangle (r, 2.0f);
        g.setColour (colour::secondary.withAlpha (0.9f * alpha));
        g.drawText (name, r, juce::Justification::centred, true);
    };
    label (nameX, true);
    label (nameY, false);

    // The cursor: a crimson dot with a soft ring, kept inside the box.
    const juce::Point<float> c (pad.getX() + valueX * pad.getWidth(), pad.getY() + (1.0f - valueY) * pad.getHeight());
    {
        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (pad.expanded (kInset - 2.0f).getSmallestIntegerContainer());

        g.setColour (colour::crimson.withAlpha (0.16f * alpha));
        g.fillEllipse (juce::Rectangle<float> (20.0f, 20.0f).withCentre (c));
        g.setColour (colour::crimson.withAlpha (0.55f * alpha));
        g.drawEllipse (juce::Rectangle<float> (13.0f, 13.0f).withCentre (c), 1.2f);
        g.setColour (colour::crimson.withAlpha (alpha));
        g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (c));
    }

    // "X 63 %" on the left, "Y 41 %" on the right: the letter in grey, the value in silver.
    const juce::Rectangle<float> line (pad.getX(), pad.getBottom() + kGap, pad.getWidth(), kTextH);
    const auto f = font (11.5f);
    g.setFont (f);

    const juce::String vx = percent (valueX), vy = percent (valueY);
    const float space = juce::GlyphArrangement::getStringWidth (f, " ");

    g.setColour (colour::secondary.withAlpha (alpha));
    g.drawText ("X", line, juce::Justification::centredLeft, false);
    g.setColour (colour::silver.withAlpha (alpha));
    g.drawText (vx, line.withTrimmedLeft (juce::GlyphArrangement::getStringWidth (f, "X") + space),
                juce::Justification::centredLeft, false);

    g.drawText (vy, line, juce::Justification::centredRight, false);
    g.setColour (colour::secondary.withAlpha (alpha));
    g.drawText ("Y", line.withTrimmedRight (juce::GlyphArrangement::getStringWidth (f, vy) + space),
                juce::Justification::centredRight, false);
}
