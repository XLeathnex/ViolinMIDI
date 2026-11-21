#include "MainComponent.h"

#include "Brand.h"
#include "Params.h"
#include "Scale.h"
#include "SettingsPanel.h"

#include <cmath>

using namespace brand;

namespace
{
    constexpr int kWidth = 1120, kHeight = 626;
    constexpr int kDisplayHeight = 292;
    constexpr int kStatusHeight = 40;
    constexpr int kMargin = 24, kColumnGap = 28;
    constexpr int kTitleHeight = 18, kTitleGap = 12, kNameHeight = 16, kNameGap = 6, kControlHeight = 24, kItemGap = 14;
    constexpr int kTimerHz = 30;

    juce::String decimalComma (double value, int places)
    {
        return juce::String (value, places).replaceCharacter ('.', ',');
    }

    juce::String signedDb (double value)
    {
        return (value < 0.0 ? U ("\xe2\x88\x92") : juce::String()) + decimalComma (std::abs (value), 1) + " dB";
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

    addAndMakeVisible (display);

    masterButton.setClickingTogglesState (true);
    masterButton.setTooltip (U ("Schaltet die Tonh\xc3\xb6henerkennung ein und aus. Aus beendet sofort alle Noten. Leertaste."));
    masterButton.onClick = [this] { pushSettings(); };
    addAndMakeVisible (masterButton);

    // --- input
    addColumn ("Eingang");
    setupName (meterName, "Pegel");
    setupValue (meterValue);
    levelMeter.setTooltip (U ("Die wei\xc3\x9f" "e Marke ist die Schwelle. Dar\xc3\xbc" "ber kann eine Note beginnen."));
    addAndMakeVisible (levelMeter);
    addItem (meterName, levelMeter, &meterValue);

    setupName (gainName, U ("Verst\xc3\xa4rkung"));
    setupValue (gainValue);
    setupSlider (gainSlider, -12.0, 36.0, 0.5);
    gainSlider.setDoubleClickReturnValue (true, 0.0);
    gainSlider.setTooltip (U ("Bei kr\xc3\xa4" "ftigem Spiel soll der Pegel weit nach rechts gehen, ohne rot zu werden. Doppelklick: 0 dB."));
    addItem (gainName, gainSlider, &gainValue);

    setupName (gateName, "Schwelle");
    setupValue (gateValue);
    setupSlider (gateSlider, -70.0, -20.0, 0.5);
    gateSlider.setDoubleClickReturnValue (true, -45.0);
    gateSlider.setTooltip (U ("Knapp \xc3\xbc" "ber das Grundrauschen legen. Niedriger spricht fr\xc3\xbc" "her an, h\xc3\xb6her verhindert Geisternoten."));
    addItem (gateName, gateSlider, &gateValue);

    // --- detection
    addColumn ("Erkennung");
    setupName (responseName, "Ansprache");
    setupSegments (responseButtons, responseRow, 2, { "Schnell", "Normal", "Sicher" },
                   { U ("K\xc3\xbcrzeste Verz\xc3\xb6gerung."),
                     U ("Ausgewogen."),
                     U ("Wartet, bis die Tonh\xc3\xb6he eindeutig ist. Hilft auf den tiefen Saiten.") });
    addItem (responseName, responseRow);

    setupName (lowestName, "Tiefster Ton");
    for (int n = vm::kAbsLowestNote; n <= 60; ++n)
    {
        juce::String item = noteName (n);
        if (n == 34) item << U (" (7 Saiten)");
        if (n == 41) item << U (" (6 Saiten)");
        if (n == 48) item << U (" (5 Saiten)");
        if (n == 55) item << U (" (4 Saiten)");
        lowestBox.addItem (item, n);
    }
    setupBox (lowestBox, U ("T\xc3\xb6ne darunter werden ignoriert. H\xc3\xb6her gesetzt braucht die Erkennung weniger Rechenzeit."));
    addItem (lowestName, lowestBox);

    setupName (tuningName, "Kammerton");
    setupValue (tuningValue);
    setupSlider (tuningSlider, 430.0, 450.0, 0.5);
    tuningSlider.setDoubleClickReturnValue (true, 440.0);
    tuningSlider.setTooltip (U ("Doppelklick: 440 Hz."));
    addItem (tuningName, tuningSlider, &tuningValue);

    // --- pitch
    addColumn (U ("Tonh\xc3\xb6he"));
    setupName (modeName, "Bewegung");
    setupSegments (modeButtons, modeRow, 1, { "Stufenlos", U ("Gegl\xc3\xa4ttet"), U ("Halbt\xc3\xb6ne") },
                   { U ("Pitch Bend folgt der Saite genau, mit Glissando und Vibrato. Taste 1."),
                     U ("Glissando wird \xc3\xbc" "bertragen, Vibrato herausgefiltert. Taste 2."),
                     U ("Kein Pitch Bend. Jeder erreichte Halbton wird neu angeschlagen, etwa f\xc3\xbcr Klavier. Taste 3.") });
    addItem (modeName, modeRow);

    setupName (bendRangeName, "Pitch-Bend-Bereich");
    for (int i = 0; i < SettingsStore::kNumBendRanges; ++i)
    {
        const float range = SettingsStore::kBendRanges[i];
        bendRangeBox.addItem (U ("\xc2\xb1") + (range < 1.0f ? decimalComma (range, 1) : juce::String (juce::roundToInt (range)))
                                  + (range == 1.0f ? U (" Halbton") : U (" Halbt\xc3\xb6ne")),
                              i + 1);
    }
    setupBox (bendRangeBox, U ("Muss mit dem Pitch-Bend-Bereich des Instruments \xc3\xbc" "bereinstimmen. Ein gr\xc3\xb6\xc3\x9f" "erer Bereich erlaubt "
                               "l\xc3\xa4ngere Glissandi ohne neuen Anschlag. Wird der Bereich \xc3\xbc" "berschritten, bleibt der Bend am Anschlag stehen."));
    addItem (bendRangeName, bendRangeBox);

    setupName (smoothingName, U ("Vibrato-Gl\xc3\xa4ttung"));
    setupValue (smoothingValue);
    setupSlider (smoothingSlider, 0.0, 1.0, 0.01);
    smoothingSlider.setDoubleClickReturnValue (true, 0.5);
    smoothingSlider.setTooltip (U ("Wie stark Vibrato in der Bewegung \xe2\x80\x9eGegl\xc3\xa4ttet\xe2\x80\x9c herausgefiltert wird. Doppelklick: 50 %."));
    addItem (smoothingName, smoothingSlider, &smoothingValue);

    // --- key and triad
    addColumn ("Tonart");
    setupName (keyName, "Tonart");
    static const char* roots[] = { "C", "C\xe2\x99\xaf", "D", "D\xe2\x99\xaf", "E", "F", "F\xe2\x99\xaf", "G", "G\xe2\x99\xaf", "A", "A\xe2\x99\xaf", "B" };
    for (int i = 0; i < 12; ++i)
        rootBox.addItem (U (roots[i]), i + 1);
    static const char* modes[] = { "Dur (ionisch)", "Dorisch", "Phrygisch", "Lydisch", "Mixolydisch", "Moll (\xc3\xa4olisch)", "Lokrisch" };
    for (int i = 0; i < 7; ++i)
        scaleBox.addItem (U (modes[i]), i + 1);
    setupBox (rootBox, "Grundton der Tonart.");
    setupBox (scaleBox, U ("Kirchentonart. Sie bestimmt auch, welche Dreikl\xc3\xa4nge entstehen."));
    keyRow.set (rootBox, scaleBox, 58);
    addAndMakeVisible (keyRow);
    addItem (keyName, keyRow);

    setupName (filterName, U ("Nur T\xc3\xb6ne der Tonart"));
    setupSegments (filterButtons, filterRow, 3, { "Aus", "Skala", "Pentatonik" },
                   { U ("Jeder gespielte Ton wird gesendet."),
                     U ("Nur die sieben T\xc3\xb6ne der Tonart werden angeschlagen. Ein fremder Ton beendet die Note."),
                     U ("Nur die f\xc3\xbcnf T\xc3\xb6ne der Pentatonik: Dur-Pentatonik bei Dur, Lydisch und Mixolydisch, sonst Moll-Pentatonik.") });
    addItem (filterName, filterRow);

    setupName (holdName, "Pentatonik-Ton halten");
    setupSegments (holdButtons, holdRow, 4, { "Aus", "An" },
                   { U ("Ein fremder Ton beendet die Note."),
                     U ("Der letzte g\xc3\xbcltige Ton klingt weiter, solange der Pegel \xc3\xbc" "ber der Schwelle liegt, bis der n\xc3\xa4" "chste g\xc3\xbcltige Ton kommt.") });
    addItem (holdName, holdRow);

    setupName (triadName, "Dreiklang");
    setupSegments (triadButtons, triadRow, 5, { "Aus", U ("3 T\xc3\xb6ne"), U ("2 T\xc3\xb6ne") },
                   { U ("Kein Dreiklang."),
                     U ("Zu jedem Ton erklingen Grundton, Terz und Quinte aus der Tonart auf dem Dreiklang-Kanal."),
                     U ("Nur Terz und Quinte auf dem Dreiklang-Kanal, der Grundton bleibt allein auf dem MIDI-Kanal.") });
    addItem (triadName, triadRow);

    // --- MIDI
    addColumn ("MIDI");
    setupName (channelName, "MIDI-Kanal");
    setupName (triadChannelName, "Dreiklang-Kanal");
    for (int channel = 1; channel <= 16; ++channel)
    {
        channelBox.addItem ("Kanal " + juce::String (channel), channel);
        triadChannelBox.addItem ("Kanal " + juce::String (channel), channel);
    }
    setupBox (channelBox, U ("Ein eigener Kanal h\xc3\xa4lt die Geige in MainStage von anderen MIDI-Ger\xc3\xa4ten getrennt."));
    setupBox (triadChannelBox, U ("Auf diesem Kanal erklingt der Dreiklang, zum Beispiel mit einem zweiten Instrument."));
    addItem (channelName, channelBox);
    addItem (triadChannelName, triadChannelBox);

    setupName (dynamicsName, "Dynamik");
    dynamicsBox.addItem ("Anschlag und Expression", 1);
    dynamicsBox.addItem ("Nur Anschlag", 2);
    dynamicsBox.addItem (U ("Feste St\xc3\xa4rke"), 3);
    setupBox (dynamicsBox, U ("Anschlag und Expression: An- und Abschwellen des Bogens steuert laufend die Lautst\xc3\xa4rke (CC 11). "
                              "Bewegen sich dabei Regler anderer Ger\xc3\xa4te mit, \xe2\x80\x9eNur Anschlag\xe2\x80\x9c w\xc3\xa4hlen."));
    addItem (dynamicsName, dynamicsBox);

    setupName (velocityName, U ("Feste Anschlagst\xc3\xa4rke"));
    setupValue (velocityValue);
    setupSlider (velocitySlider, 1.0, 127.0, 1.0);
    addItem (velocityName, velocitySlider, &velocityValue);

    // --- status line
    settingsButton.setButtonText (U ("Einstellungen \xe2\x80\xa6"));
    settingsButton.setTooltip (U ("Audio-Eingang, MIDI-Steuerung, Updates und fr\xc3\xbchere Versionen."));
    settingsButton.onClick = [this] { showSettings(); };
    addAndMakeVisible (settingsButton);

    // the one prominent button, and only while an update is waiting
    updateButton.setColour (juce::TextButton::buttonColourId, colour::accent);
    updateButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff1a1a1a));
    updateButton.onClick = [this]
    {
        const auto version = updates.availableUpdate();
        if (version.isNotEmpty())
            updates.switchTo (version);
    };
    addChildComponent (updateButton);

    panicButton.setButtonText ("Alle Noten aus");
    panicButton.setTooltip (U ("Beendet h\xc3\xa4ngende Noten und setzt Pitch Bend und Expression zur\xc3\xbc" "ck."));
    panicButton.onClick = [this] { host.requestAllNotesOff(); };
    addAndMakeVisible (panicButton);

    applySettingsToControls();
    setSize (kWidth, kHeight);

    // --- remote control by MIDI
    remote.setExcludedName (settings.midiPortName);
    remote.fromJson (settings.midiMapJson);
    remote.onParam = [this] (int index, bool isNote, int value) { remoteControl (index, isNote, value); };
    remote.addChangeListener (this);

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

    // --- MIDI out, MIDI in and audio in
    midiPortOk = host.openMidiPort (settings.midiPortName);
    remote.setInput (settings.midiInputName);

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

    remote.onParam = nullptr;
    remote.removeChangeListener (this);
    updates.removeChangeListener (this);
    deviceManager.removeChangeListener (this);
    deviceManager.removeAudioCallback (&host); // ends the sounding note, see AudioHost::audioDeviceStopped
    deviceManager.closeAudioDevice();

    store->save (settings);
    store->flush();
}

