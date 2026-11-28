#include "MainComponent.h"

#include "AudioPanel.h"
#include "Brand.h"
#include "CameraPanel.h"
#include "DescribedMenu.h"
#include "Params.h"
#include "Scale.h"
#include "SettingsFields.h"
#include "SettingsPanel.h"
#include "Theme.h"
#include "TrackingPanel.h"
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
    constexpr int kEmblemSize = 30, kBrandGap = 8, kBrandWidth = 132;   // the emblem and word mark at the left of the header
    constexpr float kWordMarkHeight = 17.0f;

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
    // "Total Free Voicing" comes first (spec 2.0); `index` keeps the files' numbering.
    const Described kVoicingMenu[] = {
        { 12, "Total Free Voicing",     "Frei von \xe2\x88\x92" "24 bis +24 Halbt\xc3\xb6nen: bis zu 10 eigene Harmoniestimmen in der Matrix kombinieren und im Preset speichern." },
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
        { 11, "Shimmer-Oktaven",        "Oktave +12 / +24 \xc2\xb7 Glitzernde, strahlende H\xc3\xb6henstaffelung \xc3\xbc" "ber zwei Oktaven nach oben f\xc3\xbcr sph\xc3\xa4rische Leads und Pads." }
    };

    /** The voicing's name as the menu shows it. */
    juce::String voicingTitle (int index)
    {
        for (const auto& entry : kVoicingMenu)
            if (entry.index == index)
                return U (entry.name);
        return {};
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
    presetBar->onFactory = [this] { applyLowestLatency(); };
    addAndMakeVisible (*presetBar);

    audioButton.setButtonText (U ("Audio-Harmonizer \xe2\x80\xa6"));
    audioButton.setTooltip (U ("Spielt die Harmoniestimmen mit dem eigenen Klang der Geige \xc3\xbc" "ber den Audio-Ausgang. "
                               "Der Punkt zeigt, ob er an (gr\xc3\xbcn) oder aus (rot) ist. Ein Klick \xc3\xb6" "ffnet sein Fenster mit Kurven und Reglern."));
    audioButton.setWantsKeyboardFocus (false);
    audioButton.onClick = [this] { showAudioPanel(); };
    addAndMakeVisible (audioButton);

    cameraButton.setButtonText (U ("Kamera \xe2\x80\xa6"));
    cameraButton.setTooltip (U ("Kamera-Steuerung: Die Geigenschnecke steuert zwei Controller wie ein XY-Pad, Nicken und Kopfsch\xc3\xbctteln schalten Funktionen. "
                                "Der Punkt zeigt, ob sie an (gr\xc3\xbcn) oder aus (rot) ist. Ein Klick \xc3\xb6" "ffnet das Kamera-Fenster."));
    cameraButton.setWantsKeyboardFocus (false);
    cameraButton.onClick = [this] { showCamera(); };
    addAndMakeVisible (cameraButton);

    cameraSettings = CameraSettings::fromJson (settings.cameraJson);
    camera.onFrame = [this] (const juce::Image& preview, const cam::Pose& pose) { cameraFrame (preview, pose); };
    padHud.setAxisNames (padTargetName (cameraSettings.xTarget), padTargetName (cameraSettings.yTarget));

    addAndMakeVisible (display);
    padHud.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (padHud);

    setupLatch (masterButton, {}, U ("Hauptschalter der Tonh\xc3\xb6henerkennung: gr\xc3\xbcn an, rot aus. Aus beendet sofort alle Noten "
                                     "und sendet nichts mehr. Leertaste."));
    setSwitchStyle (masterButton, SwitchStyle::status);
    const auto freezeTip = U ("Freeze (Halten): Friert den aktuell gespielten Ton bzw. Akkord als endlos stehenden Klangteppich (Drone) ein, "
                              "w\xc3\xa4hrend du auf der anderen Spur frei weiterspielen kannst. Klingt gerade nichts, wird der n\xc3\xa4" "chste Ton eingefroren.");
    setupLatch (freezeSoloButton, "Solo halten", freezeTip);
    setupLatch (freezeHarmonyButton, "Harmonie halten", freezeTip);
    setSwitchStyle (freezeSoloButton, SwitchStyle::latch);
    setSwitchStyle (freezeHarmonyButton, SwitchStyle::latch);

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
    tuningSlider.setTooltip (U ("Kammerton A: der Bezugston, nach dem die T\xc3\xb6ne den MIDI-Noten zugeordnet werden. "
                                "Auf die Stimmung der Band oder des Orchesters stellen. Doppelklick: 440 Hz."));
    addItem (tuningName, tuningSlider, &tuningValue);

    setupName (octaveName, "Tracking-Filter");
    octaveButton.setTooltip (U ("Schutz gegen falsche T\xc3\xb6ne: Anstreich-Schutz und Oktavsprung-Sperre gegen Oktavfehler, der Sprungschutz gegen "
                                "versehentlich angerissene Nachbarsaiten und die \xc3\x9c" "berbr\xc3\xbc" "ckung f\xc3\xbcr Bogenwechsel. Ein Klick \xc3\xb6" "ffnet die Einstellungen."));
    octaveButton.setWantsKeyboardFocus (false);
    octaveButton.onClick = [this] { showTrackingPanel(); };
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
                                  + (std::abs (range - 1.0f) < 1.0e-6f ? U (" Halbton") : U (" Halbt\xc3\xb6ne")),
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

    setupName (overlapName, U ("Spiel-\xc3\x9c" "berlappung"));
    setupValue (overlapValue);
    setupSlider (overlapSlider, 0.0, 1500.0, 10.0);
    overlapSlider.setDoubleClickReturnValue (true, 0.0);
    overlapSlider.setTooltip (U ("L\xc3\xa4sst den vorherigen Ton bei einem Tonwechsel noch f\xc3\xbcr diese Zeit weich ausklingen, w\xc3\xa4hrend der neue Ton bereits "
                                 "startet (f\xc3\xbcr polyphone Kl\xc3\xa4nge). Ausgeblendet wird im MPE-Modus und im Audio-Harmonizer; auf einem gemeinsamen "
                                 "MIDI-Kanal klingt der alte Ton in voller St\xc3\xa4rke nach. Doppelklick: 0 ms."));
    addItem (overlapName, overlapSlider, &overlapValue);

    setupName (releaseName, "Ausklingen (Spielende)");
    setupValue (releaseValue);
    setupSlider (releaseSlider, 0.0, 3000.0, 10.0);
    releaseSlider.setDoubleClickReturnValue (true, 0.0);
    releaseSlider.setTooltip (U ("H\xc3\xa4lt den Ton nach Unterschreiten der Out-Schwelle fest und blendet ihn \xc3\xbc" "ber diese Zeit sanft aus, bevor Note-Off "
                                 "gesendet wird. Doppelklick: 0 ms."));
    addItem (releaseName, releaseSlider, &releaseValue);

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
    setSwitchStyle (filterRow, SwitchStyle::status);
    addItem (filterName, filterRow);

    setupName (holdName, "Pentatonik-Ton halten");
    setupSegments (holdButtons, holdRow, 4, { "Aus", "An" },
                   { U ("Ein fremder Ton beendet die Note."),
                     U ("Bei Pentatonik- und Blues-Skala: Der letzte g\xc3\xbcltige Ton klingt weiter, solange der Pegel \xc3\xbc" "ber der Out-Schwelle liegt, bis der n\xc3\xa4" "chste g\xc3\xbcltige Ton kommt.") });
    setSwitchStyle (holdRow, SwitchStyle::status);
    addItem (holdName, holdRow);

    // --- harmony
    addColumn ("Harmonie");
    setupName (triadName, "Harmonie");
    setupSegments (triadButtons, triadRow, 5, { "Aus", "An" },
                   { U ("Keine Harmoniestimmen."),
                     U ("Zu jedem Ton erklingen die Stimmen des gew\xc3\xa4hlten Voicings auf dem Harmonie-Kanal, immer vollst\xc3\xa4ndig, also mit dem gespielten Ton.") });
    setSwitchStyle (triadRow, SwitchStyle::status);
    addItem (triadName, triadRow);

    setupName (voicingName, "Voicing");
    for (const auto& entry : kVoicingMenu)
        addDescribedItem (voicingBox, entry.index + 1, U (entry.name), U (entry.description));
    voicingBox.setTooltip (U ("Welche Stimmen die Harmonie bildet. In der Liste steht unter jedem Namen die Klangwirkung. "
                              "Nach der Wahl \xc3\xb6" "ffnet sich die Intervall-Matrix: Dort lassen sich T\xc3\xb6ne hinzuf\xc3\xbcgen oder abw\xc3\xa4hlen. "
                              "\xe2\x80\x9e" "FV\xe2\x80\x9c hinter dem Namen zeigt, dass die Harmonieart so ver\xc3\xa4ndert wurde."));
    voicingBox.onChange = [this]
    {
        pushSettings();
        if (! loading)
            showVoicingPanel();   // every type opens its matrix (2.0)
    };
    voicingEditButton.setButtonText (U ("\xe2\x80\xa6"));
    voicingEditButton.setTooltip (U ("\xc3\x96" "ffnet die Intervall-Matrix der gew\xc3\xa4hlten Harmonieart von \xe2\x88\x92" "24 bis +24 Halbt\xc3\xb6nen: "
                                     "bis zu 10 Stimmen hinzuf\xc3\xbcgen oder abw\xc3\xa4hlen, im Preset gespeichert."));
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

    setupName (offsetName, "Harmonie-Offset");
    setupValue (offsetValue);
    setupSlider (offsetSlider, -50.0, 0.0, 1.0);
    offsetSlider.setDoubleClickReturnValue (true, 0.0);
    offsetSlider.setTooltip (U ("Senkt die Lautst\xc3\xa4rke und Ausdrucksst\xc3\xa4rke (Velocity & CC11) der Harmoniestimmen relativ zum gespielten Solo-Ton ab. "
                                "Doppelklick: 0 %."));
    addItem (offsetName, offsetSlider, &offsetValue);

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
    velocitySlider.setTooltip (U ("Die Anschlagst\xc3\xa4rke (Velocity) jeder Note bei \xe2\x80\x9e" "Dynamik: Feste St\xc3\xa4rke\xe2\x80\x9c, 1 bis 127."));
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
    updateButton.setTooltip (U ("L\xc3\xa4" "dt die neue Version und startet die App damit neu. Die bisherige Version bleibt unter "
                                "\xe2\x80\x9e" "Einstellungen\xe2\x80\x9c w\xc3\xa4hlbar."));
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

    if (options.showSettingsAtStart || options.showVoicingAtStart || options.showPanelAtStart > 0 || options.showMenuAtStart > 0)
    {
        juce::Component::SafePointer<MainComponent> safeThis (this);
        juce::Timer::callAfterDelay (600, [safeThis]
        {
            if (safeThis == nullptr)
                return;
            if (safeThis->options.showSettingsAtStart) safeThis->showSettings();
            if (safeThis->options.showVoicingAtStart)  safeThis->showVoicingPanel();
            if (safeThis->options.showPanelAtStart == 1) safeThis->showTrackingPanel();
            if (safeThis->options.showPanelAtStart == 2) safeThis->showAudioPanel();
            if (safeThis->options.showPanelAtStart == 3) safeThis->showCamera();
            if (safeThis->options.showMenuAtStart == 1) safeThis->scaleBox.showPopup();
            if (safeThis->options.showMenuAtStart == 2) safeThis->voicingBox.showPopup();
        });
    }

    startTimerHz (kTimerHz);

    // The start animation covers the window while audio and MIDI come up. They start a moment
    // after the window has been drawn, so the animation is on screen while that takes its time.
    // Opening the device holds up the message thread, so it waits until the logo stands
    // complete (onShown): the picture that stays still meanwhile is the finished one.
    splash = std::make_unique<SplashOverlay>();
    splash->setFrames (brand::splashFrames(), brand::kSplashFramesPerSecond);
    splash->setStatusText (options.openAudio ? U ("Audio und MIDI werden gestartet \xe2\x80\xa6") : U ("Vorschau ohne Audio"));
    splash->onFinished = [this] { splash = nullptr; };
    if (options.openAudio)
        splash->onShown = [this] { initialiseAudio(); };
    addAndMakeVisible (*splash);
    splash->setBounds (getLocalBounds());

    if (! options.openAudio)
        splash->finish();
}

