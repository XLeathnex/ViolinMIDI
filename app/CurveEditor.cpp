#include "CurveEditor.h"

#include "Brand.h"
#include "Widgets.h"

#include <cmath>

using namespace brand;

namespace
{
    juce::String U (const char* utf8) { return juce::String::fromUTF8 (utf8); }

    constexpr float kLow = vm::curve::kLowNote, kHigh = vm::curve::kHighNote;

    // The graph: value labels on the left, note names below, string names above.
    constexpr float kLabelW = 44.0f;
    constexpr float kAxisH = 22.0f;
    constexpr float kTopStrip = 18.0f;
    constexpr float kRightPad = 14.0f;
    constexpr int kButtonW = 72, kButtonH = 22;

    constexpr float kHitRadius = 9.0f;
    constexpr float kNodeRadius = 4.0f, kNodeRadiusHot = 5.5f;

    // Two points never come closer than this (in semitones): a little more than
    // curve::kMinSpacing, so sanitising never merges the point being dragged.
    constexpr float kGap = vm::curve::kMinSpacing + 0.05f;

    // The open strings of the violin, and the top of the range.
    constexpr int kStrings[] = { 55, 62, 69, 76 };
    constexpr int kTopNote = 88;
    const char* const kStringNames[] = { "G-Saite", "D-Saite", "A-Saite", "E-Saite" };
    const char* const kStringLetters[] = { "G", "D", "A", "E" };

    juce::Rectangle<float> circle (juce::Point<float> centre, float radius)
    {
        return juce::Rectangle<float> (2.0f * radius, 2.0f * radius).withCentre (centre);
    }

    /** A note on the semitone grid between lo and hi; where no semitone fits,
        the nearest end. */
    float snapped (float note, float lo, float hi)
    {
        float n = std::round (note);
        if (n < lo)
            n = std::ceil (lo) <= hi ? std::ceil (lo) : lo;
        if (n > hi)
            n = std::floor (hi) >= lo ? std::floor (hi) : hi;
        return n;
    }
}

//==============================================================================
CurveEditor::CurveEditor (const juce::String& title, float yLow, float yHigh, bool logarithmic,
                          std::function<juce::String (float)> formatValue)
    : titleText (title),
      low (juce::jmin (yLow, yHigh)),
      high (juce::jmax (yLow, yHigh)),
      logScale (logarithmic && juce::jmin (yLow, yHigh) > 0.0f),
      format (std::move (formatValue))
{
    jassert (high > low);

    if (! format)
        format = [] (float v) { return juce::String (v, 2); };

    // Until a standard is given, a broken curve becomes a flat line in the middle.
    fallback = vm::curve::flat (logScale ? std::sqrt (low * high) : 0.5f * (low + high));
    curve = fallback;

    setTitle (title);
    setWantsKeyboardFocus (false);
    setTooltip (mouseHelp());

    standardButton.setWantsKeyboardFocus (false);
    standardButton.onClick = [this] { restoreDefault(); };
    addChildComponent (standardButton);
}

juce::String CurveEditor::mouseHelp()
{
    return U ("Klick: Punkt setzen. Ziehen: verschieben (rastet auf Halbt\xc3\xb6nen ein). Doppelklick oder "
              "Rechtsklick: l\xc3\xb6schen; mindestens zwei, h\xc3\xb6" "chstens zw\xc3\xb6lf Punkte. Rote Linie: "
              "der gerade gespielte Ton.");
}

void CurveEditor::setCurve (const vm::Curve& c)
{
    curve = vm::curve::sanitised (c, low, high, fallback);
    curveGiven = true;
    if (hoverNode >= curve.count) hoverNode = -1;
    if (dragNode >= curve.count)  dragNode = -1;
    repaint();
}

void CurveEditor::setDefaultCurve (const vm::Curve& c, const juce::String& buttonTooltip)
{
    standard = vm::curve::sanitised (c, low, high, fallback);
    fallback = standard;
    hasStandard = true;
    if (! curveGiven)
        curve = standard;   // an editor that was never given a curve shows the standard
    standardButton.setTooltip (buttonTooltip);
    standardButton.setVisible (true);
    resized();
    repaint();
}

void CurveEditor::setPlayhead (float midiNote)
{
    const float next = std::isfinite (midiNote) && midiNote >= 0.0f ? midiNote : -1.0f;
    if (next < 0.0f && playhead < 0.0f)
        return;
    if (next >= 0.0f && playhead >= 0.0f && std::abs (next - playhead) < 0.02f)
        return;

    playhead = next;
    repaint();
}

