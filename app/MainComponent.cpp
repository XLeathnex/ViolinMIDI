#include "MainComponent.h"

#include "SettingsPanel.h"

#include <cmath>

namespace
{
    constexpr int kWidth = 560, kHeight = 752;
    constexpr int kMargin = 18, kRow = 26, kGap = 6, kLabelWidth = 200;
    constexpr int kPitchHeight = 150, kStatusHeight = 56;
    constexpr int kTimerHz = 30;
    constexpr const char* kPortName = "ViolinMIDI";

    const juce::Colour kAccent  { 0xff39b6a8 };
    const juce::Colour kPanel   { 0xff22272b };
    const juce::Colour kDim     { 0xff7d8990 };
    const juce::Colour kWarning { 0xffe0a03c };

    juce::String noteName (int midiNote)
    {
        static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        return juce::String (names[((midiNote % 12) + 12) % 12]) + juce::String (midiNote / 12 - 1);
    }

    void setupLabel (juce::Component& parent, juce::Label& label, const juce::String& text)
    {
        label.setText (text, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centredLeft);
        parent.addAndMakeVisible (label);
    }

    void setupSlider (juce::Component& parent, juce::Slider& slider, double low, double high, double step,
                      const juce::String& suffix, int decimals)
    {
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 78, 22);
        slider.setRange (low, high, step);
        slider.setTextValueSuffix (suffix);
        slider.setNumDecimalPlacesToDisplay (decimals);
        parent.addAndMakeVisible (slider);
    }
}

//==============================================================================
void PitchDisplay::update (const AudioHost::Meters& m, float gateDb, bool midiActive)
{
    active = m.active;
    heard = m.voiced && m.levelDb >= gateDb - 6.0f;
    midi = midiActive;
    note = m.note;
    pitch = m.pitch;
    repaint();
}

void PitchDisplay::paint (juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat();
    g.setColour (kPanel);
    g.fillRoundedRectangle (area, 10.0f);

    // MIDI activity
    g.setColour (midi ? kAccent : kDim.withAlpha (0.35f));
    g.fillEllipse (area.getRight() - 26.0f, 14.0f, 10.0f, 10.0f);
    g.setColour (kDim);
    g.setFont (juce::Font (juce::FontOptions (12.0f)));
    g.drawText ("MIDI", juce::Rectangle<float> (area.getRight() - 70.0f, 10.0f, 40.0f, 18.0f),
                juce::Justification::centredRight);

    // note name
    const int shownNote = active ? note : (heard ? juce::roundToInt (pitch) : -1);
    if (shownNote >= 0)
    {
        g.setColour (active ? kAccent : kDim);
        g.setFont (juce::Font (juce::FontOptions (62.0f, juce::Font::bold)));
        g.drawText (noteName (shownNote), area.withTrimmedBottom (66.0f).withTrimmedTop (4.0f), juce::Justification::centred);
    }

    // tuner scale: where the string is between two semitones
    const auto scale = area.withTrimmedLeft (40.0f).withTrimmedRight (40.0f).withTop (area.getBottom() - 66.0f).withHeight (20.0f);
    g.setColour (kDim.withAlpha (0.5f));
    g.fillRoundedRectangle (scale.withSizeKeepingCentre (scale.getWidth(), 4.0f), 2.0f);
    for (int tick = -2; tick <= 2; ++tick)
    {
        const float x = scale.getCentreX() + (float) tick * scale.getWidth() * 0.25f;
        g.fillRect (x - 0.5f, scale.getY() + (tick == 0 ? 0.0f : 5.0f), 1.0f, tick == 0 ? 20.0f : 10.0f);
    }

    juce::String info = U ("Bereit \xe2\x80\x93 spiel einen Ton");
    if (active || heard)
    {
        const float cents = (pitch - std::round (pitch)) * 100.0f;
        const float x = scale.getCentreX() + juce::jlimit (-50.0f, 50.0f, cents) / 100.0f * scale.getWidth();
        g.setColour (active ? kAccent : kDim);
        g.fillRoundedRectangle (x - 3.0f, scale.getY(), 6.0f, 20.0f, 3.0f);

        const juce::String centText = (cents >= 0.0f ? "+" : U ("\xe2\x88\x92")) + juce::String (std::abs (juce::roundToInt (cents))) + " Cent";
        info = active ? "MIDI-Note " + juce::String (note) + "   " + centText
                      : U ("unter der Ansprechschwelle   ") + centText;
    }

    g.setColour (kDim);
    g.setFont (juce::Font (juce::FontOptions (14.0f)));
    g.drawText (info, area.withTop (area.getBottom() - 38.0f).withTrimmedBottom (10.0f), juce::Justification::centred);
}