//==============================================================================
void MainComponent::addColumn (const juce::String& title)
{
    columns.push_back ({});
    columns.back().title = title;
}

void MainComponent::addItem (juce::Label& name, juce::Component& control, juce::Label* value)
{
    jassert (! columns.empty());
    columns.back().items.push_back ({ &name, value, &control });
}

void MainComponent::setupName (juce::Label& label, const juce::String& text)
{
    label.setText (text, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centredLeft);
    label.setFont (font (12.5f));
    label.setColour (juce::Label::textColourId, colour::secondary);
    label.setBorderSize ({ 0, 0, 0, 0 });
    label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (label);
}

void MainComponent::setupValue (juce::Label& label)
{
    label.setJustificationType (juce::Justification::centredRight);
    label.setFont (font (12.5f));
    label.setColour (juce::Label::textColourId, colour::text);
    label.setBorderSize ({ 0, 0, 0, 0 });
    label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (label);
}

void MainComponent::setupSlider (juce::Slider& slider, double low, double high, double step)
{
    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRange (low, high, step);
    slider.setMouseDragSensitivity (300);
    slider.onValueChange = [this] { pushSettings(); };
    addAndMakeVisible (slider);
}

void MainComponent::setupBox (juce::ComboBox& box, const juce::String& tooltip)
{
    box.setTooltip (tooltip);
    box.onChange = [this] { pushSettings(); };
    addAndMakeVisible (box);
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

int MainComponent::selectedSegment (const juce::TextButton* buttons, int count)
{
    for (int i = 0; i < count; ++i)
        if (buttons[i].getToggleState())
            return i;
    return 0;
}

//==============================================================================
void MainComponent::applySettingsToControls()
{
    loading = true;
    const auto& e = settings.engine;
    const auto quiet = juce::dontSendNotification;

    masterButton.setToggleState (e.enabled, quiet);
    modeButtons[juce::jlimit (0, 2, (int) e.bendMode)].setToggleState (true, quiet);
    responseButtons[juce::jlimit (0, 2, (int) e.response)].setToggleState (true, quiet);
    filterButtons[! e.keyFilter ? 0 : (e.pentatonic ? 2 : 1)].setToggleState (true, quiet);
    holdButtons[e.pentaSustain ? 1 : 0].setToggleState (true, quiet);
    triadButtons[! e.triad ? 0 : (e.triadWithRoot ? 1 : 2)].setToggleState (true, quiet);

    bendRangeBox.setSelectedId (SettingsStore::bendRangeIndex (e.bendRangeSemis) + 1, quiet);
    dynamicsBox.setSelectedId ((int) e.dynamics + 1, quiet);
    lowestBox.setSelectedId (e.lowestNote, quiet);
    channelBox.setSelectedId (e.midiChannel, quiet);
    triadChannelBox.setSelectedId (e.triadChannel, quiet);
    rootBox.setSelectedId (e.keyRoot + 1, quiet);
    scaleBox.setSelectedId (e.scaleMode + 1, quiet);

    gainSlider.setValue (e.inputGainDb, quiet);
    gateSlider.setValue (e.gateDb, quiet);
    velocitySlider.setValue (e.fixedVelocity, quiet);
    tuningSlider.setValue (e.a4Hz, quiet);
    smoothingSlider.setValue (e.vibratoSmoothing, quiet);

    loading = false;
    pushSettings();
}

void MainComponent::pushSettings()
{
    if (loading)
        return;

    auto& e = settings.engine;
    e.enabled  = masterButton.getToggleState();
    e.bendMode = (vm::BendMode) selectedSegment (modeButtons, 3);
    e.response = (vm::Response) selectedSegment (responseButtons, 3);

    // "Aus" leaves the choice between scale and pentatonic as it was.
    const int filter = selectedSegment (filterButtons, 3);
    e.keyFilter = filter > 0;
    if (filter > 0)
        e.pentatonic = filter == 2;
    e.pentaSustain = selectedSegment (holdButtons, 2) == 1;

    const int triad = selectedSegment (triadButtons, 3);
    e.triad = triad > 0;
    if (triad > 0)
        e.triadWithRoot = triad == 1;

    e.bendRangeSemis = SettingsStore::kBendRanges[juce::jlimit (0, SettingsStore::kNumBendRanges - 1, bendRangeBox.getSelectedId() - 1)];
    e.dynamics = (vm::DynamicsMode) juce::jlimit (0, 2, dynamicsBox.getSelectedId() - 1);
    e.lowestNote = lowestBox.getSelectedId() > 0 ? lowestBox.getSelectedId() : 34;
    e.midiChannel = juce::jlimit (1, 16, channelBox.getSelectedId());
    e.triadChannel = juce::jlimit (1, 16, triadChannelBox.getSelectedId());
    e.keyRoot = juce::jlimit (0, 11, rootBox.getSelectedId() - 1);
    e.scaleMode = juce::jlimit (0, 6, scaleBox.getSelectedId() - 1);
    e.inputGainDb = (float) gainSlider.getValue();
    e.gateDb = (float) gateSlider.getValue();
    e.fixedVelocity = juce::roundToInt (velocitySlider.getValue());
    e.a4Hz = (float) tuningSlider.getValue();
    e.vibratoSmoothing = (float) smoothingSlider.getValue();
    host.setSettings (e);

    // controls that do nothing right now are dimmed, not hidden
    auto dim = [] (bool on, juce::Component& control, juce::Label& name, juce::Label* value = nullptr)
    {
        control.setEnabled (on);
        for (auto* child : control.getChildren())
            child->setEnabled (on);
        name.setAlpha (on ? 1.0f : 0.5f);
        if (value != nullptr)
            value->setAlpha (on ? 1.0f : 0.5f);
    };
    dim (e.dynamics == vm::DynamicsMode::Fixed, velocitySlider, velocityName, &velocityValue);
    dim (e.bendMode != vm::BendMode::Quantized, bendRangeBox, bendRangeName);
    dim (e.bendMode == vm::BendMode::Smoothed, smoothingSlider, smoothingName, &smoothingValue);
    dim (e.keyFilter && e.pentatonic, holdRow, holdName);
    dim (e.triad, triadChannelBox, triadChannelName);

    masterButton.setButtonText (e.enabled ? "Erkennung an" : "Erkennung aus");
    updateValueLabels();
    store->save (settings);
}

void MainComponent::updateValueLabels()
{
    const auto& e = settings.engine;
    const auto quiet = juce::dontSendNotification;
    gainValue.setText (signedDb (e.inputGainDb), quiet);
    gateValue.setText (signedDb (e.gateDb), quiet);
    velocityValue.setText (juce::String (e.fixedVelocity), quiet);
    tuningValue.setText (decimalComma (e.a4Hz, 1) + " Hz", quiet);
    smoothingValue.setText (juce::String (juce::roundToInt (e.vibratoSmoothing * 100.0f)) + " %", quiet);
}

void MainComponent::setMode (int index)
{
    modeButtons[juce::jlimit (0, 2, index)].setToggleState (true, juce::sendNotification);
    pushSettings();
}

void MainComponent::remoteControl (int paramIndex, bool isNote, int value)
{
    const auto& list = params::all();
    if (paramIndex < 0 || paramIndex >= (int) list.size())
        return;

    const auto& param = list[(size_t) paramIndex];
    if (! params::applyMidi (param, settings, isNote, value))
        return;

    if (param.kind == Param::Kind::Trigger)
        host.requestAllNotesOff();
    else
        applySettingsToControls();
}

bool MainComponent::keyPressed (const juce::KeyPress& key)
{
    if (key.isKeyCode (juce::KeyPress::spaceKey))
    {
        masterButton.setToggleState (! masterButton.getToggleState(), juce::dontSendNotification);
        pushSettings();
        return true;
    }

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
    const auto* screen = juce::Desktop::getInstance().getDisplays().getDisplayForRect (wanted);
    if (screen == nullptr || ! screen->userBounds.toNearestInt().contains (wanted.withSize (kWidth / 2, 60)))
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
    launch.content.setOwned (new SettingsPanel (deviceManager, updates, remote, options.settingsPage));
    options.settingsPage = 0;
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
        updateButton.setButtonText ("Update auf " + version);

    if (updateButton.isVisible() != show)
    {
        updateButton.setVisible (show);
        resized();
        repaint (0, getHeight() - kStatusHeight, getWidth(), kStatusHeight);
    }
}

void MainComponent::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    if (source == &deviceManager)
    {
        if (auto state = deviceManager.createStateXml())
            store->saveAudioState (*state);
    }
    else if (source == &remote)
    {
        settings.midiInputName = remote.inputName();
        settings.midiMapJson = remote.toJson();
        store->save (settings);
    }
    else
    {
        refreshUpdateButton();
    }
}