//==============================================================================
juce::Rectangle<float> CurveEditor::graphArea() const
{
    return getLocalBounds().toFloat().withTrimmedTop ((float) kHeaderHeight);
}

juce::Rectangle<float> CurveEditor::plotArea() const
{
    return graphArea().withTrimmedLeft (kLabelW).withTrimmedRight (kRightPad)
                      .withTrimmedTop (kTopStrip).withTrimmedBottom (kAxisH);
}

float CurveEditor::xForNote (float note) const
{
    const auto plot = plotArea();
    return plot.getX() + (note - kLow) / (kHigh - kLow) * plot.getWidth();
}

float CurveEditor::noteAtX (float x) const
{
    const auto plot = plotArea();
    return kLow + (x - plot.getX()) / juce::jmax (1.0f, plot.getWidth()) * (kHigh - kLow);
}

float CurveEditor::yForValue (float value) const
{
    const auto plot = plotArea();
    const float v = juce::jlimit (low, high, value);
    const float norm = logScale ? std::log (v / low) / std::log (high / low)
                                : (v - low) / (high - low);
    return plot.getBottom() - norm * plot.getHeight();
}

float CurveEditor::valueAtY (float y) const
{
    const auto plot = plotArea();
    const float norm = juce::jlimit (0.0f, 1.0f, (plot.getBottom() - y) / juce::jmax (1.0f, plot.getHeight()));
    const float v = logScale ? low * std::pow (high / low, norm) : low + norm * (high - low);
    return juce::jlimit (low, high, v);
}

juce::Point<float> CurveEditor::nodePosition (int index) const
{
    return { xForNote (curve.x[index]), yForValue (curve.y[index]) };
}

int CurveEditor::nodeAt (juce::Point<float> pos) const
{
    int best = -1;
    float bestDistance = kHitRadius;
    for (int i = 0; i < curve.count; ++i)
    {
        const float d = pos.getDistanceFrom (nodePosition (i));
        if (d <= bestDistance)
        {
            best = i;
            bestDistance = d;
        }
    }
    return best;
}

juce::String CurveEditor::readout (float note, float value) const
{
    return noteName (juce::roundToInt (note)) + U (" \xc2\xb7 ") + format (value);
}

//==============================================================================
int CurveEditor::addPoint (juce::Point<float> pos)
{
    const float note = std::round (juce::jlimit (kLow, kHigh, noteAtX (pos.x)));
    const float value = valueAtY (pos.y);

    // A note that has a point already: that point takes the new value, so a
    // click never puts two points on one note.
    for (int i = 0; i < curve.count; ++i)
        if (std::abs (curve.x[i] - note) < 0.5f)
        {
            curve.y[i] = value;
            return i;
        }

    if (curve.count >= vm::Curve::kMaxPoints)
        return -1;

    int at = 0;
    while (at < curve.count && curve.x[at] < note)
        ++at;
    for (int i = curve.count; i > at; --i)
    {
        curve.x[i] = curve.x[i - 1];
        curve.y[i] = curve.y[i - 1];
    }
    curve.x[at] = note;
    curve.y[at] = value;
    ++curve.count;
    return at;
}

void CurveEditor::movePoint (int index, juce::Point<float> pos)
{
    if (index < 0 || index >= curve.count)
        return;

    // A point stays between its neighbours, so the order (and which point is
    // being dragged) never changes under the mouse.
    const float lo = index > 0 ? curve.x[index - 1] + kGap : kLow;
    const float hi = index < curve.count - 1 ? curve.x[index + 1] - kGap : kHigh;

    const float note = lo <= hi ? snapped (noteAtX (pos.x), lo, hi) : curve.x[index];
    const float value = valueAtY (pos.y);

    if (std::abs (note - curve.x[index]) < 1.0e-4f && std::abs (value - curve.y[index]) < 1.0e-6f * (1.0f + std::abs (value)))
        return;

    curve.x[index] = note;
    curve.y[index] = value;
    changed();
}

void CurveEditor::removePoint (int index)
{
    if (index < 0 || index >= curve.count || curve.count <= 2)
        return;

    for (int i = index; i < curve.count - 1; ++i)
    {
        curve.x[i] = curve.x[i + 1];
        curve.y[i] = curve.y[i + 1];
    }
    --curve.count;
    hoverNode = dragNode = -1;
    changed();
}