//==============================================================================
void LevelMeter::update (float levelDb, float gateDb, bool clipped)
{
    shown = std::max (levelDb, shown - 1.5f); // quick up, slower down
    gate = gateDb;
    clip = clipped;
    repaint();
}

void LevelMeter::paint (juce::Graphics& g)
{
    constexpr float low = -72.0f, high = 0.0f;
    const auto area = getLocalBounds().toFloat();
    auto toX = [&] (float db) { return area.getX() + juce::jlimit (0.0f, 1.0f, (db - low) / (high - low)) * area.getWidth(); };

    g.setColour (kPanel);
    g.fillRoundedRectangle (area, 4.0f);

    const float x = toX (shown);
    if (x > area.getX() + 1.0f)
    {
        g.setColour (clip ? kWarning : (shown >= gate ? kAccent : kDim));
        g.fillRoundedRectangle (area.withRight (x), 4.0f);
    }

    // gate threshold
    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.fillRect (toX (gate) - 1.0f, area.getY(), 2.0f, area.getHeight());
}

//==============================================================================
MainComponent::MainComponent (bool openAudio, const juce::String& startupNotice, bool showSettingsAtStart)
{
    setWantsKeyboardFocus (true);

    juce::PropertiesFile::Options options;
    options.applicationName = "ViolinMIDI";
    options.filenameSuffix = "settings";
    options.osxLibrarySubFolder = "Application Support";
    appProperties.setStorageParameters (options);

    addAndMakeVisible (pitchDisplay);
    addAndMakeVisible (levelMeter);

    // --- input
    setupLabel (*this, gainLabel, U ("Eingangsverst\xc3\xa4rkung"));
    setupSlider (*this, gainSlider, -12.0, 36.0, 0.5, " dB", 1);
    gainSlider.setDoubleClickReturnValue (true, 0.0);
    gainSlider.setTooltip (U ("So einstellen, dass der Balken bei kr\xc3\xa4" "ftigem Spiel weit nach rechts geht, ohne orange zu werden."));

    setupLabel (*this, gateLabel, "Ansprechschwelle (Gate)");
    setupSlider (*this, gateSlider, -70.0, -20.0, 0.5, " dB", 1);
    gateSlider.setDoubleClickReturnValue (true, -45.0);
    gateSlider.setTooltip (U ("Wei\xc3\x9f" "e Marke im Pegelbalken. Knapp \xc3\xbc" "ber das Grundrauschen legen: niedrig = spricht fr\xc3\xbch an, h\xc3\xb6her = keine Geisternoten."));

    // --- pitch movement
    const char* modeNames[] = { "Stufenlos", "Vibrato gegl\xc3\xa4ttet", "Halbt\xc3\xb6ne" };
    const char* modeTips[] = {
        "Pitch Bend folgt der Saite exakt: Glissando und Vibrato kommen beim Instrument an. (Taste 1)",
        "Glissando wird \xc3\xbc" "bertragen, Vibrato herausgefiltert: das Instrument steht ruhig. (Taste 2)",
        "Kein Pitch Bend: beim Rutschen wird jeder erreichte Halbton neu angeschlagen, z. B. f\xc3\xbcr Klavier. (Taste 3)"
    };
    for (int i = 0; i < 3; ++i)
    {
        auto& b = modeButtons[i];
        b.setButtonText (U (modeNames[i]));
        b.setTooltip (U (modeTips[i]));
        b.setClickingTogglesState (true);
        b.setRadioGroupId (1);
        b.setColour (juce::TextButton::buttonOnColourId, kAccent.darker (0.2f));
        b.setConnectedEdges ((i > 0 ? juce::Button::ConnectedOnLeft : 0) | (i < 2 ? juce::Button::ConnectedOnRight : 0));
        b.onClick = [this, i] { if (modeButtons[i].getToggleState()) pushSettings(); };
        addAndMakeVisible (b);
    }

    setupLabel (*this, bendRangeLabel, "Pitch-Bend-Bereich");
    for (int range : { 2, 4, 12, 24, 48 })
        bendRangeBox.addItem (U ("\xc2\xb1") + juce::String (range) + U (" Halbt\xc3\xb6ne") + (range == 2 ? " (Standard)" : ""), range);
    bendRangeBox.setTooltip (U ("Muss mit dem Pitch-Bend-Bereich des Instruments \xc3\xbc" "bereinstimmen. Gr\xc3\xb6\xc3\x9f" "er = l\xc3\xa4ngere Glissandi ohne neuen Anschlag."));
    bendRangeBox.onChange = [this] { pushSettings(); };
    addAndMakeVisible (bendRangeBox);

    // --- dynamics
    setupLabel (*this, dynamicsLabel, U ("Lautst\xc3\xa4rke"));
    dynamicsBox.addItem (U ("Kontinuierlich (Anschlag + Expression CC11)"), 1);
    dynamicsBox.addItem (U ("Nur Anschlagst\xc3\xa4rke"), 2);
    dynamicsBox.addItem (U ("Feste Lautst\xc3\xa4rke"), 3);
    dynamicsBox.setTooltip (U ("Kontinuierlich: An- und Abschwellen des Bogens steuert laufend die Lautst\xc3\xa4rke des Instruments."));
    dynamicsBox.onChange = [this] { pushSettings(); };
    addAndMakeVisible (dynamicsBox);

    setupLabel (*this, velocityLabel, U ("Feste Anschlagst\xc3\xa4rke"));
    setupSlider (*this, velocitySlider, 1.0, 127.0, 1.0, "", 0);

    // --- detection
    setupLabel (*this, responseLabel, "Ansprache");
    responseBox.addItem ("Schnell", 1);
    responseBox.addItem ("Normal", 2);
    responseBox.addItem ("Sicher", 3);
    responseBox.setTooltip (U ("Schnell = k\xc3\xbcrzeste Verz\xc3\xb6gerung, Sicher = wartet l\xc3\xa4nger, bis die Tonh\xc3\xb6he eindeutig steht. Wirkt vor allem auf den tiefen Saiten."));
    responseBox.onChange = [this] { pushSettings(); };
    addAndMakeVisible (responseBox);

    setupLabel (*this, lowestLabel, "Tiefster Ton");
    for (int n = vm::kAbsLowestNote; n <= 60; ++n)
    {
        juce::String text = noteName (n) + "  (MIDI " + juce::String (n) + ")";
        if (n == 34) text += U ("  \xe2\x80\x93 B\xe2\x99\xad, tiefste Saite 7-Saiter");
        if (n == 41) text += U ("  \xe2\x80\x93 6-Saiter");
        if (n == 48) text += U ("  \xe2\x80\x93 5-Saiter");
        if (n == 55) text += U ("  \xe2\x80\x93 4-Saiter");
        lowestBox.addItem (text, n);
    }
    lowestBox.setTooltip (U ("T\xc3\xb6ne darunter werden ignoriert. H\xc3\xb6her gesetzt reagieren auch die tiefen Lagen schneller."));
    lowestBox.onChange = [this] { pushSettings(); };
    addAndMakeVisible (lowestBox);

    setupLabel (*this, tuningLabel, "Kammerton A");
    setupSlider (*this, tuningSlider, 430.0, 450.0, 0.5, " Hz", 1);
    tuningSlider.setDoubleClickReturnValue (true, 440.0);

    for (auto* s : { &gainSlider, &gateSlider, &velocitySlider, &tuningSlider })
        s->onValueChange = [this] { pushSettings(); };

    // --- MIDI output
    setupLabel (*this, channelLabel, "MIDI-Kanal");
    for (int channel = 1; channel <= 16; ++channel)
        channelBox.addItem ("Kanal " + juce::String (channel), channel);
    channelBox.setTooltip (U ("Auf diesem Kanal sendet die App. Ein eigener Kanal h\xc3\xa4lt die Geige von anderen MIDI-Ger\xc3\xa4ten getrennt."));
    channelBox.onChange = [this] { pushSettings(); };
    addAndMakeVisible (channelBox);

    // --- bottom
    statusLabel.setJustificationType (juce::Justification::topLeft);
    statusLabel.setColour (juce::Label::textColourId, kDim);
    statusLabel.setFont (juce::Font (juce::FontOptions (13.0f)));
    addAndMakeVisible (statusLabel);

    settingsButton.setButtonText (U ("Einstellungen\xe2\x80\xa6"));
    settingsButton.setTooltip (U ("Audio-Eingang w\xc3\xa4hlen, nach Updates suchen und zwischen Versionen wechseln."));
    settingsButton.onClick = [this] { showSettings(); };
    addAndMakeVisible (settingsButton);

    // Only shown while a newer version is waiting. One click loads it and restarts.
    updateButton.setColour (juce::TextButton::buttonColourId, kAccent.darker (0.25f));
    updateButton.onClick = [this]
    {
        const auto version = updates.availableUpdate();
        if (version.isNotEmpty())
            updates.switchTo (version);
    };
    addChildComponent (updateButton);

    panicButton.setButtonText ("Alle Noten aus");
    panicButton.setTooltip (U ("Beendet h\xc3\xa4ngende Noten und stellt Pitch Bend und Expression zur\xc3\xbc" "ck."));
    panicButton.onClick = [this] { host.requestAllNotesOff(); };
    addAndMakeVisible (panicButton);

    loadSettings();
    setSize (kWidth, kHeight);

    updates.onRestart = [] { juce::JUCEApplication::getInstance()->systemRequestedQuit(); };
    updates.addChangeListener (this);
    if (startupNotice.isNotEmpty())
        updates.setNotice (startupNotice);

    if (showSettingsAtStart)
    {
        juce::Component::SafePointer<MainComponent> safeThis (this);
        juce::MessageManager::callAsync ([safeThis] { if (safeThis != nullptr) safeThis->showSettings(); });
    }

    startTimerHz (kTimerHz);
    if (! openAudio)
        return;

    // --- MIDI out and audio in
    midiPortOk = host.openMidiPort (kPortName);

    auto savedState = properties() != nullptr ? properties()->getXmlValue ("audioState") : nullptr;
    const bool firstRun = savedState == nullptr;
    if (savedState != nullptr)
        savedState->removeAttribute ("audioDeviceRate"); // never change the rate other programs are using

    audioInitError = deviceManager.initialise (1, 0, savedState.get(), true);

    if (firstRun)
    {
        // A small buffer keeps the delay short. Other programs keep their own.
        if (auto* device = deviceManager.getCurrentAudioDevice())
        {
            int best = 0;
            for (int size : device->getAvailableBufferSizes())
                if (size >= 64 && (best == 0 || size < best))
                    best = size;

            auto setup = deviceManager.getAudioDeviceSetup();
            if (best > 0 && best < setup.bufferSize)
            {
                setup.bufferSize = best;
                deviceManager.setAudioDeviceSetup (setup, true);
            }
        }
    }

    deviceManager.addAudioCallback (&host);
    deviceManager.addChangeListener (this);

    // A quiet look for a newer version. Nothing is installed without a click.
    updates.check();
}

