# Offene Punkte und Ideen

## Wiederaufnahme (Stand 2026-11-25, pausiert für den Aufbau der Testschaltung)
**Nächster Schritt: Windsensor Schritt 4, Hardwaretest im Nur-Log-Modus mit 5-min-Takt** (Details in PLAN.md).
Die Firmware ist fertig (Commit `cb87c8daf`). Das Wind-Env sendet nichts, es loggt nur (`CUSTOM_SENSOR_LOG_ONLY=1`).

Vorbereitung (Nutzer):
1. Gerät: `position.gps_mode = NOT_PRESENT` (Pflicht, sonst schaltet sich das Modul mit LOG_ERROR ab),
   Primary-Channel privat.
2. RS485-Modul mit 3,3 V versorgen und die Spannung an Modul-TX gegen GND messen (≤ 3,3 V). Erst dann anschließen.
3. Verdrahtung: Modul-VCC → 3V3, GND → GND, Modul-TX → D7, Modul-RX → D6, A/B → Fahne.
4. Fahne vorerst direkt an 5 V (5V/USB-Pin des XIAO), noch ohne Step-up. EN an D0 bleibt im Test unbenutzt.
5. Pfeil am Sockel nach Nord, damit N/O/S/W gedreht werden können.
6. Dann melden: „Board ist angeschlossen“.

Bauen und flashen (Claude, erst nach der Meldung):
```
PLATFORMIO_BUILD_FLAGS="-DWIND_REPORT_INTERVAL_MS=300000" pio run -e seeed_xiao_nrf52840_kit_wind -t upload --upload-port /dev/ttyACM0
```
Log im Hintergrund mitschneiden:
`~/.platformio/penv/bin/python -I docs/custom-sensor/tools/serlog.py /dev/ttyACM0 <sek> <datei>`.
Erstes Fenster 60 s nach dem Boot, dann alle 5 min. Erwartete Log-Zeilen: `Wind: request 04 03 00 00 00 01 84 5f`,
`Wind: raw=…` bzw. `Wind: timeout, got N bytes: …`, am Ende `Wind: window done, x/60 valid, d=…`.
Ablauf: keine Antwort → TX/RX tauschen, dann A/B. Danach Fahne nacheinander auf N, O, S, W, Rohwerte notieren
(Skalierung 900 oder 90 bei Ost? Drehsinn? Nullpunkt?).

## Batterie / USB
- **Test mit echtem Akku zurückgestellt (kein Akku verfügbar).** Alles läuft im USB-Betrieb. Der Akkuwert im Text
  ist dabei Rauschen und nicht aussagekräftig. In HA ist der Fallen-Knoten deshalb von der Akku-Regel
  ausgeschlossen (mit echtem Akku die Ausschluss-Bedingung entfernen).
- **Ohne Akku wird ein Akku gemeldet.** Nur am USB zeigt die Firmware `battery hardware detected`, `usbPower=0`
  und Zufallswerte (49–94 %, 3,71–4,11 V). Darum steht in der Meldung `Akku ..%` statt `USB`. Vermutlich misst
  der ADC am offenen VBAT-Teiler (P0.31) nur Rauschen, und USB wird auf dem Kit nicht sicher erkannt.
  Mit echtem Akku gegenprüfen, ggf. Ursache in `Power.cpp` ansehen (nur lesen). Eine Änderung am Akku-Text
  (abschalten/plausibilisieren) ist vorerst verworfen, siehe DECISIONS.md "Verworfene Ideen".
- Kanal-Sperre (öffentlicher Standardkanal) am Board noch nicht ausprobiert. Zurückgestellt: Der Standardkanal
  müsste dafür erst eingerichtet werden, bei den eigenen Geräten ist er nicht vorgesehen.

## HomeAssistant
- Erledigt: Die Fallen-Automatisierung ist umgestellt, ntfy liefert 🚨 bei `ZU (gefangen oder Kabelbruch)` und ✅ bei
  `Falle jetzt OFFEN`. Beispiel mit Platzhaltern: `homeassistant-example.yaml`.
- **Lebenszeichen-Alarm:** Beispiel erstellt (`homeassistant-heartbeat-example.yaml`, >13 h ohne `Falle `-Meldung,
  Zeitpunkt in `input_datetime`, Wiederholung frühestens nach 6 h, "wieder da"-Meldung). In der echten HA-Instanz
  eingerichtet (Helfer in `configuration.yaml`, Automationen in `automations.yaml`). **Getestet
  2026-11-25:** Alarm kam, Wiederholung nach 6 h kam, Rückmeldung mit der 06:00-Statusmeldung.
- **Mehrere Fallen (erst ab der zweiten Falle umbauen):** Unterscheidung über `from` (Knotennummer).
  - Fallen-Meldung: *eine* Automation mit Liste `traps: {<Knotennummer>: "<Name>"}` in `variables`,
    Bedingung `from in traps`, Name per `traps[from]` in den ntfy-Text.
  - Lebenszeichen: braucht pro Falle zwei eigene `input_datetime`-Helfer. Als HA-**Blueprint** schreiben
    und pro Falle eine Instanz anlegen (Eingaben: Knotennummer, Name, die zwei Helfer).
  - Mit zwei echten Knoten testen.

