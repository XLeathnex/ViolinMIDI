#include "Widgets.h"

#include "Brand.h"
#include "Theme.h"

#include <cmath>

using namespace brand;

namespace
{
    juce::String text (const char* utf8) { return juce::String::fromUTF8 (utf8); }

    constexpr float kPad = 24.0f;
}

juce::String noteName (int midiNote)
{
    static const char* names[] = { "C", "C\xe2\x99\xaf", "D", "D\xe2\x99\xaf", "E", "F",
                                   "F\xe2\x99\xaf", "G", "G\xe2\x99\xaf", "A", "A\xe2\x99\xaf", "B" };
    return text (names[((midiNote % 12) + 12) % 12]) + juce::String (midiNote / 12 - 1);
}

//==============================================================================
ButtonRow::~ButtonRow()
{
    for (auto& b : items)
        if (b != nullptr)
            b->removeListener (this);
}

void ButtonRow::add (juce::Button& b)
{
    items.push_back (&b);
    b.addListener (this);
    addAndMakeVisible (b);
}

void ButtonRow::resized()
{
    const int n = (int) items.size();
    if (n == 0)
        return;

    std::vector<float> wanted;
    float total = 0.0f;
    for (auto& b : items)
    {
        wanted.push_back (b != nullptr ? juce::GlyphArrangement::getStringWidth (font (13.0f), b->getButtonText()) : 0.0f);
        total += wanted.back();
    }

    const float spare = juce::jmax (0.0f, (float) getWidth() - total) / (float) n;
    float x = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float right = i == n - 1 ? (float) getWidth() : x + wanted[(size_t) i] + spare;
        if (auto* b = items[(size_t) i].getComponent())
            b->setBounds (juce::roundToInt (x), 0, juce::roundToInt (right) - juce::roundToInt (x), getHeight());
        x = right;
    }
}

void ButtonRow::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    const int n = (int) items.size();
    const auto isOn = [this, n] (int i) { return i >= 0 && i < n && items[(size_t) i] != nullptr && items[(size_t) i]->getToggleState(); };
    const bool enabled = n == 0 || items.front() == nullptr || items.front()->isEnabled();
    const float alpha = enabled ? 1.0f : 0.5f;
    const SwitchStyle style = switchStyle (*this);

    g.setColour (colour::control.withMultipliedAlpha (alpha));
    g.fillRoundedRectangle (r, kControlRadius);

    for (int i = 0; i < n; ++i)
    {
        auto* b = items[(size_t) i].getComponent();
        if (b == nullptr)
            continue;

        const auto area = b->getBounds().toFloat();
        const auto plate = area.reduced (2.0f);

        if (isOn (i))
        {
            // An on/off switch says its state in colour: the first segment is OFF, the last ON.
            auto plateColour = colour::selected;
            if (style != SwitchStyle::plain && n == 2)
                plateColour = i == 1 ? colour::emeraldDim : (style == SwitchStyle::status ? colour::crimsonDim : colour::selected);
            g.setColour (plateColour.withMultipliedAlpha (alpha));
            g.fillRoundedRectangle (plate, kControlRadius - 1.5f);
        }
        else if (enabled && (b->isOver() || b->isDown()))
        {
            g.setColour (juce::Colours::white.withAlpha (b->isDown() ? 0.07f : 0.04f));
            g.fillRoundedRectangle (plate, kControlRadius - 1.5f);
        }

        // a hairline only between two segments that are both not chosen
        if (i < n - 1 && ! isOn (i) && ! isOn (i + 1))
        {
            g.setColour (colour::window.withAlpha (0.8f * alpha));
            g.fillRect (area.getRight() - 0.5f, r.getY() + 6.0f, 1.0f, r.getHeight() - 12.0f);
        }
    }
}

//==============================================================================
LiveDisplay::LiveDisplay()
{
    setOpaque (true);
}