//==============================================================================
AudioHost::Meters MainComponent::demoMeters()
{
    // A short phrase with vibrato and one slide, to show the displays at work.
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
    const auto& e = settings.engine;
    if (step.note < 0 || ! e.enabled)
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

    if (e.triad)
    {
        const auto chord = vm::scale::triad (step.note, e.keyRoot, e.scaleMode, e.triadWithRoot && e.triadChannel != e.midiChannel);
        m.triadCount = chord.count;
        for (int i = 0; i < chord.count; ++i)
            m.triad[i] = chord.notes[i];
    }
    return m;
}

void MainComponent::timerCallback()
{
    const auto m = options.demo ? demoMeters() : host.takeMeters();
    const auto& e = settings.engine;

    // digital silence for a while usually means a missing permission or the wrong input
    silentTicks = (m.running && m.inputPeak <= 0.0f && ! options.demo) ? silentTicks + 1 : 0;
    clipTicks = m.inputPeak >= 0.98f ? kTimerHz : juce::jmax (0, clipTicks - 1);

    LiveFrame f;
    f.enabled = e.enabled;
    f.pitch = m.pitch;
    f.outputPitch = m.active ? m.outputPitch : m.pitch;
    f.levelDb = m.running ? m.levelDb : -120.0f;
    f.note = m.note;
    f.active = m.active && e.enabled;
    f.held = m.held;
    f.heard = e.enabled && m.voiced && m.levelDb >= e.gateDb - 6.0f;
    f.triadCount = f.active ? m.triadCount : 0;
    for (int i = 0; i < 3; ++i)
        f.triad[i] = m.triad[i];

    display.push (f, e.a4Hz);
    levelMeter.update (f.levelDb, e.gateDb, clipTicks > 0);

    // the level as a number; a change of a whole dB is enough to read it
    if (std::abs (f.levelDb - lastLevelDb) >= 1.0f)
    {
        lastLevelDb = f.levelDb;
        const int db = juce::roundToInt (f.levelDb);
        meterValue.setText (f.levelDb > -90.0f ? (db < 0 ? U ("\xe2\x88\x92") : juce::String()) + juce::String (std::abs (db)) + " dB"
                                               : U ("\xe2\x80\x93"),
                            juce::dontSendNotification);
    }

    updateStatus (m);
}

