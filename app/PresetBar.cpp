#include "PresetBar.h"

#include "Brand.h"

namespace
{
    constexpr int kArrowWidth = 28, kSaveWidth = 84, kSaveAsWidth = 130, kMoreWidth = 28;
    constexpr int kGroupGap = 16, kButtonGap = 4;
    constexpr int kNameLimit = 60;

    juce::String U (const char* utf8) { return juce::String::fromUTF8 (utf8); }

    juce::String quoted (const juce::String& name)
    {
        return U ("\xe2\x80\x9e") + name + U ("\xe2\x80\x9c");
    }

    juce::String slotText (int slot)
    {
        return juce::String (slot).paddedLeft ('0', 3);
    }

    enum MoreItem { moreRename = 1, moreDelete, moreFolder };
    constexpr int kFactoryItem = 1000;   // above the slot numbers, which are the ids of the presets
}

//==============================================================================
PresetBar::PresetBar (PresetStore& s)
    : store (s)
{
    prevButton.setButtonText (U ("\xe2\x80\xb9"));
    nextButton.setButtonText (U ("\xe2\x80\xba"));
    saveButton.setButtonText ("Speichern");
    saveAsButton.setButtonText (U ("Speichern unter \xe2\x80\xa6"));
    moreButton.setButtonText (U ("\xe2\x80\xa6"));

    // The three segments of the selector and the two save buttons are told
    // apart by the theme's hairline, not by gaps.
    prevButton.setConnectedEdges (juce::Button::ConnectedOnRight);
    nameButton.setConnectedEdges (juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight);
    nextButton.setConnectedEdges (juce::Button::ConnectedOnLeft);
    saveButton.setConnectedEdges (juce::Button::ConnectedOnRight);
    saveAsButton.setConnectedEdges (juce::Button::ConnectedOnLeft);

    const auto tip = U ("Presets: Speichert alle aktuellen Einstellungen unter einem Namen. "
                        "Kann auf der B\xc3\xbchne direkt per MIDI-Fu\xc3\x9f" "schalter (Program Change oder CC) umgeschaltet werden.");
    nameButton.setTooltip (tip);
    saveButton.setTooltip (tip);
    saveAsButton.setTooltip (tip);
    moreButton.setTooltip (U ("Das aktuelle Preset umbenennen oder l\xc3\xb6schen, oder den Preset-Ordner im Finder zeigen."));
    prevButton.setTooltip (U ("Vorheriges Preset"));
    nextButton.setTooltip (U ("N\xc3\xa4" "chstes Preset"));

    prevButton.onClick   = [this] { step (-1); };
    nextButton.onClick   = [this] { step (+1); };
    nameButton.onClick   = [this] { showMenu(); };
    saveButton.onClick   = [this] { saveCurrent(); };
    saveAsButton.onClick = [this] { saveAs(); };
    moreButton.onClick   = [this] { showMoreMenu(); };

    for (auto* b : { &prevButton, &nameButton, &nextButton, &saveButton, &saveAsButton, &moreButton })
    {
        // A button that keeps the keyboard focus would turn the space bar (master switch) into a click on itself.
        b->setWantsKeyboardFocus (false);
        b->setMouseClickGrabsKeyboardFocus (false);
        addAndMakeVisible (*b);
    }

    updateTexts();
}

void PresetBar::resized()
{
    auto r = getLocalBounds();

    moreButton.setBounds (r.removeFromRight (kMoreWidth));
    r.removeFromRight (kButtonGap);
    saveAsButton.setBounds (r.removeFromRight (kSaveAsWidth));
    saveButton.setBounds (r.removeFromRight (kSaveWidth));
    r.removeFromRight (kGroupGap);

    prevButton.setBounds (r.removeFromLeft (kArrowWidth));
    nextButton.setBounds (r.removeFromRight (kArrowWidth));
    nameButton.setBounds (r);
}