void CurveEditor::restoreDefault()
{
    if (! hasStandard)
        return;

    curve = standard;
    hoverNode = dragNode = -1;
    changed();
}

void CurveEditor::changed()
{
    curve = vm::curve::sanitised (curve, low, high, fallback);
    if (hoverNode >= curve.count) hoverNode = -1;
    if (dragNode >= curve.count)  dragNode = -1;
    repaint();

    if (onChange)
        onChange (curve);
}

void CurveEditor::showMenu (int node)
{
    if (node < 0 && ! hasStandard)
        return;

    juce::PopupMenu menu;
    if (node >= 0)
        menu.addItem (1, curve.count > 2 ? U ("Punkt l\xc3\xb6schen")
                                         : U ("Punkt l\xc3\xb6schen (zwei Punkte bleiben mindestens)"),
                      curve.count > 2);
    if (hasStandard)
        menu.addItem (2, U ("Standardkurve wiederherstellen"));

    juce::Component::SafePointer<CurveEditor> safeThis (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(),
                        [safeThis, node] (int result)
                        {
                            if (safeThis == nullptr)
                                return;
                            if (result == 1)
                                safeThis->removePoint (node);
                            else if (result == 2)
                                safeThis->restoreDefault();
                        });
}

//==============================================================================
void CurveEditor::updateHover (juce::Point<float> pos)
{
    hoverPos = pos;
    hoverInside = isEnabled() && plotArea().expanded (kHitRadius).contains (pos);
    hoverNode = hoverInside ? nodeAt (pos) : -1;
    setMouseCursor (hoverNode >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    repaint();
}

void CurveEditor::mouseMove (const juce::MouseEvent& e)    { updateHover (e.position); }
void CurveEditor::mouseEnter (const juce::MouseEvent& e)   { updateHover (e.position); }

void CurveEditor::mouseExit (const juce::MouseEvent&)
{
    hoverInside = false;
    hoverNode = -1;
    repaint();
}

void CurveEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto pos = e.position;
    const int hit = nodeAt (pos);

    if (e.mods.isPopupMenu())
    {
        dragNode = -1;
        showMenu (hit);
        return;
    }

    // Only a double-click whose first click was already on a point deletes:
    // a double-click on free space just sets a point, like a single click.
    if (e.getNumberOfClicks() == 1)
        pressedOnNode = hit >= 0;

    if (hit >= 0)
    {
        dragNode = hit;
        dragOffset = nodePosition (hit) - pos;   // the point does not jump to the pointer
        repaint();
        return;
    }

    if (e.getNumberOfClicks() > 1 || ! plotArea().expanded (kHitRadius).contains (pos))
        return;

    dragOffset = {};
    dragNode = addPoint (pos);
    if (dragNode >= 0)
        changed();
    else
        repaint();   // full: the readout says so
}

void CurveEditor::mouseDrag (const juce::MouseEvent& e)
{
    hoverPos = e.position;
    if (dragNode >= 0)
        movePoint (dragNode, e.position + dragOffset);
}

void CurveEditor::mouseUp (const juce::MouseEvent& e)
{
    dragNode = -1;
    updateHover (e.position);
}

void CurveEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu() || ! pressedOnNode)
        return;

    const int hit = nodeAt (e.position);
    if (hit >= 0)
        removePoint (hit);
    updateHover (e.position);
}

void CurveEditor::enablementChanged()
{
    hoverNode = dragNode = -1;
    hoverInside = false;
    repaint();
}

void CurveEditor::resized()
{
    standardButton.setBounds (getWidth() - kButtonW, 0, kButtonW, kButtonH);
}

