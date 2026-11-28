#include "SettingsPanel.h"

#include "Brand.h"

namespace
{
    constexpr int kWidth = 560, kHeight = 640;
    constexpr int kMargin = 16;
    constexpr int kPageBarHeight = 26;

    const juce::Colour kDim = brand::colour::secondary;

    juce::String text (const char* utf8) { return juce::String::fromUTF8 (utf8); }

    /** Tooltips for the controls of JUCE's audio settings, found by the label next to them
        (the texts as installTranslations() in Main.cpp shows them). */
    struct DeviceTip { const char* label; const char* tip; };
    const DeviceTip kDeviceTips[] = {
        { "Treiber:", "Die Audio-Schnittstelle des Systems. Auf dem Mac ist das CoreAudio." },
        { "Ger\xc3\xa4t:", "Das Audio-Interface, an dem die Geige h\xc3\xa4ngt." },
        { "Eingang:", "Das Interface, an dem die Geige h\xc3\xa4ngt." },
        { "Ausgang:", "Wohin der Audio-Harmonizer spielt. Ohne Harmonizer gibt VioLogic hier nichts aus." },
        { "Abtastrate:", "Gilt f\xc3\xbcr alle Programme am Interface. Hier nicht ver\xc3\xa4ndern, sondern im Audio-MIDI-Setup des Mac." },
        { "Puffergr\xc3\xb6\xc3\x9f" "e:", "So klein wie m\xc3\xb6glich, 64 oder 128 Samples: Jedes Sample Puffer ist Verz\xc3\xb6gerung. 512 Samples kosten etwa 10 ms." },
        { "Aktive Eing\xc3\xa4nge:", "Der erste eingeschaltete Eingang tr\xc3\xa4gt die Geige." },
        { "Aktive Ausg\xc3\xa4nge:", "Die ersten beiden eingeschalteten Ausg\xc3\xa4nge spielen den Audio-Harmonizer, links und rechts." },
    };

    /** "2026-11-21" -> "21.11.2026" */
    juce::String germanDate (const juce::String& iso)
    {
        if (iso.length() != 10)
            return iso;
        return iso.substring (8, 10) + "." + iso.substring (5, 7) + "." + iso.substring (0, 4);
    }
}