//==============================================================================
void PresetBar::setCurrentSlot (int slot)
{
    if (store.find (slot) == nullptr)
        store.refresh();

    current = store.find (slot) != nullptr ? slot : 0;
    if (current == 0)
        modified = false;

    updateTexts();
}

void PresetBar::setModified (bool isModified)
{
    if (modified == isModified)
        return;

    modified = isModified;
    updateTexts();
}

void PresetBar::updateTexts()
{
    const auto* e = current != 0 ? store.find (current) : nullptr;

    auto shown = e != nullptr ? PresetStore::label (e->slot, e->name) : U ("Kein Preset");
    if (e != nullptr && modified)
        shown += U (" (ge\xc3\xa4ndert)");

    nameButton.setButtonText (shown);
    nameButton.setColour (juce::TextButton::textColourOffId, e != nullptr ? brand::colour::text : brand::colour::secondary);

    const bool any = ! store.entries().empty();
    prevButton.setEnabled (any);
    nextButton.setEnabled (any);
}

// The folder may have been changed by hand: a current preset whose file is gone is no longer current.
void PresetBar::refreshStore()
{
    store.refresh();

    if (current != 0 && store.find (current) == nullptr)
    {
        current = 0;
        modified = false;

        if (onSlotChanged)
            onSlotChanged (0);
    }

    updateTexts();
}

//==============================================================================
bool PresetBar::applyPreset (int slot)
{
    // The master switch and the freeze buttons are not part of a preset, so they start from the current state.
    vm::Settings loaded = currentSettings ? currentSettings() : vm::Settings();

    if (! store.load (slot, loaded))
        return false;

    if (onLoad)
        onLoad (loaded);

    // After onLoad: applying the settings may well report a change, which is not a modification.
    current = slot;
    modified = false;
    updateTexts();

    if (onSlotChanged)
        onSlotChanged (current);

    return true;
}

bool PresetBar::loadSlot (int slot)
{
    if (applyPreset (slot))
        return true;

    // A file copied into the folder by hand since the last look.
    store.refresh();
    updateTexts();
    return applyPreset (slot);
}

void PresetBar::step (int delta)
{
    if (store.entries().empty())
        store.refresh();

    int target = delta >= 0 ? store.next (current) : store.previous (current);

    // An unreadable file is skipped rather than leaving the pedal dead.
    for (int attempts = 0; target != 0 && attempts < PresetStore::kSlots; ++attempts)
    {
        if (applyPreset (target))
            return;

        target = delta >= 0 ? store.next (target) : store.previous (target);
    }
}

//==============================================================================
void PresetBar::showMenu()
{
    refreshStore();

    juce::PopupMenu menu;

    // The factory preset is always there and cannot be changed or deleted.
    menu.addItem (kFactoryItem, U ("Lowest Latenz (Werks-Preset)"));
    menu.addSeparator();

    if (store.entries().empty())
    {
        menu.addItem (kFactoryItem + 1, U ("Noch keine Presets gespeichert"), false, false);
    }
    else
    {
        for (const auto& e : store.entries())
            menu.addItem (e.slot, PresetStore::label (e.slot, e.name), true, e.slot == current);
    }

    juce::Component::SafePointer<PresetBar> safe (this);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (nameButton)
                                                  .withMinimumWidth (nameButton.getWidth()),
                        [safe] (int result)
                        {
                            if (safe.getComponent() == nullptr)
                                return;

                            if (result == kFactoryItem)
                                safe->chooseFactory();
                            else if (result > 0 && result <= PresetStore::kSlots)
                                safe->loadSlot (result);
                        });
}

/** The factory preset takes the place of whatever preset was current. */
void PresetBar::chooseFactory()
{
    if (current != 0)
    {
        current = 0;
        modified = false;
        updateTexts();

        if (onSlotChanged)
            onSlotChanged (0);
    }

    if (onFactory)
        onFactory();
}