MainComponent::~MainComponent()
{
    stopTimer();
    if (settingsDialog != nullptr)
        delete settingsDialog.getComponent();

    updates.removeChangeListener (this);
    deviceManager.removeChangeListener (this);
    deviceManager.removeAudioCallback (&host); // ends the sounding note, see AudioHost::audioDeviceStopped
    deviceManager.closeAudioDevice();
    saveSettings();
    appProperties.saveIfNeeded();
}

juce::PropertiesFile* MainComponent::properties()
{
    return appProperties.getUserSettings();
}

//==============================================================================
void MainComponent::loadSettings()
{
    loading = true;
    auto* p = properties();
    const vm::Settings defaults;

    auto getInt = [p] (const char* key, int fallback) { return p != nullptr ? p->getIntValue (key, fallback) : fallback; };
    auto getDouble = [p] (const char* key, double fallback) { return p != nullptr ? p->getDoubleValue (key, fallback) : fallback; };

    const int mode = juce::jlimit (0, 2, getInt ("bendMode", (int) defaults.bendMode));
    modeButtons[mode].setToggleState (true, juce::dontSendNotification);

    int range = getInt ("bendRange", defaults.bendRangeSemis);
    if (bendRangeBox.indexOfItemId (range) < 0)
        range = defaults.bendRangeSemis;
    bendRangeBox.setSelectedId (range, juce::dontSendNotification);

    dynamicsBox.setSelectedId (juce::jlimit (0, 2, getInt ("dynamics", (int) defaults.dynamics)) + 1, juce::dontSendNotification);
    responseBox.setSelectedId (juce::jlimit (0, 2, getInt ("response", (int) defaults.response)) + 1, juce::dontSendNotification);
    lowestBox.setSelectedId (juce::jlimit (vm::kAbsLowestNote, 60, getInt ("lowestNote", defaults.lowestNote)), juce::dontSendNotification);

    gainSlider.setValue (getDouble ("gainDb", defaults.inputGainDb), juce::dontSendNotification);
    gateSlider.setValue (getDouble ("gateDb", defaults.gateDb), juce::dontSendNotification);
    velocitySlider.setValue (getInt ("fixedVelocity", defaults.fixedVelocity), juce::dontSendNotification);
    tuningSlider.setValue (getDouble ("a4Hz", defaults.a4Hz), juce::dontSendNotification);
    channelBox.setSelectedId (juce::jlimit (1, 16, getInt ("midiChannel", defaults.midiChannel)), juce::dontSendNotification);

    loading = false;
    pushSettings();
}

