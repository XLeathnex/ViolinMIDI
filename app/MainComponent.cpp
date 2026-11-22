#include "MainComponent.h"

#include "Brand.h"
#include "DescribedMenu.h"
#include "OctavePanel.h"
#include "Params.h"
#include "Scale.h"
#include "SettingsFields.h"
#include "SettingsPanel.h"
#include "VoicingPanel.h"

#include <cmath>

using namespace brand;

namespace
{
    constexpr int kWidth = 1260, kHeight = 724;
    constexpr int kHeaderHeight = 44;
    constexpr int kDisplayHeight = 300;
    constexpr int kStatusHeight = 40;
    constexpr int kMargin = 24, kColumnGap = 28;
    constexpr int kTitleHeight = 18, kTitleGap = 12, kNameHeight = 16, kNameGap = 6, kControlHeight = 24, kItemGap = 12;
    constexpr int kTimerHz = 30;
    constexpr int kMpeItem = 17;   // the entry after the sixteen channels

    juce::String decimalComma (double value, int places)
    {
        return juce::String (value, places).replaceCharacter ('.', ',');
    }

    juce::String signedDb (double value)
    {
        return (value < 0.0 ? U ("\xe2\x88\x92") : juce::String()) + decimalComma (std::abs (value), 1) + " dB";
    }

    juce::String milliseconds (double value)
    {
        return juce::String (juce::roundToInt (value)) + " ms";
    }

    /** The settings a preset holds, as text: two of these compare equal exactly
        when a preset would not change anything. */
    juce::String image (const vm::Settings& e)
    {
        return juce::JSON::toString (fields::toVar (e), true);
    }

    struct Described { int index; const char* name; const char* description; };

    // In the order of the menu; `index` is what the engine and the files use.
    const Described kScaleMenu[] = {
        { 0,  "Dur (Ionisch)",              "Klassischer Pop, Folk, Singer-Songwriter, fr\xc3\xb6hliche Hymnen." },
        { 5,  "Nat\xc3\xbcrliches Moll (\xc3\x84olisch)", "Melancholische Balladen, Rock, Pop, dramatische St\xc3\xbc" "cke." },
        { 1,  "Dorisch",                    "Irish Folk / Celtic (Jigs, Reels, traditionelle Tunes), Folk-Rock, Modal Jazz, mystische Passagen." },
        { 4,  "Mixolydisch",                "Classic Rock, Blues-Rock, Celtic-Rock, treibende Folk-Grooves (Dur-Charakter mit kleiner Septime)." },
        { 7,  "Harmonisch Moll",            "Dramatischer Prog-Rock, Neoklassik, osteurop\xc3\xa4ischer und Balkan-Einschlag, spannungsgeladene Soli." },
        { 8,  "Melodisch Moll",             "Jazz, Fusion, moderne progressive Linien." },
        { 2,  "Phrygisch",                  "D\xc3\xbcsterer Prog-Metal, spanische und mystische Klangfarben." },
        { 9,  "Phrygisch Dominant",         "Flamenco, orientalische und mediterrane Klangfarben." },
        { 10, "Dur-Pentatonik",             "Country, Folk, eing\xc3\xa4ngige Pop-Melodien (ohne Reibungst\xc3\xb6ne)." },
        { 11, "Moll-Pentatonik",            "Blues, Rock, Solospiel \xc3\xbc" "ber fast alle Akkordfolgen." },
        { 12, "Blues-Skala",                "Blues, Classic Rock, expressive Slides und Bendings." },
        { 3,  "Lydisch",                    "Hell und schwebend (Dur mit erh\xc3\xb6hter Quarte): Filmmusik, Fusion, tr\xc3\xa4umerische Passagen." },
        { 6,  "Lokrisch",                   "Instabil und d\xc3\xbcster (verminderte Quinte): selten, f\xc3\xbcr Spannung und Metal." }
    };