void MainComponent::updateStatus (const AudioHost::Meters& m)
{
    juce::String text;
    bool problem = true;

    const auto error = host.lastError();
    const auto update = updates.statusText();
    const auto& e = settings.engine;

    if (error.isNotEmpty())
        text = U ("Audio-Fehler: ") + error;
    else if (options.openAudio && ! options.demo && ! m.running)
        text = U ("Kein Audio-Eingang. Unter \xe2\x80\x9e" "Einstellungen\xe2\x80\x9c das Interface w\xc3\xa4hlen.");
    else if (m.running && ! m.hasInput)
        text = U ("Das Ger\xc3\xa4t liefert keinen Eingang. Unter \xe2\x80\x9e" "Einstellungen\xe2\x80\x9c einen Eingang w\xc3\xa4hlen.");
    else if (silentTicks > kTimerHz * 3)
        text = U ("Kein Signal. Stimmt der Eingang, und ist der Mikrofon-Zugriff in den Systemeinstellungen erlaubt?");
    else if (clipTicks > 0)
        text = U ("Eingang \xc3\xbc" "bersteuert. Pegel am Interface oder die Verst\xc3\xa4rkung senken.");
    else if (options.openAudio && ! midiPortOk)
        text = U ("Der MIDI-Ausgang lie\xc3\x9f sich nicht anlegen.");
    else
        problem = false;

    if (! problem)
    {
        if (update.isNotEmpty())
        {
            text = update;
        }
        else if (! e.enabled)
        {
            text = U ("Die Tonh\xc3\xb6henerkennung ist aus, es wird nichts gesendet. Die Leertaste schaltet sie wieder ein.");
        }
        else
        {
            juce::String input = options.demo ? juce::String ("Demo") : juce::String ("Ohne Audio");
            if (auto* device = deviceManager.getCurrentAudioDevice(); device != nullptr && m.running && ! options.demo)
            {
                const double ms = m.sampleRate > 0.0 ? 1000.0 * m.blockSize / m.sampleRate : 0.0;
                input = device->getName() + ", " + decimalComma (m.sampleRate / 1000.0, 1) + " kHz, "
                      + decimalComma (ms, 1) + " ms Puffer";
            }
            text = input + U ("   \xe2\x86\x92   MIDI-Ausgang \xe2\x80\x9e") + settings.midiPortName
                 + U ("\xe2\x80\x9c, Kanal ") + juce::String (e.midiChannel);
            if (e.triad)
                text << ", Dreiklang auf Kanal " << e.triadChannel;
        }
    }

    if (text != statusText || problem != statusIsProblem)
    {
        statusText = text;
        statusIsProblem = problem;
        repaint (0, getHeight() - kStatusHeight, getWidth(), kStatusHeight);
    }
}