void MainComponent::initialiseAudio()
{
    // --- MIDI out, MIDI in and audio in
    midiPortOk = host.openMidiPort (settings.midiPortName);
    remote.setInput (settings.midiInputName);

    auto savedState = store->audioState();
    audioInitError = deviceManager.initialise (1, 0, savedState.get(), true);

    // A small buffer keeps the delay short. Not only at the very first start:
    // if the saved device could not be opened, the system's default device comes
    // up with its default buffer (512 samples, 10.7 ms at 48 kHz), and that used
    // to stay. Only a size the user chose in the settings is left alone.
    if (! settings.bufferChosen)
        useSmallBuffer();

    if (settings.engine.audioHarmony)
        ensureAudioOutput();

    lastBufferSize = deviceManager.getAudioDeviceSetup().bufferSize;
    deviceManager.addAudioCallback (&host);
    deviceManager.addChangeListener (this);

    // A quiet look for a newer version. Nothing is installed without a click.
    updates.check();

    audioReady = true;
    if (splash != nullptr)
        splash->finish();
}

MainComponent::~MainComponent()
{
    stopTimer();
    camera.onFrame = nullptr;
    camera.stop();
    splash = nullptr;
    if (cameraDialog != nullptr)
        delete cameraDialog.getComponent();
    if (audioDialog != nullptr)
        delete audioDialog.getComponent();
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
    overlapSlider.setValue (e.overlapMs, quiet);
    releaseSlider.setValue (e.releaseMs, quiet);
    offsetSlider.setValue (e.harmonyOffset * 100.0f, quiet);

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

    // A newly chosen harmony type starts with its own voices; what was added to the previous one is not carried over.
    const int voicing = vm::scale::clampVoicing (voicingBox.getSelectedId() - 1);
    if (voicing != e.voicing)
    {
        e.voicingEdited = false;
        e.voicingVoices = vm::scale::presetMask (voicing);
    }
    e.voicing = voicing;

    // An open voicing matrix belongs to one type: it closes when the type changes from
    // elsewhere (foot switch, preset) and follows a change of the harmony mode.
    if (auto* openPanel = voicingPanel.getComponent())
    {
        if (e.voicing != voicingPanelType || ! e.triad)
        {
            if (auto* box = openPanel->findParentComponentOfClass<juce::CallOutBox>())
                box->dismiss();
        }
        else
        {
            openPanel->setDiatonic (e.harmonyMode == vm::HarmonyMode::Diatonic);
        }
    }
    e.inputGainDb = (float) gainSlider.getValue();
    e.fixedVelocity = juce::roundToInt (velocitySlider.getValue());
    e.a4Hz = (float) tuningSlider.getValue();
    e.vibratoSmoothing = (float) smoothingSlider.getValue();
    e.dynamicsSmoothingMs = (float) dynSmoothingSlider.getValue();
    e.overlapMs = (float) overlapSlider.getValue();
    e.releaseMs = (float) releaseSlider.getValue();
    e.harmonyOffset = (float) (offsetSlider.getValue() / 100.0);

    // Out never above In: whichever slider moves, Out gives way.
    e.gateDb = (float) gateSlider.getValue();
    if (gateOffSlider.getValue() > gateSlider.getValue())
        gateOffSlider.setValue (gateSlider.getValue(), quiet);
    e.gateOffDb = (float) gateOffSlider.getValue();

    if (e.audioHarmony)
        ensureAudioOutput();
    host.setSettings (hostSettings());

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
    dim (e.triad, offsetSlider, offsetName, &offsetValue);
    voicingEditButton.setEnabled (e.triad);
    freezeSoloButton.setEnabled (e.enabled);
    freezeHarmonyButton.setEnabled (e.enabled && e.triad);

    masterButton.setButtonText (e.enabled ? "Erkennung an" : "Erkennung aus");
    octaveButton.setButtonText (juce::String (e.jumpGuard ? "Sprungschutz an" : "Sprungschutz aus") + U ("  \xc2\xb7  ")
                                + milliseconds (e.bridgeMs) + U (" \xe2\x80\xa6"));
    setStatusDot (audioButton, e.audioHarmony);
    setStatusDot (cameraButton, cameraSettings.enabled);
    updateValueLabels();
    refreshVoicingLabel();

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
    const auto duration = [] (float ms) { return ms >= 1000.0f ? decimalComma (ms / 1000.0, 2) + " s" : milliseconds (ms); };
    overlapValue.setText (duration (e.overlapMs), quiet);
    releaseValue.setText (duration (e.releaseMs), quiet);
    const int percent = juce::roundToInt (e.harmonyOffset * 100.0f);
    offsetValue.setText ((percent < 0 ? U ("\xe2\x88\x92") : juce::String()) + juce::String (std::abs (percent)) + " %", quiet);
}

