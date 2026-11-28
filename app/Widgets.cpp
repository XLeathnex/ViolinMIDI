#include "Widgets.h"

#include "Brand.h"
#include "Theme.h"

#include <cmath>
#include <iterator>

using namespace brand;

namespace
{
    juce::String text (const char* utf8) { return juce::String::fromUTF8 (utf8); }

    constexpr float kPad = 24.0f;

    // The graphs on the right, from the top down: pitch, a small gap, loudness, time marks.
    constexpr float kGraphTop = 16.0f, kGraphBottom = 14.0f;
    constexpr float kLabelWidth = 30.0f;
    constexpr float kLevelHeight = 64.0f, kLevelGap = 10.0f, kTimeRowHeight = 16.0f;

    // The loudness scale, the same as the input meter's.
    constexpr float kLowDb = -72.0f, kHighDb = 0.0f;

    // The emblem behind the grid: there, but never in the way of the lines.
    constexpr float kWatermarkOpacity = 0.08f;

    /** One zoom step: the time and pitch span shown, and how far apart the time marks are. */
    struct ZoomStep { float seconds, semitones, tick; };

    // Time and pitch zoom together: short and narrow for fast runs and vibrato,
    // long and wide for whole phrases. The middle one is the standard view.
    constexpr ZoomStep kZoomSteps[] = { { 1.5f, 5.0f, 0.5f }, { 3.0f, 8.0f, 1.0f }, { 6.0f, 14.0f, 1.0f },
                                        { 12.0f, 24.0f, 2.0f }, { 24.0f, 36.0f, 4.0f } };
    constexpr int kStandardZoom = 2;
    constexpr int kZoomCount = (int) std::size (kZoomSteps);

    static_assert ((float) LiveDisplay::kFrames > kZoomSteps[kZoomCount - 1].seconds * (float) LiveDisplay::kFramesPerSecond,
                   "the history must hold the longest zoom step");

    // The zoom strip in the top left corner of the pitch graph.
    constexpr int kStripInset = 6, kStripHeight = 22, kSignWidth = 26, kResetWidth = 70;

    const juce::String& minus()
    {
        static const juce::String m = text ("\xe2\x88\x92");
        return m;
    }

    /** 1.5 -> "1,5", 6.0 -> "6". */
    juce::String decimal (float value)
    {
        const float tenths = std::round (value * 10.0f);
        if (std::abs (tenths - 10.0f * std::round (value)) < 0.5f)
            return juce::String (juce::roundToInt (value));
        return juce::String (tenths / 10.0f, 1).replaceCharacter ('.', ',');
    }

    /** A level as the controls show it: one decimal, decimal comma, the real minus sign. */
    juce::String decibels (float db)
    {
        const auto digits = juce::String (std::abs (db), 1).replaceCharacter ('.', ',');
        return (db < -0.05f ? minus() : juce::String()) + digits + " dB";
    }

    bool isNatural (int pitchClass)
    {
        return pitchClass != 1 && pitchClass != 3 && pitchClass != 6 && pitchClass != 8 && pitchClass != 10;
    }

    /** The pitch to show for a live frame: the detected one while there is one,
        otherwise (a note that only rings on) the pitch being sent. */
    float measuredPitch (const LiveFrame& f)
    {
        return f.heard ? f.pitch : f.outputPitch;
    }

    /** The played melody goes out as MIDI. Not while a note is held by the
        pentatonic sustain: then what is heard is a foreign note that is not sent. */
    bool melodySent (const LiveFrame& f)
    {
        return f.active && ! f.held;
    }

    /** A line through the points with the corners rounded off: quadratic curves
        from one midpoint to the next, so 30 points a second read as a played line.
        With `join` it continues the current sub-path instead of starting one. */
    void addSmooth (juce::Path& p, const std::vector<juce::Point<float>>& points, bool join = false)
    {
        if (join)
            p.lineTo (points.front());
        else
            p.startNewSubPath (points.front());

        for (size_t i = 1; i + 1 < points.size(); ++i)
            p.quadraticTo (points[i], (points[i] + points[i + 1]) * 0.5f);
        p.lineTo (points.back());
    }

    const char* const kDisplayTip =
        "Live-Anzeige: links die gespielte Note, rechts Tonh\xc3\xb6he und Lautst\xc3\xa4rke der letzten Sekunden. "
        "Ganz rechts ist jetzt.";