SettingsPanel::SettingsPanel (juce::AudioDeviceManager& deviceManager, UpdateManager& updateManager, MidiRemote& remote, int firstPage)
    : updates (updateManager),
      selector (deviceManager, 1, 1, 0, 2, false, false, false, false),
      mapping (remote)
{
    static const char* names[] = { "Audio", "MIDI-Steuerung", "Version" };
    for (int i = 0; i < 3; ++i)
    {
        auto& b = pageButtons[i];
        b.setButtonText (names[i]);
        b.setClickingTogglesState (true);
        b.setRadioGroupId (1);
        b.setConnectedEdges ((i > 0 ? juce::Button::ConnectedOnLeft : 0) | (i < 2 ? juce::Button::ConnectedOnRight : 0));
        b.onClick = [this, i] { if (pageButtons[i].getToggleState()) showPage (i); };
        pageRow.add (b);
    }
    pageButtons[0].setTooltip (text ("Audio-Interface, Eingang der Geige, Ausgang f\xc3\xbcr den Audio-Harmonizer und Puffergr\xc3\xb6\xc3\x9f" "e."));
    pageButtons[1].setTooltip (text ("MIDI-Eingang f\xc3\xbcr Fu\xc3\x9f" "pedal oder Controller: Regler zuweisen, Presets per Program Change."));
    pageButtons[2].setTooltip (text ("Updates und fr\xc3\xbchere Versionen."));
    addAndMakeVisible (pageRow);

    addChildComponent (selector);
    addChildComponent (mapping);

    runningLabel.setJustificationType (juce::Justification::centredLeft);
    addChildComponent (runningLabel);

    versionBox.setTextWhenNothingSelected (text ("Noch keine Versionsliste geladen"));
    versionBox.setTooltip (text ("Alle ver\xc3\xb6" "ffentlichten Versionen. Darunter stehen die \xc3\x84nderungen der gew\xc3\xa4hlten Version."));
    versionBox.onChange = [this] { selectionChanged(); };
    addChildComponent (versionBox);

    useButton.setButtonText ("Diese Version verwenden");
    useButton.setTooltip (text ("L\xc3\xa4" "dt die gew\xc3\xa4hlte Version, falls n\xc3\xb6tig, und startet die App damit neu. "
                                "Jede fr\xc3\xbchere Version l\xc3\xa4sst sich so wieder holen."));
    useButton.onClick = [this]
    {
        const int index = versionBox.getSelectedItemIndex();
        if (index >= 0 && index < itemVersions.size())
            updates.switchTo (itemVersions[index]);
    };
    addChildComponent (useButton);

    notes.setMultiLine (true);
    notes.setReadOnly (true);
    notes.setScrollbarsShown (true);
    notes.setCaretVisible (false);
    notes.setFont (juce::Font (juce::FontOptions (13.0f)));
    notes.setTooltip (text ("Was sich in der gew\xc3\xa4hlten Version ge\xc3\xa4ndert hat."));
    addChildComponent (notes);

    checkButton.setButtonText ("Nach Updates suchen");
    checkButton.onClick = [this] { updates.check(); };
    checkButton.setTooltip (text ("Fragt die Projektseite nach neuen Versionen. Installiert wird nichts ohne Klick."));
    addChildComponent (checkButton);

    statusLabel.setJustificationType (juce::Justification::centredLeft);
    statusLabel.setColour (juce::Label::textColourId, kDim);
    statusLabel.setFont (juce::Font (juce::FontOptions (13.0f)));
    statusLabel.setMinimumHorizontalScale (0.8f);
    addChildComponent (statusLabel);

    updates.addChangeListener (this);
    rebuildList();
    setSize (kWidth, kHeight);
    pageButtons[juce::jlimit (0, 2, firstPage)].setToggleState (true, juce::dontSendNotification);
    showPage (juce::jlimit (0, 2, firstPage));
    startTimerHz (5); // download progress

    if (! updates.hasChecked() && ! updates.isBusy())
        updates.check();
}

SettingsPanel::~SettingsPanel()
{
    updates.removeChangeListener (this);
}

void SettingsPanel::changeListenerCallback (juce::ChangeBroadcaster*)
{
    rebuildList();
}

void SettingsPanel::showPage (int index)
{
    page = index;
    explainDeviceControls();
    selector.setVisible (page == 0);
    mapping.setVisible (page == 1);
    for (juce::Component* c : { (juce::Component*) &runningLabel, (juce::Component*) &versionBox, (juce::Component*) &useButton,
                                (juce::Component*) &notes, (juce::Component*) &checkButton, (juce::Component*) &statusLabel })
        c->setVisible (page == 2);
}

void SettingsPanel::timerCallback()
{
    refreshStatus();

    // The audio settings rebuild their controls when the device changes.
    if (page == 0)
        explainDeviceControls();
}

void SettingsPanel::explainDeviceControls()
{
    std::function<void (juce::Component&)> visit = [&visit] (juce::Component& c)
    {
        if (auto* label = dynamic_cast<juce::Label*> (&c))
        {
            if (auto* client = dynamic_cast<juce::SettableTooltipClient*> (label->getAttachedComponent()))
                for (const auto& entry : kDeviceTips)
                    if (label->getText() == text (entry.label) && client->getTooltip().isEmpty())
                        client->setTooltip (text (entry.tip));
        }

        for (auto* child : c.getChildren())
            visit (*child);
    };
    visit (selector);
}