void MainComponent::refreshVoicingLabel()
{
    // The closed box names the type and marks one whose voices were changed ("Enger Dreiklang FV").
    const auto& e = settings.engine;
    const int current = vm::scale::clampVoicing (e.voicing);
    for (const auto& entry : kVoicingMenu)
    {
        const auto wanted = U (entry.name) + (entry.index == current && vm::voicingModified (e) ? " FV" : "");
        if (voicingBox.getItemText (voicingBox.indexOfItemId (entry.index + 1)) != wanted)
            voicingBox.changeItemText (entry.index + 1, wanted);
    }
    voicingBox.setSelectedId (current + 1, juce::dontSendNotification);   // shows a changed text of the same entry
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
    const auto& e = settings.engine;
    const int voicing = vm::scale::clampVoicing (e.voicing);
    const bool isFree = voicing == vm::scale::kFreeVoicing;

    auto panel = std::make_unique<VoicingPanel> (voicingTitle (voicing), isFree, vm::voicingMask (e), vm::scale::presetMask (voicing),
                                                 e.harmonyMode == vm::HarmonyMode::Diatonic);
    juce::Component::SafePointer<MainComponent> safeThis (this);

    // The panel belongs to the type it was opened for; if the type changes meanwhile
    // (a foot switch, a preset), its clicks no longer apply.
    panel->onChange = [safeThis, voicing, isFree] (uint64_t mask)
    {
        if (safeThis == nullptr || safeThis->settings.engine.voicing != voicing)
            return;
        auto& engine = safeThis->settings.engine;
        if (isFree)
        {
            engine.freeVoicing = mask;
        }
        else
        {
            engine.voicingVoices = mask;
            engine.voicingEdited = true;
        }
        safeThis->pushSettings();
    };
    panel->onReset = [safeThis, voicing, isFree]
    {
        if (safeThis == nullptr || safeThis->settings.engine.voicing != voicing || isFree)
            return;
        auto& engine = safeThis->settings.engine;
        engine.voicingEdited = false;
        engine.voicingVoices = vm::scale::presetMask (voicing);
        safeThis->pushSettings();
    };
    voicingPanel = panel.get();
    voicingPanelType = voicing;
    juce::CallOutBox::launchAsynchronously (std::move (panel), voicingRow.getScreenBounds(), nullptr);
}