//==============================================================================
void CurveEditor::paint (juce::Graphics& g)
{
    const bool enabled = isEnabled();
    if (! enabled)
        g.beginTransparencyLayer (0.5f);

    const auto graph = graphArea();
    const auto plot = plotArea();

    // Header: the title on the left; while a tone is played, the value the
    // curve gives it on the right, next to the "Standard" button.
    const auto header = getLocalBounds().toFloat().withHeight ((float) kButtonH);
    float titleRight = header.getRight() - (hasStandard ? (float) kButtonW + 10.0f : 0.0f);
    if (playhead >= 0.0f)
    {
        const auto text = readout (playhead, vm::curve::value (curve, playhead, logScale));
        const auto f = font (12.5f);
        const float w = juce::GlyphArrangement::getStringWidth (f, text) + 2.0f;
        const auto area = juce::Rectangle<float> (titleRight - w, header.getY(), w, header.getHeight());

        g.setFont (f);
        g.setColour (colour::silver);
        g.drawText (text, area, juce::Justification::centredRight, false);
        g.setColour (colour::crimson);
        g.fillEllipse (circle ({ area.getX() - 6.0f, header.getCentreY() }, 3.0f));
        titleRight = area.getX() - 16.0f;
    }

    g.setFont (font (12.5f));
    g.setColour (colour::secondary);
    g.drawText (titleText, header.withRight (juce::jmax (header.getX(), titleRight)), juce::Justification::centredLeft, true);

    g.setColour (colour::display);
    g.fillRoundedRectangle (graph, kControlRadius);

    paintGrid (g, graph, plot);
    paintCurve (g, plot);

    // the pitch being played: a line through the graph and a dot where the curve is read
    if (playhead >= 0.0f)
    {
        const float x = std::round (xForNote (juce::jlimit (kLow, kHigh, playhead))) + 0.5f;
        const float y = yForValue (vm::curve::value (curve, playhead, logScale));
        g.setColour (colour::crimson);
        g.fillRect (x - 0.5f, plot.getY() - 4.0f, 1.0f, plot.getHeight() + 4.0f);
        g.setColour (colour::display);
        g.fillEllipse (circle ({ x, y }, 5.5f));
        g.setColour (colour::crimson);
        g.fillEllipse (circle ({ x, y }, 4.0f));
    }

    // the points: silver, crimson while under the mouse or dragged
    for (int i = 0; i < curve.count; ++i)
    {
        const auto p = nodePosition (i);
        const bool hot = enabled && (i == dragNode || (dragNode < 0 && i == hoverNode));
        const float r = hot ? kNodeRadiusHot : kNodeRadius;
        g.setColour (colour::display);
        g.fillEllipse (circle (p, r + 1.5f));
        g.setColour (hot ? colour::crimson : colour::silver);
        g.fillEllipse (circle (p, r));
    }

    // The readout: the point being dragged or under the mouse, else where a
    // click would put a new one (drawn as a hollow ring).
    if (enabled)
    {
        if (dragNode >= 0 && dragNode < curve.count)
        {
            paintTag (g, graph, nodePosition (dragNode), readout (curve.x[dragNode], curve.y[dragNode]));
        }
        else if (hoverInside && hoverNode >= 0 && hoverNode < curve.count)
        {
            paintTag (g, graph, nodePosition (hoverNode), readout (curve.x[hoverNode], curve.y[hoverNode]));
        }
        else if (hoverInside)
        {
            const float note = std::round (juce::jlimit (kLow, kHigh, noteAtX (hoverPos.x)));
            const float value = valueAtY (hoverPos.y);
            const juce::Point<float> at (xForNote (note), yForValue (value));

            bool noteTaken = false;
            for (int i = 0; i < curve.count; ++i)
                noteTaken = noteTaken || std::abs (curve.x[i] - note) < 0.5f;

            if (curve.count >= vm::Curve::kMaxPoints && ! noteTaken)
            {
                paintTag (g, graph, at, U ("H\xc3\xb6" "chstens zw\xc3\xb6lf Punkte"));
            }
            else
            {
                g.setColour (colour::silver.withAlpha (0.55f));
                g.drawEllipse (circle (at, kNodeRadius), 1.2f);
                paintTag (g, graph, at, readout (note, value));
            }
        }
    }

    if (! enabled)
        g.endTransparencyLayer();
}