void MainComponent::saveSettings()
{
    auto* p = properties();
    if (p == nullptr)
        return;

    const vm::Settings s = host.getSettings();
    p->setValue ("bendMode", (int) s.bendMode);
    p->setValue ("bendRange", s.bendRangeSemis);
    p->setValue ("dynamics", (int) s.dynamics);
    p->setValue ("response", (int) s.response);
    p->setValue ("lowestNote", s.lowestNote);
    p->setValue ("gainDb", (double) s.inputGainDb);
    p->setValue ("gateDb", (double) s.gateDb);
    p->setValue ("fixedVelocity", s.fixedVelocity);
    p->setValue ("a4Hz", (double) s.a4Hz);
    p->setValue ("midiChannel", s.midiChannel);
}

void MainComponent::pushSettings()
{
    if (loading)
        return;

    vm::Settings s;
    for (int i = 0; i < 3; ++i)
        if (modeButtons[i].getToggleState())
            s.bendMode = (vm::BendMode) i;

    s.bendRangeSemis = bendRangeBox.getSelectedId() > 0 ? bendRangeBox.getSelectedId() : 2;
    s.dynamics = (vm::DynamicsMode) juce::jlimit (0, 2, dynamicsBox.getSelectedId() - 1);
    s.response = (vm::Response) juce::jlimit (0, 2, responseBox.getSelectedId() - 1);
    s.lowestNote = lowestBox.getSelectedId() > 0 ? lowestBox.getSelectedId() : 34;
    s.inputGainDb = (float) gainSlider.getValue();
    s.gateDb = (float) gateSlider.getValue();
    s.fixedVelocity = juce::roundToInt (velocitySlider.getValue());
    s.a4Hz = (float) tuningSlider.getValue();
    s.midiChannel = channelBox.getSelectedId() > 0 ? channelBox.getSelectedId() : 1;
    host.setSettings (s);

    const bool fixed = s.dynamics == vm::DynamicsMode::Fixed;
    velocitySlider.setEnabled (fixed);
    velocityLabel.setEnabled (fixed);
    const bool bends = s.bendMode != vm::BendMode::Quantized;
    bendRangeBox.setEnabled (bends);
    bendRangeLabel.setEnabled (bends);

    saveSettings();
}