    // The closed box shows the name; the steps stand in front of the description.
    const Described kVoicingMenu[] = {
        { 0,  "Enger Dreiklang",        "1 \xe2\x80\x93 3 \xe2\x80\x93 5 \xc2\xb7 Kompakter, klassischer Chor- oder Streichersatz (diatonisch Dur/Moll je nach Tonstufe); funktioniert gut in mittleren bis hohen Lagen." },
        { 1,  "Vierklang / Septakkord", "1 \xe2\x80\x93 3 \xe2\x80\x93 5 \xe2\x80\x93 7 \xc2\xb7 Voller, jazzig-warmer 4-stimmiger Akkord (inkl. Septime); erzeugt automatisch Major-7-, Moll-7- und Dominant-7-Akkorde passend zur Tonart." },
        { 2,  "Open / Drop-2",          "1 \xe2\x80\x93 5 \xe2\x80\x93 10 \xc2\xb7 Sehr weiter, luftiger und monumentaler Klang; ideal f\xc3\xbcr schwebende Ambient-Pads und breite Synth-Teppiche ohne Frequenz-Mulm." },
        { 3,  "Power-Voicing",          "1 \xe2\x80\x93 5 \xe2\x80\x93 8 \xc2\xb7 Kraftvoll, neutral und durchsetzungsf\xc3\xa4hig ohne Dur/Moll-Festlegung; ideal f\xc3\xbcr verzerrte Sounds und Prog-Rock." },
        { 4,  "Quart-Voicing",          "1 \xe2\x80\x93 4 \xe2\x80\x93 7 \xc2\xb7 Schwebend, offen und modern (Modal Jazz / Filmscore); erzeugt geheimnisvolle, nicht-aufgel\xc3\xb6ste Spannungen." },
        { 5,  "Terz-Duett",             "1 \xe2\x80\x93 3 \xc2\xb7 Zweistimmige, warme und gesangliche Begleitung." },
        { 6,  "Sexten-Duett",           "1 \xe2\x80\x93 6 \xc2\xb7 Zweistimmige, offene, folkige und klassisch-melodische Begleitung." },
        { 7,  "Sub-Oktave",             "Oktave \xe2\x88\x92" "12 \xc2\xb7 Doppelt die Melodie eine Oktave tiefer f\xc3\xbcr ein warmes, fettes Bass-Fundament." },
        { 8,  "Doppel-Sub",             "Oktave \xe2\x88\x92" "12 / \xe2\x88\x92" "24 \xc2\xb7 Massiver, extrem tiefer Sub-Bass-Teppich \xc3\xbc" "ber zwei Oktaven nach unten." },
        { 9,  "Oktav-Klammer",          "Oktave \xe2\x88\x92" "12 / +12 \xc2\xb7 Rahmt den gespielten Ton gleichzeitig nach unten und oben ein \xe2\x80\x93 sehr breit und orchestraler Orgel-Charakter." },
        { 10, "Hohe Oktave",            "Oktave +12 \xc2\xb7 Doppelt die Melodie eine Oktave h\xc3\xb6her f\xc3\xbcr brillante Pr\xc3\xa4senz und Durchsetzungskraft." },
        { 11, "Shimmer-Oktaven",        "Oktave +12 / +24 \xc2\xb7 Glitzernde, strahlende H\xc3\xb6henstaffelung \xc3\xbc" "ber zwei Oktaven nach oben f\xc3\xbcr sph\xc3\xa4rische Leads und Pads." },
        { 12, "Free-Voicing \xe2\x80\xa6",         "\xc3\x96" "ffnet ein Auswahlfeld von \xe2\x88\x92" "24 bis +24 Halbt\xc3\xb6nen, in dem du bis zu 10 eigene Harmoniestimmen frei kombinieren und im Preset speichern kannst." }
    };
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

    // --- presets: readable files next to the settings file
    presets = std::make_unique<PresetStore> (appProperties.getUserSettings()->getFile().getParentDirectory().getChildFile ("Presets"));
    presetBar = std::make_unique<PresetBar> (*presets);
    presetBar->currentSettings = [this] { return settings.engine; };
    presetBar->onLoad = [this] (const vm::Settings& loaded)
    {
        settings.engine = loaded;
        presetImage = image (loaded);
        applySettingsToControls();
    };
    presetBar->onSlotChanged = [this] (int slot)
    {
        settings.presetSlot = slot;
        presetImage = image (settings.engine);
        store->save (settings);
    };
    addAndMakeVisible (*presetBar);

    addAndMakeVisible (display);

    setupLatch (masterButton, {}, U ("Schaltet die Tonh\xc3\xb6henerkennung ein und aus. Aus beendet sofort alle Noten. Leertaste."));
    const auto freezeTip = U ("Freeze (Halten): Friert den aktuell gespielten Ton bzw. Akkord als endlos stehenden Klangteppich (Drone) ein, "
                              "w\xc3\xa4hrend du auf der anderen Spur frei weiterspielen kannst. Klingt gerade nichts, wird der n\xc3\xa4" "chste Ton eingefroren.");
    setupLatch (freezeSoloButton, "Solo halten", freezeTip);
    setupLatch (freezeHarmonyButton, "Harmonie halten", freezeTip);

    // --- input
    addColumn ("Eingang");
    setupName (meterName, "Pegel");
    setupValue (meterValue);
    levelMeter.setTooltip (U ("Die hohe Marke ist die In-Schwelle: dar\xc3\xbc" "ber kann eine Note beginnen. Die kurze Marke ist die Out-Schwelle: darunter endet sie."));
    addAndMakeVisible (levelMeter);
    addItem (meterName, levelMeter, &meterValue);

    setupName (gainName, U ("Verst\xc3\xa4rkung"));
    setupValue (gainValue);
    setupSlider (gainSlider, -12.0, 36.0, 0.5);
    gainSlider.setDoubleClickReturnValue (true, 0.0);
    gainSlider.setTooltip (U ("Bei kr\xc3\xa4" "ftigem Spiel soll der Pegel weit nach rechts gehen, ohne rot zu werden. Doppelklick: 0 dB."));
    addItem (gainName, gainSlider, &gainValue);

