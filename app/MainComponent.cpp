#include "MainComponent.h"

#include "Brand.h"
#include "SettingsPanel.h"

#include <cmath>

using namespace brand;

namespace
{
    constexpr int kWidth = 960, kHeight = 620;
    constexpr int kMargin = 20, kHeader = 64, kFooter = 48;
    constexpr int kLeftWidth = 560, kColumnGap = 16, kNoteHeight = 212;
    constexpr int kCardPad = 16, kCardTitle = 26, kRowGap = 10, kDivider = 12, kLabelWidth = 104;
    constexpr int kTimerHz = 30;

    juce::String decimalComma (double value, int places)
    {
        return juce::String (value, places).replaceCharacter ('.', ',');
    }
}

//==============================================================================
MainComponent::MainComponent (const Options& o) : options (o)
{
    setOpaque (true);
    setWantsKeyboardFocus (true);

    // One settings file for every version, from before and after the rename.
    juce::PropertiesFile::Options fileOptions;
    fileOptions.applicationName = kLegacyName;
    fileOptions.filenameSuffix = "settings";
    fileOptions.osxLibrarySubFolder = "Application Support";
    fileOptions.millisecondsBeforeSaving = 500;
    appProperties.setStorageParameters (fileOptions);
    store = std::make_unique<SettingsStore> (*appProperties.getUserSettings());
    settings = store->load();

    addAndMakeVisible (noteDisplay);
    addAndMakeVisible (history);

    // --- expression
    addCard ("AUSDRUCK");
    setupSegments (modeButtons, modeRow, 1,
                   { "Stufenlos", U ("Vibrato gl\xc3\xa4tten"), U ("Halbt\xc3\xb6ne") },
                   { U ("Pitch Bend folgt der Saite exakt: Glissando und Vibrato kommen an. (Taste 1)"),
                     U ("Glissando wird \xc3\xbc" "bertragen, Vibrato herausgefiltert: das Instrument steht ruhig. (Taste 2)"),
                     U ("Kein Pitch Bend: jeder erreichte Halbton wird neu angeschlagen, z. B. f\xc3\xbcr Klavier. (Taste 3)") });
    addRow (nullptr, modeRow, 32);

    setupLabel (bendRangeLabel, "Pitch-Bend");
    for (int range : { 2, 4, 12, 24, 48 })
        bendRangeBox.addItem (U ("\xc2\xb1") + juce::String (range) + U (" Halbt\xc3\xb6ne"), range);
    bendRangeBox.setTooltip (U ("Muss mit dem Pitch-Bend-Bereich des Instruments \xc3\xbc" "bereinstimmen. Gr\xc3\xb6\xc3\x9f" "er = l\xc3\xa4ngere Glissandi ohne neuen Anschlag."));
    bendRangeBox.onChange = [this] { pushSettings(); };
    addAndMakeVisible (bendRangeBox);
    addRow (&bendRangeLabel, bendRangeBox);

    setupLabel (dynamicsLabel, "Dynamik");
    dynamicsBox.addItem ("Anschlag + Expression", 1);
    dynamicsBox.addItem ("Nur Anschlag", 2);
    dynamicsBox.addItem (U ("Feste St\xc3\xa4rke"), 3);
    dynamicsBox.setTooltip (U ("Anschlag + Expression: An- und Abschwellen des Bogens steuert laufend die Lautst\xc3\xa4rke (CC11). "
                               "Bewegen sich dabei Regler anderer Ger\xc3\xa4te mit, \xe2\x80\x9eNur Anschlag\xe2\x80\x9c w\xc3\xa4hlen."));
    dynamicsBox.onChange = [this] { pushSettings(); };
    addAndMakeVisible (dynamicsBox);
    addRow (&dynamicsLabel, dynamicsBox);

    setupLabel (velocityLabel, U ("Feste St\xc3\xa4rke"));
    setupSlider (velocitySlider, 1.0, 127.0, 1.0, "", 0);
    addRow (&velocityLabel, velocitySlider);

    // --- input and detection
    addCard ("EINGANG & ERKENNUNG");
    setupLabel (meterLabel, "Pegel");
    levelMeter.setTooltip (U ("Gr\xc3\xbcn: der Ton liegt \xc3\xbc" "ber der Schwelle (wei\xc3\x9f" "e Marke). Rot: \xc3\xbc" "bersteuert."));
    addAndMakeVisible (levelMeter);
    addRow (&meterLabel, levelMeter, 20);

    setupLabel (gainLabel, U ("Verst\xc3\xa4rkung"));
    setupSlider (gainSlider, -12.0, 36.0, 0.5, " dB", 1);
    gainSlider.setDoubleClickReturnValue (true, 0.0);
    gainSlider.setTooltip (U ("Kr\xc3\xa4" "ftig spielen: der Balken soll weit nach rechts gehen, ohne rot zu werden."));
    addRow (&gainLabel, gainSlider);

    setupLabel (gateLabel, "Schwelle");
    setupSlider (gateSlider, -70.0, -20.0, 0.5, " dB", 1);
    gateSlider.setDoubleClickReturnValue (true, -45.0);
    gateSlider.setTooltip (U ("Wei\xc3\x9f" "e Marke im Pegel. Knapp \xc3\xbc" "ber das Rauschen legen: niedrig spricht fr\xc3\xbch an, h\xc3\xb6her verhindert Geisternoten."));
    addRow (&gateLabel, gateSlider);

    setupLabel (responseLabel, "Ansprache");
    setupSegments (responseButtons, responseRow, 2, { "Schnell", "Normal", "Sicher" },
                   { U ("K\xc3\xbcrzeste Verz\xc3\xb6gerung."),
                     U ("Ausgewogen."),
                     U ("Wartet l\xc3\xa4nger, bis die Tonh\xc3\xb6he eindeutig ist. Hilft auf den tiefen Saiten.") });
    addRow (&responseLabel, responseRow, 28, true);

    setupLabel (lowestLabel, "Tiefster Ton");
    for (int n = vm::kAbsLowestNote; n <= 60; ++n)
    {
        juce::String item = noteName (n);
        if (n == 34) item << U ("  \xc2\xb7  7 Saiten");
        if (n == 41) item << U ("  \xc2\xb7  6 Saiten");
        if (n == 48) item << U ("  \xc2\xb7  5 Saiten");
        if (n == 55) item << U ("  \xc2\xb7  4 Saiten");
        lowestBox.addItem (item, n);
    }
    lowestBox.setTooltip (U ("T\xc3\xb6ne darunter werden ignoriert. H\xc3\xb6her gesetzt reagieren auch tiefe Lagen schneller."));
    lowestBox.onChange = [this] { pushSettings(); };
    addAndMakeVisible (lowestBox);
    addRow (&lowestLabel, lowestBox);

    setupLabel (tuningLabel, "Kammerton A");
    setupSlider (tuningSlider, 430.0, 450.0, 0.5, " Hz", 1);
    tuningSlider.setDoubleClickReturnValue (true, 440.0);
    addRow (&tuningLabel, tuningSlider);

    // --- header and footer
    for (int channel = 1; channel <= 16; ++channel)
        channelBox.addItem ("MIDI-Kanal " + juce::String (channel), channel);
    channelBox.setTooltip (U ("Auf diesem Kanal sendet die App. Ein eigener Kanal h\xc3\xa4lt die Geige von anderen MIDI-Ger\xc3\xa4ten getrennt."));
    channelBox.onChange = [this] { pushSettings(); };
    addAndMakeVisible (channelBox);

    settingsButton.setButtonText ("Einstellungen");
    settingsButton.setTooltip (U ("Audio-Eingang w\xc3\xa4hlen, nach Updates suchen, Version wechseln."));
    settingsButton.onClick = [this] { showSettings(); };
    addAndMakeVisible (settingsButton);

    updateButton.setColour (juce::TextButton::buttonColourId, colour::accent);
    updateButton.setColour (juce::TextButton::textColourOffId, colour::window);
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

    applySettingsToControls();
    setSize (kWidth, kHeight);

    updates.onRestart = [] { juce::JUCEApplication::getInstance()->systemRequestedQuit(); };
    updates.addChangeListener (this);
    if (options.notice.isNotEmpty())
        updates.setNotice (options.notice);

    if (options.showSettingsAtStart)
    {
        juce::Component::SafePointer<MainComponent> safeThis (this);
        juce::MessageManager::callAsync ([safeThis] { if (safeThis != nullptr) safeThis->showSettings(); });
    }

    startTimerHz (kTimerHz);
    if (! options.openAudio)
        return;

    // --- MIDI out and audio in
    midiPortOk = host.openMidiPort (settings.midiPortName);

    auto savedState = store->audioState();
    const bool firstRun = savedState == nullptr;
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

    store->save (settings);
    store->flush();
}

