# Offene Punkte und Ideen

## Batterie / USB
- **Ohne Akku wird ein Akku gemeldet.** Nur am USB zeigt die Firmware `battery hardware detected`, `usbPower=0`
  und Zufallswerte (49–94 %, 3,71–4,11 V). Darum steht in der Meldung `Akku ..%` statt `USB`. Vermutlich misst
  der ADC am offenen VBAT-Teiler (P0.31) nur Rauschen, und USB wird auf dem Kit nicht sicher erkannt.
  Ideen: mit echtem Akku gegenprüfen; Ursache in `Power.cpp` ansehen (nur lesen); ggf. eigene Plausibilitätsprüfung
  in `sendReport()` (z. B. stark springende Werte → "Akku ?"). Erst entscheiden, wenn ein Akku dran ist.
- Kanal-Sperre (öffentlicher Standardkanal) am Board noch nicht ausprobiert.

## HomeAssistant
- Das Gateway empfängt die Meldungen, aber die Automatisierung reagiert noch nicht. Sie muss auf die neuen Texte
  passen, z. B. beginnt mit `Falle jetzt OFFEN` / `Falle jetzt ZU`, ab Schritt 3 auch `Falle Status:` und
  `Falle Start:`. Teil hinter `|` (Akku/USB) nicht auswerten, der kann sich noch ändern.
  Beispiel mit Platzhaltern: `homeassistant-example.yaml`.
- **Lebenszeichen-Alarm (nach Schritt 3):** In HA alarmieren, wenn länger als **13 h** keine Meldung vom
  Fallen-Knoten kam (Statusmeldungen alle 12 h, plus Puffer). Z. B. Zeitstempel der letzten `Falle `-Meldung in
  einem `input_datetime` merken und per Zeit-Trigger prüfen. Achtung: Ohne gültige Zeit sendet der Knoten nur
  alle 6 h, das passt auch in die 13 h.

## Messaufbau
- **Logikanalyzer an D6 kann das Ergebnis verfälschen.** Der Pull-up hat nur 1 MOhm. Hat der Analyzer-Eingang
  einen Widerstand nach Masse (oft 100 kOhm–1 MOhm), liegt der Pin bei offenem Kontakt unter der HIGH-Schwelle
  und wird als ZU gelesen. Vor dem Messen prüfen, ob "offen" weiter als OFFEN erkannt wird.

## Energie
- **Strommessung der Gesamtschaltung**, bevor Batterie und Gehäuse gewählt werden. Der Funk bleibt im Leerlauf auf
  Empfang (mehrere mA). Das dominiert, nicht der Reed-Kontakt (~3 µA).
- **Akkulaufzeit messen** und danach Energiesparen neu bewerten.
- **Rolle SENSOR + `is_power_saving` im Dauerbetrieb?** Auf dem nRF52 heißt "Deep Sleep" in v2.7.26
  `delay(sds_secs)` + `NVIC_SystemReset()` (main-nrf52.cpp:466). Folgen: kein Interrupt-Handling währenddessen,
  Uhrzeit nach dem Reset weg. So ist es für den Fallenmelder ungeeignet. Alternativen prüfen.
- **CLIENT_MUTE** als Rolle prüfen: Fremde Pakete werden nicht weitergeleitet, das spart Sendezeit.

## Zustellsicherheit
- Aktuell `want_ack=false`, also keine Wiederholung bei Verlust.
- **Geprüft in v2.7.26:** Mit `want_ack=true` startet `ReliableRouter::send` (ReliableRouter.cpp:19) **auch bei
  Broadcasts** bis zu 3 Wiederholungen (`NUM_RELIABLE_RETX`, NextHopRouter.h:93). Sie stoppen, sobald ein Nachbar
  das Paket per LoRa weitersendet ("implicit ACK", ReliableRouter.cpp:42–62). Das bestätigt nur den **ersten Hop**,
  nicht die Ankunft beim Zuhause-Knoten.
  → Kandidat für v1.1: Ereignis-Meldungen mit `want_ack=true`.

## Gerätekonfiguration
- **Zeitzone im Gerät falsch:** `config.device.tzdef` = `CET-1CEST,M3.4.0/2:00:00,M10.4.0/3:00:00` (aus dem Boot-Log).
  `M3.4.0` heißt 4. Sonntag, die EU-Regel ist aber der **letzte** Sonntag. Richtig wäre `CET-1CEST,M3.5.0,M10.5.0/3`.
  In Jahren mit fünf Sonntagen im März oder Oktober springt die Uhr sonst eine Woche zu früh um, z. B. 2026 und 2030.
  Unser Modul ist nicht betroffen (eigene Berlin-Rechnung), nur Anzeigen. → In der App korrigieren. Andere Knoten
  prüfen?

## Funktionen später
- Zeitstempel "letztes Auslösen" (nicht in v1).
- Windsensor (UART), eigenes Board, gleiche Basisklasse.
- Weidezaun (2 Schwellen über Fototransistoren), eigenes Board, gleiche Basisklasse.