void MainComponent::setMode (int index)
{
    modeButtons[juce::jlimit (0, 2, index)].setToggleState (true, juce::sendNotification);
}

bool MainComponent::keyPressed (const juce::KeyPress& key)
{
    const auto c = key.getTextCharacter();
    if (c >= '1' && c <= '3')
    {
        setMode ((int) (c - '1'));
        return true;
    }
    return false;
}

//==============================================================================
void MainComponent::showSettings()
{
    if (settingsDialog != nullptr)
    {
        settingsDialog->toFront (true);
        return;
    }

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned (new SettingsPanel (deviceManager, updates));
    options.dialogTitle = "Einstellungen";
    options.dialogBackgroundColour = getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;
    settingsDialog = options.launchAsync();
}

void MainComponent::refreshUpdateButton()
{
    const auto version = updates.availableUpdate();
    updateButton.setVisible (version.isNotEmpty());
    updateButton.setEnabled (! updates.isBusy());
    if (version.isNotEmpty())
        updateButton.setButtonText ("Update auf " + version + " installieren");
}

void MainComponent::saveAudioState()
{
    auto* p = properties();
    if (p == nullptr)
        return;

    if (auto state = deviceManager.createStateXml())
    {
        state->removeAttribute ("audioDeviceRate");
        p->setValue ("audioState", state.get());
    }
}