//==============================================================================
MainComponent::Card& MainComponent::addCard (const juce::String& title)
{
    cards.push_back ({});
    cards.back().title = title;
    return cards.back();
}

void MainComponent::addRow (juce::Label* label, juce::Component& control, int height, bool dividerAbove)
{
    jassert (! cards.empty());
    cards.back().rows.push_back ({ label, &control, height, dividerAbove });
}

void MainComponent::setupLabel (juce::Label& label, const juce::String& text)
{
    label.setText (text, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centredLeft);
    label.setFont (font (13.5f));
    label.setColour (juce::Label::textColourId, colour::textDim);
    label.setBorderSize ({ 0, 0, 0, 0 });
    addAndMakeVisible (label);
}

void MainComponent::setupSlider (juce::Slider& slider, double low, double high, double step,
                                 const juce::String& suffix, int decimals)
{
    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 66, 24);
    slider.setRange (low, high, step);
    slider.setTextValueSuffix (suffix);
    slider.setNumDecimalPlacesToDisplay (decimals);
    slider.onValueChange = [this] { pushSettings(); };
    addAndMakeVisible (slider);
}

void MainComponent::setupSegments (juce::TextButton* buttons, ButtonRow& row, int radioGroup,
                                   const juce::StringArray& names, const juce::StringArray& tips)
{
    const int count = names.size();
    for (int i = 0; i < count; ++i)
    {
        auto& b = buttons[i];
        b.setButtonText (names[i]);
        b.setTooltip (tips[i]);
        b.setClickingTogglesState (true);
        b.setRadioGroupId (radioGroup);
        b.setConnectedEdges ((i > 0 ? juce::Button::ConnectedOnLeft : 0) | (i < count - 1 ? juce::Button::ConnectedOnRight : 0));
        b.onClick = [this, &b] { if (b.getToggleState()) pushSettings(); };
        row.add (b);
    }
    addAndMakeVisible (row);
}

