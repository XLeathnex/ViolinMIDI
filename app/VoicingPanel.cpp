#include "VoicingPanel.h"

#include "Brand.h"
#include "Scale.h"

#include <cmath>
#include <cstdlib>

namespace
{
    namespace sc = vm::scale;

    static_assert (VoicingPanel::kMaxVoices == sc::kMaxVoices && VoicingPanel::kRange == sc::kMaskOffset
                       && VoicingPanel::kCells == sc::kMaskCells,
                   "the matrix has exactly the engine's positions");

    constexpr int kMargin = 16;
    constexpr int kCellW = 13, kGap = 1, kPitch = kCellW + kGap;     // wide enough to hit on stage
    constexpr int kRowWidth = VoicingPanel::kCells * kPitch - kGap;   // 685

    constexpr int kHeadingY = 14, kHeadingH = 20;      // name (and "FV"), count on the right
    constexpr int kCountW = 170;                       // the count's part of the heading
    constexpr int kModeY = 36, kModeH = 16;            // what the distances mean in this mode
    constexpr int kCellsY = 68, kCellsH = 28;          // the row of cells
    constexpr int kCentreExtra = 4;                    // the cell at 0 is this much taller, top and bottom
    constexpr int kMarkY = 106;                        // centre of the marks under the preset's own voices
    constexpr float kMarkD = 5.0f;
    constexpr int kTickY = 114;                        // tick marks hang from here
    constexpr int kTickOctave = 9, kTickFifth = 6, kTickThird = 3;
    constexpr int kScaleY = 124, kScaleH = 14;         // numbers under the octave ticks
    constexpr int kInfoY = 150, kInfoH = 17;           // two lines: what the pointer is over, what a click does
    constexpr int kSepY = 196;
    constexpr int kButtonsY = 208, kButtonH = 26;
    constexpr int kResetW = 108, kTriadW = 88, kClearW = 72, kButtonGap = 8;

    constexpr int kMatrixTop = kCellsY - kCentreExtra;            // 64
    constexpr int kColumnBottom = kMarkY + 5;                      // a click counts down to the marks
    constexpr int kMatrixBottom = kScaleY + kScaleH;               // 138

    constexpr int kWidth = 2 * kMargin + kRowWidth;                // 717
    constexpr int kHeight = kButtonsY + kButtonH + kHeadingY;      // 248

    // What "Dreiklang" sets: the played note, the major third and the fifth.
    constexpr uint64_t kTriad = vm::scale::maskBit (0) | vm::scale::maskBit (4) | vm::scale::maskBit (7);

    juce::String U (const char* utf8) { return juce::String::fromUTF8 (utf8); }

    // The real minus sign; written as a string so digits after it are no hex digits.
    const juce::String& minus()
    {
        static const juce::String m = U ("\xe2\x88\x92");
        return m;
    }

    juce::String signedNumber (int semitones)
    {
        if (semitones < 0)
            return minus() + juce::String (-semitones);
        return (semitones > 0 ? juce::String ("+") : juce::String()) + juce::String (semitones);
    }

    int semitonesOf (int cell) noexcept              { return cell - VoicingPanel::kRange; }
    uint64_t bitOf (int cell) noexcept               { return uint64_t (1) << cell; }
    bool has (uint64_t mask, int cell) noexcept      { return (mask & bitOf (cell)) != 0; }

    int popCount (uint64_t v) noexcept
    {
        int n = 0;
        for (; v != 0; v &= v - 1)
            ++n;
        return n;
    }

    /** Cleaned the way scale::voices cleans a template, so the panel shows what the engine plays. */
    uint64_t cleaned (uint64_t mask) noexcept   { return sc::cleanFreeMask (mask); }

    juce::String intervalName (int semitoneClass)
    {
        switch (semitoneClass)
        {
            case 1:  return "kleine Sekunde";
            case 2:  return U ("gro\xc3\x9f" "e Sekunde");
            case 3:  return "kleine Terz";
            case 4:  return U ("gro\xc3\x9f" "e Terz");
            case 5:  return "Quarte";
            case 6:  return "Tritonus";
            case 7:  return "Quinte";
            case 8:  return "kleine Sexte";
            case 9:  return U ("gro\xc3\x9f" "e Sexte");
            case 10: return "kleine Septime";
            case 11: return U ("gro\xc3\x9f" "e Septime");
            default: return {};
        }
    }