void MainComponent::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    if (source == &deviceManager)
        saveAudioState();
    else
        refreshUpdateButton();
}

//==============================================================================
void MainComponent::timerCallback()
{
    const AudioHost::Meters m = host.takeMeters();
    const float gateDb = (float) gateSlider.getValue();

    // digital silence for a while usually means a missing permission or the wrong input
    silentTicks = (m.running && m.inputPeak <= 0.0f) ? silentTicks + 1 : 0;
    clipTicks = m.inputPeak >= 0.98f ? kTimerHz : std::max (0, clipTicks - 1);

    midiTicks = m.midiMessages != lastMidiCount ? 4 : std::max (0, midiTicks - 1);
    lastMidiCount = m.midiMessages;

    pitchDisplay.update (m, gateDb, midiTicks > 0);
    levelMeter.update (m.running ? m.levelDb : -120.0f, gateDb, clipTicks > 0);
    updateStatus (m);
}

void MainComponent::updateStatus (const AudioHost::Meters& m)
{
    juce::String text;

    text << (midiPortOk ? U ("MIDI-Ausgang \xe2\x80\x9eViolinMIDI\xe2\x80\x9c bereit \xe2\x80\x93 im Zielprogramm als MIDI-Ger\xc3\xa4t w\xc3\xa4hlen")
                        : U ("MIDI-Ausgang konnte nicht angelegt werden (virtuelle MIDI-Ports gibt es nur unter macOS)"))
         << "\n";

    if (auto* device = deviceManager.getCurrentAudioDevice(); device != nullptr && m.running)
    {
        const double ms = m.sampleRate > 0.0 ? 1000.0 * m.blockSize / m.sampleRate : 0.0;
        text << "Eingang: " << device->getName() << U ("  \xc2\xb7  ") << juce::String (m.sampleRate / 1000.0, 1) << " kHz"
             << U ("  \xc2\xb7  Puffer ") << m.blockSize << " Samples (" << juce::String (ms, 1) << " ms)\n";
    }
    else
    {
        text << U ("Kein Audio-Eingang aktiv \xe2\x80\x93 bitte unter \xe2\x80\x9e" "Einstellungen\xe2\x80\x9c w\xc3\xa4hlen")
             << (audioInitError.isNotEmpty() ? " (" + audioInitError + ")" : juce::String()) << "\n";
    }

    const juce::String error = host.lastError();
    const juce::String updateStatus = updates.statusText();
    if (error.isNotEmpty())
        text << U ("Audio-Fehler: ") << error;
    else if (updateStatus.isNotEmpty())
        text << updateStatus;
    else if (m.running && ! m.hasInput)
        text << U ("Das Ger\xc3\xa4t liefert keinen Eingangskanal. Bitte einen Eingang ausw\xc3\xa4hlen.");
    else if (silentTicks > kTimerHz * 3)
        text << U ("Es kommt kein Signal an. Richtiger Eingang? Mikrofon-Zugriff in den Systemeinstellungen (Datenschutz) erlaubt?");
    else if (clipTicks > 0)
        text << U ("Eingang \xc3\xbc" "bersteuert \xe2\x80\x93 Pegel am Interface oder die Eingangsverst\xc3\xa4rkung senken.");

    if (statusLabel.getText() != text)
        statusLabel.setText (text, juce::dontSendNotification);
}

