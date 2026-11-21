#include "SettingsPanel.h"

namespace
{
    constexpr int kWidth = 520, kHeight = 660;
    constexpr int kSelectorHeight = 330;
    constexpr int kMargin = 16;

    const juce::Colour kDim { 0xff7d8990 };

    juce::String text (const char* utf8) { return juce::String::fromUTF8 (utf8); }

    /** "2026-11-21" -> "21.11.2026" */
    juce::String germanDate (const juce::String& iso)
    {
        if (iso.length() != 10)
            return iso;
        return iso.substring (8, 10) + "." + iso.substring (5, 7) + "." + iso.substring (0, 4);
    }
}

SettingsPanel::SettingsPanel (juce::AudioDeviceManager& deviceManager, UpdateManager& updateManager)
    : updates (updateManager),
      selector (deviceManager, 1, 1, 0, 0, false, false, false, false)
{
    addAndMakeVisible (selector);

    runningLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (runningLabel);

    versionBox.setTextWhenNothingSelected (text ("Noch keine Versionsliste geladen"));
    versionBox.onChange = [this] { selectionChanged(); };
    addAndMakeVisible (versionBox);

    useButton.setButtonText ("Diese Version verwenden");
    useButton.setTooltip (text ("L\xc3\xa4" "dt die gew\xc3\xa4hlte Version, falls n\xc3\xb6tig, und startet die App damit neu. "
                                "Jede fr\xc3\xbchere Version l\xc3\xa4sst sich so wieder holen."));
    useButton.onClick = [this]
    {
        const int index = versionBox.getSelectedItemIndex();
        if (index >= 0 && index < itemVersions.size())
            updates.switchTo (itemVersions[index]);
    };
    addAndMakeVisible (useButton);

    notes.setMultiLine (true);
    notes.setReadOnly (true);
    notes.setScrollbarsShown (true);
    notes.setCaretVisible (false);
    notes.setFont (juce::Font (juce::FontOptions (13.0f)));
    addAndMakeVisible (notes);

    checkButton.setButtonText ("Nach Updates suchen");
    checkButton.onClick = [this] { updates.check(); };
    addAndMakeVisible (checkButton);

    statusLabel.setJustificationType (juce::Justification::centredLeft);
    statusLabel.setColour (juce::Label::textColourId, kDim);
    statusLabel.setFont (juce::Font (juce::FontOptions (13.0f)));
    statusLabel.setMinimumHorizontalScale (0.8f);
    addAndMakeVisible (statusLabel);

    updates.addChangeListener (this);
    rebuildList();
    setSize (kWidth, kHeight);
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

void SettingsPanel::timerCallback()
{
    refreshStatus();
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

    g.setColour (kDim.withAlpha (0.4f));
    g.fillRect (kMargin, headingY - 10, getWidth() - 2 * kMargin, 1);

    g.setColour (kDim);
    g.setFont (juce::Font (juce::FontOptions (12.5f, juce::Font::bold)));
    g.drawText ("VERSION", kMargin, headingY, getWidth() - 2 * kMargin, 18, juce::Justification::centredLeft);
}

void SettingsPanel::resized()
{
    auto area = getLocalBounds();
    selector.setBounds (area.removeFromTop (kSelectorHeight));

    area = area.reduced (kMargin, 0);
    area.removeFromTop (14);
    headingY = area.getY();
    area.removeFromTop (24);

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