    /** The distance in semitones and its interval: "+4 ... (grosse Terz)", "-15 ... (kleine Terz + Oktave abwaerts)". */
    juce::String describe (int s)
    {
        if (s == 0)
            return "0 (gespielter Ton)";

        const int distance = std::abs (s), semitoneClass = distance % 12, octaves = distance / 12;
        const bool down = s < 0;

        juce::String what;
        if (semitoneClass == 0)
            what = (octaves == 2 ? U ("zwei Oktaven ") : U ("Oktave ")) + (down ? U ("tiefer") : U ("h\xc3\xb6her"));
        else
            what = intervalName (semitoneClass) + (octaves > 0 ? " + Oktave" : "") + (down ? U (" abw\xc3\xa4rts") : juce::String());

        const juce::String unit = distance == 1 ? U (" Halbton") : U (" Halbt\xc3\xb6ne");
        return signedNumber (s) + unit + " (" + what + ")";
    }

    /** The scale steps diatonic harmony counts for a distance, signed: +2 is the
        third above, -2 the third below. The same reading as scale::templateInterval. */
    int diatonicSteps (int s) noexcept
    {
        const int octaves = sc::floorDiv (s, 12);
        return sc::kStepOfSemitone[s - 12 * octaves] + 7 * octaves;
    }

    /** What diatonic harmony makes of a distance: "die Terz der Tonart", "die Sexte der Tonart + Oktave abwaerts". */
    juce::String diatonicMeaning (int s)
    {
        static const char* const names[7] = { "", "die Sekunde", "die Terz", "die Quarte", "die Quinte", "die Sexte", "die Septime" };

        const int steps = diatonicSteps (s);
        const int within = std::abs (steps) % 7;
        if (within == 0)
            return "genauso, Oktaven bleiben Oktaven";

        return juce::String (names[within]) + " der Tonart" + (std::abs (steps) >= 7 ? " + Oktave" : "")
             + (steps < 0 ? U (" abw\xc3\xa4rts") : juce::String());
    }

    /** The distances of a set, lowest first, e.g. "-12, 0, +4, +7" (with the real minus sign). */
    juce::String distances (uint64_t mask)
    {
        juce::StringArray parts;
        for (int i = 0; i < VoicingPanel::kCells; ++i)
            if (has (mask, i))
                parts.add (signedNumber (semitonesOf (i)));
        return parts.joinIntoString (", ");
    }

    juce::Rectangle<int> cellRect (int cell)
    {
        const int x = kMargin + cell * kPitch;
        if (semitonesOf (cell) == 0)
            return { x, kCellsY - kCentreExtra, kCellW, kCellsH + 2 * kCentreExtra };
        return { x, kCellsY, kCellW, kCellsH };
    }

    float cellCentreX (int cell)   { return (float) (kMargin + cell * kPitch) + (float) kCellW * 0.5f; }

    //==============================================================================
    // Tooltips: what each part is for, in the words used on stage.

    juce::String headingTip (bool freeType)
    {
        if (freeType)
            return U ("Total Free Voicing: eine eigene Harmonieart aus bis zu 10 frei gew\xc3\xa4hlten Abst\xc3\xa4nden "
                      "zum gespielten Ton. Wird mit dem Preset gespeichert.");

        return U ("Die gew\xc3\xa4hlte Harmonieart. Sobald hier Stimmen hinzugef\xc3\xbcgt oder abgew\xc3\xa4hlt werden, "
                  "steht \xe2\x80\x9e" "FV\xe2\x80\x9c (Free Voicing) dahinter, auch im Hauptfenster: So ist auf der B\xc3\xbchne "
                  "zu sehen, ob die reine Harmonieart oder eine eigene Variante klingt.");
    }

    juce::String countTip()
    {
        return U ("Wie viele Stimmen gew\xc3\xa4hlt sind. H\xc3\xb6" "chstens 10 klingen gleichzeitig; ist das erreicht, "
                  "erst eine Stimme abw\xc3\xa4hlen, dann eine neue setzen.");
    }

    juce::String modeTip()
    {
        return U ("Der Harmonie-Modus wird im Hauptfenster umgeschaltet und gilt f\xc3\xbcr jede Harmonieart. "
                  "Diatonisch: Jeder Abstand wird als Tonstufe gelesen (kleine oder gro\xc3\x9f" "e Terz = die Terz, "
                  "Quinte = die Quinte) und in der Tonart abgez\xc3\xa4hlt, so wird ein Dreiklang je nach Stufe Dur, Moll "
                  "oder vermindert. Chromatisch: genau die Halbtonabst\xc3\xa4nde der Matrix. Ein Ton au\xc3\x9f" "erhalb "
                  "der Tonart bekommt auch diatonisch die Abst\xc3\xa4nde, wie sie sind.");
    }

