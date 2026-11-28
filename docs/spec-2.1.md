# VioLogic 2.1 – verbindliche Festlegungen

Grundlage: „VioLogic – Spezifikation & Freigabe Version 2.1“ (PDF vom 28. November 2026) und die
Antworten des Geigers auf die Rückfragen danach. Wo die PDF und eine spätere Antwort sich
widersprechen, gilt die Antwort; solche Stellen sind mit *(geändert)* markiert.

## 1. Stabilität und Audio-Ausgang

### 1.1 Kein Rückfall auf eine alte Version
- Nach einem abgebrochenen Vorgang (Einmessen, Bearbeitung, Update) oder einem Neustart läuft
  immer die aktuelle Version mit dem zuletzt gespeicherten Zustand. Ungespeicherte Änderungen
  eines abgebrochenen Vorgangs werden verworfen.
- Ursache im Test noch ungeklärt (der Geiger prüft, ob es in 2.0 noch vorkommt). Bekannte Wege
  zu einem alten Stand, die 2.1 schließt:
  - Eine über den Update-Knopf geladene Version liegt im Versionsordner, die App in „Programme“
    bleibt alt. Endet ein Start der neuen Version unsauber (hart beendet, abgestürzt), startet
    beim nächsten Mal die alte App aus „Programme“. 2.1: Hat eine neuere Version einmal sauber
    gestartet, ersetzt sie die App in „Programme“ (sofern beschreibbar). Der Rückfall nach einem
    fehlgeschlagenen Start landet dann auf der letzten funktionierenden Version, nie auf einer
    älteren. Eine Version, die nie sauber gestartet ist, ersetzt nichts.
  - Wird eine neuere App installiert (Terminal-Zeile, von Hand), während in den Einstellungen
    noch eine ältere Version gewählt ist, übergibt die neue App an die alte. 2.1: Die Wahl gilt
    nur für die App, unter der sie getroffen wurde; ändert sich die App in „Programme“, wird eine
    ältere Wahl verworfen.
- Ein einzelner unsauber beendeter Start (Benutzer beendet während der Start-Animation) gilt
  nicht als Fehlstart (seit 2.0).

### 1.2 Harmonizer-Ausgang
- Ausgabegerät frei wählbar (Audio-Interface, Aggregate Device, BlackHole 2ch …) und das
  Ausgangs-Kanalpaar (Out 1/2, 3/4, 5/6 …), direkt im Harmonizer-Fenster.
- Reines Wet-Routing: nur die Harmoniestimmen am gewählten Ausgang (bisher „Dry-Kill“).
- Hinweis, wenn Eingang und Ausgang verschiedene Geräte sind: deren Taktgeber laufen
  auseinander, empfohlen wird ein macOS-Aggregate-Device mit Drift-Korrektur.

## 2. Harmonie folgt externen Akkorden

- Quelle wählbar: **Feste Tonart** (wie bisher, z. B. wenn der Keyboarder Gitarre spielt) oder
  **Externe Chords** (MIDI-Eingang, z. B. Network MIDI / RTP-MIDI über Ethernet).
- Ausgewertet werden alle MIDI-Kanäle des gewählten Eingangs.
- Gehaltener Akkord = gedrückte Tasten + per Sustain-Pedal (CC 64 ≥ 64) gehaltene Noten.
- Split-Punkt einstellbar, Standard: Tasten unterhalb C4 (also bis H3). Umschaltbar auf „Ganzes
  Keyboard“.
- Auto-Latch: Nach dem Loslassen von Tasten und Pedal bleibt der letzte Akkord, bis ein neuer
  angeschlagen wird.
- Tonart: Jeder Akkord bekommt sofort seine Skala, gewählt im Zusammenhang der letzten Akkorde
  (Am nach C–F–G: Moll der Tonart C-Dur; D7 als Zwischendominante seine eigene Skala).
  Modulationen und Zwischendominanten ohne Umschalten.
- Drei Harmonie-Modi:
  - **Nur Pentatonik**: Stimmen auf die zum Akkord/zur Tonart passende Pentatonik.
  - **Töne der Tonart halten (Diatonisch)**: Stimmen bleiben in der Skala des aktuellen Akkords.
  - **Hybrid**: Akkord- und Skalentöne. Zwei wählbare Varianten *(geändert: beide anbieten)*:
    - **A – beim Tonanfang**: Ist der gespielte Ton ein Akkordton, nehmen die Stimmen
      Akkordtöne, sonst laufen sie in der Skala mit. Keine Verzögerung, kein Umspringen im Ton.
    - **B – Halteton rastet ein**: wie A; wird ein Nicht-Akkordton länger gehalten (etwa eine
      Drittelsekunde), springen die Stimmen danach auf Akkordtöne.
- Die Voicings (Dreiklang, Free Voicing …) bleiben; im Akkord-Modus rasten ihre Stimmen auf den
  nächsten passenden Ton ein. Oktav-Voicings bleiben unverändert.