    setupName (gateName, "In-Schwelle");
    setupValue (gateValue);
    setupSlider (gateSlider, -70.0, -20.0, 0.5);
    gateSlider.setDoubleClickReturnValue (true, -45.0);
    gateSlider.setTooltip (U ("Einschalt-Schwelle (In): Ab dieser Lautst\xc3\xa4rke wird ein neuer Ton ausgel\xc3\xb6st. H\xc3\xb6her stellen, um Nebenger\xc3\xa4usche und Saitenrascheln auszublenden."));
    addItem (gateName, gateSlider, &gateValue);

    setupName (gateOffName, "Out-Schwelle");
    setupValue (gateOffValue);
    setupSlider (gateOffSlider, -80.0, -20.0, 0.5);
    gateOffSlider.setDoubleClickReturnValue (true, -51.0);
    gateOffSlider.setTooltip (U ("Ausschalt-Schwelle (Out): Erst wenn der Ton unter diesen Wert abf\xc3\xa4llt, wird er beendet. "
                                 "Ein niedrigerer Wert als die In-Schwelle verhindert abgehackte T\xc3\xb6ne beim leisen Streichen. Sie kann nie \xc3\xbc" "ber der In-Schwelle liegen."));
    addItem (gateOffName, gateOffSlider, &gateOffValue);

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

    setupName (octaveName, "Oktavsprung-Schutz");
    octaveButton.setTooltip (U ("Zwei Schutzmechanismen gegen Oktavfehler auf den tiefen Saiten: der Anstreich-Schutz beim Tonbeginn und die "
                                "Oktavsprung-Sperre im gehaltenen Ton. Ein Klick \xc3\xb6" "ffnet die Einstellungen."));
    octaveButton.setWantsKeyboardFocus (false);
    octaveButton.onClick = [this] { showOctavePanel(); };
    addAndMakeVisible (octaveButton);
    addItem (octaveName, octaveButton);