    // Short on purpose: it sits next to the pointer while the lines under the matrix
    // describe the cell, and a long tip there would cover them. The full
    // explanation is on the scale under the cells.
    juce::String cellTip()
    {
        return U ("Klick schaltet die Stimme an oder aus (bis zu 10). Was sie bedeutet, steht darunter.");
    }

    juce::String matrixTip (bool freeType)
    {
        auto tip = U ("Intervall-Matrix: Jede Zelle ist ein Abstand vom gespielten Ton, von \xe2\x88\x92" "24 (zwei Oktaven "
                      "tiefer) bis +24 (zwei Oktaven h\xc3\xb6her); die hohe Zelle in der Mitte ist der gespielte Ton selbst. "
                      "Ein Klick schaltet eine Stimme an oder aus, bis zu 10 Stimmen. Silbern: die Stimmen, die klingen.");
        if (! freeType)
            tip << U (" Rote Punkte darunter: die Stimmen der reinen Harmonieart, ein Ring hei\xc3\x9ft abgew\xc3\xa4hlt. "
                      "Jede \xc3\x84nderung macht daraus ein \xe2\x80\x9e" "FV\xe2\x80\x9c.");
        return tip;
    }

    juce::String infoTip()
    {
        return U ("Beschreibt die Zelle unter der Maus: Abstand in Halbt\xc3\xb6nen, Intervall und im diatonischen Modus "
                  "die Tonstufe der Tonart, die daraus wird; darunter, was ein Klick dort bewirkt. "
                  "Ohne Maus: die gew\xc3\xa4hlten Stimmen.");
    }

    juce::String legendTip (bool freeType)
    {
        auto tip = U ("Die Striche unter den Zellen helfen beim Zielen: lange bei den Oktaven, mittlere bei den Quinten, "
                      "kurze bei den Terzen.");
        if (! freeType)
            tip << U (" Rote Punkte: die Stimmen, mit denen die Harmonieart beginnt; ein Ring hei\xc3\x9ft, "
                      "diese Stimme wurde abgew\xc3\xa4hlt.");
        return tip;
    }
}

//==============================================================================
VoicingPanel::VoicingPanel (const juce::String& typeName, bool isFree, uint64_t voices, uint64_t presetVoices, bool diatonic)
    : title (isFree ? juce::String ("Total Free Voicing") : typeName),
      freeType (isFree),
      preset (isFree ? uint64_t (0) : cleaned (presetVoices)),
      bits (cleaned (voices)),
      diatonicMode (diatonic)
{
    resetButton.setButtonText (U ("Zur\xc3\xbc" "cksetzen"));

    triadButton.setTooltip (U ("Setzt einen Dreiklang als Ausgangspunkt: gespielter Ton, gro\xc3\x9f" "e Terz (+4) und "
                               "Quinte (+7). Diatonisch wird daraus je nach Stufe der Tonart Dur, Moll oder vermindert."));
    resetButton.setTooltip (U ("Holt die reine Harmonieart zur\xc3\xbc" "ck: Hinzugef\xc3\xbcgte Stimmen fallen weg, "
                               "abgew\xc3\xa4hlte klingen wieder, das \xe2\x80\x9e" "FV\xe2\x80\x9c verschwindet. "
                               "Nur m\xc3\xb6glich, solange etwas ver\xc3\xa4ndert ist."));
    clearButton.setTooltip (U ("Nimmt alle Stimmen heraus, um von vorn zu beginnen. Ohne Stimme bleibt die Harmonie stumm."));

    for (auto* b : { &triadButton, &resetButton, &clearButton })
        b->setWantsKeyboardFocus (false);

    // The free type starts from a chord, a preset type goes back to its own voices.
    addAndMakeVisible (freeType ? triadButton : resetButton);
    addAndMakeVisible (clearButton);

    triadButton.onClick = [this] { setMask (kTriad); };
    clearButton.onClick = [this] { setMask (0); };
    resetButton.onClick = [this]
    {
        if (bits == preset)
            return;

        bits = preset;
        refresh();

        if (onReset)
            onReset();
    };

    refresh();
    setSize (kWidth, kHeight);
}