//==============================================================================
void MainComponent::applySettingsToControls()
{
    loading = true;
    const auto& e = settings.engine;

    modeButtons[juce::jlimit (0, 2, (int) e.bendMode)].setToggleState (true, juce::dontSendNotification);
    responseButtons[juce::jlimit (0, 2, (int) e.response)].setToggleState (true, juce::dontSendNotification);
    bendRangeBox.setSelectedId (e.bendRangeSemis, juce::dontSendNotification);
    dynamicsBox.setSelectedId ((int) e.dynamics + 1, juce::dontSendNotification);
    lowestBox.setSelectedId (e.lowestNote, juce::dontSendNotification);
    channelBox.setSelectedId (e.midiChannel, juce::dontSendNotification);
    gainSlider.setValue (e.inputGainDb, juce::dontSendNotification);
    gateSlider.setValue (e.gateDb, juce::dontSendNotification);
    velocitySlider.setValue (e.fixedVelocity, juce::dontSendNotification);
    tuningSlider.setValue (e.a4Hz, juce::dontSendNotification);

    loading = false;
    pushSettings();
}

void MainComponent::pushSettings()
{
    if (loading)
        return;

    auto& e = settings.engine;
    for (int i = 0; i < 3; ++i)
    {
        if (modeButtons[i].getToggleState())     e.bendMode = (vm::BendMode) i;
        if (responseButtons[i].getToggleState()) e.response = (vm::Response) i;
    }

    e.bendRangeSemis = bendRangeBox.getSelectedId() > 0 ? bendRangeBox.getSelectedId() : 2;
    e.dynamics = (vm::DynamicsMode) juce::jlimit (0, 2, dynamicsBox.getSelectedId() - 1);
    e.lowestNote = lowestBox.getSelectedId() > 0 ? lowestBox.getSelectedId() : 34;
    e.midiChannel = channelBox.getSelectedId() > 0 ? channelBox.getSelectedId() : 1;
    e.inputGainDb = (float) gainSlider.getValue();
    e.gateDb = (float) gateSlider.getValue();
    e.fixedVelocity = juce::roundToInt (velocitySlider.getValue());
    e.a4Hz = (float) tuningSlider.getValue();
    host.setSettings (e);

    const bool fixed = e.dynamics == vm::DynamicsMode::Fixed;
    velocitySlider.setEnabled (fixed);
    velocityLabel.setAlpha (fixed ? 1.0f : 0.45f);
    const bool bends = e.bendMode != vm::BendMode::Quantized;
    bendRangeBox.setEnabled (bends);
    bendRangeLabel.setAlpha (bends ? 1.0f : 0.45f);

    store->save (settings);
}

