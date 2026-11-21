# Archet

Archet hört über das Audio-Interface auf deine Geige und gibt ihre Tonhöhe als MIDI aus.
Die App legt dafür einen virtuellen MIDI-Ausgang an, den MainStage, Logic und andere Programme
wie ein angeschlossenes Keyboard sehen. Er heißt **Archet**, bei Installationen von vor
Version 1.2 weiter **ViolinMIDI**, damit eine bestehende Einrichtung in MainStage bleibt. Der
aktuelle Name steht oben im Fenster.

Bis Version 1.1 hieß die App ViolinMIDI.

Sie ist einstimmig, auf gestrichene Saiten abgestimmt und für eine 7-saitige Geige mit
Tonabnehmer ausgelegt (B♭1 bis A7).

## Installation auf dem Mac

Das ist nur einmal nötig. Spätere Versionen holt die App selbst, siehe „Updates“.

### Mit einer Zeile im Terminal

1. Das Programm „Terminal“ öffnen (mit Cmd + Leertaste danach suchen).
2. Diese Zeile einfügen und Enter drücken:

   ```
   curl -fsSL https://raw.githubusercontent.com/XLeathnex/ViolinMIDI/main/install.sh | bash
   ```

3. Die App wird geladen, in „Programme“ gelegt und gestartet. Beim ersten Start fragt macOS nach
   dem Mikrofon-Zugriff. Mit „Erlauben“ bestätigen, sonst kommt kein Signal an.

Dafür muss nichts weiter installiert sein. Eine App unter dem früheren Namen ViolinMIDI wird dabei
entfernt, die Einstellungen bleiben. Liegt schon eine ältere Archet-App in „Programme“
und macOS lässt das Ersetzen nicht zu, sagt die Zeile das. Dann die alte App in den Papierkorb
legen und die Zeile noch einmal ausführen.

### Von Hand