void CurveEditor::paintGrid (juce::Graphics& g, juce::Rectangle<float> graph, juce::Rectangle<float> plot) const
{
    const float bandTop = plot.getY() - kTopStrip + 3.0f;

    // The four strings as zones, every second one a shade lighter, named above the plot.
    for (int k = 0; k < 4; ++k)
    {
        const float x0 = xForNote ((float) kStrings[k]);
        const float x1 = xForNote ((float) (k < 3 ? kStrings[k + 1] : kTopNote));

        if (k % 2 == 1)
        {
            g.setColour (juce::Colours::white.withAlpha (0.022f));
            g.fillRect (juce::Rectangle<float> (x0, bandTop, x1 - x0, plot.getBottom() - bandTop));
        }

        const auto nameFont = font (10.5f);
        const auto name = juce::GlyphArrangement::getStringWidth (nameFont, kStringNames[k]) + 6.0f < x1 - x0
                              ? juce::String (kStringNames[k]) : juce::String (kStringLetters[k]);
        g.setFont (nameFont);
        g.setColour (colour::tertiary);
        g.drawText (name, juce::Rectangle<float> (x0, bandTop, x1 - x0, kTopStrip - 5.0f),
                    juce::Justification::centred, false);
    }

    // value grid: octaves for frequencies, quarters otherwise
    juce::Array<float> ticks;
    if (logScale)
    {
        for (float v = low; v <= high * 1.001f; v *= 2.0f)
            ticks.add (v);
    }
    else
    {
        for (int i = 0; i <= 4; ++i)
            ticks.add (low + (high - low) * (float) i / 4.0f);
    }

    g.setFont (font (10.5f));
    for (const float v : ticks)
    {
        const float y = std::round (yForValue (v));
        g.setColour (colour::grid);
        g.fillRect (plot.getX(), y, plot.getWidth(), 1.0f);
        g.setColour (colour::silver);
        g.drawText (format (v), juce::Rectangle<float> (graph.getX(), y - 7.0f, kLabelW - 8.0f, 14.0f),
                    juce::Justification::centredRight, false);
    }

    // the string boundaries, and a small tick per semitone (where a point can sit)
    g.setColour (colour::separator);
    for (int k = 1; k < 4; ++k)
        g.fillRect (std::round (xForNote ((float) kStrings[k])), bandTop, 1.0f, plot.getBottom() - bandTop);
    for (int n = kStrings[0]; n <= kTopNote; ++n)
    {
        const bool open = n == kTopNote || n == kStrings[0] || n == kStrings[1] || n == kStrings[2] || n == kStrings[3];
        const float h = open ? 5.0f : 3.0f;
        g.fillRect (std::round (xForNote ((float) n)), plot.getBottom() - h, 1.0f, h);
    }

    // note names of the open strings and the top
    g.setFont (font (10.5f));
    g.setColour (colour::silver);
    for (const int n : { kStrings[0], kStrings[1], kStrings[2], kStrings[3], kTopNote })
        g.drawText (noteName (n), juce::Rectangle<float> (xForNote ((float) n) - 20.0f, plot.getBottom() + 6.0f, 40.0f, 14.0f),
                    juce::Justification::centred, false);
}

void CurveEditor::paintCurve (juce::Graphics& g, juce::Rectangle<float> plot) const
{
    // Sampled every two pixels with the same function the harmonizer uses, so
    // what is drawn is what is heard.
    const int steps = juce::jmax (2, juce::roundToInt (plot.getWidth() * 0.5f));
    juce::Path line;
    for (int i = 0; i <= steps; ++i)
    {
        const float x = plot.getX() + plot.getWidth() * (float) i / (float) steps;
        const float y = yForValue (vm::curve::value (curve, noteAtX (x), logScale));
        if (i == 0)
            line.startNewSubPath (x, y);
        else
            line.lineTo (x, y);
    }

    juce::Path fill (line);
    fill.lineTo (plot.getRight(), plot.getBottom());
    fill.lineTo (plot.getX(), plot.getBottom());
    fill.closeSubPath();

    g.setColour (colour::crimson.withAlpha (0.13f));
    g.fillPath (fill);
    g.setColour (colour::text);
    g.strokePath (line, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void CurveEditor::paintTag (juce::Graphics& g, juce::Rectangle<float> graph, juce::Point<float> at, const juce::String& text) const
{
    const auto f = font (11.5f);
    const float w = juce::GlyphArrangement::getStringWidth (f, text) + 14.0f, h = 18.0f;

    // above the point, or below it where there is no room
    float y = at.y - h - 9.0f;
    if (y < graph.getY() + 2.0f)
        y = at.y + 9.0f;
    const float x = juce::jmax (graph.getX() + 2.0f, juce::jmin (at.x - 0.5f * w, graph.getRight() - w - 2.0f));

    const juce::Rectangle<float> r (x, y, w, h);
    g.setColour (colour::control);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (colour::separator.brighter (0.2f));
    g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
    g.setFont (f);
    g.setColour (colour::silver);
    g.drawText (text, r, juce::Justification::centred, false);
}