## Messaufbau
- **Logikanalyzer an D6 kann das Ergebnis verfälschen.** Der Pull-up hat nur 1 MOhm. Hat der Analyzer-Eingang
  einen Widerstand nach Masse (oft 100 kOhm–1 MOhm), liegt der Pin bei offenem Kontakt unter der HIGH-Schwelle
  und wird als LOW gelesen, also als `OFFEN` (scharf) statt `ZU (gefangen oder Kabelbruch)`. Ein Fang würde
  dann übersehen. Vor dem Messen prüfen, ob "Magnet weg" weiter als ZU erkannt wird.
  **Am Board bestätigt** (DEVLOG, Test Fix + 6/18): Mit angeschlossenem Analyzer wurde "Magnet weg" gar nicht
  erkannt. Abhilfe für Messungen: Analyzer mit hochohmigem Eingang (≥10 MOhm) oder Pull-up zum Messen
  vorübergehend kleiner wählen.

## Energie
- **Strommessung der Gesamtschaltung**, bevor Batterie und Gehäuse gewählt werden. Der Funk bleibt im Leerlauf auf
  Empfang (mehrere mA). Das dominiert, nicht der Reed-Kontakt (~3 µA).
- **Akkulaufzeit messen** und danach Energiesparen neu bewerten.
- **Rolle SENSOR + `is_power_saving` im Dauerbetrieb?** Auf dem nRF52 heißt "Deep Sleep" in v2.7.26
  `delay(sds_secs)` + `NVIC_SystemReset()` (main-nrf52.cpp:466). Folgen: kein Interrupt-Handling währenddessen,
  Uhrzeit nach dem Reset weg. So ist es für den Fallenmelder ungeeignet. Alternativen prüfen.
- **CLIENT_MUTE** als Rolle prüfen: Fremde Pakete werden nicht weitergeleitet, das spart Sendezeit.

## Zustellsicherheit
- **`want_ack=true` umgesetzt und am Board bestätigt** (DEVLOG 2026-11-25, eine echte Wiederholung beobachtet).
  Log-Zeilen zum Nachsehen: `Generate implicit ack`, `Sending retransmission ... tries left=`, am Ende ohne Erfolg
  `Reliable send failed` (alles DEBUG).
- Echte Ende-zu-Ende-Bestätigung nur per Direktnachricht an den Zuhause-Knoten. Offen: Kommt eine
  Direktnachricht über MQTT bei HA an? Erst prüfen, falls nötig.

## Gerätekonfiguration
- **Zeitzone im Gerät falsch:** `config.device.tzdef` = `CET-1CEST,M3.4.0/2:00:00,M10.4.0/3:00:00` (aus dem Boot-Log).
  `M3.4.0` heißt 4. Sonntag, die EU-Regel ist aber der **letzte** Sonntag. Richtig wäre `CET-1CEST,M3.5.0,M10.5.0/3`.
  In Jahren mit fünf Sonntagen im März oder Oktober springt die Uhr sonst eine Woche zu früh um, z. B. 2026 und 2030.
  Unser Modul ist nicht betroffen (eigene Berlin-Rechnung), nur Anzeigen. → In der App korrigieren. Andere Knoten
  prüfen?
  **Laut Nutzer in der App korrigiert (2026-11-25).** Beim nächsten Boot-Log gegenprüfen.

## Windsensor
- **`-DCUSTOM_SENSOR_LOG_ONLY=1` im Wind-Env** in Schritt 5 entfernen, sonst sendet der Windsensor nie.
- **Skalierung, Drehsinn und Nullpunkt** im Hardwaretest (Schritt 4) klären: Fahne auf N/O/S/W, Rohwerte loggen.
  Das Datenblatt widerspricht sich (0,1° laut Tabelle, 1° laut Beispiel).
- **R2 auf dem Wio-SX1262-Board auslöten? (Entscheidung Nutzer, Schritt 6).** An D0 (EN des Step-ups) hängen
  R2 = 10 kΩ Pull-up nach 3V3 und Taster K1 nach GND. Folgen, wenn R2 bleibt:
  - Solange der Sensor aus ist (D0 LOW), fließen dauernd **~330 µA** durch R2 (~8 mAh/Tag).
  - Beim Boot/Reset ist D0 noch Eingang, R2 zieht EN hoch und der Step-up läuft kurz an.
  - Taster K1 drücken, während D0 HIGH ist (Messfenster), schließt den Ausgang gegen GND kurz.
  Vorschlag: R2 (0402) auslöten, 100 kΩ Pull-down an EN, ggf. 1 kΩ in Reihe zwischen D0 und EN.
- **Ruhestrom des RS485-Moduls** messen (es hängt dauernd an 3V3). Eine Power-LED ggf. auslöten.
- Step-up 3,7 → 5 V mit EN aussuchen und aufbauen (Schritt 6). Vorher klären, ob EN einen eigenen Pull-down hat.

## Funktionen später
- Zeitstempel "letztes Auslösen" (nicht in v1).
- Windsensor: in Arbeit, siehe WINDSENSOR.md und PLAN.md.
- Weidezaun (2 Schwellen über Fototransistoren), eigenes Board, gleiche Basisklasse.