void MainComponent::showTrackingPanel()
{
    const auto& e = settings.engine;
    TrackingPanel::Values values;
    values.attackOn = e.octaveAttackGuard;
    values.attackMs = e.octaveAttackMs;
    values.sustainOn = e.octaveSustainGuard;
    values.sustainMs = e.octaveSustainMs;
    values.jumpOn = e.jumpGuard;
    values.jumpInterval = e.jumpIntervalSemis;
    values.jumpWindowMs = e.jumpWindowMs;
    values.jumpLockMs = e.jumpLockMs;
    values.bridgeMs = e.bridgeMs;

    auto panel = std::make_unique<TrackingPanel> (values);
    juce::Component::SafePointer<MainComponent> safeThis (this);
    panel->onChange = [safeThis] (const TrackingPanel::Values& v)
    {
        if (safeThis == nullptr)
            return;
        auto& engine = safeThis->settings.engine;
        engine.octaveAttackGuard = v.attackOn;
        engine.octaveAttackMs = v.attackMs;
        engine.octaveSustainGuard = v.sustainOn;
        engine.octaveSustainMs = v.sustainMs;
        engine.jumpGuard = v.jumpOn;
        engine.jumpIntervalSemis = v.jumpInterval;
        engine.jumpWindowMs = v.jumpWindowMs;
        engine.jumpLockMs = v.jumpLockMs;
        engine.bridgeMs = v.bridgeMs;
        safeThis->pushSettings();
    };
    juce::CallOutBox::launchAsynchronously (std::move (panel), octaveButton.getScreenBounds(), nullptr);
}

