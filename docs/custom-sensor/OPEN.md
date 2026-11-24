# Offene Punkte und Ideen

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