1. `ViolinMIDI-macOS.zip` von der [Seite mit den Versionen](https://github.com/XLeathnex/ViolinMIDI/releases/latest)
   laden und doppelklicken. Es entsteht `ViolinMIDI.app` (der Dateiname ist aus Gründen der
   Update-Kompatibilität geblieben; nach Belieben in `Archet.app` umbenennen).
2. Die App in den Ordner „Programme“ ziehen.
3. Die App ist nicht bei Apple registriert. macOS blockiert deshalb den ersten Start, wenn die
   Datei über den Browser kam. Einmalig im Terminal ausführen:

   ```
   xattr -dr com.apple.quarantine /Applications/Archet.app
   ```

   Alternativ: App starten, die Meldung schließen, dann unter Systemeinstellungen →
   Datenschutz & Sicherheit auf „Dennoch öffnen“ klicken.
4. Beim ersten Start den Mikrofon-Zugriff erlauben.

Voraussetzung: macOS 11 oder neuer, Intel oder Apple Silicon.

## Einrichtung

### In Archet

1. **Einstellungen…** öffnen, das Interface und den Eingang wählen, an dem die Geige hängt.
   Puffergröße so klein wie möglich (64 oder 128 Samples). Die Abtastrate nicht verändern,
   sie gilt für alle Programme am Interface.
2. Kräftig spielen und die **Eingangsverstärkung** so stellen, dass der Pegelbalken weit nach
   rechts geht, ohne orange zu werden.
3. Nicht spielen und die **Ansprechschwelle** (weiße Marke) knapp über das Grundrauschen legen.
4. Einen Ton spielen: Die große Anzeige zeigt die Note, der Punkt „MIDI“ leuchtet.

### In MainStage

1. Archet zuerst starten, dann MainStage. Der MIDI-Ausgang (Name oben im Fenster) erscheint dort
   als MIDI-Gerät.
2. Ein Software-Instrument anlegen. Steht dessen Keyboard-Eingang auf „Alle“, spielt die Geige
   es sofort.
3. Den **Pitch-Bend-Bereich des Instruments** auf denselben Wert stellen wie in Archet.
   Ab Werk stehen die meisten Instrumente auf ±2 Halbtöne, das ist auch hier die Voreinstellung.
   Für lange Glissandi beide Seiten auf ±12 oder ±24 stellen.
4. Im Channel-Strip-Informationsfenster unter „MIDI-Eingang“ dürfen Pitch Bend und
   Expression (CC11) nicht herausgefiltert werden.

Die Menünamen in MainStage sind aus der Dokumentation übernommen und können je nach Version
leicht abweichen.

### Wenn andere MIDI-Geräte mit angeschlossen sind

MainStage wirft alle MIDI-Geräte zusammen. Archet sendet neben den Noten laufend Pitch Bend
und, bei „Lautstärke: Kontinuierlich“, Expression (Regler Nr. 11). Dieselbe Nummer benutzen
Expression-Pedale, deshalb können sich Regler anderer Geräte mitbewegen.

- Oben im Fenster einen eigenen **MIDI-Kanal** wählen, den kein anderes Gerät benutzt.
- In MainStage im Layout-Modus ein eigenes Keyboard-Bedienelement für die Geige anlegen und ihm
  den MIDI-Ausgang von Archet zuweisen. Den übrigen Bedienelementen ihr jeweiliges Gerät zuweisen
  statt „Alle“.
- Schnelle Abhilfe ohne Umbau: „Lautstärke“ auf „Nur Anschlagstärke“ stellen. Dann sendet die
  App keine Expression mehr.

## Bedienung

| Einstellung | Wirkung |
| --- | --- |
| **Stufenlos** (Taste 1) | Pitch Bend folgt der Saite exakt, mit Glissando und Vibrato. |
| **Vibrato geglättet** (Taste 2) | Glissando wird übertragen, Vibrato bis etwa ±40 Cent herausgefiltert. |
| **Halbtöne** (Taste 3) | Kein Pitch Bend. Beim Rutschen wird jeder erreichte Halbton neu angeschlagen. |
| **Pitch-Bend-Bereich** | Muss zum Instrument passen. Überschreitet ein Glissando den Bereich, wird neu angeschlagen. |
| **Lautstärke: Kontinuierlich** | Anschlagstärke plus laufende Expression (CC11): An- und Abschwellen des Bogens steuert das Instrument. |
| **Lautstärke: Nur Anschlagstärke** | Velocity aus dem Pegel beim Tonbeginn, danach keine Dynamikdaten. |
| **Lautstärke: Fest** | Jede Note mit derselben Velocity. |
| **Ansprache** | Schnell, Normal oder Sicher, siehe Tabelle unten. |
| **Tiefster Ton** | Töne darunter werden ignoriert. Höher gesetzt reagieren auch tiefe Lagen schneller. |
| **Kammerton A** | Bezugston für die Notenzuordnung, 430 bis 450 Hz. |
| **MIDI-Kanal** | Kanal 1 bis 16, auf dem die App sendet (oben im Fenster). |
| **Alle Noten aus** | Beendet hängende Noten, stellt Pitch Bend und Expression zurück. |

Die Einstellungen werden gespeichert, aber nicht pro MainStage-Patch. Der Modus lässt sich
mit den Tasten 1, 2 und 3 umschalten, solange das Archet-Fenster vorne ist.

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
liegt als eigene Kopie unter `~/Library/Application Support/ViolinMIDI/Versions`. Beim Start
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

Dazu kommen der Audiopuffer von Archet und der von MainStage. Ein weicher Bogenansatz
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
| Geisternoten in Pausen | Ansprechschwelle höher stellen. |
| Leise Töne fehlen | Ansprechschwelle senken oder Eingangsverstärkung erhöhen. |
| Falsche Noten beim Bogenansatz tiefer Saiten | Ansprache auf „Sicher“. |
| Zu träge | Ansprache auf „Schnell“, kleinere Puffer, „Tiefster Ton“ höher setzen. |
| Instrument klingt verstimmt oder springt | Pitch-Bend-Bereich in App und Instrument angleichen. |
| Instrument bleibt leise | Auf „Nur Anschlagstärke“ wechseln oder „Alle Noten aus“ drücken, das setzt Expression zurück. |
| MainStage sieht den MIDI-Ausgang nicht | Archet vor MainStage starten. |
| Eine neue Version macht Ärger | Unter „Einstellungen…“ die vorige Version wählen. |

## Aufnahmen prüfen

Das Werkzeug `violinmidi_wav` schickt eine WAV- oder AIFF-Aufnahme durch dieselbe Erkennung
wie die App und listet die Noten:

```
violinmidi_wav aufnahme.wav --mode continuous --range 2 --trace verlauf.csv
```

Damit lässt sich die Erkennung an einer Aufnahme der eigenen Geige nachstellen und abstimmen.

## Selbst bauen

Voraussetzung: CMake 3.22 oder neuer und ein C++17-Compiler (auf dem Mac Xcode). JUCE 9.0.6
wird beim Konfigurieren automatisch geladen.

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64"
cmake --build build --parallel
./build/violinmidi_tests
```

Die App liegt danach unter `build/app/ViolinMIDI_artefacts/Release/ViolinMIDI.app`.
`ViolinMIDI --selftest` prüft Erkennung und virtuellen MIDI-Port ohne Fenster.

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
| `tools/` | `violinmidi_wav` zum Prüfen von Aufnahmen |
| `assets/` | Das App-Icon, erzeugt mit `ViolinMIDI --render-icon assets/AppIcon.png` |

## Erweitern

Die App ist so aufgebaut, dass Neues an genau einer Stelle dazukommt:

| Vorhaben | Wo |
| --- | --- |
| Name, Farben, Logo ändern | `app/Brand.h`, `app/Brand.cpp` (Anzeigename für macOS zusätzlich `VIOLINMIDI_DISPLAY_NAME` in `app/CMakeLists.txt`) |
| Aussehen aller Standard-Bedienelemente | `app/Theme.cpp` |
| Neue Einstellung, die gespeichert wird | Feld in `AppSettings`, je eine Zeile in `load()` und `save()` in `app/SettingsStore.cpp` |
| Neues Bedienelement im Fenster | im Konstruktor von `app/MainComponent.cpp` anlegen und mit `addRow()` auf eine Karte setzen; das Layout ergibt sich |
| Neue Live-Anzeige | `app/Widgets.cpp`, gefüttert aus `MainComponent::timerCallback()` |
| Andere Erkennung oder MIDI-Logik | `core/` (ohne JUCE, mit eigenen Tests in `tests/test_main.cpp`) |

Regeln, damit es schnell und sicher bleibt:

- Im Audio-Thread (`AudioHost::audioDeviceIOCallbackWithContext`, `vm::Engine::process`) nichts
  anlegen, nichts sperren, nicht warten. Die Erkennung braucht derzeit etwa 3 % eines
  Prozessorkerns.
- Anzeigen zeichnen sich nur neu, wenn sich etwas geändert hat (30 Bilder pro Sekunde).
- Bestehende Einstellungs-Schlüssel nie umbenennen: Alle Versionen lesen dieselbe Datei. Ändert
  sich die Bedeutung eines Werts, `kSchema` erhöhen und in `migrate()` umrechnen.
- Dateiname, Programmname und Bundle-ID (`ViolinMIDI`, `app.violinmidi.ViolinMIDI`) nicht ändern:
  Ältere Versionen erkennen Updates daran, und macOS hängt die Mikrofon-Freigabe daran.

## Lizenz

Die App verwendet JUCE und steht deshalb wie JUCE unter der AGPLv3. Der Quelltext liegt
vollständig in diesem Repository.
