#include "Brand.h"

namespace brand
{

juce::Font font (float height, bool bold)
{
    return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
}

juce::Font headingFont()
{
    return font (11.0f, true).withExtraKerningFactor (0.08f);
}

void drawMark (juce::Graphics& g, juce::Rectangle<float> a)
{
    const float w = a.getWidth(), h = a.getHeight();
    auto P = [&] (float x, float y) { return juce::Point<float> (a.getX() + x * w, a.getY() + y * h); };

    // f-hole: an integral-shaped stroke with a round eye at each end and the
    // small cross cut in the middle.
    juce::Path stem;
    stem.startNewSubPath (P (0.47f, 0.14f));
    stem.cubicTo (P (0.34f, 0.08f), P (0.31f, 0.30f), P (0.31f, 0.50f));
    stem.cubicTo (P (0.31f, 0.70f), P (0.28f, 0.92f), P (0.15f, 0.86f));

    juce::Path f;
    juce::PathStrokeType (h * 0.055f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded).createStrokedPath (f, stem);

    juce::Path hole;
    hole.addPath (f);
    hole.addEllipse (juce::Rectangle<float> (w * 0.13f, w * 0.13f).withCentre (P (0.47f, 0.15f)));
    hole.addEllipse (juce::Rectangle<float> (w * 0.15f, w * 0.15f).withCentre (P (0.15f, 0.85f)));
    hole.addRoundedRectangle (juce::Rectangle<float> (w * 0.17f, h * 0.04f).withCentre (P (0.31f, 0.50f)), h * 0.02f);

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffffd98f), P (0.3f, 0.05f),
                                             colour::accentDeep, P (0.3f, 0.95f), false));
    g.fillPath (hole);

    // Notes rising out of it: the pitch becoming MIDI.
    struct Bar { float x0, x1, y; float alpha; };
    const Bar bars[] = { { 0.58f, 0.72f, 0.70f, 0.45f },
                         { 0.66f, 0.82f, 0.53f, 0.72f },
                         { 0.76f, 0.95f, 0.36f, 1.00f } };
    for (const auto& b : bars)
    {
        const auto r = juce::Rectangle<float> (P (b.x0, b.y - 0.04f), P (b.x1, b.y + 0.04f));
        g.setColour (colour::accent.withAlpha (b.alpha));
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    }
}

void drawIcon (juce::Graphics& g, juce::Rectangle<float> r)
{
    // macOS app icon grid: the shape is 824 of 1024, with a soft shadow below.
    const float s = juce::jmin (r.getWidth(), r.getHeight());
    const auto body = r.withSizeKeepingCentre (s * 824.0f / 1024.0f, s * 824.0f / 1024.0f)
                       .translated (0.0f, -s * 0.008f);
    const float radius = body.getWidth() * 0.2237f;

    juce::Path shape;
    shape.addRoundedRectangle (body, radius);

    juce::DropShadow (juce::Colours::black.withAlpha (0.5f), juce::jmax (1, (int) (s * 0.028f)),
                      { 0, juce::jmax (1, (int) (s * 0.012f)) }).drawForPath (g, shape);

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff262c34), body.getX(), body.getY(),
                                             juce::Colour (0xff0a0c0f), body.getX(), body.getBottom(), false));
    g.fillPath (shape);

    // warm glow behind the f-hole
    const auto glowCentre = body.getRelativePoint (0.40f, 0.52f);
    g.setGradientFill (juce::ColourGradient (colour::accent.withAlpha (0.20f), glowCentre,
                                             colour::accent.withAlpha (0.0f), glowCentre.translated (body.getWidth() * 0.55f, 0.0f), true));
    g.fillPath (shape);

    // fine rim
    g.setColour (juce::Colours::white.withAlpha (0.09f));
    g.strokePath (shape, juce::PathStrokeType (juce::jmax (1.0f, s * 0.004f)));

    drawMark (g, body.reduced (body.getWidth() * 0.17f));
}

bool renderIconPng (const juce::File& file, int size)
{
    juce::Image image (juce::Image::ARGB, size, size, true);
    {
        juce::Graphics g (image);
        drawIcon (g, juce::Rectangle<float> ((float) size, (float) size));
    }

    file.deleteFile();
    juce::FileOutputStream out (file);
    return out.openedOk() && juce::PNGImageFormat().writeImageToStream (image, out);
}

} // namespace brand