void VoicingPanel::setDiatonic (bool diatonic)
{
    if (diatonic != diatonicMode)
    {
        diatonicMode = diatonic;
        repaint();
    }
}

void VoicingPanel::setMask (uint64_t newMask)
{
    if (newMask == bits)
        return;

    bits = newMask;
    refresh();

    if (onChange)
        onChange (bits);
}

void VoicingPanel::refresh()
{
    // A button that would change nothing is greyed out, as on the Mac.
    resetButton.setEnabled (isModified());
    clearButton.setEnabled (bits != 0);
    triadButton.setEnabled (bits != kTriad);
    repaint();
}

int VoicingPanel::cellAt (juce::Point<int> p) const
{
    // The whole column counts, from the top of the tall cell down to the marks, and
    // the gap to the right belongs to the cell, so the pointer never falls between two.
    if (p.y < kMatrixTop || p.y >= kColumnBottom || p.x < kMargin)
        return -1;

    const int cell = (p.x - kMargin) / kPitch;
    return cell < kCells ? cell : -1;
}

void VoicingPanel::resized()
{
    clearButton.setBounds (getWidth() - kMargin - kClearW, kButtonsY, kClearW, kButtonH);

    const int w = freeType ? kTriadW : kResetW;
    (freeType ? triadButton : resetButton).setBounds (clearButton.getX() - kButtonGap - w, kButtonsY, w, kButtonH);
}

void VoicingPanel::setHovered (int cell)
{
    if (cell != hovered)
    {
        hovered = cell;
        repaint();
    }
}

void VoicingPanel::mouseMove (const juce::MouseEvent& e)   { setHovered (cellAt (e.getPosition())); }
void VoicingPanel::mouseDrag (const juce::MouseEvent& e)   { setHovered (cellAt (e.getPosition())); }
void VoicingPanel::mouseExit (const juce::MouseEvent&)     { setHovered (-1); }

void VoicingPanel::mouseDown (const juce::MouseEvent& e)
{
    const int cell = cellAt (e.getPosition());
    setHovered (cell);

    if (cell < 0)
        return;

    // A full set refuses a new voice; the line under the matrix says why.
    if (has (bits, cell))
        setMask (bits & ~bitOf (cell));
    else if (popCount (bits) < kMaxVoices)
        setMask (bits | bitOf (cell));
}

juce::String VoicingPanel::getTooltip()
{
    const auto p = getMouseXYRelative();

    const juce::Rectangle<int> heading (kMargin, kHeadingY, kRowWidth, kHeadingH);
    if (heading.contains (p))
        return p.x >= heading.getRight() - kCountW ? countTip() : headingTip (freeType);

    if (juce::Rectangle<int> (kMargin, kModeY, kRowWidth, kModeH).contains (p))
        return modeTip();

    if (cellAt (p) >= 0)
        return cellTip();

    if (juce::Rectangle<int> (kMargin, kMatrixTop, kRowWidth, kMatrixBottom - kMatrixTop).contains (p))
        return matrixTip (freeType);

    if (juce::Rectangle<int> (kMargin, kInfoY, kRowWidth, 2 * kInfoH).contains (p))
        return infoTip();

    const int legendRight = (freeType ? triadButton : resetButton).getX() - kButtonGap;
    if (juce::Rectangle<int> (kMargin, kButtonsY, legendRight - kMargin, kButtonH).contains (p))
        return legendTip (freeType);

    return {};
}

//==============================================================================
void VoicingPanel::paint (juce::Graphics& g)
{
    paintHeading (g);
    paintMatrix (g);
    paintInfo (g);

    g.setColour (brand::colour::separator);
    g.fillRect (kMargin, kSepY, kRowWidth, 1);

    paintLegend (g);
}

