# Versionen

Der Abschnitt einer Version erscheint in der App unter Einstellungen, wenn man die Version auswählt.

## 2.0.0

- Neuer Name: VioLogic (vorher Archet). Neues Logo als App-Icon, im Fenster oben, als Start-Animation, im Fenster „Über VioLogic“ (Menü VioLogic) und als Wasserzeichen in der Tonhöhen-Grafik. Der MIDI-Ausgang heißt jetzt auch bei bestehenden Installationen „VioLogic“: in MainStage einmal neu zuordnen.
- Neues Farbkonzept aus dem Logo: Anthrazit, Karmesinrot und Silber. Schalter zeigen AN in Grün und AUS in Rot. Jedes Bedienelement erklärt sich beim Darüberfahren mit der Maus.
- Echtzeit-Grafik: Unter dem Tonhöhenverlauf läuft im selben Zeitraster die Lautstärkekurve mit den Linien von In- und Out-Schwelle. Zoom (+, −, Standard) für schnelle Läufe, Vibrato oder lange Phrasen.
- Harmonie: „Total Free Voicing“ steht oben in der Liste. Bei jeder Harmonieart öffnet sich die Intervall-Matrix mit ihren Stimmen; Töne lassen sich hinzufügen und abwählen, eine veränderte Harmonieart heißt dann zum Beispiel „Enger Dreiklang FV“. Diatonisch gilt jetzt auch im Total Free Voicing: eine Terz wird die Terz der Tonart.
- Audio-Harmonizer in einem eigenen Fenster: Formantkorrektur und High-Cut als Kurven über die Tonhöhe mit Stützpunkten (Standard: tiefe Saiten natürlich, E-Saite warm statt schrill), Drop-Voicing (in hohen Lagen sinkt die oberste Stimme eine Oktave), Ensemble-Entkopplung (Stimmen 8–22 ms versetzt, leicht verstimmt, ±35 % im Stereobild). Die Regler „Korpus-Erhalt“ und „Piezo-Glättung“ sind in den Kurven aufgegangen und starten mit der Standard-Kurve.
- Kamera: Die Geigenschnecke steuert jetzt zwei Controller (X und Y) wie ein XY-Pad. Die Ruheposition ist der Nullpunkt; innerhalb des silbernen Totzonen-Ovals bleiben die Werte stehen, außerhalb wandern sie, je nach Regelkurve fein oder schnell. Während der Bewegung zeigt ein Fadenkreuz die Werte, auch im Hauptfenster.
- Einstellungsfenster: Die Tooltips erscheinen jetzt auch dort.
- Behoben: War „Harmonie halten“ schon vor dem Ton eingeschaltet, blieb der Audio-Harmonizer für die ganze Dauer stumm. Jetzt klingen die Stimmen beim Spielen und danach weiter.
- Behoben: Zwei Versionen (etwa 1.5 und 2.0) konnten gleichzeitig laufen und jede Note doppelt senden; ein Klick auf das installierte Programm konnte den Wechsel zu einer anderen Version rückgängig machen.
- Behoben: In den Fenstern „Speichern unter“ und „Umbenennen“ waren die Knöpfe nicht als Knöpfe zu erkennen. Ein Preset aus 1.5, das in 2.0 neu gespeichert wird, behält seine Harmonizer-Werte für 1.5.

## 1.5.0

- Verzögerung: Die Erkennung war nie langsamer geworden, aber zwei Dinge konnten bremsen. Der Audio-Puffer fiel unter Umständen unbemerkt auf 512 Samples zurück (etwa 10 ms mehr); er wird jetzt bei jedem Start klein gehalten, solange man ihn nicht selbst in den Einstellungen wählt. Und die Harmonie setzte in 1.4.0 bis zu 29 ms nach dem Soloton ein; sie beginnt jetzt wieder im selben Augenblick.
- Werks-Preset „Lowest Latenz“ (im Preset-Menü ganz oben): stellt Ansprache, Glättungen, Filter und Puffer auf die kürzeste Verzögerung.
- Tracking-Filter (Spalte „Erkennung“): Sprungschutz gegen versehentlich angerissene Nachbarsaiten und einstellbare Überbrückung für Bogenwechsel, zusammen mit dem Oktavsprung-Schutz.
- Nachklingen: „Spiel-Überlappung“ lässt den vorherigen Ton beim Tonwechsel weiterklingen, „Ausklingen (Spielende)“ blendet den letzten Ton aus, statt ihn abzuschneiden.
- Harmonie-Offset: Die Harmoniestimmen lassen sich bis zu 50 % leiser als der Soloton ausgeben.
- Audio-Harmonizer (Knopf oben rechts): spielt die Harmoniestimmen mit dem eigenen Klang der Geige über den Audio-Ausgang, mit Korpus-Erhalt, Bogen-Variation, Vibrato-Kopplung, Stereo-Panorama, Piezo-Glättung, Pegel und Dry-Kill.
- Kamera-Steuerung (Knopf oben rechts): Die Höhe des Geigenhalses steuert einen Controller oder den Harmonie-Mix, Nicken und Kopfschütteln schalten Funktionen. Das Bild wird nur auf dem Mac ausgewertet und nicht gespeichert.

