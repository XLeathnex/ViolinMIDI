# VioLogic

VioLogic hört über das Audio-Interface auf deine Geige und gibt ihre Tonhöhe als MIDI aus.
Die App legt dafür einen virtuellen MIDI-Ausgang an, den MainStage, Logic und andere Programme
wie ein angeschlossenes Keyboard sehen. Er heißt **VioLogic**, bei Installationen von vor
Version 1.2 weiter **VioLogic**, damit eine bestehende Einrichtung in MainStage bleibt. Der
aktuelle Name steht oben im Fenster.


Sie ist einstimmig, auf gestrichene Saiten abgestimmt und für eine 7-saitige Geige mit
Tonabnehmer ausgelegt (B♭1 bis A7).

## Installation auf dem Mac

Das ist nur einmal nötig. Spätere Versionen holt die App selbst, siehe „Updates“.

### Mit einer Zeile im Terminal

1. Das Programm „Terminal“ öffnen (mit Cmd + Leertaste danach suchen).
2. Diese Zeile einfügen und Enter drücken:

   ```
   curl -fsSL https://raw.githubusercontent.com/XLeathnex/VioLogic/main/install.sh | bash
   ```

3. Die App wird geladen, in „Programme“ gelegt und gestartet. Beim ersten Start fragt macOS nach
   dem Mikrofon-Zugriff. Mit „Erlauben“ bestätigen, sonst kommt kein Signal an.

Dafür muss nichts weiter installiert sein. Eine App unter dem früheren Namen VioLogic wird dabei
entfernt, die Einstellungen bleiben. Liegt schon eine ältere VioLogic-App in „Programme“
und macOS lässt das Ersetzen nicht zu, sagt die Zeile das. Dann die alte App in den Papierkorb
legen und die Zeile noch einmal ausführen.

### Von Hand