juce::String MainComponent::audioOutputInfo (bool& isProblem) const
{
    isProblem = false;
    if (! options.openAudio)
        return U ("Vorschau ohne Audio.");

    if (auto* device = deviceManager.getCurrentAudioDevice(); device != nullptr && device->getActiveOutputChannels().countNumberOfSetBits() > 0)
        return U ("Ausgang: ") + deviceManager.getAudioDeviceSetup().outputDeviceName;

    isProblem = settings.engine.audioHarmony;
    return U ("Kein Audio-Ausgang. Unter \xe2\x80\x9e" "Einstellungen\xe2\x80\x9c einen Ausgang w\xc3\xa4hlen.");
}

void MainComponent::showAudioPanel()
{
    if (audioDialog != nullptr)
    {
        audioDialog->toFront (true);
        return;
    }

    const auto& e = settings.engine;
    AudioPanel::Values values;
    values.on = e.audioHarmony;
    values.formant = e.formantCurve;
    values.hiCut = e.hiCutCurve;
    values.dropVoicing = e.dropVoicing;
    values.ensemble = e.ensemble;
    values.vibratoCoupling = e.vibratoCoupling;
    values.stereoSpread = e.stereoSpread;
    values.harmonyGainDb = e.harmonyGainDb;
    values.dryKill = e.dryKill;

    bool problem = false;
    const auto info = audioOutputInfo (problem);
    auto* panel = new AudioPanel (values, info);
    panel->setOutputInfo (info, problem);

    juce::Component::SafePointer<MainComponent> safeThis (this);
    juce::Component::SafePointer<AudioPanel> safePanel (panel);
    panel->onChange = [safeThis, safePanel] (const AudioPanel::Values& v)
    {
        if (safeThis == nullptr)
            return;
        auto& engine = safeThis->settings.engine;
        engine.audioHarmony = v.on;
        engine.formantCurve = v.formant;
        engine.hiCutCurve = v.hiCut;
        engine.dropVoicing = v.dropVoicing;
        engine.ensemble = v.ensemble;
        engine.vibratoCoupling = v.vibratoCoupling;
        engine.stereoSpread = v.stereoSpread;
        engine.harmonyGainDb = v.harmonyGainDb;
        engine.dryKill = v.dryKill;
        safeThis->pushSettings();

        if (safePanel != nullptr)
        {
            bool isProblem = false;
            const auto text = safeThis->audioOutputInfo (isProblem);
            safePanel->setOutputInfo (text, isProblem);
        }
    };

    juce::DialogWindow::LaunchOptions launch;
    launch.content.setOwned (panel);
    launch.dialogTitle = "Audio-Harmonizer";
    launch.dialogBackgroundColour = colour::window;
    launch.escapeKeyTriggersCloseButton = true;
    launch.useNativeTitleBar = true;
    launch.resizable = false;
    audioDialog = launch.launchAsync();
}