    setupName (articulationName, "Artikulation (Pizzicato/Legato)");
    {
        const auto tip = U ("Erkennt anhand des Einschwingverhaltens, ob gezupft (Pizzicato/Spiccato) oder weich gestrichen (Legato) wird, "
                            "und passt die Anschlagst\xc3\xa4rke (Velocity) automatisch an.");
        setupSegments (articulationButtons, articulationRow, 7, { "Aus", "Sanft", "Stark" },
                       { tip + U (" Aus: Die Anschlagst\xc3\xa4rke folgt nur dem Pegel."),
                         tip + U (" Sanft: halbe Wirkung."),
                         tip + U (" Stark: Gezupftes klingt knackig (95\xe2\x80\x93" "127), Gestrichenes weich (45\xe2\x80\x93" "65).") });
    }
    addItem (articulationName, articulationRow);

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
                               "l\xc3\xa4ngere Glissandi ohne neuen Anschlag. Wird der Bereich \xc3\xbc" "berschritten, bleibt der Bend am Anschlag stehen. "
                               "Im MPE-Modus sind es immer \xc2\xb1" "48 Halbt\xc3\xb6ne."));
    addItem (bendRangeName, bendRangeBox);

    setupName (smoothingName, U ("Vibrato-Gl\xc3\xa4ttung"));
    setupValue (smoothingValue);
    setupSlider (smoothingSlider, 0.0, 1.0, 0.01);
    smoothingSlider.setDoubleClickReturnValue (true, 0.5);
    smoothingSlider.setTooltip (U ("Wie stark Vibrato in der Bewegung \xe2\x80\x9eGegl\xc3\xa4ttet\xe2\x80\x9c herausgefiltert wird. Doppelklick: 50 %."));
    addItem (smoothingName, smoothingSlider, &smoothingValue);

    // --- key
    addColumn ("Tonart");
    setupName (keyName, "Tonart / Skala");
    static const char* roots[] = { "C", "C\xe2\x99\xaf", "D", "D\xe2\x99\xaf", "E", "F", "F\xe2\x99\xaf", "G", "G\xe2\x99\xaf", "A", "A\xe2\x99\xaf", "B" };
    for (int i = 0; i < 12; ++i)
        rootBox.addItem (U (roots[i]), i + 1);
    for (const auto& entry : kScaleMenu)
        addDescribedItem (scaleBox, entry.index + 1, U (entry.name), U (entry.description));
    setupBox (rootBox, "Grundton der Tonart.");
    setupBox (scaleBox, U ("Die Skala. Sie bestimmt, welche T\xc3\xb6ne zur Tonart geh\xc3\xb6ren und welche Akkorde die Harmonie bildet. "
                           "In der Liste steht unter jedem Namen, wof\xc3\xbcr die Skala typisch ist."));
    keyRow.set (rootBox, scaleBox, 58);
    addAndMakeVisible (keyRow);
    addItem (keyName, keyRow);

    setupName (filterName, U ("Nur T\xc3\xb6ne der Tonart"));
    setupSegments (filterButtons, filterRow, 3, { "Aus", "An" },
                   { U ("Jeder gespielte Ton wird gesendet."),
                     U ("Nur die T\xc3\xb6ne der gew\xc3\xa4hlten Skala werden angeschlagen. Ein fremder Ton beendet die Note.") });
    addItem (filterName, filterRow);

    setupName (holdName, "Pentatonik-Ton halten");
    setupSegments (holdButtons, holdRow, 4, { "Aus", "An" },
                   { U ("Ein fremder Ton beendet die Note."),
                     U ("Bei Pentatonik- und Blues-Skala: Der letzte g\xc3\xbcltige Ton klingt weiter, solange der Pegel \xc3\xbc" "ber der Out-Schwelle liegt, bis der n\xc3\xa4" "chste g\xc3\xbcltige Ton kommt.") });
    addItem (holdName, holdRow);

    // --- harmony
    addColumn ("Harmonie");
    setupName (triadName, "Harmonie");
    setupSegments (triadButtons, triadRow, 5, { "Aus", "An" },
                   { U ("Keine Harmoniestimmen."),
                     U ("Zu jedem Ton erklingen die Stimmen des gew\xc3\xa4hlten Voicings auf dem Harmonie-Kanal, immer vollst\xc3\xa4ndig, also mit dem gespielten Ton.") });
    addItem (triadName, triadRow);

    setupName (voicingName, "Voicing");
    for (const auto& entry : kVoicingMenu)
        addDescribedItem (voicingBox, entry.index + 1, U (entry.name), U (entry.description));
    voicingBox.setTooltip (U ("Welche Stimmen die Harmonie bildet. In der Liste steht unter jedem Namen die Klangwirkung."));
    voicingBox.onChange = [this]
    {
        pushSettings();
        if (! loading && settings.engine.voicing == vm::scale::kFreeVoicing)
            showVoicingPanel();
    };
    voicingEditButton.setButtonText (U ("\xe2\x80\xa6"));
    voicingEditButton.setTooltip (U ("Free-Voicing: \xc3\x96" "ffnet ein Auswahlfeld von \xe2\x88\x92" "24 bis +24 Halbt\xc3\xb6nen, in dem du bis zu 10 eigene "
                                     "Harmoniestimmen frei kombinieren und im Preset speichern kannst."));
    voicingEditButton.setWantsKeyboardFocus (false);
    voicingEditButton.onClick = [this] { showVoicingPanel(); };
    voicingRow.set (voicingBox, voicingEditButton, 30, true);
    addAndMakeVisible (voicingRow);
    addItem (voicingName, voicingRow);

    setupName (harmonyModeName, "Harmonie-Modus");
    setupSegments (harmonyModeButtons, harmonyModeRow, 6, { "Diatonisch", "Chromatisch" },
                   { U ("Diatonisch (Empfohlen): Passt Akkorde automatisch an die gew\xc3\xa4hlte Tonart an \xe2\x80\x93 spielt je nach Tonstufe automatisch "
                        "Dur-, Moll- oder 7er-Akkorde, sodass kein Ton schief klingt."),
                     U ("Chromatisch (Starr): Verschiebt die Akkord-Intervalle immer um exakt dieselben Halbtonschritte "
                        "(z. B. f\xc3\xbcr parallele Powerchords oder experimentelle Effekte au\xc3\x9f" "erhalb der Tonart).") });
    addItem (harmonyModeName, harmonyModeRow);

    setupName (triadChannelName, "Harmonie-Kanal");
    for (int channel = 1; channel <= 16; ++channel)
        triadChannelBox.addItem ("Kanal " + juce::String (channel), channel);
    setupBox (triadChannelBox, U ("Auf diesem Kanal erklingt die Harmonie, zum Beispiel mit einem Pad oder Streichern."));
    addItem (triadChannelName, triadChannelBox);

    // --- MIDI
    addColumn ("MIDI");
    setupName (channelName, "MIDI-Kanal");
    for (int channel = 1; channel <= 16; ++channel)
        channelBox.addItem ("Kanal " + juce::String (channel), channel);
    channelBox.addSeparator();
    addDescribedItem (channelBox, kMpeItem, U ("MPE (Kan\xc3\xa4le 2\xe2\x80\x93" "16)"),
                      U ("Jede Note auf einem eigenen Kanal mit \xc2\xb1" "48 Halbt\xc3\xb6nen Pitch-Bend, f\xc3\xbcr MPE-f\xc3\xa4hige Synthesizer."));
    setupBox (channelBox, U ("Ein eigener Kanal h\xc3\xa4lt die Geige in MainStage von anderen MIDI-Ger\xc3\xa4ten getrennt. "
                             "MPE-Modus (MIDI Polyphonic Expression): Sendet jede Note auf einem eigenen MIDI-Kanal (2\xe2\x80\x93" "16) mit \xc2\xb1" "48 Halbt\xc3\xb6nen Pitch-Bend. "
                             "Verhindert Tonh\xc3\xb6henspr\xc3\xbcnge bei \xc3\xbc" "berlappenden Legato-B\xc3\xb6gen und erm\xc3\xb6glicht unabh\xc3\xa4ngiges Vibrato sowie Glissandi pro Stimme "
                             "in MPE-f\xc3\xa4higen Synthesizern."));
    addItem (channelName, channelBox);

    setupName (dynamicsName, "Dynamik");
    dynamicsBox.addItem ("Anschlag und Verlauf", 1);
    dynamicsBox.addItem ("Nur Anschlag", 2);
    dynamicsBox.addItem (U ("Feste St\xc3\xa4rke"), 3);
    setupBox (dynamicsBox, U ("Anschlag und Verlauf: An- und Abschwellen des Bogens steuert laufend den Controller, der unter \xe2\x80\x9e" "Dynamik-Ziel\xe2\x80\x9c gew\xc3\xa4hlt ist. "
                              "Bewegen sich dabei Regler anderer Ger\xc3\xa4te mit, \xe2\x80\x9eNur Anschlag\xe2\x80\x9c w\xc3\xa4hlen."));
    addItem (dynamicsName, dynamicsBox);

    const auto dynamicsTip = U ("Dynamik-Gl\xc3\xa4ttung & Ziel-CC: Bestimmt, auf welchen MIDI-Controller der Bogendruck gesendet wird "
                                "(CC11 Lautst\xc3\xa4rke, CC1 Filter/Modwheel, CC2 Breath) und gl\xc3\xa4ttet schnelle Pegelschwankungen f\xc3\xbcr weiche "
                                "Streicher-Crescendi ohne Zittern.");
    setupName (targetName, "Dynamik-Ziel");
    targetBox.addItem ("CC11 (Expression)", 1);
    targetBox.addItem ("CC1 (Modwheel)", 2);
    targetBox.addItem ("CC2 (Breath Controller)", 3);
    targetBox.addItem ("CC1 + CC11 (Kombi)", 4);
    setupBox (targetBox, dynamicsTip);
    addItem (targetName, targetBox);

    setupName (dynSmoothingName, U ("Dynamik-Gl\xc3\xa4ttung"));
    setupValue (dynSmoothingValue);
    setupSlider (dynSmoothingSlider, 0.0, 150.0, 1.0);
    dynSmoothingSlider.setDoubleClickReturnValue (true, 0.0);
    dynSmoothingSlider.setTooltip (dynamicsTip + U (" Doppelklick: 0 ms."));
    addItem (dynSmoothingName, dynSmoothingSlider, &dynSmoothingValue);

    setupName (velocityName, U ("Feste Anschlagst\xc3\xa4rke"));
    setupValue (velocityValue);
    setupSlider (velocitySlider, 1.0, 127.0, 1.0);
    addItem (velocityName, velocitySlider, &velocityValue);

    // --- status line
    settingsButton.setButtonText (U ("Einstellungen \xe2\x80\xa6"));
    settingsButton.setTooltip (U ("Audio-Eingang, MIDI-Steuerung, Updates und fr\xc3\xbchere Versionen."));
    settingsButton.setWantsKeyboardFocus (false);
    settingsButton.onClick = [this] { showSettings(); };
    addAndMakeVisible (settingsButton);

    // the one prominent button, and only while an update is waiting
    updateButton.setColour (juce::TextButton::buttonColourId, colour::accent);
    updateButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff1a1a1a));
    updateButton.setWantsKeyboardFocus (false);
    updateButton.onClick = [this]
    {
        const auto version = updates.availableUpdate();
        if (version.isNotEmpty())
            updates.switchTo (version);
    };
    addChildComponent (updateButton);

    panicButton.setButtonText ("Alle Noten aus");
    panicButton.setTooltip (U ("Beendet alle Noten, auch eingefrorene, und setzt Pitch Bend und Controller zur\xc3\xbc" "ck."));
    panicButton.setWantsKeyboardFocus (false);
    panicButton.onClick = [this] { allNotesOff(); };
    addAndMakeVisible (panicButton);

    applySettingsToControls();
    setSize (kWidth, kHeight);

    // The preset of the last session is shown again, marked if the settings have moved on since.
    {
        vm::Settings fromFile = settings.engine;
        if (settings.presetSlot > 0 && presets->load (settings.presetSlot, fromFile))
        {
            presetImage = image (fromFile);
            presetBar->setCurrentSlot (settings.presetSlot);
            presetBar->setModified (image (settings.engine) != presetImage);
        }
        else
        {
            settings.presetSlot = 0;
        }
    }

    // --- remote control by MIDI
    remote.setExcludedName (settings.midiPortName);
    remote.fromJson (settings.midiMapJson);
    remote.setProgramChannel (settings.midiProgramChannel);
    remote.onParam = [this] (int index, bool isNote, int value) { remoteControl (index, isNote, value); };
    remote.onProgramChange = [this] (int program) { presetBar->loadSlot (program + 1); };
    remote.addChangeListener (this);

    updates.onRestart = [] { juce::JUCEApplication::getInstance()->systemRequestedQuit(); };
    updates.addChangeListener (this);
    if (options.notice.isNotEmpty())
        updates.setNotice (options.notice);

    if (options.showSettingsAtStart || options.showVoicingAtStart || options.showOctaveAtStart || options.showMenuAtStart > 0)
    {
        juce::Component::SafePointer<MainComponent> safeThis (this);
        juce::Timer::callAfterDelay (600, [safeThis]
        {
            if (safeThis == nullptr)
                return;
            if (safeThis->options.showSettingsAtStart) safeThis->showSettings();
            if (safeThis->options.showVoicingAtStart)  safeThis->showVoicingPanel();
            if (safeThis->options.showOctaveAtStart)   safeThis->showOctavePanel();
            if (safeThis->options.showMenuAtStart == 1) safeThis->scaleBox.showPopup();
            if (safeThis->options.showMenuAtStart == 2) safeThis->voicingBox.showPopup();
        });
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
    remote.onProgramChange = nullptr;
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
    slider.setWantsKeyboardFocus (false);
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
        b.setWantsKeyboardFocus (false);   // the space bar belongs to the master switch
        b.onClick = [this, &b] { if (b.getToggleState()) pushSettings(); };
        row.add (b);
    }
    addAndMakeVisible (row);
}

