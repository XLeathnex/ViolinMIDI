// Free voicing: up to ten harmony voices at any distance from the played note
// between two octaves down and two octaves up. Meant for a CallOutBox.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <cstdint>
#include <functional>

class VoicingPanel final : public juce::Component
{
public:
    static constexpr int kMaxVoices = 10;
    static constexpr int kRange = 24;                 // -24 .. +24 semitones
    static constexpr int kCells = 2 * kRange + 1;     // 49

    /** Bit (semitones + 24) set = a voice at that distance from the played note, -24 .. +24.
        More than ten bits, or bits outside the 49 positions, are cleaned: the voices
        nearest to the played note are kept. */
    explicit VoicingPanel (uint64_t mask);

    uint64_t mask() const noexcept       { return bits; }

    std::function<void (uint64_t mask)> onChange;   // after every click that changed something

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    int cellAt (juce::Point<int> p) const;
    void setMask (uint64_t newMask);

    uint64_t bits = 0;
    int hovered = -1;

    juce::TextButton triadButton { "Dreiklang" }, clearButton { "Leeren" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoicingPanel)
};