void LiveDisplay::push (const LiveFrame& f, float a4Hz)
{
    head = (head + 1) % kFrames;
    frames[(size_t) head] = f;
    a4 = a4Hz;

    const bool live = f.active || f.heard;
    const auto& before = frames[(size_t) ((head + kFrames - 1) % kFrames)];
    const bool changed = f.enabled != before.enabled || f.triadCount != before.triadCount || f.held != before.held
                      || f.frozenNote != before.frozenNote || f.harmonyFrozen != before.harmonyFrozen;
    idleFrames = live || changed ? 0 : juce::jmin (kFrames, idleFrames + 1);

    if (live)
    {
        centre += ((f.active ? f.outputPitch : f.pitch) - centre) * 0.06f;
        shownCents += ((f.pitch - std::round (f.pitch)) * 100.0f - shownCents) * 0.45f;
    }

    // Once everything has scrolled out there is nothing left to move.
    if (idleFrames < kFrames)
        repaint();
}

void LiveDisplay::paint (juce::Graphics& g)
{
    g.fillAll (colour::display);
    auto area = getLocalBounds().toFloat();

    auto left = area.removeFromLeft ((float) kNoteWidth);
    g.setColour (colour::separator);
    g.fillRect (area.getX(), area.getY() + kPad, 1.0f, area.getHeight() - 2.0f * kPad);

    paintNote (g, left.reduced (kPad));
    paintHistory (g, area.reduced (kPad, 20.0f).withTrimmedLeft (8.0f));
}

void LiveDisplay::paintNote (juce::Graphics& g, juce::Rectangle<float> r)
{
    const auto& f = frames[(size_t) head];
    const bool live = f.active || f.heard;
    const int shown = f.active ? f.note : (f.heard ? juce::roundToInt (f.pitch) : -1);

    // the note, large: what is sent in the accent colour, what is only heard in grey
    const float baseline = r.getY() + 74.0f;
    if (! f.enabled)
    {
        g.setColour (colour::tertiary);
        g.setFont (font (68.0f).withExtraKerningFactor (-0.02f));
        g.drawSingleLineText ("Aus", juce::roundToInt (r.getX()) - 3, juce::roundToInt (baseline));
    }
    else if (shown >= 0)
    {
        const auto name = noteName (shown);
        const auto letter = name.dropLastCharacters (shown >= 12 ? 1 : 2);
        const auto octave = name.substring (letter.length());
        const auto big = font (68.0f).withExtraKerningFactor (-0.02f);
        const auto small = font (24.0f);
        const float wBig = juce::GlyphArrangement::getStringWidth (big, letter);

        g.setColour (f.active ? colour::accent : colour::secondary);
        g.setFont (big);
        g.drawSingleLineText (letter, juce::roundToInt (r.getX()) - 3, juce::roundToInt (baseline));
        g.setFont (small);
        g.drawSingleLineText (octave, juce::roundToInt (r.getX() + wBig + 1.0f), juce::roundToInt (baseline));
    }

    // tuning: a plain scale from -50 to +50 cent
    const float y = baseline + 34.0f;
    const auto scale = juce::Rectangle<float> (r.getX(), y, r.getWidth(), 1.0f);
    g.setColour (colour::separator.brighter (0.25f));
    g.fillRect (scale);
    for (int t = -2; t <= 2; ++t)
    {
        const float x = scale.getX() + scale.getWidth() * (0.5f + (float) t * 0.25f);
        const float h = t == 0 ? 10.0f : 5.0f;
        g.fillRect (x - 0.5f, y - h, 1.0f, h);
    }

    if (live && f.enabled)
    {
        const float cents = juce::jlimit (-50.0f, 50.0f, shownCents);
        const float x = scale.getX() + scale.getWidth() * (0.5f + cents / 100.0f);
        g.setColour (f.active ? colour::accent : colour::secondary);
        g.fillRect (x - 1.0f, y - 12.0f, 2.0f, 16.0f);
    }

    // numbers under the scale
    const auto row = juce::Rectangle<float> (r.getX(), y + 12.0f, r.getWidth(), 18.0f);
    g.setFont (font (13.0f));
    if (! f.enabled)
    {
        g.setColour (colour::tertiary);
        g.drawText (text ("Es wird nichts gesendet"), row, juce::Justification::centredLeft);
    }
    else if (live)
    {
        const int c = juce::roundToInt (juce::jlimit (-50.0f, 50.0f, shownCents));
        const auto centsText = (c > 0 ? "+" : (c < 0 ? text ("\xe2\x88\x92") : juce::String())) + juce::String (std::abs (c)) + " ct";
        const float hz = a4 * std::pow (2.0f, (f.pitch - 69.0f) / 12.0f);
        g.setColour (colour::text);
        g.drawText (centsText, row, juce::Justification::centredLeft);
        g.setColour (colour::secondary);
        g.drawText (juce::String (hz, 1).replaceCharacter ('.', ',') + " Hz", row, juce::Justification::centredRight);
    }
    else
    {
        g.setColour (colour::tertiary);
        g.drawText (text ("Kein Ton"), row, juce::Justification::centredLeft);
    }

    // what goes out; the master switch sits below these two lines
    if (! f.enabled)
        return;

    auto out = juce::Rectangle<float> (r.getX(), row.getBottom() + 12.0f, r.getWidth(), 17.0f);
    g.setFont (font (12.5f));
    g.setColour (f.active || f.frozenNote >= 0 ? colour::secondary : colour::tertiary);
    g.drawText (f.frozenNote >= 0 ? "Solo eingefroren: " + noteName (f.frozenNote)
                : ! f.active      ? text ("Keine MIDI-Note")
                                  : "MIDI-Note " + juce::String (f.note) + (f.held ? ", gehalten" : ""),
                out, juce::Justification::centredLeft);

    if (f.triadCount > 0)
    {
        juce::String chord = f.harmonyFrozen ? "Harmonie eingefroren:" : "Harmonie";
        for (int i = 0; i < f.triadCount; ++i)
            chord << " " << noteName (f.triad[i]);
        g.setColour (colour::secondary);
        g.drawText (chord, out.translated (0.0f, 17.0f), juce::Justification::centredLeft, true);
    }
}