void MainComponent::setupLatch (juce::TextButton& button, const juce::String& text, const juce::String& tooltip)
{
    button.setButtonText (text);
    button.setTooltip (tooltip);
    button.setClickingTogglesState (true);
    button.setWantsKeyboardFocus (false);
    button.onClick = [this] { pushSettings(); };
    addAndMakeVisible (button);
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
    freezeSoloButton.setToggleState (e.freezeSolo, quiet);
    freezeHarmonyButton.setToggleState (e.freezeHarmony, quiet);
    modeButtons[juce::jlimit (0, 2, (int) e.bendMode)].setToggleState (true, quiet);
    responseButtons[juce::jlimit (0, 2, (int) e.response)].setToggleState (true, quiet);
    articulationButtons[juce::jlimit (0, 2, (int) e.articulation)].setToggleState (true, quiet);
    filterButtons[e.keyFilter ? 1 : 0].setToggleState (true, quiet);
    holdButtons[e.pentaSustain ? 1 : 0].setToggleState (true, quiet);
    triadButtons[e.triad ? 1 : 0].setToggleState (true, quiet);
    harmonyModeButtons[e.harmonyMode == vm::HarmonyMode::Chromatic ? 1 : 0].setToggleState (true, quiet);

    bendRangeBox.setSelectedId (SettingsStore::bendRangeIndex (e.bendRangeSemis) + 1, quiet);
    dynamicsBox.setSelectedId ((int) e.dynamics + 1, quiet);
    targetBox.setSelectedId (juce::jlimit (0, 3, (int) e.dynamicsTarget) + 1, quiet);
    lowestBox.setSelectedId (e.lowestNote, quiet);
    channelBox.setSelectedId (e.mpe ? kMpeItem : e.midiChannel, quiet);
    triadChannelBox.setSelectedId (e.triadChannel, quiet);
    rootBox.setSelectedId (e.keyRoot + 1, quiet);
    scaleBox.setSelectedId (vm::scale::clampMode (e.scaleMode) + 1, quiet);
    voicingBox.setSelectedId (vm::scale::clampVoicing (e.voicing) + 1, quiet);

    gainSlider.setValue (e.inputGainDb, quiet);
    gateSlider.setValue (e.gateDb, quiet);
    gateOffSlider.setValue (e.gateOffDb, quiet);
    velocitySlider.setValue (e.fixedVelocity, quiet);
    tuningSlider.setValue (e.a4Hz, quiet);
    smoothingSlider.setValue (e.vibratoSmoothing, quiet);
    dynSmoothingSlider.setValue (e.dynamicsSmoothingMs, quiet);

    loading = false;
    pushSettings();
}