    const char* const kNoteTip =
        "Die gespielte Note: rot, solange sie als MIDI gesendet wird, grau, wenn sie nur geh\xc3\xb6rt wird. "
        "Darunter die Stimmung von \xe2\x88\x92" "50 bis +50 Cent und die Frequenz, ganz unten, was gerade hinausgeht.";

    const char* const kPitchTip =
        "Tonh\xc3\xb6henverlauf, ganz rechts ist jetzt. Die silberwei\xc3\x9f" "e Linie mit rotem Schein ist die erkannte "
        "Melodie, solange sie als MIDI gesendet wird; grau ist, was nur geh\xc3\xb6rt, aber nicht gesendet wird. Die roten "
        "Balken sind die gesendeten MIDI-Noten, ihre Kante links ist der Notenbeginn. Mit \xe2\x88\x92 und + oben links "
        "zoomen: kurz f\xc3\xbcr schnelle L\xc3\xa4ufe und Vibrato, lang f\xc3\xbcr ganze Phrasen. Standard holt die "
        "normale Ansicht zur\xc3\xbc" "ck.";

    const char* const kLevelTip =
        "Lautst\xc3\xa4rkeverlauf im selben Zeitraster wie die Tonh\xc3\xb6he dar\xc3\xbc" "ber, von \xe2\x88\x92" "72 bis 0 dB: "
        "Bogenansatz, Crescendo und Decrescendo direkt unter den gespielten T\xc3\xb6nen. Rot, solange eine Note gesendet wird. "
        "Die obere Linie (In) ist die Einschalt-Schwelle: dar\xc3\xbc" "ber kann ein Ton beginnen. Die gestrichelte (Out) "
        "ist die Ausschalt-Schwelle: darunter endet er. Beide folgen den Reglern. Die Punkte zeigen, wo eine Note "
        "beginnt (rot) und wo sie endet (silbern).";
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
    : frames ((size_t) kFrames)
{
    setOpaque (true);
    setTooltip (text (kDisplayTip));

    // A small segmented strip: zoom out, zoom in, back to the standard view.
    zoomOutButton.setButtonText (minus());
    zoomInButton.setButtonText ("+");
    zoomResetButton.setButtonText ("Standard");
    zoomOutButton.setConnectedEdges (juce::Button::ConnectedOnRight);
    zoomInButton.setConnectedEdges (juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight);
    zoomResetButton.setConnectedEdges (juce::Button::ConnectedOnLeft);

    for (auto* b : { &zoomOutButton, &zoomInButton, &zoomResetButton })
    {
        // The space bar belongs to the master switch, so these never take the keyboard.
        b->setWantsKeyboardFocus (false);
        b->setMouseClickGrabsKeyboardFocus (false);
        addAndMakeVisible (*b);
    }

    zoomOutButton.onClick   = [this] { setZoom (zoom + 1); };
    zoomInButton.onClick    = [this] { setZoom (zoom - 1); };
    zoomResetButton.onClick = [this] { setZoom (kStandardZoom); };
    setZoom (kStandardZoom);
}

void LiveDisplay::push (const LiveFrame& f, float a4Hz, float gateDb, float gateOffDb)
{
    const auto& before = frames[(size_t) head];
    const bool changed = f.enabled != before.enabled || f.triadCount != before.triadCount || f.held != before.held
                      || f.frozenNote != before.frozenNote || f.harmonyFrozen != before.harmonyFrozen;

    head = (head + 1) % kFrames;
    frames[(size_t) head] = f;
    a4 = a4Hz;

    const bool thresholdsMoved = ! juce::exactlyEqual (gateDb, gateIn) || ! juce::exactlyEqual (gateOffDb, gateOut);
    gateIn = gateDb;
    gateOut = gateOffDb;

    // Something to show: a pitch, a note, or a level the loudness curve lifts off its floor for.
    const bool live = f.active || f.heard;
    const bool audible = f.levelDb > kLowDb + 0.5f;
    idleFrames = live || audible || changed ? 0 : juce::jmin (kFrames + 1, idleFrames + 1);

    if (live)
    {
        // What is played; while a note only rings on (held, bridged, released) the
        // detector has no pitch (0), so what is sent stands in for it.
        const float target = measuredPitch (f);

        // Follow the playing calmly. A leap to near the edge or beyond is caught up
        // with fast, so it never leaves a narrow view for long - but only once it
        // has lasted a few frames, so a one-frame octave error does not shake the grid.
        const float room = spanSemitones() * 0.5f - 1.0f;
        outsideFrames = std::abs (target - centre) > room ? outsideFrames + 1 : 0;
        centre += (target - centre) * (outsideFrames >= 3 ? 0.35f : 0.06f);
        shownCents += ((target - std::round (target)) * 100.0f - shownCents) * 0.45f;
    }
    else
    {
        outsideFrames = 0;
    }

    // Once everything has scrolled out of the view there is nothing left to move;
    // the last repaint after that shows the empty graph with its hint.
    if (idleFrames <= spanFrames() + 1 || thresholdsMoved)
        repaint();
}

int LiveDisplay::spanFrames() const
{
    return juce::roundToInt (kZoomSteps[zoom].seconds * (float) kFramesPerSecond);
}

float LiveDisplay::spanSemitones() const
{
    return kZoomSteps[zoom].semitones;
}

void LiveDisplay::setZoom (int step)
{
    zoom = juce::jlimit (0, kZoomCount - 1, step);

    const auto describe = [] (const ZoomStep& z)
    {
        return decimal (z.seconds) + " s und " + juce::String (juce::roundToInt (z.semitones)) + text (" Halbt\xc3\xb6ne");
    };

    zoomOutButton.setTooltip (zoom < kZoomCount - 1
                                ? text ("Herauszoomen auf ") + describe (kZoomSteps[zoom + 1])
                                    + text (": mehr \xc3\x9c" "berblick f\xc3\xbcr lange Phrasen.")
                                : text ("Weiter geht es nicht heraus: ") + describe (kZoomSteps[zoom]) + ".");
    zoomInButton.setTooltip (zoom > 0
                               ? text ("Hineinzoomen auf ") + describe (kZoomSteps[zoom - 1])
                                   + text (": f\xc3\xbcr schnelle L\xc3\xa4ufe und Vibrato-Details.")
                               : text ("N\xc3\xa4her geht es nicht heran: ") + describe (kZoomSteps[zoom]) + ".");
    zoomResetButton.setTooltip (text ("Zur\xc3\xbc" "ck zur Standardansicht: ") + describe (kZoomSteps[kStandardZoom]) + ".");

    zoomOutButton.setEnabled (zoom < kZoomCount - 1);
    zoomInButton.setEnabled (zoom > 0);
    zoomResetButton.setEnabled (zoom != kStandardZoom);
    repaint();
}

LiveDisplay::Layout LiveDisplay::layout() const
{
    Layout l;
    auto area = getLocalBounds().toFloat();
    l.note = area.removeFromLeft ((float) kNoteWidth).reduced (kPad);

    auto r = area.reduced (kPad, 0.0f).withTrimmedLeft (8.0f).withTrimmedTop (kGraphTop).withTrimmedBottom (kGraphBottom);
    l.labels = r.removeFromLeft (kLabelWidth);
    l.timeRow = r.removeFromBottom (kTimeRowHeight);
    l.level = r.removeFromBottom (kLevelHeight);
    r.removeFromBottom (kLevelGap);
    l.pitch = r;
    return l;
}

juce::Rectangle<int> LiveDisplay::overlayArea() const
{
    // Top left of the pitch graph, under the zoom strip: over the older part of the
    // history. The right edge is "now", where the player looks; nothing may cover it.
    constexpr float kWidth = 120.0f, kHeight = 140.0f, kGap = 6.0f;
    const auto plot = layout().pitch;
    const float top = plot.getY() + (float) (kStripInset + kStripHeight) + kGap;
    const float height = juce::jmax (0.0f, juce::jmin (kHeight, plot.getBottom() - (float) kStripInset - top));
    const float width = juce::jmax (0.0f, juce::jmin (kWidth, plot.getWidth() * 0.4f));
    return juce::Rectangle<float> (plot.getX() + (float) kStripInset, top, width, height).toNearestInt();
}

void LiveDisplay::resized()
{
    const auto plot = layout().pitch.toNearestInt();
    auto strip = juce::Rectangle<int> (plot.getX() + kStripInset, plot.getY() + kStripInset,
                                       2 * kSignWidth + kResetWidth, kStripHeight);
    zoomOutButton.setBounds (strip.removeFromLeft (kSignWidth));
    zoomInButton.setBounds (strip.removeFromLeft (kSignWidth));
    zoomResetButton.setBounds (strip);
}

juce::String LiveDisplay::getTooltip()
{
    // One explanation for each part, so the tip says what is under the mouse.
    const auto l = layout();
    const auto p = getMouseXYRelative().toFloat();
    const float split = l.pitch.getBottom() + kLevelGap * 0.5f;

    if (p.x < (float) kNoteWidth)
        return text (kNoteTip);
    if (p.x >= l.labels.getX() && p.y < split)
        return text (kPitchTip);
    if (p.x >= l.labels.getX() && p.y < l.level.getBottom())
        return text (kLevelTip);
    return SettableTooltipClient::getTooltip();
}

void LiveDisplay::paint (juce::Graphics& g)
{
    g.fillAll (colour::display);
    const auto l = layout();

    g.setColour (colour::hairline);
    g.fillRect ((float) kNoteWidth, kPad, 1.0f, (float) getHeight() - 2.0f * kPad);

    paintNote (g, l.note);
    paintWatermark (g, l.pitch);
    paintTimeGrid (g, l);
    paintPitch (g, l);
    paintLevel (g, l);
}

void LiveDisplay::paintNote (juce::Graphics& g, juce::Rectangle<float> r)
{
    const auto& f = frames[(size_t) head];
    const bool live = f.active || f.heard;
    const int shown = f.active ? f.note : (f.heard ? juce::roundToInt (f.pitch) : -1);

    // the note, large: what is sent in crimson, what is only heard in grey
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

        g.setColour (f.active ? colour::crimson : colour::secondary);
        g.setFont (big);
        g.drawSingleLineText (letter, juce::roundToInt (r.getX()) - 3, juce::roundToInt (baseline));
        g.setFont (small);
        g.drawSingleLineText (octave, juce::roundToInt (r.getX() + wBig + 1.0f), juce::roundToInt (baseline));
    }

