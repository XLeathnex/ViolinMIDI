// The voicing matrix: the voices of a harmony type as distances from the played
// note, from two octaves down to two octaves up. Meant for a CallOutBox under the
// voicing menu, and the same for every type since 2.0: a preset type (third,
// triad, octave ...) opens with its own voices, which can be added to or taken
// away from here ("Dreiklang FV"); the Total Free Voicing is only what is set here.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <cstdint>
#include <functional>

class VoicingPanel final : public juce::Component,
                           public juce::TooltipClient
{
public:
    static constexpr int kMaxVoices = 10;
    static constexpr int kRange = 24;                 // -24 .. +24 semitones
    static constexpr int kCells = 2 * kRange + 1;     // 49

    /** Both masks: bit (semitones + 24) set = a voice at that distance from the
        played note, -24 .. +24 (scale::maskBit). `presetVoices` is scale::presetMask
        of the type, 0 for the free type. Bits outside the 49 positions and voices
        beyond the tenth are dropped as the engine drops them (the lowest ten stay),
        so the panel shows what is heard. `typeName` is the name in the voicing menu,
        without " FV"; the panel adds that itself while the voices differ from the
        preset's. */
    VoicingPanel (const juce::String& typeName, bool isFree, uint64_t voices, uint64_t presetVoices, bool diatonic);

    /** The voices as they are now. */
    uint64_t mask() const noexcept       { return bits; }

    /** A preset type whose voices differ from its own (the heading says "FV"). */
    bool isModified() const noexcept     { return ! freeType && bits != preset; }

    /** Optional: the harmony mode changed while the panel is open. */
    void setDiatonic (bool);

    std::function<void (uint64_t voices)> onChange;   // after every click that changed the set
    std::function<void()> onReset;                    // the reset button (preset types): mask() is the preset's again; onChange is not called

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;

    /** One explanation for each part, so the tip says what is under the mouse. */
    juce::String getTooltip() override;

private:
    int cellAt (juce::Point<int>) const;
    void setHovered (int cell);
    void setMask (uint64_t newMask);
    void refresh();

    void paintHeading (juce::Graphics&) const;
    void paintMatrix (juce::Graphics&) const;
    void paintInfo (juce::Graphics&) const;
    void paintLegend (juce::Graphics&) const;

    const juce::String title;
    const bool freeType;
    const uint64_t preset;
    uint64_t bits = 0;
    bool diatonicMode = true;
    int hovered = -1;

    juce::TextButton triadButton { "Dreiklang" }, resetButton, clearButton { "Leeren" };

    // A CallOutBox is a window of its own, so it brings its own tooltips.
    juce::TooltipWindow tooltips { this, 700 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoicingPanel)
};