//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (colour::window);

    // hairline under the display
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRect (0, kDisplayHeight, getWidth(), 1);

    // column titles
    g.setFont (font (13.0f, true));
    g.setColour (colour::text);
    for (const auto& column : columns)
        g.drawText (column.title, column.bounds.withHeight (kTitleHeight), juce::Justification::centredLeft);

    // status line
    const int sy = getHeight() - kStatusHeight;
    g.setColour (colour::separator);
    g.fillRect (0, sy, getWidth(), 1);
    g.setColour (statusIsProblem ? colour::problem : colour::secondary);
    g.setFont (font (12.5f));
    const int textRight = (updateButton.isVisible() ? updateButton.getX() : settingsButton.getX()) - 16;
    g.drawText (statusText, kMargin, sy, textRight - kMargin, kStatusHeight, juce::Justification::centredLeft, true);
}

void MainComponent::resized()
{
    auto area = getLocalBounds();
    display.setBounds (area.removeFromTop (kDisplayHeight));
    masterButton.setBounds (kMargin, kDisplayHeight - kMargin - 28, LiveDisplay::kNoteWidth - 2 * kMargin, 28);

    auto status = area.removeFromBottom (kStatusHeight).reduced (kMargin, 8);
    panicButton.setBounds (status.removeFromRight (116));
    status.removeFromRight (8);
    settingsButton.setBounds (status.removeFromRight (116));
    status.removeFromRight (8);
    if (updateButton.isVisible())
        updateButton.setBounds (status.removeFromRight (124));

    auto body = area.reduced (kMargin, 0).withTrimmedTop (20);
    const int n = (int) columns.size();
    const int width = (body.getWidth() - (n - 1) * kColumnGap) / juce::jmax (1, n);

    for (int c = 0; c < n; ++c)
    {
        auto& column = columns[(size_t) c];
        column.bounds = body.removeFromLeft (width);
        body.removeFromLeft (kColumnGap);

        auto inner = column.bounds.withTrimmedTop (kTitleHeight + kTitleGap);
        for (const auto& item : column.items)
        {
            auto nameRow = inner.removeFromTop (kNameHeight);
            if (item.value != nullptr)
                item.value->setBounds (nameRow.removeFromRight (70));
            item.name->setBounds (nameRow);
            inner.removeFromTop (kNameGap);
            item.control->setBounds (inner.removeFromTop (kControlHeight));
            inner.removeFromTop (kItemGap);
        }
    }
}