/** The harmonizer needs somewhere to play. Until now the app only listened. */
void MainComponent::ensureAudioOutput()
{
    if (! options.openAudio)
        return;

    if (auto* device = deviceManager.getCurrentAudioDevice(); device != nullptr && device->getActiveOutputChannels().countNumberOfSetBits() > 0)
        return;

    auto setup = deviceManager.getAudioDeviceSetup();
    if (setup.outputDeviceName.isEmpty())
    {
        if (auto* type = deviceManager.getCurrentDeviceTypeObject())
        {
            const auto names = type->getDeviceNames (false);
            const int index = type->getDefaultDeviceIndex (false);
            if (juce::isPositiveAndBelow (index, names.size()))
                setup.outputDeviceName = names[index];
        }
    }

    if (setup.outputDeviceName.isEmpty())
        return;

    setup.useDefaultOutputChannels = false;
    setup.outputChannels.clear();
    setup.outputChannels.setRange (0, 2, true);
    deviceManager.setAudioDeviceSetup (setup, true);
}

/** The smallest buffer the interface offers from 64 samples up: every sample of it is delay. */
void MainComponent::useSmallBuffer()
{
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

/** The factory preset: everything that costs time is off or at its shortest.
    Key, harmony and MIDI settings stay as they are. */
void MainComponent::applyLowestLatency()
{
    auto& e = settings.engine;
    e.response = vm::Response::Fast;
    e.bendMode = vm::BendMode::Continuous;
    e.vibratoSmoothing = 0.0f;
    e.dynamicsSmoothingMs = 0.0f;
    // The two octave guards stay on: measured, switching them off saves nothing
    // on a clean start and lets wrong first notes through. The attack guard only
    // waits when the octave is in doubt, and then for a shorter time here.
    e.octaveAttackGuard = true;
    e.octaveAttackMs = 20.0f;
    e.octaveSustainGuard = true;
    e.octaveSustainMs = 11.0f;
    e.jumpGuard = false;
    e.overlapMs = 0.0f;
    e.releaseMs = 0.0f;

    settings.bufferChosen = false;   // the preset decides the buffer again
    useSmallBuffer();
    applySettingsToControls();
}

//==============================================================================
// Camera

vm::Settings MainComponent::hostSettings() const
{
    auto e = settings.engine;

    // "Harmonie-Mix" from the camera: an axis of the pad fades the harmony
    // between half as loud (0 %) and the level that is set (100 %).
    if (cameraSettings.enabled && (cameraSettings.xTarget == CameraSettings::targetHarmonyMix
                                   || cameraSettings.yTarget == CameraSettings::targetHarmonyMix))
    {
        e.harmonyOffset = -0.5f + cameraMix * (e.harmonyOffset + 0.5f);
        e.harmonyGainDb -= 24.0f * (1.0f - cameraMix);
    }
    return e;
}

juce::String MainComponent::padTargetName (int target)
{
    if (target == CameraSettings::targetHarmonyMix)
        return "Harmonie-Mix";
    if (target >= 0 && target <= 127)
        return "CC " + juce::String (target);
    return "aus";
}

void MainComponent::applyCameraSettings (const CameraSettings& next)
{
    const bool wasOn = cameraSettings.enabled;
    const bool deviceChanged = next.deviceName != cameraSettings.deviceName;
    const bool targetsChanged = next.xTarget != cameraSettings.xTarget || next.yTarget != cameraSettings.yTarget;
    cameraSettings = next;
    settings.cameraJson = cameraSettings.toJson();
    store->save (settings);

    if (targetsChanged)
        for (auto& sent : padSent)
            sent = -1;   // a new target gets the value at the next movement

    if (cameraSettings.xTarget != CameraSettings::targetHarmonyMix && cameraSettings.yTarget != CameraSettings::targetHarmonyMix)
        cameraMix = 1.0f;

    if (cameraSettings.enabled && (! wasOn || deviceChanged))
    {
        // Switching on: where the violin rests now is the zero point (spec 6). The values stay where they were.
        pad.recalibrate();
        gestures.reset();
        cameraHasPicture = false;
        cameraSettings.enabled = camera.start (cameraSettings.deviceName);   // false: see camera.error()

        // It did not start: the window's switch goes back to "Aus" next to the reason.
        if (! cameraSettings.enabled && cameraDialog != nullptr)
            if (auto* panel = dynamic_cast<CameraPanel*> (cameraDialog->getContentComponent()))
                panel->setPowerState (false);
    }
    else if (! cameraSettings.enabled && wasOn)
    {
        camera.stop();
        cameraHasPicture = false;
        cameraMix = 1.0f;
        padHud.setState (pad.x(), pad.y(), false);
    }

    padHud.setAxisNames (padTargetName (cameraSettings.xTarget), padTargetName (cameraSettings.yTarget));
    setStatusDot (cameraButton, cameraSettings.enabled);
    host.setSettings (hostSettings());
    updateCameraStatus();
}

void MainComponent::updateCameraStatus()
{
    auto* panel = cameraDialog != nullptr ? dynamic_cast<CameraPanel*> (cameraDialog->getContentComponent()) : nullptr;
    if (panel == nullptr)
        return;

    const auto error = camera.error();
    if (error.isNotEmpty())
        panel->setStatus (error, ! error.startsWith ("Warte"));
    else if (! cameraSettings.enabled)
        panel->setStatus (U ("Die Kamera ist aus."), false);
    else
        panel->setStatus (U ("Die Kamera l\xc3\xa4uft. Das Bild bleibt auf diesem Mac."), false);

    panel->setWaiting (cameraSettings.enabled && ! cameraHasPicture);
}

void MainComponent::sendPadValue (int slot, int target, int value)
{
    if (slot < 0 || slot >= AudioHost::kControllerSlots || value == padSent[slot])
        return;
    padSent[slot] = value;

    if (target >= 0 && target <= 127)
    {
        host.sendController (slot, target, value);
    }
    else if (target == CameraSettings::targetHarmonyMix)
    {
        cameraMix = (float) value / 127.0f;
        host.setSettings (hostSettings());
    }
}

void MainComponent::cameraFrame (const juce::Image& preview, const cam::Pose& pose)
{
    if (! cameraSettings.enabled)
        return;

    if (preview.isValid() && ! cameraHasPicture)
    {
        cameraHasPicture = true;
        updateCameraStatus();
    }

    if (pad.update (pose, cameraSettings.pad))
    {
        sendPadValue (0, cameraSettings.xTarget, pad.valueX());
        sendPadValue (1, cameraSettings.yTarget, pad.valueY());
    }
    padHud.setState (pad.x(), pad.y(), pad.active());

    const auto gesture = gestures.update (pose, cameraSettings.gestures);
    if (gesture == cam::Gesture::Nod)         cameraAction (cameraSettings.nodAction);
    else if (gesture == cam::Gesture::Shake)  cameraAction (cameraSettings.shakeAction);

    if (cameraDialog != nullptr)
        if (auto* panel = dynamic_cast<CameraPanel*> (cameraDialog->getContentComponent()))
            panel->showFrame (preview, pose, pad, gesture);
}

void MainComponent::cameraAction (int action)
{
    auto& e = settings.engine;

    switch (action)
    {
        case CameraSettings::actionHarmony:        e.triad = ! e.triad; break;
        case CameraSettings::actionFreezeSolo:     e.freezeSolo = ! e.freezeSolo; break;
        case CameraSettings::actionFreezeHarmony:  e.freezeHarmony = ! e.freezeHarmony; break;
        case CameraSettings::actionDetection:      e.enabled = ! e.enabled; break;
        case CameraSettings::actionPresetNext:     presetBar->step (1); return;
        case CameraSettings::actionPresetPrevious: presetBar->step (-1); return;
        default: return;
    }
    applySettingsToControls();
}

void MainComponent::showCamera()
{
    if (cameraDialog != nullptr)
    {
        cameraDialog->toFront (true);
        return;
    }

    auto* panel = new CameraPanel (cameraSettings, CameraTracker::availableCameras());
    juce::Component::SafePointer<MainComponent> safeThis (this);
    panel->onChange = [safeThis] (const CameraSettings& next)
    {
        if (safeThis != nullptr)
            safeThis->applyCameraSettings (next);
    };
    panel->onRecalibrate = [safeThis]
    {
        if (safeThis != nullptr)
            safeThis->pad.recalibrate();
    };

    juce::DialogWindow::LaunchOptions launch;
    launch.content.setOwned (panel);
    launch.dialogTitle = "Kamera";
    launch.dialogBackgroundColour = colour::window;
    launch.escapeKeyTriggersCloseButton = true;
    launch.useNativeTitleBar = true;
    launch.resizable = false;
    cameraDialog = launch.launchAsync();
    updateCameraStatus();

    // Layout check without a camera: a made-up picture of a player, the hand moved up and to the right.
    if (options.demo)
    {
        juce::Image picture (juce::Image::RGB, 320, 240, true);
        {
            juce::Graphics g (picture);
            g.fillAll (juce::Colour (0xff2a2a2a));
        }

        const auto at = [] (float x, float y) { cam::Point point; point.x = x; point.y = y; point.confidence = 0.9f; return point; };
        cam::Pose pose;
        pose.valid = true;
        pose.nose = at (0.50f, 0.30f);
        pose.leftEye = at (0.53f, 0.27f);
        pose.rightEye = at (0.47f, 0.27f);
        pose.leftEar = at (0.57f, 0.29f);
        pose.rightEar = at (0.43f, 0.29f);
        pose.neck = at (0.50f, 0.42f);
        pose.leftShoulder = at (0.64f, 0.46f);
        pose.rightShoulder = at (0.36f, 0.46f);
        pose.leftElbow = at (0.76f, 0.56f);
        pose.leftWrist = at (0.82f, 0.38f);
        pose.rightElbow = at (0.30f, 0.66f);
        pose.rightWrist = at (0.46f, 0.62f);

        cam::PadTracker demoPad;
        demoPad.update (pose, cameraSettings.pad);        // zero point
        for (int i = 1; i <= 8; ++i)
        {
            pose.timeSec = i / 15.0;
            pose.leftWrist = at (0.82f + 0.012f * (float) i, 0.38f - 0.012f * (float) i);
            demoPad.update (pose, cameraSettings.pad);
        }
        panel->showFrame (picture, pose, demoPad, cam::Gesture::Nod);
        panel->setStatus (U ("Die Kamera l\xc3\xa4uft. Das Bild bleibt auf diesem Mac."), false);
        padHud.setAxisNames (padTargetName (cameraSettings.xTarget), padTargetName (cameraSettings.yTarget));
        padHud.setState (demoPad.x(), demoPad.y(), true);
    }
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
        // A change of the buffer size while the settings are open is the user's choice.
        const int bufferSize = deviceManager.getAudioDeviceSetup().bufferSize;
        if (settingsDialog != nullptr && lastBufferSize > 0 && bufferSize != lastBufferSize && ! settings.bufferChosen)
        {
            settings.bufferChosen = true;
            store->save (settings);
        }
        lastBufferSize = bufferSize;

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
        const auto chord = vm::scale::voices (step.note, e.keyRoot, e.scaleMode, vm::voicingMask (e), e.harmonyMode == vm::HarmonyMode::Diatonic,
                                              ! e.mpe && e.triadChannel == e.midiChannel);
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

    display.push (f, e.a4Hz, e.gateDb, e.gateOffDb);
    levelMeter.update (f.levelDb, e.gateDb, e.gateOffDb, clipTicks > 0);

    // The harmonizer's curves show where the played pitch is on them.
    if (audioDialog != nullptr)
        if (auto* panel = dynamic_cast<AudioPanel*> (audioDialog->getContentComponent()))
            panel->setPlayhead (f.heard || f.active ? f.pitch : -1.0f);

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

    if (options.openAudio && ! audioReady)
        text = U ("Audio und MIDI werden gestartet \xe2\x80\xa6");
    else if (error.isNotEmpty())
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

    if (options.openAudio && ! audioReady)
        problem = false;

    if (! problem && audioReady == options.openAudio)
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

    // the emblem and the word mark "VioLogic" ("Vio" in crimson, "Logic" in silver, as the logo writes it)
    const auto header = juce::Rectangle<int> (kMargin, 0, kBrandWidth, kHeaderHeight);
    brand::drawEmblem (g, header.withWidth (kEmblemSize).withSizeKeepingCentre (kEmblemSize, kEmblemSize).toFloat());
    {
        const auto wordFont = font (kWordMarkHeight, true);
        const float x = (float) (header.getX() + kEmblemSize + kBrandGap);
        const float baseline = (float) header.getCentreY() + kWordMarkHeight * 0.33f;
        g.setFont (wordFont);
        g.setColour (colour::crimson);
        g.drawSingleLineText ("Vio", juce::roundToInt (x), juce::roundToInt (baseline));
        g.setColour (colour::silver);
        g.drawSingleLineText ("Logic", juce::roundToInt (x + juce::GlyphArrangement::getStringWidth (wordFont, "Vio")), juce::roundToInt (baseline));
    }

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
    auto headerRow = header.withSizeKeepingCentre (header.getWidth() - 2 * kMargin, PresetBar::kHeight);
    headerRow.removeFromLeft (kBrandWidth + 16);   // the emblem and the word mark, painted
    cameraButton.setBounds (headerRow.removeFromRight (108));
    headerRow.removeFromRight (8);
    audioButton.setBounds (headerRow.removeFromRight (176));
    headerRow.removeFromRight (24);
    presetBar->setBounds (headerRow.withWidth (juce::jmin (640, headerRow.getWidth())));

    const auto displayArea = area.removeFromTop (kDisplayHeight);
    display.setBounds (displayArea);
    padHud.setBounds (display.overlayArea().translated (displayArea.getX(), displayArea.getY()));

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

    if (splash != nullptr)
        splash->setBounds (getLocalBounds());
}
