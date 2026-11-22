#include "VoicingPanel.h"

#include "Brand.h"

#include <cstdlib>

namespace
{
    constexpr int kMargin = 16;
    constexpr int kCellW = 11, kGap = 1, kPitch = kCellW + kGap;
    constexpr int kRowWidth = VoicingPanel::kCells * kPitch - kGap;   // 587

    constexpr int kHeadingY = 14, kHeadingH = 20;
    constexpr int kCellsY = 46, kCellsH = 28;          // the row of cells
    constexpr int kCentreExtra = 4;                    // the cell at 0 is this much taller, top and bottom
    constexpr int kTickY = 82;                         // tick marks hang from here
    constexpr int kTickOctave = 9, kTickFifth = 6, kTickThird = 3;
    constexpr int kScaleY = 92, kScaleH = 14;          // numbers under the ticks
    constexpr int kNoteY = 110, kNoteH = 16;           // hover line
    constexpr int kButtonsY = 136, kButtonH = 26;
    constexpr int kTriadW = 88, kClearW = 72, kButtonGap = 8;

    constexpr int kWidth = 2 * kMargin + kRowWidth;    // 619
    constexpr int kHeight = kButtonsY + kButtonH + kHeadingY;   // 176

    juce::String U (const char* utf8) { return juce::String::fromUTF8 (utf8); }

    // The real minus sign; written as a string so digits after it are no hex digits.
    const juce::String& minus()
    {
        static const juce::String m = U ("\xe2\x88\x92");
        return m;
    }

    juce::String signedNumber (int semitones, bool withPlus)
    {
        if (semitones < 0)
            return minus() + juce::String (-semitones);
        return (withPlus && semitones > 0 ? juce::String ("+") : juce::String()) + juce::String (semitones);
    }

    int semitonesOf (int cell) noexcept         { return cell - VoicingPanel::kRange; }
    int classOf (int semitones) noexcept        { return ((semitones % 12) + 12) % 12; }   // 0 .. 11 upwards from 0

    int popCount (uint64_t v) noexcept
    {
        int n = 0;
        for (; v != 0; v &= v - 1)
            ++n;
        return n;
    }

    uint64_t allCells() noexcept { return (uint64_t (1) << VoicingPanel::kCells) - 1; }

    uint64_t cleaned (uint64_t mask)
    {
        mask &= allCells();
        if (popCount (mask) <= VoicingPanel::kMaxVoices)
            return mask;

        // nearest to the played note first; at equal distance the lower one
        uint64_t kept = 0;
        int count = 0;
        for (int distance = 0; distance <= VoicingPanel::kRange && count < VoicingPanel::kMaxVoices; ++distance)
        {
            for (int sign = -1; sign <= 1; sign += 2)
            {
                if (distance == 0 && sign == 1)
                    continue;

                const uint64_t bit = uint64_t (1) << (VoicingPanel::kRange + sign * distance);
                if ((mask & bit) != 0 && count < VoicingPanel::kMaxVoices)
                {
                    kept |= bit;
                    ++count;
                }
            }
        }
        return kept;
    }

    juce::String describe (int s)
    {
        if (s == 0)
            return "0 (gespielter Ton)";

        const juce::String unit = (s == 1 || s == -1) ? U (" Halbton") : U (" Halbt\xc3\xb6ne");
        juce::String what;

        if (s == 24)        what = U ("zwei Oktaven h\xc3\xb6her");
        else if (s == -24)  what = U ("zwei Oktaven tiefer");
        else if (s == 12)   what = U ("Oktave h\xc3\xb6her");
        else if (s == -12)  what = U ("Oktave tiefer");
        else
        {
            switch (classOf (s))
            {
                case 7:  what = "Quinte"; break;
                case 3:  what = "kleine Terz"; break;
                case 4:  what = U ("gro\xc3\x9f" "e Terz"); break;
                default: break;
            }
        }

        return signedNumber (s, true) + unit + (what.isEmpty() ? juce::String() : " (" + what + ")");
    }

    juce::Rectangle<int> cellRect (int rowX, int cell)
    {
        const int x = rowX + cell * kPitch;
        if (semitonesOf (cell) == 0)
            return { x, kCellsY - kCentreExtra, kCellW, kCellsH + 2 * kCentreExtra };
        return { x, kCellsY, kCellW, kCellsH };
    }
}

//==============================================================================
VoicingPanel::VoicingPanel (uint64_t mask)
    : bits (cleaned (mask))
{
    for (auto* b : { &triadButton, &clearButton })
    {
        b->setWantsKeyboardFocus (false);
        addAndMakeVisible (*b);
    }

    triadButton.onClick = [this] { setMask ((uint64_t (1) << kRange) | (uint64_t (1) << (kRange + 4)) | (uint64_t (1) << (kRange + 7))); };
    clearButton.onClick = [this] { setMask (0); };

    setSize (kWidth, kHeight);
}