void MainComponent::pushSettings()
{
    if (loading)
        return;

    const auto quiet = juce::dontSendNotification;
    auto& e = settings.engine;
    e.enabled  = masterButton.getToggleState();

    // Switching the detection off ends everything, frozen notes included.
    if (! e.enabled)
    {
        freezeSoloButton.setToggleState (false, quiet);
        freezeHarmonyButton.setToggleState (false, quiet);
    }
    e.freezeSolo = freezeSoloButton.getToggleState();
    e.freezeHarmony = freezeHarmonyButton.getToggleState();

    e.bendMode = (vm::BendMode) selectedSegment (modeButtons, 3);
    e.response = (vm::Response) selectedSegment (responseButtons, 3);
    e.articulation = (vm::Articulation) selectedSegment (articulationButtons, 3);
    e.keyFilter = selectedSegment (filterButtons, 2) == 1;
    e.pentatonic = false;   // the pentatonic scales are in the list of scales
    e.pentaSustain = selectedSegment (holdButtons, 2) == 1;
    e.triad = selectedSegment (triadButtons, 2) == 1;
    e.harmonyMode = selectedSegment (harmonyModeButtons, 2) == 1 ? vm::HarmonyMode::Chromatic : vm::HarmonyMode::Diatonic;

    e.bendRangeSemis = SettingsStore::kBendRanges[juce::jlimit (0, SettingsStore::kNumBendRanges - 1, bendRangeBox.getSelectedId() - 1)];
    e.dynamics = (vm::DynamicsMode) juce::jlimit (0, 2, dynamicsBox.getSelectedId() - 1);
    e.dynamicsTarget = (vm::DynamicsTarget) juce::jlimit (0, 3, targetBox.getSelectedId() - 1);
    e.lowestNote = lowestBox.getSelectedId() > 0 ? lowestBox.getSelectedId() : 34;
    e.mpe = channelBox.getSelectedId() == kMpeItem;
    if (channelBox.getSelectedId() >= 1 && channelBox.getSelectedId() <= 16)
        e.midiChannel = channelBox.getSelectedId();
    e.triadChannel = juce::jlimit (1, 16, triadChannelBox.getSelectedId());
    e.keyRoot = juce::jlimit (0, 11, rootBox.getSelectedId() - 1);
    e.scaleMode = vm::scale::clampMode (scaleBox.getSelectedId() - 1);
    e.voicing = vm::scale::clampVoicing (voicingBox.getSelectedId() - 1);
    e.inputGainDb = (float) gainSlider.getValue();
    e.fixedVelocity = juce::roundToInt (velocitySlider.getValue());
    e.a4Hz = (float) tuningSlider.getValue();
    e.vibratoSmoothing = (float) smoothingSlider.getValue();
    e.dynamicsSmoothingMs = (float) dynSmoothingSlider.getValue();

    // Out never above In: whichever slider moves, Out gives way.
    e.gateDb = (float) gateSlider.getValue();
    if (gateOffSlider.getValue() > gateSlider.getValue())
        gateOffSlider.setValue (gateSlider.getValue(), quiet);
    e.gateOffDb = (float) gateOffSlider.getValue();

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
    const bool continuous = e.dynamics == vm::DynamicsMode::Continuous;
    dim (e.dynamics == vm::DynamicsMode::Fixed, velocitySlider, velocityName, &velocityValue);
    dim (e.dynamics != vm::DynamicsMode::Fixed, articulationRow, articulationName);
    dim (continuous, targetBox, targetName);
    dim (continuous, dynSmoothingSlider, dynSmoothingName, &dynSmoothingValue);
    dim (e.bendMode != vm::BendMode::Quantized && ! e.mpe, bendRangeBox, bendRangeName);
    dim (e.bendMode == vm::BendMode::Smoothed, smoothingSlider, smoothingName, &smoothingValue);
    dim (e.keyFilter && vm::scale::noteCount (e.scaleMode) < 7, holdRow, holdName);
    dim (e.triad, voicingRow, voicingName);
    dim (e.triad, harmonyModeRow, harmonyModeName);
    dim (e.triad, triadChannelBox, triadChannelName);
    voicingEditButton.setEnabled (e.triad && e.voicing == vm::scale::kFreeVoicing);
    freezeSoloButton.setEnabled (e.enabled);
    freezeHarmonyButton.setEnabled (e.enabled && e.triad);

    masterButton.setButtonText (e.enabled ? "Erkennung an" : "Erkennung aus");
    octaveButton.setButtonText ((e.octaveAttackGuard ? milliseconds (e.octaveAttackMs) : juce::String ("Aus")) + "  /  "
                                + (e.octaveSustainGuard ? milliseconds (e.octaveSustainMs) : juce::String ("Aus")) + U (" \xe2\x80\xa6"));
    updateValueLabels();

    if (presetBar->currentSlot() > 0)
        presetBar->setModified (image (e) != presetImage);

    store->save (settings);
}

