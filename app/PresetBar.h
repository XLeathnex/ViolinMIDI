// The preset selector in the header of the main window: previous / name /
// next, save, save as and a menu for rename, delete and the folder.
//
// loadSlot() and step() are also called from MIDI (program change, foot
// switch), so they never open a window. Everything else is for the message
// thread and uses only asynchronous dialogs.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "PresetStore.h"
#include "Types.h"

#include <functional>

class PresetBar final : public juce::Component
{
public:
    static constexpr int kHeight = 28;

    explicit PresetBar (PresetStore&);

    std::function<vm::Settings()> currentSettings;           ///< what "Speichern" writes
    std::function<void (const vm::Settings&)> onLoad;        ///< a preset was chosen: apply it
    std::function<void (int slot)> onSlotChanged;            ///< to remember it for the next start (0 = none)
    std::function<void()> onFactory;                         ///< the factory preset "Lowest Latenz" was chosen

    /** Shows that slot as current without loading it (0 or a missing slot = none). */
    void setCurrentSlot (int slot);
    int currentSlot() const noexcept   { return current; }

    /** Loads and applies the preset; false (and nothing changes) if there is none or it is unreadable. */
    bool loadSlot (int slot);

    /** +1 next / -1 previous existing preset, wrapping; with none current: first / last. */
    void step (int delta);

    /** The settings no longer match the preset. */
    void setModified (bool);

    void resized() override;

private:
    bool applyPreset (int slot);
    void refreshStore();
    void updateTexts();

    void showMenu();
    void chooseFactory();
    void showMoreMenu();
    void saveCurrent();
    void saveAs();
    void commitSaveAs (const juce::String& name, int slot);
    void doSave (int slot, const juce::String& name, const vm::Settings&);
    void rename();
    void askRemove();
    void doRemove (int slot);
    void showFolder();
    void showError (const juce::String& message);

    PresetStore& store;
    int current = 0;
    bool modified = false;

    juce::TextButton prevButton, nameButton, nextButton, saveButton, saveAsButton, moreButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetBar)
};