//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour (kDim);
    g.setFont (juce::Font (juce::FontOptions (12.5f, juce::Font::bold)));
    for (const auto& h : headings)
        g.drawText (h.text, kMargin, h.y, getWidth() - 2 * kMargin, 18, juce::Justification::centredLeft);
}

void MainComponent::resized()
{
    headings.clear();
    auto area = getLocalBounds().reduced (kMargin, 14);

    auto heading = [&] (const juce::String& text)
    {
        area.removeFromTop (8);
        headings.add ({ area.getY(), text });
        area.removeFromTop (22);
    };
    auto row = [&] (juce::Label& label, juce::Component& control)
    {
        auto r = area.removeFromTop (kRow);
        label.setBounds (r.removeFromLeft (kLabelWidth));
        control.setBounds (r);
        area.removeFromTop (kGap);
    };

    pitchDisplay.setBounds (area.removeFromTop (kPitchHeight));

    heading ("EINGANG");
    levelMeter.setBounds (area.removeFromTop (16));
    area.removeFromTop (kGap);
    row (gainLabel, gainSlider);
    row (gateLabel, gateSlider);

    heading ("TONZIEHEN");
    {
        auto r = area.removeFromTop (kRow + 4);
        const int w = r.getWidth() / 3;
        modeButtons[0].setBounds (r.removeFromLeft (w));
        modeButtons[1].setBounds (r.removeFromLeft (w));
        modeButtons[2].setBounds (r);
        area.removeFromTop (kGap);
    }
    row (bendRangeLabel, bendRangeBox);

    heading (U ("LAUTST\xc3\x84RKE"));
    row (dynamicsLabel, dynamicsBox);
    row (velocityLabel, velocitySlider);

    heading ("ERKENNUNG");
    row (responseLabel, responseBox);
    row (lowestLabel, lowestBox);
    row (tuningLabel, tuningSlider);

    heading ("MIDI-AUSGANG");
    row (channelLabel, channelBox);

    auto buttons = area.removeFromBottom (kRow + 2);
    settingsButton.setBounds (buttons.removeFromLeft (150));
    panicButton.setBounds (buttons.removeFromRight (140));
    updateButton.setBounds (buttons.reduced (8, 0));
    area.removeFromBottom (kGap);
    statusLabel.setBounds (area.removeFromBottom (kStatusHeight));
}