void PresetBar::showMoreMenu()
{
    refreshStore();

    const bool hasCurrent = current != 0;

    juce::PopupMenu menu;
    menu.addItem (moreRename, U ("Umbenennen \xe2\x80\xa6"), hasCurrent);
    menu.addItem (moreDelete, U ("L\xc3\xb6schen"), hasCurrent);
    menu.addSeparator();
    menu.addItem (moreFolder, "Preset-Ordner zeigen");

    juce::Component::SafePointer<PresetBar> safe (this);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (moreButton),
                        [safe] (int result)
                        {
                            auto* bar = safe.getComponent();
                            if (bar == nullptr)
                                return;

                            if (result == moreRename)       bar->rename();
                            else if (result == moreDelete)  bar->askRemove();
                            else if (result == moreFolder)  bar->showFolder();
                        });
}

//==============================================================================
void PresetBar::saveCurrent()
{
    if (! currentSettings)
        return;

    refreshStore();

    const auto* e = store.find (current);
    if (e == nullptr)
    {
        saveAs();
        return;
    }

    doSave (e->slot, e->name, currentSettings());
}

void PresetBar::saveAs()
{
    if (! currentSettings)
        return;

    refreshStore();

    const auto* existing = store.find (current);

    auto* window = new juce::AlertWindow (U ("Speichern unter"), {}, juce::MessageBoxIconType::NoIcon, this);
    window->addTextEditor ("name", existing != nullptr ? existing->name : juce::String(), "Name");

    if (auto* editor = window->getTextEditor ("name"))
        editor->setInputRestrictions (kNameLimit);

    // Item ids are 1..128 in this order, so the selected id is the slot.
    juce::StringArray slots;
    for (int slot = 1; slot <= PresetStore::kSlots; ++slot)
    {
        const auto* e = store.find (slot);
        slots.add (e != nullptr ? PresetStore::label (slot, e->name) : slotText (slot));
    }
    window->addComboBox ("slot", slots, "Platz");

    int preselected = store.firstFreeSlot();
    if (preselected == 0)
        preselected = current != 0 ? current : 1;

    if (auto* box = window->getComboBoxComponent ("slot"))
        box->setSelectedId (preselected, juce::dontSendNotification);

    window->addButton ("Speichern", 1, juce::KeyPress (juce::KeyPress::returnKey));
    window->addButton ("Abbrechen", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<PresetBar> safe (this);
    juce::Component::SafePointer<juce::AlertWindow> alert (window);

    window->enterModalState (true, juce::ModalCallbackFunction::create ([safe, alert] (int result)
    {
        auto* bar = safe.getComponent();
        auto* w = alert.getComponent();

        if (result != 1 || bar == nullptr || w == nullptr)
            return;

        const auto* box = w->getComboBoxComponent ("slot");
        bar->commitSaveAs (w->getTextEditorContents ("name"), box != nullptr ? box->getSelectedId() : 0);
    }), true);

    if (auto* editor = window->getTextEditor ("name"))
        editor->grabKeyboardFocus();
}

void PresetBar::commitSaveAs (const juce::String& name, int slot)
{
    if (! currentSettings || slot < 1 || slot > PresetStore::kSlots)
        return;

    // What was on screen when the user pressed the button, even if they confirm a question afterwards.
    const auto settings = currentSettings();

    store.refresh();
    const auto* existing = store.find (slot);

    if (existing == nullptr || slot == current)
    {
        doSave (slot, name, settings);
        return;
    }

    const auto message = U ("Platz ") + slotText (slot) + U (" enth\xc3\xa4lt bereits ") + quoted (existing->name)
                       + U (". \xc3\x9c" "berschreiben?");

    juce::Component::SafePointer<PresetBar> safe (this);

    juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                      .withIconType (juce::MessageBoxIconType::QuestionIcon)
                                      .withTitle ("Speichern unter")
                                      .withMessage (message)
                                      .withButton (U ("\xc3\x9c" "berschreiben"), 1)
                                      .withButton ("Abbrechen", 0)
                                      .withAssociatedComponent (this),
                                  [safe, slot, name, settings] (int result)
                                  {
                                      if (result == 1 && safe.getComponent() != nullptr)
                                          safe->doSave (slot, name, settings);
                                  });
}

