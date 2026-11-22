#include "DescribedMenu.h"

#include "Brand.h"

#include <cmath>

namespace
{
    constexpr int kMaxWidth = 380;
    constexpr int kTopPad = 6, kBottomPad = 6;
    constexpr int kNameLine = 16, kDescLine = 14;
    constexpr int kMaxDescLines = 2;

    // Plain rows (LookAndFeel_V4::drawPopupMenuItem) start their text after
    // 1 px frame + 5 px inset + a tick column as wide as the row height / 1.3.
    // For the usual 28 px combo box that column is 20 px. Described rows use
    // the same left edge so both kinds line up.
    constexpr int kInset = 6, kTickColumn = 20;
    constexpr int kTextLeft = kInset + kTickColumn;
    constexpr int kTextRight = 9;

    constexpr float kNameSize = 13.0f, kDescSize = 11.5f;

    /** The wrapped description at a given width. */
    juce::TextLayout layoutDescription (const juce::String& description, float width)
    {
        juce::AttributedString s;
        s.setWordWrap (juce::AttributedString::byWord);
        s.append (description, brand::font (kDescSize), brand::colour::secondary);

        juce::TextLayout layout;
        layout.createLayout (s, width);
        return layout;
    }

    class DescribedRow final : public juce::PopupMenu::CustomComponent
    {
    public:
        DescribedRow (const juce::String& n, const juce::String& d)
            : juce::PopupMenu::CustomComponent (true), name (n), description (d) {}

        void getIdealSize (int& idealWidth, int& idealHeight) override
        {
            const float nameWidth = juce::GlyphArrangement::getStringWidth (brand::font (kNameSize), name);
            const float descWidth = juce::GlyphArrangement::getStringWidth (brand::font (kDescSize), description);

            idealWidth = juce::jmin (kMaxWidth, (int) std::ceil (juce::jmax (nameWidth, descWidth)) + kTextLeft + kTextRight);

            const auto layout = layoutDescription (description, (float) (idealWidth - kTextLeft - kTextRight));
            const int lines = juce::jlimit (1, kMaxDescLines, layout.getNumLines());
            idealHeight = kTopPad + kNameLine + lines * kDescLine + kBottomPad;
        }

        void paint (juce::Graphics& g) override
        {
            const bool enabled = getItem() == nullptr || getItem()->isEnabled;
            const bool ticked = getItem() != nullptr && getItem()->isTicked;
            const float alpha = enabled ? 1.0f : 0.5f;

            if (isItemHighlighted() && enabled)
            {
                g.setColour (findColour (juce::PopupMenu::highlightedBackgroundColourId));
                g.fillRect (getLocalBounds().reduced (1));
            }

            const int textWidth = getWidth() - kTextLeft - kTextRight;
            const juce::Rectangle<int> nameArea (kTextLeft, kTopPad, textWidth, kNameLine);

            if (ticked)
            {
                // the tick of a plain row, sized to the name line
                auto tick = getLookAndFeel().getTickShape (1.0f);
                const juce::Rectangle<float> area ((float) (kInset + kTickColumn / 5), (float) kTopPad,
                                                   (float) (kTickColumn - 2 * (kTickColumn / 5)), (float) kNameLine);
                g.setColour (brand::colour::text.withMultipliedAlpha (alpha));
                g.fillPath (tick, tick.getTransformToScaleToFit (area, true));
            }

            g.setColour (brand::colour::text.withMultipliedAlpha (alpha));
            g.setFont (brand::font (kNameSize));
            g.drawText (name, nameArea, juce::Justification::centredLeft, true);

            const juce::Rectangle<int> descArea (kTextLeft, kTopPad + kNameLine, textWidth, kMaxDescLines * kDescLine);
            g.setColour (brand::colour::secondary.withMultipliedAlpha (alpha));
            g.setFont (brand::font (kDescSize));
            g.drawFittedText (description, descArea, juce::Justification::topLeft, kMaxDescLines, 1.0f);
        }

    private:
        juce::String name, description;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DescribedRow)
    };
}

void addDescribedItem (juce::ComboBox& box, int itemId, const juce::String& name, const juce::String& description)
{
    // The combo box takes the shown text and the selection from itemID and text,
    // so both are set even though the custom component draws the row.
    juce::PopupMenu::Item item (name);
    item.itemID = itemId;
    item.customComponent = new DescribedRow (name, description);

    box.getRootMenu()->addItem (std::move (item));
}
