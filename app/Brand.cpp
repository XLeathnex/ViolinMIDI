#include "Brand.h"

namespace brand
{

namespace
{
    // Layout sizes are nominal heights; the typeface is set in points so the
    // text has the same size whatever the face's own line height is.
    constexpr float kPointsPerHeight = 0.90f;
    constexpr float kHeadingWeight = 600.0f;   // semibold on the weight axis of a variable typeface

    /** How wide a sample line is in this face and how much ink it puts down. */
    struct Sample
    {
        explicit Sample (const juce::Typeface::Ptr& face)
        {
            const juce::Font f (juce::FontOptions (face).withPointHeight (24.0f));
            const juce::String text ("Harmonie Erkennung 440");
            width = juce::GlyphArrangement::getStringWidth (f, text);

            juce::Image image (juce::Image::ARGB, 420, 48, true);
            {
                juce::Graphics g (image);
                g.setColour (juce::Colours::white);
                g.setFont (f);
                g.drawSingleLineText (text, 4, 34);
            }

            const juce::Image::BitmapData pixels (image, juce::Image::BitmapData::readOnly);
            for (int y = 0; y < image.getHeight(); ++y)
                for (int x = 0; x < image.getWidth(); ++x)
                    ink += pixels.getPixelColour (x, y).getFloatAlpha();
        }

        float width = 0.0f;
        double ink = 0.0;
    };

    // The typeface is asked for through the system, not by name, so it cannot
    // quietly turn into another face. A heavier cut for headings comes from
    // the weight axis (San Francisco is a variable typeface) or, failing that,
    // by name - and is used only if it really draws like a heavier cut of the
    // same face: about as wide, clearly but not wildly more ink.
    struct Faces final : public juce::DeletedAtShutdown
    {
        Faces()
        {
            regular = juce::Typeface::findSystemTypeface();

           #if JUCE_WINDOWS
            // Only for looking at the layout on a Windows PC, where that call can come back empty.
            if (regular == nullptr)
                regular = juce::Font (juce::FontOptions ("Segoe UI", "Regular", 13.0f)).getTypefacePtr();
           #endif

            if (regular == nullptr)
                return;

            const Sample plain (regular);
            const auto accept = [this, &plain] (const juce::Typeface::Ptr& face, const juce::String& how)
            {
                if (face == nullptr || plain.ink <= 0.0 || plain.width <= 0.0f)
                    return false;

                const Sample heavy (face);
                const double inkRatio = heavy.ink / plain.ink, widthRatio = (double) heavy.width / (double) plain.width;
                if (inkRatio < 1.06 || inkRatio > 1.9 || widthRatio < 0.97 || widthRatio > 1.2)
                    return false;

                strong = face;
                strongHow = how + ", " + juce::String (inkRatio, 2) + " x the ink, " + juce::String (widthRatio, 2) + " x the width";
                return true;
            };

            const juce::FontFeatureTag weight ("wght");
            if (const auto range = regular->getRangeForVariable (weight); range.has_value() && range->contains (kHeadingWeight))
            {
                const juce::FontVariableSetting setting { weight, kHeadingWeight };
                if (accept (regular->cloneWithVariableSettings (juce::Span<const juce::FontVariableSetting> (&setting, 1)), "weight axis at 600"))
                    return;
            }

            for (const char* style : { "Semibold", "Bold" })
            {
                const juce::Font candidate (juce::FontOptions (regular->getName(), style, 13.0f));
                const auto face = candidate.getTypefacePtr();
                if (face != nullptr && face->getName() == regular->getName() && accept (face, juce::String ("style ") + style))
                    return;
            }
        }

        ~Faces() override { instance = nullptr; }

        static Faces& get()
        {
            if (instance == nullptr)
                instance = new Faces();
            return *instance;
        }

        juce::Typeface::Ptr regular, strong;
        juce::String strongHow;
        static inline Faces* instance = nullptr;
    };
}

juce::Font font (float height, bool bold)
{
    const auto& faces = Faces::get();
    if (faces.regular == nullptr)
        return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));

    // Without a heavier cut that passed the check, headings stay in the regular one.
    const auto& face = bold && faces.strong != nullptr ? faces.strong : faces.regular;
    return juce::Font (juce::FontOptions (face).withPointHeight (height * kPointsPerHeight));
}

juce::Typeface::Ptr systemTypeface()
{
    return Faces::get().regular;
}

juce::String fontDescription()
{
    const auto& faces = Faces::get();
    if (faces.regular == nullptr)
        return "the system typeface was not found, using the default";

    return "regular: " + faces.regular->getName() + " " + faces.regular->getStyle()
         + "; headings: " + (faces.strong != nullptr ? faces.strongHow : juce::String ("no heavier cut passed the check, regular is used"));
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

    juce::Path hole;
    juce::PathStrokeType (h * 0.055f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded).createStrokedPath (hole, stem);
    hole.addEllipse (juce::Rectangle<float> (w * 0.13f, w * 0.13f).withCentre (P (0.47f, 0.15f)));
    hole.addEllipse (juce::Rectangle<float> (w * 0.15f, w * 0.15f).withCentre (P (0.15f, 0.85f)));
    hole.addRoundedRectangle (juce::Rectangle<float> (w * 0.17f, h * 0.04f).withCentre (P (0.31f, 0.50f)), h * 0.02f);

    g.setColour (colour::accent);
    g.fillPath (hole);

    // Notes rising out of it: the pitch becoming MIDI.
    struct Bar { float x0, x1, y; };
    const Bar bars[] = { { 0.58f, 0.72f, 0.70f }, { 0.66f, 0.82f, 0.53f }, { 0.76f, 0.95f, 0.36f } };
    for (const auto& b : bars)
    {
        const auto r = juce::Rectangle<float> (P (b.x0, b.y - 0.04f), P (b.x1, b.y + 0.04f));
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

    juce::DropShadow (juce::Colours::black.withAlpha (0.45f), juce::jmax (1, (int) (s * 0.024f)),
                      { 0, juce::jmax (1, (int) (s * 0.010f)) }).drawForPath (g, shape);

    // the slight top-to-bottom shading every macOS icon has
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2b2b2b), body.getX(), body.getY(),
                                             juce::Colour (0xff181818), body.getX(), body.getBottom(), false));
    g.fillPath (shape);

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