## 1.4.0

- Behoben: Auf dem Harmonie-Kanal kommen jetzt immer alle Stimmen an, auch der gespielte Ton. Die Auswahl „2 Töne“ gibt es nicht mehr.
- Behoben: Akkorde bleiben in der Tonart. Bisher wurde ein Akkord beim Gleiten und bei tief angesetzten Tönen einfach mitgebogen; so wurde aus C-Dur ein D-Dur mit F♯. Jetzt richtet sich der Akkord nach der Tonhöhe, die wirklich klingt, und wird neu gebildet (in C-Dur auf D also D-Moll).
- Harmonie-Modus: „Diatonisch“ (Akkorde aus der Tonart) oder „Chromatisch“ (immer dieselben Abstände).
- Skalen: zusätzlich Harmonisch Moll, Melodisch Moll, Phrygisch Dominant, Dur- und Moll-Pentatonik und Blues-Skala. In der Liste steht unter jedem Namen, wofür die Skala typisch ist.
- Voicings für die Harmonie: Dreiklang, Septakkord, Open, Power, Quart, Terz- und Sexten-Duett, fünf Oktav-Voicings und ein freies Voicing mit bis zu 10 Stimmen von −24 bis +24 Halbtönen.
- Freeze: „Solo halten“ und „Harmonie halten“ frieren Ton oder Akkord ein, während auf der anderen Spur weitergespielt wird. Beide lassen sich per MIDI schalten.
- Zwei Schwellen statt einer: Über der In-Schwelle beginnt ein Ton, erst unter der Out-Schwelle endet er.
- Presets: Alle Einstellungen lassen sich unter einem Namen speichern, laden, umbenennen und löschen (oben im Fenster). Ein Program Change lädt das Preset mit dieser Nummer; „Nächstes“ und „Vorheriges Preset“ lassen sich einem Fußschalter zuweisen.
- Oktavsprung-Schutz: Anstreich-Schutz und Oktavsprung-Sperre lassen sich einzeln schalten und in Millisekunden einstellen.
- MPE-Modus: jede Note auf einem eigenen Kanal (2–16) mit ±48 Halbtönen Pitch Bend.
- Dynamik: Ziel wählbar (CC11, CC1, CC2 oder CC1 + CC11), dazu eine Glättung von 0 bis 150 ms.
- Artikulation: erkennt gezupfte und gestrichene Tonanfänge und passt die Anschlagstärke an (Aus, Sanft, Stark).
- Die Statuszeile zeigt Abtastrate und Puffergröße des Audio-Eingangs.
- Aussehen: die Systemschrift von macOS (San Francisco) und Schalter in der Form der macOS-Bedienelemente.

## 1.3.0

- Neuer Name: Archet. Neues Icon.
- Neue Oberfläche: große Notenanzeige mit Stimmgerät, Verlauf der letzten Sekunden (was gespielt und was als MIDI gesendet wird), darunter alle Regler in der Reihenfolge des Signals.
- Taster „Erkennung an/aus“ (auch Leertaste). Aus beendet sofort alle Noten.
- Tonart: Grundton und Kirchentonart wählbar. „Nur Töne der Tonart“ lässt nur die Skala oder nur die Pentatonik durch. „Pentatonik-Ton halten“ lässt den letzten gültigen Ton weiterklingen, solange gespielt wird.
- Dreiklang: Zu jedem Ton erklingen Terz und Quinte aus der Tonart (wahlweise mit Grundton) auf einem zweiten MIDI-Kanal.
- Pitch-Bend-Bereich zusätzlich ±1 Halbton und ±0,5 / 0,4 / 0,3 / 0,2 Halbtöne.
- Vibrato-Glättung stufenlos einstellbar.
- MIDI-Steuerung: Unter Einstellungen lässt sich ein MIDI-Eingang wählen und jeder Regler einem Controller oder einer Note zuweisen, auch per „Lernen“.
- Alle Einstellungen werden sofort gespeichert und gelten für jede Version, auch beim Zurückwechseln. Das Fenster öffnet sich wieder dort, wo es zuletzt war.
- Der MIDI-Ausgang behält bei bestehenden Installationen seinen Namen „ViolinMIDI“, damit die Einrichtung in MainStage bleibt.
- Audio-Einstellungen auf Deutsch.

## 1.1.1

- Die Suche nach Updates lädt die Versionsliste jetzt als einfache Datei. Vorher konnte die Projektseite die Abfrage in geteilten Netzen zeitweise ablehnen.

## 1.1.0

- MIDI-Kanal wählbar (1 bis 16). Mit einem eigenen Kanal lässt sich die Geige in MainStage von anderen MIDI-Geräten trennen.
- Update-Knopf: Gibt es eine neuere Version, erscheint unten im Fenster „Update auf … installieren“. Ein Klick lädt sie und startet die App neu.
- Einstellungen: Liste aller Versionen. Jede frühere Version lässt sich mit „Diese Version verwenden“ wieder holen.
- Das Fenster zeigt in der Titelzeile, welche Version läuft.

## 1.0.0

- Erste Version: Tonhöhe der Geige als MIDI, drei Modi für das Tonziehen, laufende Lautstärke über Expression.