- Melodie-Filter *(geändert: ergänzt)*:
  - „Nur Töne der Tonart“ folgt bei externen Chords der Skala des aktuellen Akkords.
  - Eigener Schalter **„Nur Pentatonik-Töne“**, auch bei externen Chords.
  - **„Ton halten“** (bisher „Pentatonik-Ton halten“) gilt für jeden Filter: der letzte gültige
    Ton klingt weiter, bis ein passender kommt.
- Bezeichnung im Programm: **„Externe Chords“** *(geändert: kein Personenname)*.

## 3. Kamera

- Totzone heißt immer die **Kamera-Totzone** *(geändert)*. Maximalwert **25 %** *(geändert, vorher 15 %)*.
- **„Nullpunkt setzen“** bleibt: die aktuelle Haltung der Schnecke wird der Nullpunkt.
- Neuer, zusätzlicher Taster **„Totzone mittig setzen“** *(geändert)*: setzt den Nullpunkt genau
  in die Mitte des Kamerabilds. Beide Taster auch per MIDI und Tastenkürzel.
- **Farb-Marker mit Pipette**: Klick ins Kamerabild auf einen Sticker an der Schnecke (z. B.
  Neongrün/Cyan) kalibriert dessen Farbe (HSV); der Marker liefert dann die Position der
  Schnecke für Pad und Gesten. Ohne Marker: Körpererkennung wie bisher.
- **Schnecken-Geste**: schnell hoch = Chord Freeze an, schnell runter = aus. Mit
  Geschwindigkeitsschwelle, Sperrzeit und deutlicher Freeze-Anzeige. Solche Rucke werden aus dem
  XY-Pad herausgerechnet (der Wert springt danach zurück); langsame Bewegungen steuern weiter.
- **Bogenhand**: Die Kamera verfolgt die Bogenhand und erkennt den Richtungswechsel. Den
  Zeitpunkt eines Bogenwechsels bestimmt weiter das Audiosignal (sofort); die Kamera bestätigt
  ihn. Bei gleichem Ton: Neuanschlag (Note-Off + Note-On, neuer Einsatz im Harmonizer); bei neuem
  Ton: sauberer Wechsel. Schwelle und Entprellung einstellbar.

## 4. Anzeige und Vollbild

- Grafik oben **75 %** Tonhöhe, unten **25 %** Lautstärke mit In- und Out-Linie, zeitgleich.
  *(geändert: keine Totzonen-Linie in der Lautstärke-Grafik)*
- Zoom getrennt für **X (Zeit)** und **Y (Tonhöhe/Pegel)**.
- **Vollbild** (Knopf, Ctrl + Cmd + F, ESC beendet): alles wird größer; die Kamera-Steuerung sitzt
  dann rechts im Fenster statt in einem eigenen Fenster *(geändert)*. Nur im Vollbild; im normalen
  Fenster bleibt sie ein eigenes Fenster.

## 5. Automatisches Einmessen

- **Normalmodus**: stellt die vorhandenen Regler ein, zeigt vorher/nachher, 1 Klick zurück oder
  speichern. **Profimodus**: zusätzlich Aufnahme (Roh-Audio) und Telemetrie, Diagnose-Bericht als
  Ordner auf dem Schreibtisch für Claude. „Abbrechen“ verwirft alles.
- Geltungsbereich:
  - **Erkennung** (Eingangspegel, In-/Out-Schwelle, Oktavschutz, Tracking-Filter, tiefster Ton,
    Kamera-Totzone): pro **Instrumenten-Profil**, Anzahl unbegrenzt. Presets ändern diese Werte
    nicht mehr (auch „Lowest Latenz“ nicht).
  - **Audio-Harmonizer** (Formant- und High-Cut-Kurve, Mix-Pegel): pro Preset, mit Knopf „Auf alle
    Presets anwenden“.
- Profimodus erkennt zusätzlich: Regler am Anschlag, verbleibende Oktavsprünge/Aussetzer,
  kritische Frequenzbereiche pro Saite (schrill, dünn, matschig), Knackser beim Bogenwechsel,
  Latenz der Stimmen.
- Geführtes 7-Stufen-Protokoll, Schritt für Schritt mit „Weiter“:

| Schritt | Spielvorgabe | Messziel |
| --- | --- | --- |
| 1 | Leersaiten lang (jede Saite einzeln aushalten) | Grundpegel pro Saite, Einschwingen, Oktavschutz-Basis, Formant-Referenz |
| 2 | Leersaiten kurz (deutlich getrennte Striche) | In-/Out-Hysterese, Release, sauberes Ausklingen |
| 3 | Leersaiten staccato | Transienten, Knackser beim Bogenwechsel, minimale Attack-Latenz |
| 4 | Lauf auf jeder Saite, legato | Übergänge ohne Glissando-Fehlauslöser, Phasenstabilität des Harmonizers |
| 5 | Lauf auf jeder Saite, detaché | Alltags-Spielgefühl, Durchgangstöne im Hybrid-Modus, Saiten-Balance |
| 6 | Lauf auf jeder Saite, staccato | schnelle Notenfolgen, Oktavsprung-Sicherheit, Neuanschlag |
| 7 | E-Saite, 3. Lage (legato, normal, staccato) | High-Cut- und Formant-Kurve im oberen Register |