void MainComponent::setMode (int index)
{
    modeButtons[juce::jlimit (0, 2, index)].setToggleState (true, juce::sendNotification);
    pushSettings();
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

bool MainComponent::savedWindowPosition (juce::Point<int>& topLeft) const
{
    if (! settings.hasWindowPosition)
        return false;

    // Only if the window would still be on a screen (a display may be gone).
    const juce::Rectangle<int> wanted (settings.windowX, settings.windowY, kWidth, kHeight);
    const auto* display = juce::Desktop::getInstance().getDisplays().getDisplayForRect (wanted);
    if (display == nullptr || ! display->userBounds.toNearestInt().contains (wanted.withSize (kWidth / 2, 60)))
        return false;

    topLeft = { settings.windowX, settings.windowY };
    return true;
}

void MainComponent::rememberWindowPosition (juce::Point<int> topLeft)
{
    settings.hasWindowPosition = true;
    settings.windowX = topLeft.x;
    settings.windowY = topLeft.y;
    store->save (settings);
}

//==============================================================================
void MainComponent::showSettings()
{
    if (settingsDialog != nullptr)
    {
        settingsDialog->toFront (true);
        return;
    }

    juce::DialogWindow::LaunchOptions launch;
    launch.content.setOwned (new SettingsPanel (deviceManager, updates));
    launch.dialogTitle = "Einstellungen";
    launch.dialogBackgroundColour = colour::window;
    launch.escapeKeyTriggersCloseButton = true;
    launch.useNativeTitleBar = true;
    launch.resizable = false;
    settingsDialog = launch.launchAsync();
}

void MainComponent::refreshUpdateButton()
{
    const auto version = updates.availableUpdate();
    const bool show = version.isNotEmpty();
    updateButton.setEnabled (! updates.isBusy());
    if (show)
        updateButton.setButtonText ("Update " + version + " installieren");

    if (updateButton.isVisible() != show)
    {
        updateButton.setVisible (show);
        resized();
        repaint (0, 0, getWidth(), kHeader);
    }
}

void MainComponent::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    if (source == &deviceManager)
    {
        if (auto state = deviceManager.createStateXml())
            store->saveAudioState (*state);
    }
    else
    {
        refreshUpdateButton();
    }
}