//==============================================================================
void SettingsPanel::rebuildList()
{
    const auto running = updater::runningVersion();
    const auto state = updater::readState();
    const bool downloadedCopy = updater::runningDownloadedCopy();
    const auto installed = downloadedCopy ? state.installedVersion : running;

    juce::String runningText = text ("Es l\xc3\xa4uft Version ") + running;
    if (downloadedCopy && installed.isNotEmpty())
        runningText << text (" (geladen; im Programme-Ordner liegt ") << installed << ")";
    runningLabel.setText (runningText, juce::dontSendNotification);

    const auto previous = versionBox.getSelectedItemIndex() >= 0 && versionBox.getSelectedItemIndex() < itemVersions.size()
                              ? itemVersions[versionBox.getSelectedItemIndex()]
                              : running;

    versionBox.clear (juce::dontSendNotification);
    itemVersions.clear();
    itemNotes.clear();

    const auto& releases = updates.releases();
    bool installedListed = false;

    for (int i = 0; i < releases.size(); ++i)
    {
        const auto& release = releases.getReference (i);
        juce::String label = "Version " + release.version;
        if (release.date.isNotEmpty())  label << text ("  \xc2\xb7  ") << germanDate (release.date);
        if (i == 0)                     label << "  (neueste)";
        if (release.version == running) label << text ("  \xe2\x80\x93  l\xc3\xa4uft gerade");

        itemVersions.add (release.version);
        itemNotes.add (release.notes);
        versionBox.addItem (label, itemVersions.size());
        installedListed = installedListed || release.version == installed;
    }

    // A version that was never published (built by hand) can still be gone back to.
    if (! installedListed && installed.isNotEmpty() && ! releases.isEmpty())
    {
        juce::String label = "Version " + installed + text ("  \xc2\xb7  im Programme-Ordner");
        if (installed == running) label << text ("  \xe2\x80\x93  l\xc3\xa4uft gerade");
        itemVersions.add (installed);
        itemNotes.add (juce::String());
        versionBox.addItem (label, itemVersions.size());
    }

    const int index = itemVersions.indexOf (previous);
    if (index >= 0)
        versionBox.setSelectedItemIndex (index, juce::dontSendNotification);
    else if (! itemVersions.isEmpty())
        versionBox.setSelectedItemIndex (0, juce::dontSendNotification);

    selectionChanged();
}

void SettingsPanel::selectionChanged()
{
    const int index = versionBox.getSelectedItemIndex();
    const bool valid = index >= 0 && index < itemVersions.size();

    notes.setText (valid ? itemNotes[index] : juce::String(), false);
    useButton.setEnabled (valid && ! updates.isBusy() && itemVersions[index] != updater::runningVersion());
    checkButton.setEnabled (! updates.isBusy());
    refreshStatus();
}

void SettingsPanel::refreshStatus()
{
    juce::String status = updates.statusText();

    if (status.isEmpty() && updates.hasChecked())
    {
        const auto newer = updates.availableUpdate();
        status = newer.isNotEmpty() ? "Version " + newer + text (" ist verf\xc3\xbcgbar.")
                                    : text ("Du hast die neueste Version.");
    }

    if (statusLabel.getText() != status)
        statusLabel.setText (status, juce::dontSendNotification);

    const int index = versionBox.getSelectedItemIndex();
    const bool valid = index >= 0 && index < itemVersions.size();
    useButton.setEnabled (valid && ! updates.isBusy() && itemVersions[index] != updater::runningVersion());
    checkButton.setEnabled (! updates.isBusy());
}

//==============================================================================
void SettingsPanel::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour (brand::colour::hairline);
    g.fillRect (0, kMargin + kPageBarHeight + kMargin - 1, getWidth(), 1);
}

void SettingsPanel::resized()
{
    auto area = getLocalBounds();
    pageRow.setBounds (area.removeFromTop (kMargin + kPageBarHeight).withTrimmedTop (kMargin).withSizeKeepingCentre (330, kPageBarHeight));
    area.removeFromTop (kMargin);

    selector.setBounds (area.withTrimmedTop (8));
    mapping.setBounds (area.withTrimmedTop (kMargin));

    area = area.reduced (kMargin, 0);
    area.removeFromTop (kMargin);

    runningLabel.setBounds (area.removeFromTop (24));
    area.removeFromTop (6);

    auto row = area.removeFromTop (28);
    useButton.setBounds (row.removeFromRight (190));
    row.removeFromRight (8);
    versionBox.setBounds (row);
    area.removeFromTop (8);

    auto bottom = area.removeFromBottom (28 + kMargin).withTrimmedBottom (kMargin);
    checkButton.setBounds (bottom.removeFromLeft (170));
    bottom.removeFromLeft (10);
    statusLabel.setBounds (bottom);

    area.removeFromBottom (8);
    notes.setBounds (area);
}