void VoicingPanel::paintHeading (juce::Graphics& g) const
{
    using namespace brand;

    const juce::Rectangle<int> heading (kMargin, kHeadingY, kRowWidth, kHeadingH);
    const bool modified = isModified();

    const auto nameFont = font (13.0f, true);
    const auto badgeFont = font (11.0f, true);
    const float badgeW = juce::GlyphArrangement::getStringWidth (badgeFont, "FV") + 10.0f;

    // The badge always keeps its place: a long name is shortened, never the "FV".
    auto nameArea = heading.withTrimmedRight (kCountW);
    if (modified)
        nameArea = nameArea.withTrimmedRight ((int) std::ceil (badgeW) + 8);

    g.setColour (colour::text);
    g.setFont (nameFont);
    g.drawText (title, nameArea, juce::Justification::centredLeft, true);

    // "FV": this preset type plays voices of its own, visible at a glance on stage.
    if (modified)
    {
        const float nameW = juce::jmin (juce::GlyphArrangement::getStringWidth (nameFont, title), (float) nameArea.getWidth());
        const juce::Rectangle<float> badge ((float) nameArea.getX() + nameW + 8.0f, (float) heading.getCentreY() - 8.0f, badgeW, 16.0f);

        g.setColour (colour::crimson);
        g.fillRoundedRectangle (badge, 4.0f);
        g.setColour (colour::text);
        g.setFont (badgeFont);
        g.drawText ("FV", badge, juce::Justification::centred, false);
    }

    const int count = popCount (bits);
    g.setColour (colour::silver);
    g.setFont (font (12.5f));
    g.drawText (juce::String (count) + " von " + juce::String (kMaxVoices) + " Stimmen"
                    + (count >= kMaxVoices ? U (" \xc2\xb7 voll") : juce::String()),
                heading, juce::Justification::centredRight, false);

    // What the distances mean in the harmony mode that is on.
    g.setColour (colour::secondary);
    g.setFont (font (12.0f));
    g.drawText (diatonicMode ? U ("Diatonisch: Abst\xc3\xa4nde werden als Tonstufen der Tonart gelesen")
                             : U ("Chromatisch: Abst\xc3\xa4nde gelten genau so"),
                juce::Rectangle<int> (kMargin, kModeY, kRowWidth, kModeH), juce::Justification::centredLeft, false);
}

void VoicingPanel::paintMatrix (juce::Graphics& g) const
{
    using namespace brand;

    const bool full = popCount (bits) >= kMaxVoices;

    // cells: the voices that sound in silver-white, the one under the pointer brighter
    for (int i = 0; i < kCells; ++i)
    {
        const bool on = has (bits, i);
        const bool hot = i == hovered && (on || ! full);

        if (on)
            g.setColour (hot ? colour::text : colour::silver);
        else
            g.setColour (hot ? colour::selected : colour::control);

        g.fillRoundedRectangle (cellRect (i).toFloat(), 2.0f);
    }

    // Under the preset's own voices: a dot while the voice is there, a ring once it
    // was taken away. Added voices have none, so every change is visible.
    g.setColour (colour::crimson);
    for (int i = 0; i < kCells; ++i)
    {
        if (! has (preset, i))
            continue;

        const auto mark = juce::Rectangle<float> (kMarkD, kMarkD).withCentre ({ cellCentreX (i), (float) kMarkY });
        if (has (bits, i))
            g.fillEllipse (mark);
        else
            g.drawEllipse (mark.reduced (0.6f), 1.2f);
    }

    // scale: ticks of three lengths, numbers under the octaves
    g.setFont (font (11.5f));
    for (int i = 0; i < kCells; ++i)
    {
        const int s = semitonesOf (i);
        const int semitoneClass = sc::pitchClass (s);
        const int tickX = kMargin + i * kPitch + kCellW / 2;

        int length = 0;
        if (semitoneClass == 0)                             length = kTickOctave;
        else if (semitoneClass == 7)                        length = kTickFifth;
        else if (semitoneClass == 3 || semitoneClass == 4)  length = kTickThird;

        if (length > 0)
        {
            g.setColour (colour::secondary);
            g.fillRect (tickX, kTickY, 1, length);
        }

        if (semitoneClass == 0)
        {
            g.setColour (s == 0 ? colour::text : colour::silver);
            g.drawText (signedNumber (s), juce::Rectangle<int> (44, kScaleH).withCentre ({ tickX, kScaleY + kScaleH / 2 }),
                        juce::Justification::centred, false);
        }
    }
}