    // tuning: a plain scale from -50 to +50 cent, the ticks in silver
    const float y = baseline + 34.0f;
    const auto scale = juce::Rectangle<float> (r.getX(), y, r.getWidth(), 1.0f);
    g.setColour (colour::separator.brighter (0.35f));
    g.fillRect (scale);
    for (int t = -2; t <= 2; ++t)
    {
        const float x = scale.getX() + scale.getWidth() * (0.5f + (float) t * 0.25f);
        const float h = t == 0 ? 10.0f : 5.0f;
        g.setColour (t == 0 ? colour::silver : colour::silver.withAlpha (0.6f));
        g.fillRect (x - 0.5f, y - h, 1.0f, h);
    }

    if (live && f.enabled)
    {
        const float cents = juce::jlimit (-50.0f, 50.0f, shownCents);
        const float x = scale.getX() + scale.getWidth() * (0.5f + cents / 100.0f);
        g.setColour (f.active ? colour::crimson : colour::secondary);
        g.fillRect (x - 1.0f, y - 12.0f, 2.0f, 16.0f);
    }

    // the measured values under the scale
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
        const auto centsText = (c > 0 ? juce::String ("+") : (c < 0 ? minus() : juce::String())) + juce::String (std::abs (c)) + " ct";
        const float hz = a4 * std::pow (2.0f, (measuredPitch (f) - 69.0f) / 12.0f);
        g.setColour (colour::silver);
        g.drawText (centsText, row, juce::Justification::centredLeft);
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

void LiveDisplay::paintWatermark (juce::Graphics& g, juce::Rectangle<float> plot)
{
    // The emblem in the middle of the pitch graph, faint and a touch redder than
    // the logo. Drawn into an image once per size and screen scale, so each frame
    // only copies it.
    const float side = std::round (juce::jmin (plot.getWidth(), plot.getHeight()) * 0.9f);
    const auto centred = plot.withSizeKeepingCentre (side, side);
    const auto area = centred.withPosition (std::round (centred.getX()), std::round (centred.getY()));   // whole pixels, no blur
    const float scale = juce::jmax (1.0f, g.getInternalContext().getPhysicalPixelScaleFactor());

    if (area != watermarkArea || ! juce::approximatelyEqual (scale, watermarkScale))
    {
        watermarkArea = area;
        watermarkScale = scale;
        watermark = {};

        const auto logo = brand::emblem();
        const int pixels = juce::roundToInt (side * scale);
        if (logo.isValid() && pixels > 0)
        {
            juce::Image image (juce::Image::ARGB, pixels, pixels, true);
            {
                juce::Graphics ig (image);
                const auto all = juce::Rectangle<float> ((float) pixels, (float) pixels);
                brand::drawEmblem (ig, all, 1.0f);
                ig.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
                ig.setColour (colour::crimson.withAlpha (0.3f));
                ig.drawImage (logo, all, juce::RectanglePlacement::centred, true);
            }
            image.multiplyAllAlphas (kWatermarkOpacity);
            watermark = image;
        }
    }

    if (watermark.isValid())
        g.drawImage (watermark, area);
}

void LiveDisplay::paintTimeGrid (juce::Graphics& g, const Layout& l)
{
    // One time grid for both graphs, with the marks below; they follow the zoom.
    const auto& z = kZoomSteps[zoom];
    const int marks = juce::roundToInt (z.seconds / z.tick);
    const auto& plot = l.pitch;

    g.setFont (font (11.0f));
    for (int k = 0; k <= marks; ++k)
    {
        const float seconds = (float) k * z.tick;
        const float x = plot.getRight() - seconds / z.seconds * plot.getWidth();

        if (k > 0 && k < marks)
        {
            g.setColour (colour::grid);
            g.fillRect (std::round (x), plot.getY(), 1.0f, plot.getHeight());
            g.fillRect (std::round (x), l.level.getY(), 1.0f, l.level.getHeight());
        }

        const auto row = l.timeRow.withTrimmedTop (3.0f);
        g.setColour (colour::silver.withAlpha (k == 0 ? 0.9f : 0.6f));
        if (k == 0)
            g.drawText ("jetzt", row.withLeft (x - 60.0f).withRight (x), juce::Justification::centredRight);
        else if (k == marks)
            g.drawText (minus() + decimal (seconds) + " s", row.withLeft (x).withWidth (60.0f), juce::Justification::centredLeft);
        else
            g.drawText (minus() + decimal (seconds) + " s", row.withLeft (x - 30.0f).withWidth (60.0f), juce::Justification::centred);
    }
}

void LiveDisplay::paintPitch (juce::Graphics& g, const Layout& l)
{
    const auto plot = l.pitch;
    const float span = spanSemitones();
    const float top = centre + span * 0.5f;
    const int frameSpan = spanFrames();
    const float rowH = plot.getHeight() / span;
    const auto yFor = [plot, top, span] (float pitch) { return plot.getY() + (top - pitch) / span * plot.getHeight(); };
    const auto xFor = [plot, frameSpan] (int age) { return plot.getRight() - (float) age / (float) frameSpan * plot.getWidth(); };

    // Which rows get a name: all naturals as standard, also the sharps close up,
    // and further out only C, E, G and then C, G, so the names never collide.
    const auto named = [rowH] (int pc)
    {
        if (rowH >= 20.0f) return true;
        if (rowH >= 11.0f) return isNatural (pc);
        if (rowH >= 6.5f)  return pc == 0 || pc == 4 || pc == 7;
        return pc == 0 || pc == 7;
    };

    // semitone lines, C a little brighter; the note names in silver
    {
        juce::Graphics::ScopedSaveState state (g);
        // the names may reach a little left of their column (a sharp makes them wider); there is room before the hairline
        const float labelsLeft = l.labels.getX() - 8.0f;
        g.reduceClipRegion (juce::Rectangle<float> (labelsLeft, plot.getY() - 7.0f,
                                                    plot.getRight() - labelsLeft, plot.getHeight() + 14.0f).toNearestInt());
        g.setFont (font (11.0f));
        for (int n = (int) std::floor (top - span) - 1; n <= (int) std::ceil (top) + 1; ++n)
        {
            const float y = yFor ((float) n);
            if (y < plot.getY() - 1.0f || y > plot.getBottom() + 1.0f)
                continue;

            // far out, the sharps' lines would only add noise
            const int pc = ((n % 12) + 12) % 12;
            if (rowH < 6.5f && ! isNatural (pc))
                continue;

            g.setColour (pc == 0 ? colour::separator.brighter (0.15f) : colour::grid);
            g.fillRect (plot.getX(), std::round (y), plot.getWidth(), 1.0f);

            if (named (pc) && y < plot.getBottom() - 3.0f)
            {
                g.setColour (colour::silver.withAlpha (pc == 0 ? 1.0f : (isNatural (pc) ? 0.6f : 0.4f)));
                g.drawText (noteName (n), juce::Rectangle<float> (labelsLeft, y - 7.0f, kLabelWidth, 14.0f),
                            juce::Justification::centredRight, false);
            }
        }
    }

    // the curves may reach a few pixels past "now", so the live dot is whole
    juce::Graphics::ScopedSaveState state (g);
    g.reduceClipRegion (plot.withTrimmedRight (-7.0f).toNearestInt());

    // the MIDI notes sent, as plain bars; the left edge of each is its note-on
    const float barH = juce::jmin (rowH, 18.0f);
    g.saveState();
    g.reduceClipRegion (plot.toNearestInt());
    for (int age = 0; age <= frameSpan;)
    {
        const auto& f = frameAt (age);
        if (! f.active || f.note < 0)
        {
            ++age;
            continue;
        }

        int end = age;
        while (end + 1 <= frameSpan)
        {
            const auto& next = frameAt (end + 1);
            if (! next.active || next.note != f.note)
                break;
            ++end;
        }

        const float x0 = xFor (end), x1 = xFor (age);
        const auto bar = juce::Rectangle<float> (x0, yFor ((float) f.note) - barH * 0.5f, juce::jmax (2.0f, x1 - x0), barH);
        g.setColour (colour::crimson.withAlpha (0.16f));
        g.fillRect (bar);
        if (end < frameSpan)
        {
            g.setColour (colour::crimson.withAlpha (0.55f));
            g.fillRect (bar.withWidth (1.5f));
        }
        age = end + 1;
    }
    g.restoreState();

    // The detected melody: while it is sent, a crisp silver-white line with a
    // soft crimson glow; while it is only heard, a plain grey one.
    juce::Path sent, heard;
    std::vector<juce::Point<float>> points;
    points.reserve ((size_t) frameSpan + 2);
    bool runSent = false;
    const auto flush = [&]
    {
        if (points.size() > 1)
            addSmooth (runSent ? sent : heard, points);
        points.clear();
    };

    for (int age = frameSpan; age >= 0; --age)
    {
        const auto& f = frameAt (age);
        if (! f.heard)
        {
            flush();
            continue;
        }

        const bool sentNow = melodySent (f);
        if (! points.empty() && runSent != sentNow)
        {
            // the next colour starts where the last one ended, so the line has no gap
            const auto last = points.back();
            flush();
            points.push_back (last);
        }
        runSent = sentNow;
        points.push_back ({ xFor (age), yFor (f.pitch) });
    }
    flush();

    const juce::PathStrokeType::JointStyle joint = juce::PathStrokeType::curved;
    const juce::PathStrokeType::EndCapStyle cap = juce::PathStrokeType::rounded;
    g.setColour (colour::tertiary);
    g.strokePath (heard, juce::PathStrokeType (1.5f, joint, cap));
    g.setColour (colour::crimson.withAlpha (0.1f));
    g.strokePath (sent, juce::PathStrokeType (8.0f, joint, cap));
    g.setColour (colour::crimson.withAlpha (0.24f));
    g.strokePath (sent, juce::PathStrokeType (3.5f, joint, cap));
    g.setColour (colour::text);
    g.strokePath (sent, juce::PathStrokeType (1.5f, joint, cap));

    // the live end of the line
    const auto& now = frameAt (0);
    if (now.heard)
    {
        const juce::Point<float> pt (xFor (0), yFor (now.pitch));
        if (melodySent (now))
        {
            g.setColour (colour::crimson.withAlpha (0.3f));
            g.fillEllipse (juce::Rectangle<float> (11.0f, 11.0f).withCentre (pt));
        }
        g.setColour (melodySent (now) ? colour::crimson : colour::tertiary);
        g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (pt));
    }

