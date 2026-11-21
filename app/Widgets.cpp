#include "Widgets.h"

#include "Brand.h"

#include <cmath>

using namespace brand;

namespace
{
    juce::String text (const char* utf8) { return juce::String::fromUTF8 (utf8); }

    void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title)
    {
        g.fillAll (colour::window);
        g.setColour (colour::panel);
        g.fillRoundedRectangle (r, 12.0f);
        g.setColour (colour::line.withAlpha (0.7f));
        g.drawRoundedRectangle (r.reduced (0.5f), 12.0f, 1.0f);

        g.setColour (colour::textFaint);
        g.setFont (headingFont());
        g.drawText (title, r.reduced (18.0f, 14.0f).removeFromTop (14.0f), juce::Justification::centredLeft);
    }
}

juce::String noteName (int midiNote)
{
    static const char* names[] = { "C", "C\xe2\x99\xaf", "D", "D\xe2\x99\xaf", "E", "F",
                                   "F\xe2\x99\xaf", "G", "G\xe2\x99\xaf", "A", "A\xe2\x99\xaf", "B" };
    return text (names[((midiNote % 12) + 12) % 12]) + juce::String (midiNote / 12 - 1);
}

//==============================================================================
NoteDisplay::NoteDisplay()
{
    setOpaque (true);
}

void NoteDisplay::update (const LiveFrame& f, float a4Hz)
{
    const bool changed = f.active != frame.active || f.heard != frame.heard || f.note != frame.note
                      || std::abs (f.pitch - frame.pitch) > 0.002f || std::abs (f.levelDb - frame.levelDb) > 0.5f;
    frame = f;
    a4 = a4Hz;

    const float target = (f.active || f.heard) ? (f.pitch - std::round (f.pitch)) * 100.0f : 0.0f;
    const float before = shownCents;
    shownCents += (target - shownCents) * 0.45f;

    if (changed || std::abs (shownCents - before) > 0.05f)
        repaint();
}