//==============================================================================
AudioHost::Meters MainComponent::demoMeters()
{
    // A short phrase with vibrato and one slide, to show every display at work.
    struct Step { int note; double seconds; bool slide; };
    static const Step steps[] = { { 67, 0.8, false }, { 69, 0.45, false }, { 71, 0.45, false }, { 72, 1.0, false },
                                  { 74, 0.5, false }, { 79, 1.2, true },   { 76, 0.5, false }, { 74, 0.5, false },
                                  { 72, 0.9, false }, { -1, 0.7, false } };

    demoTime += 1.0 / kTimerHz;
    double total = 0.0;
    for (const auto& s : steps)
        total += s.seconds;

    double t = std::fmod (demoTime, total);
    int index = 0;
    while (index < (int) std::size (steps) - 1 && t >= steps[index].seconds)
        t -= steps[index++].seconds;

    AudioHost::Meters m;
    m.running = m.hasInput = true;
    m.sampleRate = 48000.0;
    m.blockSize = 128;
    const auto& step = steps[index];
    if (step.note < 0)
    {
        m.levelDb = -80.0f;
        return m;
    }

    double pitch = step.note + 0.2 * std::sin (2.0 * juce::MathConstants<double>::pi * 5.5 * t) * juce::jmin (1.0, t / 0.3);
    if (step.slide && index > 0 && t < 0.22)
        pitch = steps[index - 1].note + (step.note - steps[index - 1].note) * (t / 0.22);

    m.pitch = m.outputPitch = (float) pitch;
    m.note = step.note;
    m.voiced = m.active = true;
    m.clarity = 0.97f;
    m.levelDb = (float) (-24.0 + 4.0 * std::sin (demoTime * 1.3));
    m.midiMessages = ++demoMessages;
    return m;
}

void MainComponent::timerCallback()
{
    const auto m = options.demo ? demoMeters() : host.takeMeters();
    const float gateDb = settings.engine.gateDb;

    // digital silence for a while usually means a missing permission or the wrong input
    silentTicks = (m.running && m.inputPeak <= 0.0f && ! options.demo) ? silentTicks + 1 : 0;
    clipTicks = m.inputPeak >= 0.98f ? kTimerHz : juce::jmax (0, clipTicks - 1);
    midiTicks = m.midiMessages != lastMidiCount ? 4 : juce::jmax (0, midiTicks - 1);
    lastMidiCount = m.midiMessages;

    LiveFrame f;
    f.pitch = m.pitch;
    f.outputPitch = m.active ? m.outputPitch : m.pitch;
    f.levelDb = m.running ? m.levelDb : -120.0f;
    f.note = m.note;
    f.active = m.active;
    f.heard = m.voiced && m.levelDb >= gateDb - 6.0f;

    noteDisplay.update (f, settings.engine.a4Hz);
    history.push (f);
    levelMeter.update (f.levelDb, gateDb, clipTicks > 0);
    updateStatus (m);
}