void VoicingPanel::setMask (uint64_t newMask)
{
    if (newMask == bits)
        return;

    bits = newMask;
    repaint();

    if (onChange)
        onChange (bits);
}

int VoicingPanel::cellAt (juce::Point<int> p) const
{
    for (int i = 0; i < kCells; ++i)
    {
        // the gap to the right belongs to the cell, so the pointer never falls between two
        auto r = cellRect (kMargin, i);
        if (r.withWidth (kPitch).contains (p))
            return i;
    }
    return -1;
}

void VoicingPanel::resized()
{
    const int y = kButtonsY;
    clearButton.setBounds (getWidth() - kMargin - kClearW, y, kClearW, kButtonH);
    triadButton.setBounds (clearButton.getX() - kButtonGap - kTriadW, y, kTriadW, kButtonH);
}

void VoicingPanel::mouseMove (const juce::MouseEvent& e)
{
    const int cell = cellAt (e.getPosition());
    if (cell != hovered)
    {
        hovered = cell;
        repaint();
    }
}

void VoicingPanel::mouseExit (const juce::MouseEvent&)
{
    if (hovered != -1)
    {
        hovered = -1;
        repaint();
    }
}

void VoicingPanel::mouseDown (const juce::MouseEvent& e)
{
    const int cell = cellAt (e.getPosition());
    if (cell < 0)
        return;

    const uint64_t bit = uint64_t (1) << cell;
    if ((bits & bit) != 0)
        setMask (bits & ~bit);
    else if (popCount (bits) < kMaxVoices)
        setMask (bits | bit);
}

void VoicingPanel::paint (juce::Graphics& g)
{
    using namespace brand;

    const int count = popCount (bits);
    const bool full = count >= kMaxVoices;

    // heading and count
    const juce::Rectangle<int> heading (kMargin, kHeadingY, kRowWidth, kHeadingH);
    g.setColour (colour::text);
    g.setFont (font (13.0f, true));
    g.drawText ("Freies Voicing", heading, juce::Justification::centredLeft, false);

    g.setColour (colour::secondary);
    g.setFont (font (12.5f));
    g.drawText (juce::String (count) + " von " + juce::String (kMaxVoices) + " Stimmen" + (full ? ", mehr geht nicht" : ""),
                heading, juce::Justification::centredRight, false);

    // cells
    for (int i = 0; i < kCells; ++i)
    {
        const bool on = (bits & (uint64_t (1) << i)) != 0;
        const bool hot = i == hovered && (on || ! full);

        if (on)
            g.setColour (hot ? colour::text.darker (0.15f) : colour::text);
        else
            g.setColour (hot ? colour::controlHi : colour::control);

        g.fillRect (cellRect (kMargin, i));
    }

    // scale: ticks of three lengths, numbers under the octaves
    g.setFont (font (11.5f));
    for (int i = 0; i < kCells; ++i)
    {
        const int s = semitonesOf (i);
        const int cls = classOf (s);
        const int tickX = kMargin + i * kPitch + kCellW / 2;

        int length = 0;
        if (cls == 0)                       length = kTickOctave;
        else if (cls == 7)                  length = kTickFifth;
        else if (cls == 3 || cls == 4)      length = kTickThird;

        if (length > 0)
        {
            g.setColour (colour::secondary);
            g.fillRect (tickX, kTickY, 1, length);
        }

        if (cls == 0)
        {
            g.setColour (s == 0 ? colour::text : colour::secondary);
            g.drawText (signedNumber (s, true), juce::Rectangle<int> (44, kScaleH).withCentre ({ tickX, kScaleY + kScaleH / 2 }),
                        juce::Justification::centred, false);
        }
    }

    // what the pointer is over
    if (hovered >= 0)
    {
        g.setColour (colour::text);
        g.setFont (font (12.5f));
        g.drawText (describe (semitonesOf (hovered)), juce::Rectangle<int> (kMargin, kNoteY, kRowWidth, kNoteH),
                    juce::Justification::centredLeft, false);
    }

    // legend, level with the buttons
    g.setColour (colour::secondary);
    g.setFont (font (11.5f));
    g.drawText ("Lange Striche: Oktaven, mittlere: Quinten, kurze: Terzen",
                juce::Rectangle<int> (kMargin, kButtonsY, triadButton.getX() - kMargin - kButtonGap, kButtonH),
                juce::Justification::centredLeft, false);
}