void NoteDisplay::paint (juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat();
    drawPanel (g, area, "NOTE");

    const bool live = frame.active || frame.heard;
    const int shownNote = frame.active ? frame.note : (frame.heard ? juce::roundToInt (frame.pitch) : -1);

    // state pill, top right
    {
        const juce::String state = frame.active ? "SENDET" : (frame.heard ? text ("H\xc3\x96RT") : "BEREIT");
        const auto c = frame.active ? colour::accent : (frame.heard ? colour::textDim : colour::textFaint);
        auto pill = juce::Rectangle<float> (area.getRight() - 104.0f, 12.0f, 86.0f, 20.0f);
        g.setColour (c.withAlpha (frame.active ? 0.16f : 0.08f));
        g.fillRoundedRectangle (pill, 10.0f);
        g.setColour (c);
        g.fillEllipse (pill.getX() + 10.0f, pill.getCentreY() - 3.0f, 6.0f, 6.0f);
        g.setFont (headingFont());
        g.drawText (state, pill.withTrimmedLeft (22.0f), juce::Justification::centredLeft);
    }

    // the note
    auto left = area.withWidth (area.getWidth() * 0.46f).withTrimmedTop (30.0f).withTrimmedBottom (12.0f);
    if (shownNote >= 0)
    {
        const auto name = noteName (shownNote);
        const auto letter = name.dropLastCharacters (shownNote >= 12 ? 1 : 2);
        const auto octave = name.substring (letter.length());

        const auto big = font (96.0f, true), small = font (34.0f, true);
        const float wBig = juce::GlyphArrangement::getStringWidth (big, letter);
        const float wSmall = juce::GlyphArrangement::getStringWidth (small, octave);
        const float x = left.getCentreX() - (wBig + wSmall + 4.0f) * 0.5f;
        const float baseline = left.getCentreY() + 34.0f;

        if (frame.active)
        {
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xffffdc9a), x, baseline - 70.0f,
                                                     colour::accentDeep, x, baseline, false));
        }
        else
        {
            g.setColour (colour::textDim);
        }
        g.setFont (big);
        g.drawSingleLineText (letter, juce::roundToInt (x), juce::roundToInt (baseline));
        g.setFont (small);
        g.drawSingleLineText (octave, juce::roundToInt (x + wBig + 4.0f), juce::roundToInt (baseline));
    }
    else
    {
        g.setColour (colour::textFaint);
        g.setFont (font (15.0f));
        g.drawText (text ("Spiel einen Ton"), left, juce::Justification::centred);
    }

    // details and tuner on the right
    auto right = area.withTrimmedLeft (area.getWidth() * 0.46f).reduced (8.0f, 0.0f).withTrimmedRight (18.0f);
    right.removeFromTop (44.0f);

    const float hz = a4 * std::pow (2.0f, (frame.pitch - 69.0f) / 12.0f);
    auto info = right.removeFromTop (44.0f);
    auto infoCol = [&] (juce::Rectangle<float> r, const juce::String& label, const juce::String& value)
    {
        g.setColour (colour::textFaint);
        g.setFont (headingFont());
        g.drawText (label, r.removeFromTop (16.0f), juce::Justification::centredLeft);
        g.setColour (live ? colour::text : colour::textFaint);
        g.setFont (font (17.0f));
        g.drawText (value, r, juce::Justification::centredLeft);
    };
    const float colW = info.getWidth() / 3.0f;
    infoCol (info.removeFromLeft (colW), "FREQUENZ", live ? juce::String (hz, 1).replaceCharacter ('.', ',') + " Hz" : text ("\xe2\x80\x93"));
    infoCol (info.removeFromLeft (colW), "MIDI", frame.active ? juce::String (frame.note) : text ("\xe2\x80\x93"));
    infoCol (info, "PEGEL", frame.levelDb > -100.0f ? juce::String (juce::roundToInt (frame.levelDb)) + " dB" : text ("\xe2\x80\x93"));

    // tuner: where the string sits between two semitones
    right.removeFromTop (22.0f);
    auto tuner = right.removeFromTop (30.0f);
    const float cx = tuner.getCentreX(), cy = tuner.getCentreY();
    const float half = tuner.getWidth() * 0.5f;

    g.setColour (colour::raised);
    g.fillRoundedRectangle (tuner.withSizeKeepingCentre (tuner.getWidth(), 6.0f), 3.0f);
    g.setColour (colour::good.withAlpha (0.25f));
    g.fillRoundedRectangle (juce::Rectangle<float> (half * 0.2f, 6.0f).withCentre ({ cx, cy }), 3.0f);
    g.setColour (colour::line.brighter (0.2f));
    for (int t = -2; t <= 2; ++t)
    {
        const float x = cx + (float) t * half * 0.5f;
        g.fillRect (x - 0.5f, cy - (t == 0 ? 11.0f : 7.0f), 1.0f, t == 0 ? 22.0f : 14.0f);
    }

    const float cents = juce::jlimit (-50.0f, 50.0f, shownCents);
    if (live)
    {
        const bool inTune = std::abs (cents) <= 5.0f;
        const auto c = inTune ? colour::good : (frame.active ? colour::accent : colour::textDim);
        const float x = cx + cents / 50.0f * half;
        g.setColour (c.withAlpha (0.25f));
        g.fillRoundedRectangle (juce::Rectangle<float> (10.0f, 28.0f).withCentre ({ x, cy }), 5.0f);
        g.setColour (c);
        g.fillRoundedRectangle (juce::Rectangle<float> (4.0f, 24.0f).withCentre ({ x, cy }), 2.0f);
    }

    auto scale = right.removeFromTop (20.0f);
    g.setFont (font (11.5f));
    g.setColour (colour::textFaint);
    g.drawText (text ("\xe2\x88\x92" "50"), scale, juce::Justification::centredLeft);
    g.drawText ("+50", scale, juce::Justification::centredRight);
    g.setColour (live ? colour::text : colour::textFaint);
    g.setFont (font (13.0f, true));
    const int c = juce::roundToInt (cents);
    g.drawText (live ? (c > 0 ? "+" : (c < 0 ? text ("\xe2\x88\x92") : juce::String())) + juce::String (std::abs (c)) + " Cent" : juce::String ("Cent"),
                scale, juce::Justification::centred);
}

//==============================================================================
PitchHistory::PitchHistory()
{
    setOpaque (true);
}

void PitchHistory::clear()
{
    frames.fill (LiveFrame());
    idleFrames = kFrames;
    repaint();
}

void PitchHistory::push (const LiveFrame& f)
{
    head = (head + 1) % kFrames;
    frames[(size_t) head] = f;

    const bool something = f.active || f.heard;
    idleFrames = something ? 0 : juce::jmin (kFrames, idleFrames + 1);

    if (something)
    {
        const float target = f.active ? f.outputPitch : f.pitch;
        centre += (target - centre) * 0.06f;
    }

    // Once everything has scrolled out of view there is nothing left to move.
    if (idleFrames < kFrames)
        repaint();
}