void LiveDisplay::paintHistory (juce::Graphics& g, juce::Rectangle<float> r)
{
    constexpr float labelWidth = 30.0f;
    const auto plot = r.withTrimmedLeft (labelWidth).withTrimmedBottom (16.0f);
    constexpr float span = 14.0f; // semitones from bottom to top
    const float top = centre + span * 0.5f;
    auto yFor = [&] (float pitch) { return plot.getY() + (top - pitch) / span * plot.getHeight(); };
    auto xFor = [&] (int age) { return plot.getRight() - (float) age / (float) (kFrames - 1) * plot.getWidth(); };

    // semitone lines; natural notes labelled
    g.saveState();
    g.reduceClipRegion (r.toNearestInt());
    for (int n = (int) std::floor (top - span) - 1; n <= (int) std::ceil (top) + 1; ++n)
    {
        const float y = yFor ((float) n);
        if (y < plot.getY() - 1.0f || y > plot.getBottom() + 1.0f)
            continue;

        const int pc = ((n % 12) + 12) % 12;
        const bool natural = pc != 1 && pc != 3 && pc != 6 && pc != 8 && pc != 10;
        g.setColour (pc == 0 ? colour::separator.brighter (0.15f) : colour::grid);
        g.fillRect (plot.getX(), std::round (y), plot.getWidth(), 1.0f);
        if (natural)
        {
            g.setColour (pc == 0 ? colour::secondary : colour::tertiary);
            g.setFont (font (11.0f));
            g.drawText (noteName (n), juce::Rectangle<float> (r.getX(), y - 7.0f, labelWidth - 8.0f, 14.0f),
                        juce::Justification::centredRight);
        }
    }

    // time marks
    g.setColour (colour::tertiary);
    g.setFont (font (11.0f));
    const auto timeRow = juce::Rectangle<float> (plot.getX(), plot.getBottom() + 3.0f, plot.getWidth(), 13.0f);
    g.drawText (text ("\xe2\x88\x92" "6 s"), timeRow, juce::Justification::centredLeft);
    g.drawText (text ("\xe2\x88\x92" "3 s"), timeRow, juce::Justification::centred);
    g.drawText ("jetzt", timeRow, juce::Justification::centredRight);
    g.restoreState();

    g.saveState();
    g.reduceClipRegion (plot.toNearestInt());

    // the MIDI notes sent, as plain bars
    const float rowH = plot.getHeight() / span;
    for (int age = 0; age < kFrames;)
    {
        const auto& f = frames[(size_t) ((head - age + kFrames) % kFrames)];
        if (! f.active) { ++age; continue; }

        int end = age;
        while (end + 1 < kFrames)
        {
            const auto& next = frames[(size_t) ((head - end - 1 + kFrames) % kFrames)];
            if (! next.active || next.note != f.note)
                break;
            ++end;
        }

        const float x0 = xFor (end), x1 = xFor (age);
        g.setColour (colour::accent.withAlpha (0.18f));
        g.fillRect (juce::Rectangle<float> (x0, yFor ((float) f.note) - rowH * 0.5f, juce::jmax (2.0f, x1 - x0), rowH));
        age = end + 1;
    }

    // the pitch itself: accent while sent, grey while only heard
    for (const bool sent : { false, true })
    {
        juce::Path p;
        bool drawing = false;
        for (int a = kFrames - 1; a >= 0; --a)
        {
            const auto& f = frames[(size_t) ((head - a + kFrames) % kFrames)];
            const bool use = sent ? f.active : (f.heard && ! f.active);
            if (! use) { drawing = false; continue; }

            const juce::Point<float> pt (xFor (a), yFor (sent ? f.outputPitch : f.pitch));
            if (drawing) p.lineTo (pt); else p.startNewSubPath (pt);
            drawing = true;
        }
        g.setColour (sent ? colour::accent : colour::tertiary);
        g.strokePath (p, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    g.restoreState();

    if (idleFrames >= kFrames)
    {
        g.setColour (colour::tertiary);
        g.setFont (font (13.0f));
        g.drawText (text ("Gespielte T\xc3\xb6ne und gesendete MIDI-Noten der letzten 6 Sekunden"),
                    plot, juce::Justification::centred);
    }
}

//==============================================================================
void LevelMeter::update (float levelDb, float gateDb, float gateOffDb, bool clipped)
{
    const float next = juce::jmax (levelDb, shown - 1.5f); // quick up, slower down
    if (std::abs (next - shown) < 0.05f && gateDb == gate && gateOffDb == gateOff && clipped == clip)
        return;
    shown = next;
    gate = gateDb;
    gateOff = gateOffDb;
    clip = clipped;
    repaint();
}

void LevelMeter::paint (juce::Graphics& g)
{
    constexpr float low = -72.0f, high = 0.0f;
    const auto r = getLocalBounds().toFloat().withSizeKeepingCentre ((float) getWidth(), 4.0f);
    auto toX = [&] (float db) { return r.getX() + juce::jlimit (0.0f, 1.0f, (db - low) / (high - low)) * r.getWidth(); };

    g.setColour (colour::control.brighter (0.12f));
    g.fillRoundedRectangle (r, 2.0f);

    const float x = toX (shown);
    if (x > r.getX() + 0.5f)
    {
        // grey below the threshold, accent where a note can start, red when clipping
        g.setColour (clip ? colour::problem : (shown >= gate ? colour::accent : colour::secondary));
        g.fillRoundedRectangle (r.withRight (x), 2.0f);
    }

    // the two thresholds: a note may start above the tall mark and ends below the short one
    const float gx = std::round (toX (gate)), ox = std::round (toX (juce::jmin (gate, gateOff)));
    if (ox < gx)
    {
        g.setColour (colour::secondary);
        g.fillRect (ox - 0.5f, (float) getHeight() * 0.5f - 4.0f, 1.0f, 8.0f);
    }
    g.setColour (colour::text);
    g.fillRect (gx - 0.5f, (float) getHeight() * 0.5f - 6.0f, 1.0f, 12.0f);
}