    // nothing left in the view (push() repaints once more when the last trace has gone)
    if (idleFrames > frameSpan)
    {
        // The grid is wiped from under the words and the watermark put back, so
        // no line runs through the text and the emblem stays whole.
        const auto hint = text ("Gespielte T\xc3\xb6ne und gesendete MIDI-Noten der letzten ") + decimal (kZoomSteps[zoom].seconds)
                        + " Sekunden";
        const auto hintFont = font (13.0f);
        const auto box = plot.withSizeKeepingCentre (juce::GlyphArrangement::getStringWidth (hintFont, hint) + 16.0f, 20.0f)
                             .toNearestInt();
        {
            juce::Graphics::ScopedSaveState hintState (g);
            g.reduceClipRegion (box);
            g.fillAll (colour::display);
            if (watermark.isValid())
                g.drawImage (watermark, watermarkArea);
        }
        g.setColour (colour::tertiary);
        g.setFont (hintFont);
        g.drawText (hint, box, juce::Justification::centred, false);
    }
}

void LiveDisplay::paintLevel (juce::Graphics& g, const Layout& l)
{
    const auto lane = l.level;
    const int frameSpan = spanFrames();
    const float base = lane.getBottom();
    const auto yFor = [lane] (float db)
    {
        return lane.getY() + (kHighDb - juce::jlimit (kLowDb, kHighDb, db)) / (kHighDb - kLowDb) * lane.getHeight();
    };
    const auto xFor = [lane, frameSpan] (int age) { return lane.getRight() - (float) age / (float) frameSpan * lane.getWidth(); };

    // dB lines with their marks in the label column; the floor is a hairline
    g.setFont (font (10.0f));
    for (const float db : { 0.0f, -24.0f, -48.0f })
    {
        const float y = std::round (yFor (db));
        g.setColour (colour::grid);
        g.fillRect (lane.getX(), y, lane.getWidth(), 1.0f);
        g.setColour (colour::silver.withAlpha (0.5f));
        g.drawText (db < 0.0f ? minus() + juce::String (juce::roundToInt (-db)) : juce::String ("0"),
                    juce::Rectangle<float> (l.labels.getX(), y - 6.0f, kLabelWidth - 8.0f, 12.0f),
                    juce::Justification::centredRight);
    }
    g.setColour (colour::separator);
    g.fillRect (lane.getX(), std::round (base) - 1.0f, lane.getWidth(), 1.0f);

    // The level history in two colours: crimson while a note is sent, silver
    // otherwise. Each run is a line and a faint area down to the floor; where
    // the colour changes, the new run starts at the last point of the old one.
    juce::Path line[2], area[2];
    std::vector<std::pair<juce::Point<float>, bool>> edges;   // note-on (true) and note-off points
    std::vector<juce::Point<float>> points;
    points.reserve ((size_t) frameSpan + 2);
    int run = -1;
    const auto flush = [&]
    {
        if (run >= 0 && points.size() > 1)
        {
            addSmooth (line[run], points);
            area[run].startNewSubPath (points.front().x, base);
            addSmooth (area[run], points, true);
            area[run].lineTo (points.back().x, base);
            area[run].closeSubPath();
        }
        points.clear();
    };

    for (int age = frameSpan; age >= 0; --age)
    {
        const auto& f = frameAt (age);
        const int c = f.active ? 1 : 0;
        const juce::Point<float> pt (xFor (age), yFor (f.levelDb));

        if (c != run && run >= 0)
        {
            const auto last = points.back();
            edges.push_back ({ c == 1 ? pt : last, c == 1 });
            flush();
            points.push_back (last);
        }
        run = c;
        points.push_back (pt);
    }
    flush();

    juce::Graphics::ScopedSaveState state (g);
    g.reduceClipRegion (lane.expanded (0.0f, 2.0f).withTrimmedRight (-7.0f).toNearestInt());

    g.setColour (colour::silver.withAlpha (0.05f));
    g.fillPath (area[0]);
    g.setColour (colour::crimson.withAlpha (0.12f));
    g.fillPath (area[1]);

    // the thresholds: the In line solid, the Out line dashed, as the meter's tall and short marks
    const float inDb = gateIn, outDb = juce::jmin (gateIn, gateOut);
    const float yIn = std::round (yFor (inDb)), yOut = std::round (yFor (outDb));
    g.setColour (colour::silver.withAlpha (0.75f));
    g.fillRect (lane.getX(), yIn, lane.getWidth(), 1.0f);
    const float dashes[] = { 4.0f, 3.0f };
    g.setColour (colour::silver.withAlpha (0.55f));
    g.drawDashedLine ({ lane.getX(), yOut + 0.5f, lane.getRight(), yOut + 0.5f }, dashes, (int) std::size (dashes), 1.0f);

    const juce::PathStrokeType::JointStyle joint = juce::PathStrokeType::curved;
    const juce::PathStrokeType::EndCapStyle cap = juce::PathStrokeType::rounded;
    g.setColour (colour::silver.withAlpha (0.55f));
    g.strokePath (line[0], juce::PathStrokeType (1.2f, joint, cap));
    g.setColour (colour::crimson);
    g.strokePath (line[1], juce::PathStrokeType (1.5f, joint, cap));

    // where a note started (crimson) and ended (silver), right on the curve
    for (const auto& [pt, on] : edges)
    {
        g.setColour (on ? colour::crimson : colour::silver);
        g.fillEllipse (juce::Rectangle<float> (4.5f, 4.5f).withCentre (pt));
    }

    // the live end
    const auto& now = frameAt (0);
    if (now.levelDb > kLowDb + 0.5f)
    {
        const juce::Point<float> pt (xFor (0), yFor (now.levelDb));
        if (now.active)
        {
            g.setColour (colour::crimson.withAlpha (0.3f));
            g.fillEllipse (juce::Rectangle<float> (11.0f, 11.0f).withCentre (pt));
        }
        g.setColour (now.active ? colour::crimson : colour::silver);
        g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (pt));
    }

    // Names and values of the two lines, at the left where the oldest data is.
    // The In label sits above its line, the Out label below its own and to the
    // right of the first, so the two never cover each other.
    const auto labelFont = font (10.5f);
    const auto label = [&g, &labelFont, lane] (const juce::String& name, float db, float x, float lineY, bool above)
    {
        const auto value = decibels (db);
        const float wName = juce::GlyphArrangement::getStringWidth (labelFont, name);
        const float wValue = juce::GlyphArrangement::getStringWidth (labelFont, value);
        constexpr float h = 12.0f;

        float y = above ? lineY - h - 1.0f : lineY + 2.0f;
        if (y < lane.getY())
            y = lineY + 2.0f;
        if (y + h > lane.getBottom())
            y = lineY - h - 1.0f;

        const auto box = juce::Rectangle<float> (x, y, wName + 4.0f + wValue + 1.0f, h);
        g.setColour (colour::display.withAlpha (0.85f));
        g.fillRect (box.expanded (3.0f, 0.0f));
        g.setFont (labelFont);
        g.setColour (colour::secondary);
        g.drawText (name, box.withWidth (wName + 1.0f), juce::Justification::centredLeft, false);
        g.setColour (colour::silver);
        g.drawText (value, box.withTrimmedLeft (wName + 4.0f), juce::Justification::centredLeft, false);
        return box.getRight();
    };

    const float afterIn = label ("In", inDb, lane.getX() + 6.0f, yIn, true);
    label ("Out", outDb, afterIn + 12.0f, yOut, false);
}