void PitchHistory::paint (juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat();
    drawPanel (g, area, "VERLAUF");

    g.setColour (colour::textFaint);
    g.setFont (font (11.5f));
    g.drawText (text ("letzte 6 s"), area.reduced (18.0f, 14.0f).removeFromTop (14.0f), juce::Justification::centredRight);

    const auto plot = area.reduced (18.0f, 0.0f).withTrimmedTop (38.0f).withTrimmedBottom (16.0f).withTrimmedLeft (30.0f);
    constexpr float span = 14.0f; // semitones from bottom to top
    const float top = centre + span * 0.5f;
    auto yFor = [&] (float pitch) { return plot.getY() + (top - pitch) / span * plot.getHeight(); };
    auto xFor = [&] (int age) { return plot.getRight() - (float) age / (float) (kFrames - 1) * plot.getWidth(); };

    // semitone grid with the note names
    g.saveState();
    g.reduceClipRegion (plot.expanded (30.0f, 0.0f).toNearestInt());
    for (int n = (int) std::floor (top - span); n <= (int) std::ceil (top); ++n)
    {
        const float y = yFor ((float) n);
        const int pitchClass = ((n % 12) + 12) % 12;
        const bool isC = pitchClass == 0;
        const bool natural = pitchClass != 1 && pitchClass != 3 && pitchClass != 6 && pitchClass != 8 && pitchClass != 10;
        g.setColour (colour::line.withAlpha (isC ? 0.9f : (natural ? 0.45f : 0.2f)));
        g.fillRect (plot.getX(), y - 0.5f, plot.getWidth(), 1.0f);
        if (natural)
        {
            g.setColour (isC ? colour::textDim : colour::textFaint);
            g.setFont (font (10.5f, isC));
            g.drawText (noteName (n), juce::Rectangle<float> (plot.getX() - 32.0f, y - 7.0f, 28.0f, 14.0f),
                        juce::Justification::centredRight);
        }
    }
    g.restoreState();

    g.saveState();
    g.reduceClipRegion (plot.toNearestInt());

    // the notes sent, as bars
    const float rowH = plot.getHeight() / span;
    int age = 0;
    while (age < kFrames)
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
        const auto bar = juce::Rectangle<float> (x0, yFor ((float) f.note) - rowH * 0.36f, juce::jmax (3.0f, x1 - x0), rowH * 0.72f);
        g.setColour (colour::accent.withAlpha (0.16f));
        g.fillRoundedRectangle (bar, juce::jmin (5.0f, bar.getHeight() * 0.5f));
        g.setColour (colour::accent.withAlpha (0.35f));
        g.drawRoundedRectangle (bar, juce::jmin (5.0f, bar.getHeight() * 0.5f), 1.0f);
        age = end + 1;
    }

    // the pitch itself: bright while sent, faint while only heard
    auto drawTrace = [&] (bool sent)
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
        g.setColour (sent ? colour::accent : colour::textFaint);
        g.strokePath (p, juce::PathStrokeType (sent ? 2.2f : 1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };
    drawTrace (false);
    drawTrace (true);

    // the newest point
    const auto& now = frames[(size_t) head];
    if (now.active || now.heard)
    {
        const juce::Point<float> pt (xFor (0), yFor (now.active ? now.outputPitch : now.pitch));
        const auto c = now.active ? colour::accent : colour::textDim;
        g.setColour (c.withAlpha (0.25f));
        g.fillEllipse (juce::Rectangle<float> (14.0f, 14.0f).withCentre (pt));
        g.setColour (c);
        g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre (pt));
    }
    g.restoreState();

    if (idleFrames >= kFrames)
    {
        g.setColour (colour::textFaint);
        g.setFont (font (13.0f));
        g.drawText (text ("Hier erscheint, was du spielst und was als MIDI rausgeht."), plot, juce::Justification::centred);
    }
}

//==============================================================================
void LevelMeter::update (float levelDb, float gateDb, bool clipped)
{
    const float next = juce::jmax (levelDb, shown - 1.5f); // quick up, slower down
    if (std::abs (next - shown) < 0.05f && gateDb == gate && clipped == clip)
        return;
    shown = next;
    gate = gateDb;
    clip = clipped;
    repaint();
}

void LevelMeter::paint (juce::Graphics& g)
{
    constexpr float low = -72.0f, high = 0.0f;
    const auto r = getLocalBounds().toFloat().reduced (0.0f, 2.0f);
    auto toX = [&] (float db) { return r.getX() + juce::jlimit (0.0f, 1.0f, (db - low) / (high - low)) * r.getWidth(); };

    g.setColour (colour::raised);
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);

    const float x = toX (shown);
    if (x > r.getX() + 1.0f)
    {
        const auto c = clip ? colour::bad : (shown >= gate ? colour::good : colour::textFaint);
        g.setGradientFill (juce::ColourGradient (c.withAlpha (0.55f), r.getX(), 0.0f, c, x, 0.0f, false));
        g.fillRoundedRectangle (r.withRight (x), r.getHeight() * 0.5f);
    }

    // gate threshold
    const float gx = toX (gate);
    g.setColour (colour::text);
    g.fillRoundedRectangle (gx - 1.0f, r.getY() - 2.0f, 2.0f, r.getHeight() + 4.0f, 1.0f);
}