void PresetBar::doSave (int slot, const juce::String& name, const vm::Settings& settings)
{
    // Saved back into the preset that is loaded: what other versions keep in it stays.
    if (! store.save (slot, name, settings, slot == current))
    {
        updateTexts();
        showError (U ("Das Preset konnte nicht gespeichert werden.\nOrdner: ") + store.folder().getFullPathName());
        return;
    }

    current = slot;
    modified = false;
    updateTexts();

    if (onSlotChanged)
        onSlotChanged (current);
}

//==============================================================================
void PresetBar::rename()
{
    refreshStore();

    const auto* e = store.find (current);
    if (e == nullptr)
        return;

    const int slot = e->slot;

    auto* window = new juce::AlertWindow (U ("Preset umbenennen"), {}, juce::MessageBoxIconType::NoIcon, this);
    window->addTextEditor ("name", e->name, "Name");

    if (auto* editor = window->getTextEditor ("name"))
    {
        editor->setInputRestrictions (kNameLimit);
        editor->selectAll();
    }

    window->addButton ("Umbenennen", 1, juce::KeyPress (juce::KeyPress::returnKey));
    window->addButton ("Abbrechen", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<PresetBar> safe (this);
    juce::Component::SafePointer<juce::AlertWindow> alert (window);

    window->enterModalState (true, juce::ModalCallbackFunction::create ([safe, alert, slot] (int result)
    {
        auto* bar = safe.getComponent();
        auto* w = alert.getComponent();

        if (result != 1 || bar == nullptr || w == nullptr)
            return;

        if (! bar->store.rename (slot, w->getTextEditorContents ("name")))
            bar->showError (U ("Das Preset konnte nicht umbenannt werden."));

        bar->updateTexts();
    }), true);

    if (auto* editor = window->getTextEditor ("name"))
        editor->grabKeyboardFocus();
}

void PresetBar::askRemove()
{
    refreshStore();

    const auto* e = store.find (current);
    if (e == nullptr)
        return;

    const int slot = e->slot;
    juce::Component::SafePointer<PresetBar> safe (this);

    juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                      .withIconType (juce::MessageBoxIconType::QuestionIcon)
                                      .withTitle (U ("L\xc3\xb6schen"))
                                      .withMessage (U ("Preset ") + quoted (e->name) + U (" l\xc3\xb6schen?"))
                                      .withButton (U ("L\xc3\xb6schen"), 1)
                                      .withButton ("Abbrechen", 0)
                                      .withAssociatedComponent (this),
                                  [safe, slot] (int result)
                                  {
                                      if (result == 1 && safe.getComponent() != nullptr)
                                          safe->doRemove (slot);
                                  });
}

// By slot, not "the current one": a program change may have switched presets while the question was open.
void PresetBar::doRemove (int slot)
{
    if (! store.remove (slot))
    {
        updateTexts();
        showError (U ("Das Preset konnte nicht gel\xc3\xb6scht werden."));
        return;
    }

    if (current == slot)
    {
        current = 0;
        modified = false;
        updateTexts();

        if (onSlotChanged)
            onSlotChanged (0);
    }
    else
    {
        updateTexts();
    }
}

void PresetBar::showFolder()
{
    const auto folder = store.folder();
    folder.createDirectory();
    folder.revealToUser();
}

void PresetBar::showError (const juce::String& message)
{
    juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                      .withIconType (juce::MessageBoxIconType::WarningIcon)
                                      .withTitle ("Presets")
                                      .withMessage (message)
                                      .withButton ("OK", 0)
                                      .withAssociatedComponent (this),
                                  [] (int) {});
}