void MainComponent::updateStatus (const AudioHost::Meters& m)
{
    juce::String input, midi, footer;
    juce::Colour inputC, midiC, footerC;

    if (options.demo)
    {
        input = "Demo";
        inputC = colour::good;
    }
    else if (auto* device = deviceManager.getCurrentAudioDevice(); device != nullptr && m.running)
    {
        const double ms = m.sampleRate > 0.0 ? 1000.0 * m.blockSize / m.sampleRate : 0.0;
        input = device->getName() + U ("  \xc2\xb7  ") + decimalComma (m.sampleRate / 1000.0, 1) + " kHz"
              + U ("  \xc2\xb7  ") + decimalComma (ms, 1) + " ms";
        inputC = (m.hasInput && silentTicks <= kTimerHz * 3) ? colour::good : colour::accent;
    }
    else
    {
        input = options.openAudio ? "Kein Eingang" : "Ohne Audio";
        inputC = colour::bad;
    }

    if (midiPortOk || options.demo)
    {
        midi = U ("MIDI-Ausgang \xe2\x80\x9e") + settings.midiPortName + U ("\xe2\x80\x9c");
        midiC = midiTicks > 0 ? colour::accent : colour::good;
    }
    else
    {
        midi = options.openAudio ? U ("MIDI-Ausgang fehlt") : U ("MIDI aus");
        midiC = colour::bad;
    }

    const auto error = host.lastError();
    const auto update = updates.statusText();
    if (error.isNotEmpty())
        footer = U ("Audio-Fehler: ") + error, footerC = colour::bad;
    else if (update.isNotEmpty())
        footer = update, footerC = colour::accent;
    else if (options.openAudio && ! options.demo && ! m.running)
        footer = U ("Kein Audio-Eingang aktiv \xe2\x80\x93 unter \xe2\x80\x9e" "Einstellungen\xe2\x80\x9c das Interface w\xc3\xa4hlen."), footerC = colour::bad;
    else if (m.running && ! m.hasInput)
        footer = U ("Das Ger\xc3\xa4t liefert keinen Eingang. Unter \xe2\x80\x9e" "Einstellungen\xe2\x80\x9c einen Eingang w\xc3\xa4hlen."), footerC = colour::bad;
    else if (silentTicks > kTimerHz * 3)
        footer = U ("Es kommt kein Signal an. Richtiger Eingang? Mikrofon-Zugriff unter Systemeinstellungen \xe2\x86\x92 Datenschutz erlaubt?"), footerC = colour::accent;
    else if (clipTicks > 0)
        footer = U ("Eingang \xc3\xbc" "bersteuert \xe2\x80\x93 Pegel am Interface oder die Verst\xc3\xa4rkung senken."), footerC = colour::bad;
    else
        footer = U ("Tasten 1 \xc2\xb7 2 \xc2\xb7 3 wechseln den Modus. Alles wird automatisch gespeichert."), footerC = colour::textFaint;

    if (input != inputChip || midi != midiChip || inputC != inputDot || midiC != midiDot)
    {
        inputChip = input;
        midiChip = midi;
        inputDot = inputC;
        midiDot = midiC;
        repaint (0, 0, getWidth(), kHeader);
    }

    if (footer != footerText || footerC != footerDot)
    {
        footerText = footer;
        footerDot = footerC;
        repaint (0, getHeight() - kFooter, getWidth(), kFooter);
    }
}

//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (colour::window);

    // --- header: name on the left, status in the middle
    const int x0 = kMargin;
    drawMark (g, juce::Rectangle<float> ((float) x0, 15.0f, 34.0f, 34.0f));
    g.setColour (colour::text);
    g.setFont (font (20.0f, true));
    g.drawText (kName, x0 + 44, 12, 180, 24, juce::Justification::centredLeft);
    g.setColour (colour::textFaint);
    g.setFont (font (12.0f));
    g.drawText (juce::String (kTagline) + U ("  \xc2\xb7  ") + updater::runningVersion(), x0 + 44, 35, 200, 16,
                juce::Justification::centredLeft);

    const int chipsLeft = x0 + 200;
    const int chipsRight = (updateButton.isVisible() ? updateButton.getX() : channelBox.getX()) - 12;
    auto drawChip = [&] (juce::Rectangle<float> r, const juce::String& text, juce::Colour dot)
    {
        g.setColour (colour::raised);
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
        g.setColour (colour::line);
        g.drawRoundedRectangle (r.reduced (0.5f), r.getHeight() * 0.5f, 1.0f);
        g.setColour (dot.withAlpha (0.25f));
        g.fillEllipse (r.getX() + 9.0f, r.getCentreY() - 6.5f, 13.0f, 13.0f);
        g.setColour (dot);
        g.fillEllipse (r.getX() + 12.5f, r.getCentreY() - 3.0f, 6.0f, 6.0f);
        g.setColour (colour::textDim);
        g.setFont (font (12.5f));
        g.drawText (text, r.withTrimmedLeft (28.0f).withTrimmedRight (12.0f), juce::Justification::centredLeft, true);
    };

    const auto chipFont = font (12.5f);
    const float available = (float) juce::jmax (0, chipsRight - chipsLeft);
    const float wMidi = juce::jmin (available * 0.45f, juce::GlyphArrangement::getStringWidth (chipFont, midiChip) + 42.0f);
    const float wInput = juce::jmin (available - wMidi - 8.0f, juce::GlyphArrangement::getStringWidth (chipFont, inputChip) + 42.0f);
    float x = (float) chipsRight;
    if (wMidi > 60.0f)
    {
        x -= wMidi;
        drawChip ({ x, 18.0f, wMidi, 28.0f }, midiChip, midiDot);
        x -= 8.0f;
    }
    if (wInput > 60.0f)
        drawChip ({ x - wInput, 18.0f, wInput, 28.0f }, inputChip, inputDot);

    g.setColour (colour::line.withAlpha (0.6f));
    g.fillRect (0, kHeader - 1, getWidth(), 1);

    // --- cards
    for (const auto& card : cards)
    {
        const auto r = card.bounds.toFloat();
        g.setColour (colour::panel);
        g.fillRoundedRectangle (r, 12.0f);
        g.setColour (colour::line.withAlpha (0.7f));
        g.drawRoundedRectangle (r.reduced (0.5f), 12.0f, 1.0f);
        g.setColour (colour::textFaint);
        g.setFont (headingFont());
        g.drawText (card.title, card.bounds.reduced (kCardPad + 2, kCardPad - 2).removeFromTop (14), juce::Justification::centredLeft);

        g.setColour (colour::line.withAlpha (0.8f));
        for (int y : card.dividerY)
            g.fillRect (card.bounds.getX() + kCardPad, y, card.bounds.getWidth() - 2 * kCardPad, 1);
    }

    // --- footer: one line saying what is going on
    const int fy = getHeight() - kFooter;
    g.setColour (colour::line.withAlpha (0.6f));
    g.fillRect (0, fy, getWidth(), 1);
    g.setColour (footerDot);
    g.fillEllipse ((float) kMargin, (float) fy + (float) kFooter * 0.5f - 3.0f, 6.0f, 6.0f);
    g.setColour (footerDot == colour::textFaint ? colour::textFaint : colour::textDim);
    g.setFont (font (13.0f));
    g.drawText (footerText, kMargin + 16, fy, panicButton.getX() - kMargin - 28, kFooter, juce::Justification::centredLeft, true);
}