## 6. Mouse-Over-Texte

Wortlaut aus der PDF; angepasst nur, wo eine spätere Festlegung es verlangt.

| Element | Text |
| --- | --- |
| Harmonizer-Ausgang (Gerät & Kanäle) | „Wähle hier das Audio-Interface (oder einen virtuellen Audio-Bus wie BlackHole 2ch) und das Ausgangs-Kanalpaar, über das die erzeugten Harmoniestimmen an MainStage oder dein Mischpult ausgegeben werden.“ |
| Externe Chords | „Empfängt live die gespielten Akkorde eines externen Keyboards über macOS Network MIDI (Ethernet empfohlen). Wertet physisch gehaltene Tasten und das Sustain-Pedal (CC 64) gemeinsam aus.“ *(angepasst: kein Personenname)* |
| Akkordbereich / Split-Punkt | „Begrenzt die Akkorderkennung auf den Bereich unterhalb der gewählten Trenn-Note (z. B. C4), damit Melodien und Soli in der rechten Hand die Harmonie nicht verändern.“ |
| Harmonie-Modus | „Legt fest, welchen Tönen die Stimmen folgen: ‚Nur Pentatonik‘ filtert auf pentatonische Intervalle, ‚Töne der Tonart halten‘ bleibt strikt diatonisch in der Skala, und ‚Hybrid‘ verbindet Akkord-Zieltöne mit fließenden Skalen-Durchgängen.“ |
| Akkord halten (Auto-Latch) | „Hält den zuletzt erkannten Akkord auch nach dem Loslassen von Tasten und Sustain-Pedal solange aktiv, bis ein neuer Akkord angeschlagen wird.“ |
| Totzone | „Legt den neutralen Ruhebereich (Bewegungstoleranz) um die Ausgangsposition der Schnecke/Bogenhand fest. Bewegungen innerhalb dieser Totzone werden ignoriert, damit normale Spielbewegungen oder leichtes Wippen keinen versehentlichen Trigger auslösen. Maximalwert für unruhige Bühnenbedingungen auf 25 % erweitert.“ *(angepasst: 25 %)* |
| Totzone mittig setzen | „Setzt den Nullpunkt genau in die Mitte des Kamerabilds – stell dich so hin, dass die Schnecke im Oval liegt. ‚Nullpunkt setzen‘ übernimmt dagegen die aktuelle Haltung der Schnecke.“ *(angepasst an die Festlegung)* |
| Schnecken-Geste (Chord Freeze) | „Steuert Chord Freeze per Kamera: Hebe die Geigenschnecke schnell nach oben, um den aktuellen Akkord einzufrieren; bewege sie schnell nach unten, um den Freeze wieder aufzuheben.“ |
| Farb-Marker-Pipette | „Klicke mit der Pipette auf einen farbigen Sticker an der Geigenschnecke (z. B. Neongrün oder Cyan), um die Gestenerkennung bei wechselndem Bühnenlicht zu stabilisieren.“ |
| Bogenhand-Erkennung | „Erkennt den Richtungswechsel deiner Bogenhand (Ab-/Aufstrich) über die Kamera: Wechselt die Tonhöhe, springt die Harmonie sauber mit; bleibt der Ton gleich, wird er beim Bogenwechsel rhythmisch neu angeschlagen (Re-Trigger).“ |
| Analyse-Ansicht & Zoom | „Oben (75 %) siehst du den Tonhöhenverlauf, unten (25 %) die synchronisierte Lautstärke-Waveform inklusive Schwellenwert-Linien. Zoome Zeit (X-Achse) und Tonhöhe/Pegel (Y-Achse) unabhängig voneinander.“ |
| Vollbildmodus | „Schaltet die Tracking- und Analyse-Ansicht für maximale Übersicht auf der Bühne in den Vollbildmodus (Shortcut: Ctrl + Cmd + F, Beenden mit ESC).“ |
| Automatisches Einmessen | „Führt dich durch 7 Spielschritte zur Kalibrierung von Geige und Harmonizer. Der Normalmodus stellt alle Regler automatisch ein; der Profimodus erstellt zusätzlich eine Diagnose-Aufnahme für Claude.“ |
| Abbrechen | „Bricht den aktuellen Vorgang ab und stellt den zuletzt gespeicherten Zustand der aktuellen Programmversion wieder her – ohne Rückfall auf ältere Versionen.“ |

Neue Elemente ohne Text in der PDF (Hybrid A/B, „Nur Pentatonik-Töne“, „Ton halten“, Instrumenten-
Profil, „Auf alle Presets anwenden“, X-/Y-Zoom) bekommen einen Text im selben Stil.