void MainComponent::updateValueLabels()
{
    const auto& e = settings.engine;
    const auto quiet = juce::dontSendNotification;
    gainValue.setText (signedDb (e.inputGainDb), quiet);
    gateValue.setText (signedDb (e.gateDb), quiet);
    gateOffValue.setText (signedDb (e.gateOffDb), quiet);
    velocityValue.setText (juce::String (e.fixedVelocity), quiet);
    tuningValue.setText (decimalComma (e.a4Hz, 1) + " Hz", quiet);
    smoothingValue.setText (juce::String (juce::roundToInt (e.vibratoSmoothing * 100.0f)) + " %", quiet);
    dynSmoothingValue.setText (milliseconds (e.dynamicsSmoothingMs), quiet);
}

void MainComponent::setMode (int index)
{
    modeButtons[juce::jlimit (0, 2, index)].setToggleState (true, juce::sendNotification);
    pushSettings();
}

void MainComponent::allNotesOff()
{
    freezeSoloButton.setToggleState (false, juce::dontSendNotification);
    freezeHarmonyButton.setToggleState (false, juce::dontSendNotification);
    pushSettings();
    host.requestAllNotesOff();
}

void MainComponent::remoteControl (int paramIndex, bool isNote, int value)
{
    const auto& list = params::all();
    if (paramIndex < 0 || paramIndex >= (int) list.size())
        return;

    const auto& param = list[(size_t) paramIndex];
    if (! params::applyMidi (param, settings, isNote, value))
        return;

    if (param.kind != Param::Kind::Trigger)
        applySettingsToControls();
    else if (param.id == "presetNext")
        presetBar->step (1);
    else if (param.id == "presetPrevious")
        presetBar->step (-1);
    else
        allNotesOff();
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

void MainComponent::showVoicingPanel()
{
    auto panel = std::make_unique<VoicingPanel> (settings.engine.freeVoicing);
    juce::Component::SafePointer<MainComponent> safeThis (this);
    panel->onChange = [safeThis] (uint64_t mask)
    {
        if (safeThis == nullptr)
            return;
        safeThis->settings.engine.freeVoicing = mask;
        safeThis->pushSettings();
    };
    juce::CallOutBox::launchAsynchronously (std::move (panel), voicingRow.getScreenBounds(), nullptr);
}

void MainComponent::showOctavePanel()
{
    const auto& e = settings.engine;
    OctavePanel::Values values;
    values.attackOn = e.octaveAttackGuard;
    values.attackMs = e.octaveAttackMs;
    values.sustainOn = e.octaveSustainGuard;
    values.sustainMs = e.octaveSustainMs;

    auto panel = std::make_unique<OctavePanel> (values);
    juce::Component::SafePointer<MainComponent> safeThis (this);
    panel->onChange = [safeThis] (const OctavePanel::Values& v)
    {
        if (safeThis == nullptr)
            return;
        auto& engine = safeThis->settings.engine;
        engine.octaveAttackGuard = v.attackOn;
        engine.octaveAttackMs = v.attackMs;
        engine.octaveSustainGuard = v.sustainOn;
        engine.octaveSustainMs = v.sustainMs;
        safeThis->pushSettings();
    };
    juce::CallOutBox::launchAsynchronously (std::move (panel), octaveButton.getScreenBounds(), nullptr);
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
        settings.midiProgramChannel = remote.programChannel();
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
        const auto chord = vm::scale::voices (step.note, e.keyRoot, e.scaleMode, e.voicing, e.harmonyMode == vm::HarmonyMode::Diatonic,
                                              e.freeVoicing, ! e.mpe && e.triadChannel == e.midiChannel);
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
    f.heard = e.enabled && m.voiced && m.levelDb >= e.gateOffDb;
    f.frozenNote = e.enabled && e.freezeSolo ? m.frozenNote : -1;
    f.harmonyFrozen = e.enabled && e.freezeHarmony && m.triadCount > 0;
    f.triadCount = f.active || f.harmonyFrozen ? m.triadCount : 0;
    for (int i = 0; i < vm::scale::kMaxVoices; ++i)
        f.triad[i] = m.triad[i];

    display.push (f, e.a4Hz);
    levelMeter.update (f.levelDb, e.gateDb, e.gateOffDb, clipTicks > 0);

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
            // the audio configuration as it is right now: rate | buffer (its length)
            juce::String audio;
            if (m.running && m.sampleRate > 0.0)
            {
                const double kHz = m.sampleRate / 1000.0;
                const bool whole = std::abs (kHz - std::round (kHz)) < 0.005;
                audio = (whole ? juce::String (juce::roundToInt (kHz)) : decimalComma (kHz, 1)) + " kHz | "
                      + juce::String (m.blockSize) + " Samples (~" + decimalComma (1000.0 * m.blockSize / m.sampleRate, 1) + " ms)";
            }

            juce::String input = options.demo ? juce::String ("Demo") : juce::String ("Ohne Audio");
            if (auto* device = deviceManager.getCurrentAudioDevice(); device != nullptr && m.running && ! options.demo)
                input = device->getName();
            if (audio.isNotEmpty())
                input << U ("  \xc2\xb7  ") << audio;

            text = input + U ("   \xe2\x86\x92   MIDI-Ausgang \xe2\x80\x9e") + settings.midiPortName + U ("\xe2\x80\x9c, ")
                 + (e.mpe ? U ("MPE auf Kanal 2\xe2\x80\x93" "16") : "Kanal " + juce::String (e.midiChannel));
            if (e.triad)
                text << ", Harmonie auf Kanal " << e.triadChannel;
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

    // hairlines under the presets and under the display
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRect (0, kHeaderHeight - 1, getWidth(), 1);
    g.fillRect (0, kHeaderHeight + kDisplayHeight, getWidth(), 1);

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

    auto header = area.removeFromTop (kHeaderHeight);
    presetBar->setBounds (header.withSizeKeepingCentre (header.getWidth() - 2 * kMargin, PresetBar::kHeight)
                                .withWidth (juce::jmin (640, header.getWidth() - 2 * kMargin)));

    const auto displayArea = area.removeFromTop (kDisplayHeight);
    display.setBounds (displayArea);

    // under the note: the master switch, below it the two freeze buttons side by side
    const int buttonsWidth = LiveDisplay::kNoteWidth - 2 * kMargin;
    auto freezeRow = juce::Rectangle<int> (kMargin, displayArea.getBottom() - kMargin - 26, buttonsWidth, 26);
    masterButton.setBounds (freezeRow.translated (0, -32).withHeight (26));
    freezeSoloButton.setBounds (freezeRow.removeFromLeft ((buttonsWidth - 6) / 2));
    freezeRow.removeFromLeft (6);
    freezeHarmonyButton.setBounds (freezeRow);

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
