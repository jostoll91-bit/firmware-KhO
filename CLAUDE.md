# Projekt: Custom Sensor-Modul für Meshtastic (Fallenmelder zuerst)

**Einstieg (z. B. nach /clear):** Aktueller Stand und nächster Schritt stehen in
`docs/custom-sensor/DEVLOG.md` und `docs/custom-sensor/PLAN.md`. Diese zuerst lesen.

## Kontext
- Fork von meshtastic/firmware, Branch `custom-sensor-module`, Basis-Tag v2.7.26.54e0d8d
- Hardware: Seeed XIAO nRF52840 + Wio-SX1262 Kit
- PlatformIO-Environment: `seeed_xiao_nrf52840_kit`
  - Hardware-Variante ausschließlich `seeed_xiao_nrf52840_kit`; abgeleitete Environments per `extends` sind nur
    für Modul-Flags erlaubt (z. B. `seeed_xiao_nrf52840_kit_wind` für den Windsensor). Das Fallen-Environment
    bleibt unverändert.
  - NIEMALS `seeed_xiao_nrf52840_btb` verwenden (falsche Pins, SX1262-Init-Fehler, Boot-Loop)
  - `_i2c`-Variante wird nicht gebraucht
- Bestehendes Netz (Zentral-Solarknoten, Zuhause-Knoten mit MQTT/HomeAssistant/ntfy)
  läuft stabil auf aktueller Firmware und darf nicht beeinträchtigt werden.
  => Keine Protobuf-Änderungen, keine Änderungen am Wire-Protokoll.
- Warum eigenes Modul: Das eingebaute Detection Sensor Module hat offene Bugs
  (GitHub #7541, #8977, Discussion #10924). Es wird NICHT gepatcht.
  `DetectionSensorModule.cpp/.h` nur lesen als Strukturvorlage, nie ändern.

## Ziel
Schlankes eigenes Modul als gemeinsame Basis für drei spätere Sensortypen
(jeweils eigenes Board): Fallenmelder (Reed), UART-Windsensor,
Weidezaun (2 Schwellen via Fototransistoren).
Jetzt NUR der Fallenmelder. Erfassung (sensorspezifisch) sauber trennen von
Senden/Zeitplan/Batterie (gemeinsam), aber nicht überengineeren.

## Anforderungen Fallenmelder v1
- Reed-Kontakt an D6 (Symbol verwenden, nicht die Zahl 43). Zuordnung bestätigt:
  D6 = P1.11 = GPIO 43 (variant.cpp, g_ADigitalPinMap; Boot-Log zeigt es auch)
- Gerätevoraussetzung (Pflicht): `position.gps_mode = NOT_PRESENT`, denn D6 ist in der
  Kit-Variante GPS_TX. Sonst schaltet sich das Modul mit LOG_ERROR ab.
- Externer 1-MOhm-Pull-up nach 3V3, daher pinMode(INPUT), KEIN INPUT_PULLUP
- Logik (`TRAP_CLOSED_MEANS_OPEN=1`, Standard):
  Magnet am Reed = Kontakt geschlossen = Pin LOW = Falle OFFEN (scharf, nichts gefangen) = Normalzustand.
  Magnet weg = Kontakt offen = Pin HIGH = Falle ZU (gefangen) oder Kabelbruch = Alarm.
  Ruhestrom: scharf/LOW ca. 3 uA, gefangen/HIGH 0.
  Texte: "OFFEN" bzw. "ZU (gefangen oder Kabelbruch)".
- Debounce 200 ms, Auslösung per Interrupt (kein Dauer-Polling)
- Bei Zustandswechsel sofort Meldung (Mindestabstand 10 s, dann nur Endzustand)
- Statusmeldung 2x täglich: 06:00 und 18:00 Europe/Berlin
- Ohne gültige Zeit: Statusmeldung alle 6 h ab Boot
- Zeitquelle: Solarknoten hat GPS, Zeit kommt per Mesh
- Inhalt: Zustand + Batteriestand (ohne Batterie: "USB" statt Prozent/Volt)
- Zeitstempel "letztes Auslösen": NICHT in v1
- Ausgabe: Textnachricht (TEXT_MESSAGE_APP), Broadcast, Primary-Channel
- Zielkonfiguration: Rolle SENSOR (Test zunächst is_power_saving=false;
  Akkulaufzeit später messen und Energiesparen neu bewerten)

## Arbeitsregeln
- Nur anfassen: neue Moduldateien, minimale Registrierung (Modules.cpp oder main.cpp,
  Stand in v2.7.26 prüfen), ggf. Build-Flags. Alles andere: erst fragen.
- Nichts über APIs von v2.7.26 annehmen. Immer im Quelltext nachsehen.
- Kleine Schritte, ein Git-Commit pro Schritt auf dem Branch.
- Code und Kommentare auf Englisch. Erklärungen an mich auf Deutsch, einfach gehalten
  (ich bin kein C++-Profi).
- Build: `pio run -e seeed_xiao_nrf52840_kit` (Fallenmelder), `pio run -e seeed_xiao_nrf52840_kit_wind` (Windsensor,
  ab Windsensor-Schritt 3). Beim Flashen das passende Env nehmen.
- Flash: `pio run -e seeed_xiao_nrf52840_kit -t upload --upload-port /dev/ttyACM0`
  Vor dem Flashen immer fragen (ich muss am Board sein).
- Keine blockierenden Befehle starten.
- Log lesen: `pio device monitor` geht hier nicht (braucht interaktives Terminal). Stattdessen:
  `~/.platformio/penv/bin/python -I docs/custom-sensor/tools/serlog.py /dev/ttyACM0 <sek> <datei>`
  (übersteht Resets; lange Mitschnitte im Hintergrund starten)
## Dokumentation
- Ordner `docs/custom-sensor/` (eigener Unterordner, damit es keine Konflikte mit
  Upstream-Dateien beim Rebase gibt). Schlank halten, Deutsch.
  - `DEVLOG.md`: pro Schritt ein kurzer Eintrag (Datum, was gemacht, Commit-Hash, Ergebnis)
  - `DECISIONS.md`: Entscheidungen mit Begründung (z. B. warum eigenes Modul,
    warum externer Pull-up, warum Berlin-Zeit im Code)
  - `OPEN.md`: offene Punkte und Ideen (z. B. Akkulaufzeit, Zeitstempel letztes Auslösen,
    Windsensor, Weidezaun)
  - `PLAN.md`: freigegebener Plan für die nächsten Schritte (ohne Chatverlauf umsetzbar)
- Nach JEDEM Schritt werden die vier Dateien aktualisiert und im selben Commit mit
  eingecheckt.
