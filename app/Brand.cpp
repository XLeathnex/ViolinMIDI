#include "Brand.h"

namespace brand
{

namespace
{
    // Layout sizes are nominal heights; the typeface is set in points so the
    // text has the same size whatever the face's own line height is.
    constexpr float kPointsPerHeight = 0.90f;

    // The typeface is asked for through the system, not by name, so it cannot
    // quietly turn into another face. The heavier cut has to be looked up by
    // name; it is used only if the system really answers with the same family.
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

            for (const char* style : { "Semibold", "Bold" })
            {
                const juce::Font candidate (juce::FontOptions (regular->getName(), style, 13.0f));
                const auto face = candidate.getTypefacePtr();
                if (face != nullptr && face->getName() == regular->getName() && face->getStyle().containsIgnoreCase ("bold"))
                {
                    strong = face;
                    break;
                }
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
        static inline Faces* instance = nullptr;
    };
}

juce::Font font (float height, bool bold)
{
    const auto& faces = Faces::get();
    if (faces.regular == nullptr)
        return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));

    if (bold && faces.strong == nullptr)
        return juce::Font (juce::FontOptions (faces.regular).withPointHeight (height * kPointsPerHeight)).boldened();

    return juce::Font (juce::FontOptions (bold ? faces.strong : faces.regular).withPointHeight (height * kPointsPerHeight));
}

juce::Typeface::Ptr systemTypeface()
{
    return Faces::get().regular;
}

juce::String fontDescription()
{
    const auto& faces = Faces::get();
    const auto describe = [] (const juce::Typeface::Ptr& face)
    {
        return face != nullptr ? face->getName() + " " + face->getStyle() : juce::String ("none");
    };
    return "regular: " + describe (faces.regular) + ", headings: " + describe (faces.strong);
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
