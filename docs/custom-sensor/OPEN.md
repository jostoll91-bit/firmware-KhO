# Offene Punkte und Ideen

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
- **`want_ack=true` umgesetzt** (DECISIONS.md, Senden), am Board noch nicht getestet. Test: Meldung auslösen,
  im Log `Rx someone rebroadcasting for us` und `Generate implicit ack` suchen; ohne erreichbaren Nachbarn
  `Sending retransmission ... tries left=` und am Ende `Reliable send failed` (nur mit DEBUG-Log sichtbar).
- Echte Ende-zu-Ende-Bestätigung nur per Direktnachricht an den Zuhause-Knoten. Offen: Kommt eine
  Direktnachricht über MQTT bei HA an? Erst prüfen, falls nötig.

## Gerätekonfiguration
- **Zeitzone im Gerät falsch:** `config.device.tzdef` = `CET-1CEST,M3.4.0/2:00:00,M10.4.0/3:00:00` (aus dem Boot-Log).
  `M3.4.0` heißt 4. Sonntag, die EU-Regel ist aber der **letzte** Sonntag. Richtig wäre `CET-1CEST,M3.5.0,M10.5.0/3`.
  In Jahren mit fünf Sonntagen im März oder Oktober springt die Uhr sonst eine Woche zu früh um, z. B. 2026 und 2030.
  Unser Modul ist nicht betroffen (eigene Berlin-Rechnung), nur Anzeigen. → In der App korrigieren. Andere Knoten
  prüfen?
  **Laut Nutzer in der App korrigiert (2026-11-25).** Beim nächsten Boot-Log gegenprüfen.

## Funktionen später
- Zeitstempel "letztes Auslösen" (nicht in v1).
- Windsensor (UART), eigenes Board, gleiche Basisklasse.
- Weidezaun (2 Schwellen über Fototransistoren), eigenes Board, gleiche Basisklasse.