//==============================================================================
void LevelMeter::update (float levelDb, float gateDb, float gateOffDb, bool clipped)
{
    const float next = juce::jmax (levelDb, shown - 1.5f); // quick up, slower down
    if (std::abs (next - shown) < 0.05f && juce::exactlyEqual (gateDb, gate) && juce::exactlyEqual (gateOffDb, gateOff)
        && clipped == clip)
        return;
    shown = next;
    gate = gateDb;
    gateOff = gateOffDb;
    clip = clipped;
    repaint();
}

void LevelMeter::paint (juce::Graphics& g)
{
    constexpr float low = kLowDb, high = kHighDb;
    const auto r = getLocalBounds().toFloat().withSizeKeepingCentre ((float) getWidth(), 4.0f);
    auto toX = [&] (float db) { return r.getX() + juce::jlimit (0.0f, 1.0f, (db - low) / (high - low)) * r.getWidth(); };

    g.setColour (colour::control.brighter (0.12f));
    g.fillRoundedRectangle (r, 2.0f);

    const float x = toX (shown);
    if (x > r.getX() + 0.5f)
    {
        // silver below the In threshold, crimson where a note can start, amber when clipping
        g.setColour (clip ? colour::problem : (shown >= gate ? colour::crimson : colour::silver));
        g.fillRoundedRectangle (r.withRight (x), 2.0f);
    }

    // the two thresholds: a note may start above the tall mark and ends below the short one
    const float gx = std::round (toX (gate)), ox = std::round (toX (juce::jmin (gate, gateOff)));
    if (ox < gx)
    {
        g.setColour (colour::silver);
        g.fillRect (ox - 0.5f, (float) getHeight() * 0.5f - 4.0f, 1.0f, 8.0f);
    }
    g.setColour (colour::text);
    g.fillRect (gx - 0.5f, (float) getHeight() * 0.5f - 6.0f, 1.0f, 12.0f);
}
