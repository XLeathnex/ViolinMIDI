// Presets as readable JSON files in one folder: one file per preset, slot
// 1..128 (slot n answers program change n - 1). The files can be copied,
// renamed and edited by hand; refresh() picks up what is in the folder.
//
// Message thread only.
#pragma once

#include <juce_core/juce_core.h>

#include "Types.h"

#include <vector>

class PresetStore
{
public:
    static constexpr int kSlots = 128;   // slot 1..128 = program change 0..127

    struct Entry
    {
        int slot = 0;
        juce::String name;
        juce::File file;
    };

    /** The folder is created on the first save. */
    explicit PresetStore (const juce::File& folder);

    /** Reads the folder again. */
    void refresh();

    /** Sorted by slot, one entry per slot (only slots that hold a preset). */
    const std::vector<Entry>& entries() const noexcept   { return list; }
    const Entry* find (int slot) const noexcept;
    juce::File folder() const                            { return dir; }

    /** False: no such preset or unreadable; `into` is untouched then. The master
        switch and the freeze buttons are not part of a preset and stay as they are in `into`. */
    bool load (int slot, vm::Settings& into) const;

    /** Creates or replaces the preset in that slot. `keepUnknown`: the preset is saved
        back into its own slot, so keys this version does not know (those of other
        versions, e.g. 1.5's harmonizer values) are carried over, as rename() does. */
    bool save (int slot, const juce::String& name, const vm::Settings&, bool keepUnknown = false);
    bool rename (int slot, const juce::String& newName);
    bool remove (int slot);

    /** 0 if all 128 are taken. */
    int firstFreeSlot() const noexcept;
    /** The next existing slot after `slot`, wrapping; 0 if there is none. */
    int next (int slot) const noexcept;
    int previous (int slot) const noexcept;

    /** "001 - Name" with an en dash (U+2013). */
    static juce::String label (int slot, const juce::String& name);

private:
    bool write (int slot, const juce::String& name, const juce::var& settings);

    juce::File dir;
    std::vector<Entry> list;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetStore)
};