void VoicingPanel::paintInfo (juce::Graphics& g) const
{
    using namespace brand;

    const juce::Rectangle<int> line1 (kMargin, kInfoY, kRowWidth, kInfoH);
    const auto line2 = line1.translated (0, kInfoH);
    const int count = popCount (bits);

    if (hovered < 0)
    {
        g.setColour (colour::tertiary);
        g.setFont (font (12.5f));
        g.drawText (U ("Auf eine Zelle zeigen: Abstand und Bedeutung. Klicken: Stimme an oder aus."),
                    line1, juce::Justification::centredLeft, false);

        g.setFont (font (12.0f));
        if (count == 0)
        {
            g.setColour (colour::problem);
            g.drawText (U ("Keine Stimme gew\xc3\xa4hlt: Die Harmonie bleibt stumm."), line2, juce::Justification::centredLeft, false);
            return;
        }

        // the set as numbers, and for a changed preset what differs from it
        juce::String text = "Stimmen: " + distances (bits);
        if (isModified())
        {
            const uint64_t added = bits & ~preset, removed = preset & ~bits;
            if (added != 0)
                text << U (" \xc2\xb7 hinzugef\xc3\xbcgt: ") << distances (added);
            if (removed != 0)
                text << U (" \xc2\xb7 abgew\xc3\xa4hlt: ") << distances (removed);
        }

        g.setColour (colour::silver);
        g.drawFittedText (text, line2, juce::Justification::centredLeft, 1, 0.85f);
        return;
    }

    // the distance under the pointer, and what diatonic harmony makes of it
    const int s = semitonesOf (hovered);
    juce::String what = describe (s);
    if (diatonicMode && s != 0)
        what << U (" \xc2\xb7 diatonisch: ") << diatonicMeaning (s);

    g.setColour (colour::silver);
    g.setFont (font (12.5f));
    g.drawFittedText (what, line1, juce::Justification::centredLeft, 1, 0.85f);

    // what this voice is to the preset, and what a click does here
    const bool on = has (bits, hovered), inPreset = has (preset, hovered);
    const bool refused = ! on && count >= kMaxVoices;

    juce::String hint;
    if (refused)
        hint = "Schon " + juce::String (kMaxVoices) + U (" Stimmen: erst eine andere abw\xc3\xa4hlen");
    else if (freeType || (! inPreset && ! on))
        hint = on ? U ("Klick nimmt diese Stimme heraus") : U ("Klick f\xc3\xbcgt diese Stimme hinzu");
    else if (inPreset)
        hint = on ? U ("Stimme der reinen Harmonieart \xc2\xb7 Klick nimmt sie heraus")
                  : U ("Abgew\xc3\xa4hlt (FV) \xc2\xb7 Klick holt sie zur\xc3\xbc" "ck");
    else
        hint = U ("Hinzugef\xc3\xbcgt (FV) \xc2\xb7 Klick nimmt sie heraus");

    // Read diatonically, both thirds are the third: two such voices give one note.
    if (diatonicMode && ! refused)
    {
        const int steps = diatonicSteps (s);
        for (int i = 0; i < kCells; ++i)
        {
            if (i != hovered && has (bits, i) && diatonicSteps (semitonesOf (i)) == steps)
            {
                hint << U (" \xc2\xb7 in der Tonart derselbe Ton wie ") << signedNumber (semitonesOf (i)) << ", klingt einmal";
                break;
            }
        }
    }

    g.setColour (refused ? colour::problem : colour::secondary);
    g.setFont (font (12.0f));
    g.drawFittedText (hint, line2, juce::Justification::centredLeft, 1, 0.85f);
}

void VoicingPanel::paintLegend (juce::Graphics& g) const
{
    using namespace brand;

    const int right = (freeType ? triadButton : resetButton).getX() - kButtonGap;
    const juce::Rectangle<int> area (kMargin, kButtonsY, right - kMargin, kButtonH);
    const auto ticks = U ("Lange Striche: Oktaven, mittlere: Quinten, kurze: Terzen");

    g.setColour (colour::secondary);
    g.setFont (font (11.5f));

    if (freeType)
    {
        g.drawText (ticks, area, juce::Justification::centredLeft, true);
        return;
    }

    const auto top = area.withHeight (kButtonH / 2);
    const auto bottom = area.withTrimmedTop (kButtonH / 2);
    g.drawText (ticks, top, juce::Justification::centredLeft, true);

    // the same mark as under the cells
    g.setColour (colour::crimson);
    g.fillEllipse (juce::Rectangle<float> (kMarkD, kMarkD).withCentre ({ (float) kMargin + kMarkD * 0.5f, (float) bottom.getCentreY() }));

    g.setColour (colour::secondary);
    g.drawText (U ("Stimmen der reinen Harmonieart (Ring: abgew\xc3\xa4hlt)"), bottom.withTrimmedLeft (11),
                juce::Justification::centredLeft, true);
}