void MainComponent::resized()
{
    auto area = getLocalBounds();

    auto header = area.removeFromTop (kHeader).reduced (kMargin, 16);
    settingsButton.setBounds (header.removeFromRight (124));
    header.removeFromRight (10);
    channelBox.setBounds (header.removeFromRight (136));
    header.removeFromRight (10);
    if (updateButton.isVisible())
        updateButton.setBounds (header.removeFromRight (196));

    auto footer = area.removeFromBottom (kFooter).reduced (kMargin, 9);
    panicButton.setBounds (footer.removeFromRight (140));

    auto body = area.reduced (kMargin, 12);
    auto left = body.removeFromLeft (kLeftWidth);
    body.removeFromLeft (kColumnGap);

    noteDisplay.setBounds (left.removeFromTop (kNoteHeight));
    left.removeFromTop (12);
    history.setBounds (left);

    auto right = body;
    for (size_t i = 0; i < cards.size(); ++i)
    {
        auto& card = cards[i];
        int height = 2 * kCardPad + kCardTitle;
        for (size_t r = 0; r < card.rows.size(); ++r)
            height += card.rows[r].height + (r > 0 ? kRowGap : 0) + (card.rows[r].dividerAbove ? kDivider : 0);

        // the last card reaches down to the bottom of the live view
        card.bounds = (i + 1 == cards.size()) ? right : right.removeFromTop (height);
        right.removeFromTop (12);

        auto inner = card.bounds.reduced (kCardPad);
        inner.removeFromTop (kCardTitle);
        card.dividerY.clear();

        for (size_t r = 0; r < card.rows.size(); ++r)
        {
            const auto& row = card.rows[r];
            if (r > 0)
                inner.removeFromTop (kRowGap);
            if (row.dividerAbove)
            {
                card.dividerY.push_back (inner.getY() + kDivider / 2 - 4);
                inner.removeFromTop (kDivider);
            }

            auto line = inner.removeFromTop (row.height);
            if (row.label != nullptr)
                row.label->setBounds (line.removeFromLeft (kLabelWidth));
            row.control->setBounds (line);
        }
    }
}