1. `VioLogic-macOS.zip` von der [Seite mit den Versionen](https://github.com/XLeathnex/VioLogic/releases/latest)
   laden und doppelklicken. Es entsteht `VioLogic.app` (der Dateiname ist aus Gründen der
   Update-Kompatibilität geblieben; nach Belieben in `VioLogic.app` umbenennen).
2. Die App in den Ordner „Programme“ ziehen.
3. Die App ist nicht bei Apple registriert. macOS blockiert deshalb den ersten Start, wenn die
   Datei über den Browser kam. Einmalig im Terminal ausführen:

   ```
   xattr -dr com.apple.quarantine /Applications/VioLogic.app
   ```

   Alternativ: App starten, die Meldung schließen, dann unter Systemeinstellungen →
   Datenschutz & Sicherheit auf „Dennoch öffnen“ klicken.
4. Beim ersten Start den Mikrofon-Zugriff erlauben.

Voraussetzung: macOS 11 oder neuer, Intel oder Apple Silicon.

## Einrichtung

### In VioLogic

1. **Einstellungen…** öffnen, das Interface und den Eingang wählen, an dem die Geige hängt.
   Puffergröße so klein wie möglich (64 oder 128 Samples). Die Abtastrate nicht verändern,
   sie gilt für alle Programme am Interface.
2. Kräftig spielen und die **Eingangsverstärkung** so stellen, dass der Pegelbalken weit nach
   rechts geht, ohne orange zu werden.
3. Nicht spielen und die **Ansprechschwelle** (weiße Marke) knapp über das Grundrauschen legen.
4. Einen Ton spielen: Die große Anzeige zeigt die Note, der Punkt „MIDI“ leuchtet.

### In MainStage

1. VioLogic zuerst starten, dann MainStage. Der MIDI-Ausgang (Name oben im Fenster) erscheint dort
   als MIDI-Gerät.
2. Ein Software-Instrument anlegen. Steht dessen Keyboard-Eingang auf „Alle“, spielt die Geige
   es sofort.
3. Den **Pitch-Bend-Bereich des Instruments** auf denselben Wert stellen wie in VioLogic.
   Ab Werk stehen die meisten Instrumente auf ±2 Halbtöne, das ist auch hier die Voreinstellung.
   Für lange Glissandi beide Seiten auf ±12 oder ±24 stellen.
4. Im Channel-Strip-Informationsfenster unter „MIDI-Eingang“ dürfen Pitch Bend und
   Expression (CC11) nicht herausgefiltert werden.

Die Menünamen in MainStage sind aus der Dokumentation übernommen und können je nach Version
leicht abweichen.

### Wenn andere MIDI-Geräte mit angeschlossen sind

MainStage wirft alle MIDI-Geräte zusammen. VioLogic sendet neben den Noten laufend Pitch Bend
und, bei „Lautstärke: Kontinuierlich“, Expression (Regler Nr. 11). Dieselbe Nummer benutzen
Expression-Pedale, deshalb können sich Regler anderer Geräte mitbewegen.

- Oben im Fenster einen eigenen **MIDI-Kanal** wählen, den kein anderes Gerät benutzt.
- In MainStage im Layout-Modus ein eigenes Keyboard-Bedienelement für die Geige anlegen und ihm
  den MIDI-Ausgang von VioLogic zuweisen. Den übrigen Bedienelementen ihr jeweiliges Gerät zuweisen
  statt „Alle“.
- Schnelle Abhilfe ohne Umbau: „Lautstärke“ auf „Nur Anschlagstärke“ stellen. Dann sendet die
  App keine Expression mehr.

## Bedienung

| Einstellung | Wirkung |
| --- | --- |
| **Stufenlos** (Taste 1) | Pitch Bend folgt der Saite exakt, mit Glissando und Vibrato. |
| **Vibrato geglättet** (Taste 2) | Glissando wird übertragen, Vibrato bis etwa ±40 Cent herausgefiltert. |
| **Halbtöne** (Taste 3) | Kein Pitch Bend. Beim Rutschen wird jeder erreichte Halbton neu angeschlagen. |
| **Pitch-Bend-Bereich** | Muss zum Instrument passen, ±0,2 bis ±48 Halbtöne. Der Bend (14 Bit, Mitte 8192) wird genau auf den Bereich skaliert und bleibt darüber bei 0 bzw. 16383 stehen. Erreicht ein Glissando den nächsten Halbton außerhalb des Bereichs, wird neu angeschlagen. |
| **Vibrato-Glättung** | Wie stark „Geglättet“ das Vibrato herausfiltert, 0 bis 100 %. |
| **Erkennung an/aus** (Leertaste) | Aus beendet sofort alle Noten und sendet nichts mehr. |
| **Tonart / Skala** | Grundton und Skala: Dur, natürliches, harmonisches und melodisches Moll, Dorisch, Mixolydisch, Phrygisch, Phrygisch Dominant, Lydisch, Lokrisch, Dur- und Moll-Pentatonik, Blues-Skala. In der Liste steht unter jedem Namen, wofür die Skala typisch ist. |
| **Nur Töne der Tonart** | Fremde Töne werden nicht angeschlagen und beenden die laufende Note. Rutscht der Finger in einen Ton der Skala, beginnt dieser. |
| **Pentatonik-Ton halten** | Bei Pentatonik- und Blues-Skala: Der letzte gültige Ton klingt bei fremden Tönen weiter, bis ein neuer gültiger Ton kommt oder der Pegel unter die Out-Schwelle fällt. |
| **Harmonie** | Zu jedem Ton erklingen die Stimmen des Voicings auf dem Harmonie-Kanal, immer vollständig, also mit dem gespielten Ton. Der Akkord richtet sich nach der Tonhöhe, die wirklich klingt: Gleitet ein Ton von C nach D, wird aus C-Dur nach 30 ms D-Moll, statt dass C-Dur mitgebogen wird. |
| **Voicing** | Enger Dreiklang (1-3-5), Septakkord (1-3-5-7), Open (1-5-10), Power (1-5-8), Quart (1-4-7), Terz-Duett, Sexten-Duett, fünf Oktav-Voicings (bei diesen klingen nur die genannten Oktaven) und das freie Voicing: bis zu 10 Stimmen zwischen −24 und +24 Halbtönen, über „…“ einstellbar. |
| **Harmonie-Modus** | Diatonisch: Die Stimmen werden in der Tonart abgezählt, so entstehen je nach Stufe Dur-, Moll- oder verminderte Akkorde. Chromatisch: immer dieselben Halbtonabstände. Ein Ton außerhalb der Tonart bekommt auch diatonisch die festen Abstände. |
| **Solo halten / Harmonie halten** | Freeze: Was auf der Spur gerade klingt, bleibt stehen, bis der Taster wieder gelöst wird; auf der anderen Spur wird weitergespielt. Klingt nichts, wird der nächste Ton eingefroren. „Alle Noten aus“ und „Erkennung aus“ lösen beide. |
| **In-Schwelle / Out-Schwelle** | Über der In-Schwelle kann ein Ton beginnen, erst unter der Out-Schwelle endet er. Die Out-Schwelle liegt nie über der In-Schwelle. |
| **Tracking-Filter** | Vier Schutzfunktionen in einem Feld. Anstreich-Schutz (wartet am Tonbeginn bis zu 60 ms, wenn auch die Oktave darunter möglich ist) und Oktavsprung-Sperre (ein Oktavsprung im Ton muss bis zu 100 ms anhalten). Sprungschutz: Kurz nach einem Tonbeginn wird ein Sprung ab dem gewählten Intervall erst nach der Sperrzeit gespielt; ein kurz angerissener Nachbarton fällt so weg, ein gewollter Sprung kommt um die Sperrzeit später. Überbrückung: So lange hält der Ton, wenn die Tonhöhe kurz nicht zu erkennen ist (Bogenwechsel), solange der Pegel über der Out-Schwelle bleibt; auf den tiefen Saiten wirkt sie erst ab etwa 45 ms. |
| **Spiel-Überlappung** | 0 bis 1500 ms. Beim Tonwechsel klingt der vorherige Ton so lange weiter. Im MPE-Modus und im Audio-Harmonizer wird er dabei ausgeblendet; auf einem gemeinsamen MIDI-Kanal klingt er in voller Stärke nach, das passt zu Klängen mit eigenem Ausklang. |
| **Ausklingen (Spielende)** | 0 bis 3000 ms. Fällt der Pegel unter die Out-Schwelle, bleibt der Ton stehen und wird über diese Zeit mit Expression (CC11) ausgeblendet; erst dann kommt Note-Off. Ein neuer Ton beendet das Ausklingen. |
| **Harmonie-Offset** | −50 % bis 0 %. Die Harmoniestimmen werden um diesen Anteil leiser gesendet (Anschlagstärke und Dynamik-Controller). |
| **Artikulation** | Aus, Sanft oder Stark: Ein steiler Tonanfang (gezupft) bekommt eine hohe, ein weicher (gestrichen) eine niedrige Anschlagstärke. Verzögert nichts. Auf den tiefsten Saiten ist der Unterschied kleiner. |
| **Werks-Preset „Lowest Latenz“** | Im Preset-Menü ganz oben. Stellt Ansprache auf „Schnell“, Bewegung auf „Stufenlos“, alle Glättungen auf null, den Sprungschutz und das Nachklingen aus und den Audio-Puffer auf die kleinste Größe ab 64 Samples. Tonart, Harmonie und MIDI-Einstellungen bleiben. |
| **Presets** | Oben im Fenster: alle Einstellungen unter einem Namen speichern, laden, umbenennen, löschen. Jedes Preset hat einen Platz von 001 bis 128 und liegt als lesbare JSON-Datei im Preset-Ordner („…“ → „Preset-Ordner zeigen“). |
| **Dynamik: Anschlag und Verlauf** | Anschlagstärke plus laufender Controller: An- und Abschwellen des Bogens steuert das Instrument. |
| **Dynamik: Nur Anschlag** | Velocity aus dem Pegel beim Tonbeginn, danach keine Dynamikdaten. |
| **Dynamik: Feste Stärke** | Jede Note mit derselben Velocity. |
| **Dynamik-Ziel** | Welcher Controller den Verlauf trägt: CC11 (Expression), CC1 (Modwheel), CC2 (Breath) oder CC1 und CC11 zusammen. |
| **Dynamik-Glättung** | 0 bis 150 ms. Glättet schnelle Pegelschwankungen des Bogens im Controller. |
| **Ansprache** | Schnell, Normal oder Sicher, siehe Tabelle unten. |
| **Tiefster Ton** | Töne darunter werden ignoriert. Höher gesetzt reagieren auch tiefe Lagen schneller. |
| **Kammerton A** | Bezugston für die Notenzuordnung, 430 bis 450 Hz. |
| **MIDI-Kanal** | Kanal 1 bis 16, oder MPE: jede Note auf einem eigenen Kanal (2 bis 16) mit ±48 Halbtönen Pitch Bend, Dynamik zusätzlich als Channel Pressure. Der Harmonie-Kanal bleibt dabei ein gewöhnlicher Kanal. |
| **Alle Noten aus** | Beendet alle Noten, auch eingefrorene, und stellt Pitch Bend und Controller zurück. |

Die Einstellungen werden gespeichert, aber nicht pro MainStage-Patch. Der Modus lässt sich
mit den Tasten 1, 2 und 3 umschalten, solange das VioLogic-Fenster vorne ist.

### Steuerung per MIDI

Unter Einstellungen, Seite „MIDI-Steuerung“: einen MIDI-Eingang wählen (zum Beispiel ein
Fußpedal) und jedem Regler einen Controller oder eine Note zuweisen. „Lernen“ übernimmt die
nächste eintreffende Nachricht. Ein Controller schaltet Schalter ab Wert 64 ein, verteilt sich
bei Auswahlen auf die Möglichkeiten und stellt Regler stufenlos; eine Note schaltet um oder
springt zur nächsten Möglichkeit. Die Zuweisungen werden gespeichert.

Presets: Ein Program Change lädt das Preset mit dieser Nummer (Program 0 lädt Preset 001); auf
welchem Kanal, steht auf derselben Seite. „Nächstes Preset“ und „Vorheriges Preset“ stehen in
der Liste und lassen sich wie jeder Regler einem Fußschalter zuweisen, ebenso „Freeze Solo“
und „Freeze Harmonie“.

### Audio-Harmonizer

Knopf „Audio-Harmonizer …“ oben rechts. Eingeschaltet spielt VioLogic die Harmoniestimmen zusätzlich
mit dem eigenen Klang der Geige: Das Eingangssignal wird in der Tonhöhe verschoben und über den
Audio-Ausgang ausgegeben, die Geige selbst unverändert in der Mitte. Es gelten Tonart, Voicing
und Harmonie-Modus der Harmonie; die Harmonie muss eingeschaltet sein. Den Ausgang wählt man
unter „Einstellungen“.

| Regler | Wirkung |
| --- | --- |
| **Korpus-Erhalt** | 100 %: Die Resonanz des Geigenkorpus bleibt, wo sie ist, während die Tonhöhe wandert. Niedrige Werte lassen tiefe Oktaven wie ein Cello klingen. |
| **Bogen-Variation** | Kleine Unterschiede in Tonhöhe und Zeit zwischen den Stimmen, gegen den Kammfilter-Klang. |
| **Vibrato-Kopplung** | Wie stark die Stimmen dem eigenen Vibrato folgen. |
| **Stereo-Panorama** | Verteilt die Stimmen links und rechts. |
| **Piezo-Glättung** | Höhenfilter nur für die Stimmen, 4 bis 16 kHz. |
| **Harmonie-Pegel, Dry-Kill** | Lautstärke der Stimmen; Dry-Kill lässt die Geige selbst aus dem Ausgang weg. |

Die Stimmen kommen etwa eine halbe Schwingung des gespielten Tons später als die Geige: unter
2 ms in hohen Lagen, 7 bis 12 ms auf der tiefsten Saite. Doppelgriffe kann der Harmonizer nicht,
er folgt einem Ton. „Spiel-Überlappung“ und
„Ausklingen“ gelten auch hier; „Harmonie halten“ friert den Akkord als stehenden Klang ein.

### Kamera-Steuerung

Knopf „Kamera …“ oben rechts öffnet ein eigenes Fenster. Die Kamera sucht im Bild die Schultern,
Arme und den Kopf; das Bild wird nur auf dem Mac ausgewertet und nicht gespeichert. macOS fragt
beim ersten Einschalten nach dem Zugriff auf die Kamera.

- **Schnecken-Höhe:** Die Höhe der Hand am Geigenhals wird zu einem Controller (CC1, CC11 oder
  CC74) oder regelt den Harmonie-Mix. Geige in die tiefste Haltung bringen, „Tief-Position
  setzen“ drücken, dann die höchste und „Hoch-Position setzen“. Glättung und Totzone beruhigen
  den Verlauf.
- **Kopf-Gesten:** Nicken und Kopfschütteln können Harmonie, Halten, Presets oder die Erkennung
  schalten. Empfindlichkeit und Sperrzeit einstellen, bis es beim Spielen nicht versehentlich
  auslöst. Eine Kopfbewegung, die zur Musik gehört, sieht für die Kamera wie ein Nicken aus:
  Was auf der Bühne nicht versehentlich passieren darf (etwa „Erkennung an/aus“), besser auf
  einen Fußschalter legen.

Die Bogenbewegung wird nicht ausgewertet: Bei 15 bis 30 Bildern pro Sekunde käme sie 50 bis
100 ms zu spät, um die Artikulation zu steuern.

### Was die App aus dem Spiel macht

- **Fingerwechsel unter einem Bogen** wird eine neue Note. Die neue Note beginnt, bevor die alte
  endet, sodass einstimmige Synthesizer im Legato-Modus binden.
- **Rutschen auf der Saite** wird Pitch Bend auf derselben Note.
- **Neuer Bogenstrich auf demselben Ton** wird eine neue Note, wenn der Pegel dabei deutlich
  einbricht (etwa 9 dB). Ein weicher Bogenwechsel bleibt eine Note.
- **Einschwingen**: Ein gestrichener Ton beginnt oft zu tief. Solange der Bogenansatz noch
  anschwillt, wird die Note nachgezogen statt neu angeschlagen.
- **Ausklingen**: Obertöne oder leere Saiten, die nach dem Ton weiterklingen, lösen keine
  neue Note aus.

## Updates und frühere Versionen

Beim Start schaut die App nach, ob es eine neuere Version gibt. Installiert wird nichts ohne
Klick.

- **Update:** Gibt es eine neuere Version, erscheint unten im Fenster der Knopf
  „Update auf … installieren“. Ein Klick lädt sie und startet die App neu.
- **Frühere Version:** Unter „Einstellungen…“ steht die Liste aller Versionen mit ihren
  Änderungen. Eine Version auswählen und „Diese Version verwenden“ klicken. Das geht in beide
  Richtungen und beliebig oft.

So funktioniert es: Die App im Ordner „Programme“ bleibt unverändert. Jede geladene Version
liegt als eigene Kopie unter `~/Library/Application Support/VioLogic/Versions`. Beim Start
übergibt die App aus „Programme“ an die gewählte Version. Startet eine gewählte Version nicht,
läuft beim nächsten Start wieder die App aus „Programme“.

Zu wissen:

- Nach jedem Versionswechsel fragt macOS einmal neu nach dem Mikrofon-Zugriff, weil die App
  nicht bei Apple registriert ist.
- Geladene Versionen belegen je etwa 30 MB. Alte Kopien lassen sich im Finder aus dem oben
  genannten Ordner löschen, solange sie nicht gerade laufen.
- Version 1.0.0 hat noch keinen Update-Knopf. Wer sie benutzt, installiert einmal von Hand
  eine neuere Version.
- Die Versionsliste kommt von der Projektseite bei GitHub. Ohne Internet läuft die App normal
  weiter, nur ohne Update-Hinweis.

## Verzögerung

Eine Tonhöhe lässt sich erst bestimmen, wenn etwa zwei Schwingungen vorliegen. Tiefe Töne
brauchen deshalb grundsätzlich länger. Gemessen mit künstlichen Geigentönen bei 48 kHz, vom
Tonbeginn bis zur MIDI-Note, ohne Audiopuffer:

| Ton | Schnell | Normal | Sicher |
| --- | --- | --- | --- |
| B♭1 (58 Hz, tiefste Saite) | 35 ms | 51 ms | 74 ms |
| F2 (87 Hz) | 25 ms | 35 ms | 50 ms |
| C3 (131 Hz) | 17 ms | 23 ms | 33 ms |
| G3 (196 Hz) | 13 ms | 17 ms | 25 ms |
| D4 (294 Hz) | 13 ms | 14 ms | 18 ms |
| A4 (440 Hz) | 10 ms | 11 ms | 15 ms |
| E5 (659 Hz) | 9 ms | 10 ms | 14 ms |
| E6 (1319 Hz) | 6 ms | 7 ms | 11 ms |

Dazu kommen der Audiopuffer von VioLogic und der von MainStage. Ein weicher Bogenansatz
verlängert die Zeit, weil die Saite später eine klare Tonhöhe hat.

## Grenzen

- **Einstimmig.** Bei Doppelgriffen gewinnt der deutlich lautere Ton. Sind beide gleich laut,
  kann eine falsche, tiefere Note entstehen.
- **Pitch Bend gilt für den ganzen MIDI-Kanal.** Springt bei einem Glissando über den
  Bend-Bereich hinaus die Note um, verstimmt das kurz den Ausklang der alten Note. Ein größerer
  Bend-Bereich vermeidet das.
- **Oktav-Pfeifen**: Spricht eine Saite zuerst in der Oktave an, wartet die App bis zu 50 ms ab.
  Hält das Pfeifen länger, wird die Oktave gespielt.
- **Sehr hohe Lagen** (ab C6) können im Ausklang kurze Extra-Noten erzeugen.

## Wenn etwas nicht stimmt

| Beobachtung | Abhilfe |
| --- | --- |
| Kein Pegel, obwohl gespielt wird | Richtigen Eingang wählen. Mikrofon-Zugriff unter Systemeinstellungen → Datenschutz & Sicherheit → Mikrofon erlauben. |
| Kein Pegel nach einem Update | macOS hat neu nach dem Mikrofon gefragt. Zugriff erlauben und die App neu starten. |
| Regler anderer MIDI-Geräte bewegen sich mit | Eigenen MIDI-Kanal wählen oder „Lautstärke“ auf „Nur Anschlagstärke“, siehe oben. |
| Geisternoten in Pausen | In-Schwelle höher stellen. |
| Leise Töne fehlen | In-Schwelle senken oder Verstärkung erhöhen. |
| Töne brechen beim leisen Streichen ab | Out-Schwelle senken. |
| Falsche Noten beim Bogenansatz tiefer Saiten | Ansprache auf „Sicher“. |
| Zu träge | Werks-Preset „Lowest Latenz“ wählen. Unten im Fenster steht die Puffergröße: 64 oder 128 Samples sind gut, 512 kosten etwa 10 ms. |
| Instrument klingt verstimmt oder springt | Pitch-Bend-Bereich in App und Instrument angleichen. |
| Instrument bleibt leise | Auf „Nur Anschlagstärke“ wechseln oder „Alle Noten aus“ drücken, das setzt Expression zurück. |
| MainStage sieht den MIDI-Ausgang nicht | VioLogic vor MainStage starten. |
| Eine neue Version macht Ärger | Unter „Einstellungen…“ die vorige Version wählen. |

## Aufnahmen prüfen

Das Werkzeug `VioLogic_wav` schickt eine WAV- oder AIFF-Aufnahme durch dieselbe Erkennung
wie die App und listet die Noten:

```
VioLogic_wav aufnahme.wav --mode continuous --range 2 --trace verlauf.csv
```

Damit lässt sich die Erkennung an einer Aufnahme der eigenen Geige nachstellen und abstimmen.

## Selbst bauen

Voraussetzung: CMake 3.22 oder neuer und ein C++17-Compiler (auf dem Mac Xcode). JUCE 9.0.6
wird beim Konfigurieren automatisch geladen.

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64"
cmake --build build --parallel
./build/VioLogic_tests
```

Die App liegt danach unter `build/app/VioLogic_artefacts/Release/VioLogic.app`.
`VioLogic --selftest` prüft Erkennung und virtuellen MIDI-Port ohne Fenster.

Jeder Push auf `main` baut die App bei GitHub (`.github/workflows/build.yml`) und lässt alle
Tests laufen. Eine neue Version entsteht so:

1. Versionsnummer in `CMakeLists.txt` erhöhen und in `CHANGELOG.md` einen Abschnitt dafür anlegen.
2. Ein Tag mit derselben Nummer pushen, zum Beispiel `v1.2.0`.

Besteht der Build, erscheint die Version auf der Projektseite und der Update-Knopf findet sie.

| Ordner | Inhalt |
| --- | --- |
| `core/` | Tonhöhenerkennung und Notenlogik, reines C++ ohne Abhängigkeiten |
| `app/` | Fenster, Audio-Eingang, MIDI-Ausgang und Updater (JUCE) |
| `tests/` | Automatische Tests mit künstlichen Geigensignalen |
| `tools/` | `VioLogic_wav` zum Prüfen von Aufnahmen |
| `assets/` | Das App-Icon, erzeugt mit `VioLogic --render-icon assets/AppIcon.png` |

## Erweitern

Die App ist so aufgebaut, dass Neues an genau einer Stelle dazukommt:

| Vorhaben | Wo |
| --- | --- |
| Name, Farben, Logo ändern | `app/Brand.h`, `app/Brand.cpp` (Anzeigename für macOS zusätzlich `VioLogic_DISPLAY_NAME` in `app/CMakeLists.txt`) |
| Aussehen aller Standard-Bedienelemente | `app/Theme.cpp` |
| Neue Einstellung, die gespeichert wird | eine Zeile in der Tabelle in `app/SettingsFields.cpp`; sie landet damit in der Einstellungsdatei und in den Presets |
| Kamera: was die Körperpunkte bedeuten | `app/CameraLogic.cpp` (ohne JUCE, getestet in `tests/test_camera.h`); das Bild und die Punkte kommen aus `app/CameraTracker_mac.mm` |
| Klang der Audio-Harmonie | `core/Harmonizer.cpp`; ausprobieren ohne App mit `VioLogic_harmonize` |
| Neues Bedienelement im Fenster | im Konstruktor von `app/MainComponent.cpp` anlegen und mit `addItem()` in eine Spalte setzen; das Layout ergibt sich |
| Regler per MIDI steuerbar machen | ein Eintrag in `app/Params.cpp`; er erscheint von selbst in der Mapping-Tabelle |
| Neue Live-Anzeige | `app/Widgets.cpp`, gefüttert aus `MainComponent::timerCallback()` |
| Andere Erkennung | `core/PitchDetector.cpp`, `core/NoteTracker.cpp` (ohne JUCE, Tests in `tests/`) |
| Was aus den erkannten Noten gesendet wird (Tonart, Harmonie, Freeze, MPE) | `core/OutputStage.cpp`; die Regeln stehen oben in `core/OutputStage.h` |

Regeln, damit es schnell und sicher bleibt:

- Im Audio-Thread (`AudioHost::audioDeviceIOCallbackWithContext`, `vm::Engine::process`) nichts
  anlegen, nichts sperren, nicht warten. Die Erkennung braucht derzeit etwa 3 % eines
  Prozessorkerns.
- Anzeigen zeichnen sich nur neu, wenn sich etwas geändert hat (30 Bilder pro Sekunde).
- Bestehende Einstellungs-Schlüssel nie umbenennen: Alle Versionen lesen dieselbe Datei. Ändert
  sich die Bedeutung eines Werts, `kSchema` erhöhen und in `migrate()` umrechnen.
- Dateiname, Programmname und Bundle-ID (`VioLogic`, `app.VioLogic.VioLogic`) nicht ändern:
  Ältere Versionen erkennen Updates daran, und macOS hängt die Mikrofon-Freigabe daran.

## Lizenz

Die App verwendet JUCE und steht deshalb wie JUCE unter der AGPLv3. Der Quelltext liegt
vollständig in diesem Repository.
