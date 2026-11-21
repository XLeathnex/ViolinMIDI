# ViolinMIDI

ViolinMIDI hört über das Audio-Interface auf deine Geige und gibt ihre Tonhöhe als MIDI aus.
Die App legt dafür einen virtuellen MIDI-Ausgang namens **ViolinMIDI** an, den MainStage, Logic
und andere Programme wie ein angeschlossenes Keyboard sehen.

Sie ist einstimmig, auf gestrichene Saiten abgestimmt und für eine 7-saitige Geige mit
Tonabnehmer ausgelegt (B♭1 bis A7).

## Installation auf dem Mac

1. `ViolinMIDI-macOS.zip` auf den Mac kopieren und doppelklicken. Es entsteht `ViolinMIDI.app`.
2. `ViolinMIDI.app` in den Ordner „Programme“ ziehen.
3. Die App ist nicht bei Apple registriert. macOS blockiert deshalb den ersten Start, wenn die
   Datei über Browser, Mail oder AirDrop kam. Einmalig im Terminal ausführen:

   ```
   xattr -dr com.apple.quarantine /Applications/ViolinMIDI.app
   ```

   Alternativ: App starten, die Meldung schließen, dann unter Systemeinstellungen →
   Datenschutz & Sicherheit auf „Dennoch öffnen“ klicken.
4. Beim ersten Start fragt macOS nach dem Mikrofon-Zugriff. Mit „Erlauben“ bestätigen, sonst kommt
   kein Signal an.

Voraussetzung: macOS 11 oder neuer, Intel oder Apple Silicon.

## Einrichtung

### In ViolinMIDI

1. **Audio-Einstellungen…** öffnen, das Interface und den Eingang wählen, an dem die Geige hängt.
   Puffergröße so klein wie möglich (64 oder 128 Samples). Die Abtastrate nicht verändern,
   sie gilt für alle Programme am Interface.
2. Kräftig spielen und die **Eingangsverstärkung** so stellen, dass der Pegelbalken weit nach
   rechts geht, ohne orange zu werden.
3. Nicht spielen und die **Ansprechschwelle** (weiße Marke) knapp über das Grundrauschen legen.
4. Einen Ton spielen: Die große Anzeige zeigt die Note, der Punkt „MIDI“ leuchtet.

### In MainStage

1. ViolinMIDI zuerst starten, dann MainStage. „ViolinMIDI“ erscheint dort als MIDI-Gerät.
2. Ein Software-Instrument anlegen. Steht dessen Keyboard-Eingang auf „Alle“, spielt die Geige
   es sofort. Soll nur die Geige es spielen, im Layout-Modus dem Keyboard das Gerät
   „ViolinMIDI“ zuweisen.
3. Den **Pitch-Bend-Bereich des Instruments** auf denselben Wert stellen wie in ViolinMIDI.
   Ab Werk stehen die meisten Instrumente auf ±2 Halbtöne, das ist auch hier die Voreinstellung.
   Für lange Glissandi beide Seiten auf ±12 oder ±24 stellen.
4. Im Channel-Strip-Informationsfenster unter „MIDI-Eingang“ dürfen Pitch Bend und
   Expression (CC11) nicht herausgefiltert werden.

Die Menünamen in MainStage sind aus der Dokumentation übernommen und können je nach Version
leicht abweichen.

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
| **Alle Noten aus** | Beendet hängende Noten, stellt Pitch Bend und Expression zurück. |

Die Einstellungen werden gespeichert, aber nicht pro MainStage-Patch. Der Modus lässt sich
mit den Tasten 1, 2 und 3 umschalten, solange das ViolinMIDI-Fenster vorne ist.

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

Dazu kommen der Audiopuffer von ViolinMIDI und der von MainStage. Ein weicher Bogenansatz
verlängert die Zeit, weil die Saite später eine klare Tonhöhe hat.

## Grenzen

- **Einstimmig.** Bei Doppelgriffen gewinnt der deutlich lautere Ton. Sind beide gleich laut,
  kann eine falsche, tiefere Note entstehen.
- **Pitch Bend gilt für den ganzen MIDI-Kanal.** Springt bei einem Glissando über den
  Bend-Bereich hinaus die Note um, verstimmt das kurz den Ausklang der alten Note. Ein größerer
  Bend-Bereich vermeidet das.
- **Oktav-Pfeifen**: Spricht eine Saite zuerst in der Oktave an, wartet die App bis zu 50 ms ab.
  Hält das Pfeifen länger, wird die Oktave gespielt.
- **Getestet ohne die Zielgeige.** Die Erkennung ist mit künstlichen Signalen und mit
  Mikrofonaufnahmen von Violine, Cello und Kontrabass geprüft, nicht mit einer 7-saitigen
  Piezo-Geige. Für die Feinabstimmung hilft eine kurze, trockene Aufnahme des Instruments
  (siehe unten).

## Wenn etwas nicht stimmt

| Beobachtung | Abhilfe |
| --- | --- |
| Kein Pegel, obwohl gespielt wird | Richtigen Eingang wählen. Mikrofon-Zugriff unter Systemeinstellungen → Datenschutz & Sicherheit → Mikrofon erlauben. |
| Geisternoten in Pausen | Ansprechschwelle höher stellen. |
| Leise Töne fehlen | Ansprechschwelle senken oder Eingangsverstärkung erhöhen. |
| Falsche Noten beim Bogenansatz tiefer Saiten | Ansprache auf „Sicher“. |
| Zu träge | Ansprache auf „Schnell“, kleinere Puffer, „Tiefster Ton“ höher setzen. |
| Instrument klingt verstimmt oder springt | Pitch-Bend-Bereich in App und Instrument angleichen. |
| Instrument bleibt leise | Auf „Nur Anschlagstärke“ wechseln oder „Alle Noten aus“ drücken, das setzt Expression zurück. |
| MainStage sieht „ViolinMIDI“ nicht | ViolinMIDI vor MainStage starten. |

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

Jeder Push auf `main` baut die App bei GitHub (`.github/workflows/build.yml`), lässt alle Tests
laufen und legt `ViolinMIDI-macOS.zip` als Artefakt ab.

| Ordner | Inhalt |
| --- | --- |
| `core/` | Tonhöhenerkennung und Notenlogik, reines C++ ohne Abhängigkeiten |
| `app/` | Fenster, Audio-Eingang und MIDI-Ausgang (JUCE) |
| `tests/` | Automatische Tests mit künstlichen Geigensignalen |
| `tools/` | `violinmidi_wav` zum Prüfen von Aufnahmen |

## Lizenz

Die App verwendet JUCE, das unter der AGPLv3 steht. Für den eigenen Gebrauch ist das ohne
Auflagen. Wer die App weitergibt, muss den Quelltext unter der AGPLv3 mitgeben oder eine
kommerzielle JUCE-Lizenz haben.
