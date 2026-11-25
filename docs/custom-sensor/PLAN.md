# Plan Windsensor (Schritte 1–6)

**Status:** Freigegeben am 2026-11-25. Schritte 1–3 erledigt (Doku, Basis, Windmodul im Log-Modus), als Nächstes kommt Schritt 4 (Hardwaretest).
Der Fallenmelder ist fertig und am Board getestet. Sein Plan (Schritte 1–3) steht in der Git-Historie
(PLAN.md bis Commit `dd608a154`). Was gebaut wurde, steht in DECISIONS.md und DEVLOG.md.

Sensordaten, Verdrahtung und das festgelegte Textformat stehen in `WINDSENSOR.md`. Der Plan ist so geschrieben, dass
man ihn ohne den Chatverlauf umsetzen kann. Alle Fundstellen gelten für v2.7.26. Vor der Benutzung im Quelltext
gegenprüfen.

## Grundlagen (geprüft)
- **Serial1:** Auf nRF52 benutzt nur der GPS-Treiber Serial1 (`src/gps/GPS.cpp:42`). Er wird nur angelegt, wenn
  `gps_mode != NOT_PRESENT` ist (`src/main.cpp:890`). Das Serial-Modul nimmt auf nRF52 `Serial2`
  (`SERIAL_PRINT_PORT` = 2, SerialModule.cpp:66), nicht Serial1.
  Die Pins von Serial1 sind schon D6 (TX) und D7 (RX) (`PIN_SERIAL1_TX/RX` = `GPS_TX/RX_PIN`, variant.h:195).
  → `Serial1.begin(9600)` ist 8N1 (Adafruit-Core `Uart::begin`). Nach jedem Messfenster `Serial1.end()` aufrufen,
  sonst zieht der UART laut Core-Kommentar ~900 µA.
- **D0 als EN:** Am Header ist nur D0 frei. In der Standard-Kit-Variante ist `BUTTON_PIN` **nicht** definiert
  (variant.h:112 auskommentiert, nur `_i2c` und `_btb` setzen ihn). Darum gibt es keinen `UserButtonThread`
  (InputBroker.cpp:148/172). `config.device.button_gpio` wird nur innerhalb von `#ifdef BUTTON_PIN` benutzt.
  D0 ist sonst nur `PIN_GPS_STANDBY`, und das benutzt nur das GPS-Objekt, das es bei NOT_PRESENT nicht gibt.
  Hardware: R2 = 10 kΩ Pull-up nach 3V3 und Taster K1 nach GND auf dem Wio-Board, siehe OPEN.md.

## Schritt 1: Nur Doku ✅
`WINDSENSOR.md`, dieser Plan, CLAUDE.md-Regel zu abgeleiteten Envs, DEVLOG/DECISIONS/OPEN.

## Schritt 2: Basis um Taktmodus erweitern (Fallenmelder bleibt unverändert) ✅
- **Schutz:** In `CustomSensorModule.h/.cpp` wird `#if CUSTOM_SENSOR_TRAP` zu
  `#if CUSTOM_SENSOR_TRAP || CUSTOM_SENSOR_WIND`. Dazu kommt
  `#if CUSTOM_SENSOR_TRAP && CUSTOM_SENSOR_WIND` → `#error`, damit nicht beide gleichzeitig gebaut werden
  (ein `static_assert` geht nicht, weil ein nicht definiertes Flag dort kein gültiger Ausdruck ist).
  `CUSTOM_SENSOR_NAME` ist standardmäßig `"Wind"`, wenn `CUSTOM_SENSOR_WIND` gesetzt ist, sonst `"Falle"`.
- **Neue virtuelle Hooks.** Die Standardwerte lassen den Fallenmelder genau so laufen wie bisher:
  - `virtual uint32_t reportIntervalMs() { return 0; }`: 0 heißt Fallenmodus (Start, 06/18, 6-h-Fallback, wie
    bisher). Ein Wert > 0 heißt Taktmodus: `handleSchedule()` läuft nicht, stattdessen `handlePeriodic()`.
  - `virtual void startMeasurement() {}`: Beginn des Messfensters.
  - `virtual int32_t runMeasurement() { return 0; }`: ms bis zum nächsten Aufruf, **0 = fertig**.
  - `virtual void formatReport(char *buf, size_t len, const char *battery) {}`: der komplette Meldungstext.
- **`handlePeriodic()`** (private):
  - `nextCycleMs` startet bei `Setup + CUSTOM_SENSOR_BOOT_REPORT_DELAY_MS` (60 s).
  - Ist ein Zyklus fällig und läuft gerade keine Messung: `nextCycleMs += reportIntervalMs()`. Liegt das schon in
    der Vergangenheit, stattdessen `millis() + Intervall`. Dann `startMeasurement()` aufrufen und `measuring = true`
    setzen. Danach sofort `runMeasurement()`.
  - Während `measuring`: Ist `measureDueMs` erreicht, `r = runMeasurement()` aufrufen. Ist `r == 0`, dann
    `measuring = false` und die Meldung senden. Sonst `measureDueMs = millis() + r`.
  - Der Takt zählt ab Fensterbeginn, so verschiebt er sich nicht. Die Meldung kommt ~122 s nach Fensterbeginn.
- **Rückgabe `runOnce()`:** Zusätzlich das Minimum mit der Restzeit bis `nextCycleMs` bzw. `measureDueMs` bilden.
  `sensorWakeMs()` bleibt die letzte Abfrage.
- **Batterie:** Neue protected Hilfsfunktion `batteryPercentText(char *buf, size_t len)` liefert `"87%"` oder `"USB"`
  (gleiche Quelle und gleiche `#if`-Bedingung wie in `sendReport`). Der Text des Fallenmelders bleibt unverändert.
- **Nur-Log-Modus:** Neues Flag `CUSTOM_SENSOR_LOG_ONLY`, Standard 0. Bei 1 loggt `sendText()` nur
  `"%s: LOG ONLY, not sent '%s'"` und gibt true zurück. Bei 0 bleibt der Code genau wie bisher (`#if`).
- `sendText()` wird `protected`. Kanal-Sperre und `want_ack` gelten damit auch für den Windsensor.
- Danach den **Fallenmelder bauen** und Flash/RAM vergleichen (vorher 725 896 B / 102 204 B).
  Erwartet: wenige Bytes mehr, keine Änderung im Ablauf. Ergebnis: +448 B, RAM gleich (DEVLOG).

## Schritt 3: `WindSensorModule` (nur Log) + Environment ✅
- **Neues Environment** in `variants/nrf52840/seeed_xiao_nrf52840_kit/platformio.ini`:
  ```
  [env:seeed_xiao_nrf52840_kit_wind]
  extends = env:seeed_xiao_nrf52840_kit
  board_level = extra
  build_flags = ${env:seeed_xiao_nrf52840_kit.build_flags}
    -UCUSTOM_SENSOR_TRAP
    -DCUSTOM_SENSOR_WIND=1
  ```
  Nur Modul-Flags, keine Pin-Änderungen. Das Fallen-Env bleibt unverändert. Prüfen, dass `-U` nach `-D` wirkt:
  In der Wind-ELF dürfen keine TrapSensor-Symbole stehen (`nm`).
- **Registrierung** in `src/modules/Modules.cpp`: `#if CUSTOM_SENSOR_WIND` mit Include und
  `windSensorModule = new WindSensorModule();` direkt beim Fallenmelder.
- **Neue Dateien** `src/modules/CustomSensor/WindSensorModule.{h,cpp}`, Schutz `#if CUSTOM_SENSOR_WIND`.
- **Build-Flags (Standardwert im Header):** `WIND_EN_PIN D0`, `WIND_MODBUS_ADDR 0x04`, `WIND_RAW_PER_DEGREE 10`,
  `WIND_WARMUP_MS 2000`, `WIND_WINDOW_MS 120000`, `WIND_SAMPLE_MS 2000`, `WIND_RESPONSE_TIMEOUT_MS 200`,
  `WIND_REPORT_INTERVAL_MS (30 * 60 * 1000UL)`. Im Wind-Env in Schritt 3 zusätzlich `-DCUSTOM_SENSOR_LOG_ONLY=1`.
- **Sperre beim Kompilieren:** `#if defined(BUTTON_PIN) && (BUTTON_PIN == WIND_EN_PIN)` → `#error`. Dann wäre D0
  ein Taster, siehe Grundlagen.
- **`setupSensor()`:**
  - Ist GPS nicht `NOT_PRESENT`: `LOG_ERROR("Wind: DISABLED - D6/D7 are the GPS UART (Serial1) ...")` und false.
  - Sonst `pinMode(WIND_EN_PIN, OUTPUT); digitalWrite(WIND_EN_PIN, LOW);` (HIGH = Sensor an).
  - Pin-Map loggen wie beim Fallenmelder (EN, TX, RX).
  - `takeChangeEvent()` gibt immer false zurück. `formatState()` liefert den letzten Wert, nur fürs Init-Log.
- **Zustandsmaschine** (`startMeasurement`/`runMeasurement`, nie ein langes `delay`):
  1. `startMeasurement()`: EN HIGH, `Serial1.begin(9600)`, Zähler und Summen auf 0, `windowStartMs` merken,
     Zustand WARMUP. Rückgabe von `runMeasurement()`: `WIND_WARMUP_MS`.
  2. REQUEST: Empfangspuffer leeren (`while (Serial1.available()) Serial1.read();`), dann 8 Bytes
     `04 03 00 00 00 01 CRClo CRChi` senden (CRC im Code berechnet, ergibt `84 5f`). Zustand WAIT, Rückgabe 20.
  3. WAIT: Verfügbare Bytes in einen Puffer (max. 16) lesen. Bei 7 Bytes, oder nach `WIND_RESPONSE_TIMEOUT_MS`,
     die Antwort auswerten. Sonst Rückgabe 20.
  4. Auswerten: Adresse, Funktion 0x03, Bytezahl 2, CRC (0xA001, Low-Byte zuerst), dann
     `raw <= 360 * WIND_RAW_PER_DEGREE`. Ist alles gültig:
     `deg = raw / (float)WIND_RAW_PER_DEGREE`, `sumSin += sinf(rad)`, `sumCos += cosf(rad)`, `valid++`.
  5. Nächste Anfrage im 2-s-Raster ab Fensterbeginn. Sind seit Fensterbeginn `WIND_WARMUP_MS + WIND_WINDOW_MS`
     vergangen: EN LOW, `Serial1.end()`, Ergebnis berechnen, Rückgabe 0.
  - Ergebnis: Bei `valid == 0` ist es `ERR`. Sonst `atan2f(sumSin, sumCos)` in Grad, auf 0..359 bringen,
    ganzzahlig runden, 360 → 0.
- **`formatReport`:** `"WIND d=%d bat=%s"` bzw. `"WIND d=ERR bat=%s"`.
- **Logs:**
  - Pro Antwort: `Wind: raw=%u (0x%04x)`, oder `Wind: timeout, got %u bytes: <hex>`, `Wind: CRC bad: <hex>`,
    `Wind: out of range raw=%u`.
  - Pro Fenster: `Wind: window done, %u/%u valid, d=%d`.
- Danach **beide Envs bauen** und die Größe messen (Falle muss gleich bleiben wie nach Schritt 2).

## Schritt 4: Erster Hardwaretest (nur nach „Board ist angeschlossen“)
- **Aufbau:** Fahne direkt an 5 V (USB-Pin des XIAO), noch ohne Step-up. RS485-Modul an 3V3. Modul-TX → D7,
  Modul-RX → D6, A/B zur Fahne. Vorher die TX-Spannung des Moduls messen (≤ 3,3 V).
- **Gerät:** `gps_mode = NOT_PRESENT`, Primary-Channel privat.
- **Flash:** `pio run -e seeed_xiao_nrf52840_kit_wind -t upload --upload-port /dev/ttyACM0`, im Log-Modus.
  Für schnellere Fenster einmalig mit `PLATFORMIO_BUILD_FLAGS="-DWIND_REPORT_INTERVAL_MS=300000"` (5 min) bauen.
- **Mitschnitt:** `~/.platformio/penv/bin/python -I docs/custom-sensor/tools/serlog.py /dev/ttyACM0 <sek> <datei>`
  im Hintergrund laufen lassen.
- **Ablauf:**
  - Kommt keine Antwort (nur Timeouts mit 0 Bytes): erst TX/RX tauschen, dann A/B.
  - Kommen Bytes, aber der CRC ist falsch: Baudrate oder Adresse prüfen und den Hex-Dump ansehen.
  - **Fahne nacheinander auf N, O, S, W** stellen, jeweils ein paar Werte lang festhalten, Rohwert notieren.
    Erwartet bei 0,1°: N≈0/3600, O≈900, S≈1800, W≈2700. Prüfen:
    - **Skalierung:** 900 oder 90 bei Ost?
    - **Drehsinn:** O vor S im Uhrzeigersinn steigend?
    - **Nullpunkt:** N ≈ 0, wenn der Pfeil am Sockel nach Nord zeigt.
- Ergebnis ins DEVLOG, die Entscheidung zur Skalierung ins DECISIONS.

## Schritt 5: Senden einschalten, Skalierung festlegen
- `WIND_RAW_PER_DEGREE` nach dem Testergebnis setzen. Passen Drehsinn oder Nullpunkt nicht, minimal mit eigenem
  Flag korrigieren (z. B. `WIND_OFFSET_DEG`), nur wenn der Test das verlangt.
- `-DCUSTOM_SENSOR_LOG_ONLY=1` aus dem Wind-Env entfernen.
- Board-Test: `WIND d=… bat=…` erscheint in der App und kommt in HA an. Einmal die Fahne abziehen →
  `WIND d=ERR bat=…`.

## Schritt 6: Versorgung (später)
- Step-up 3,7 → 5 V mit EN an D0. Entscheidung R2 auslöten ja/nein, siehe OPEN.md.
- Ruhestrom des RS485-Moduls und der Gesamtschaltung messen.

## Allgemeines Vorgehen
- Nach jedem Schritt bauen:
  - Falle: `pio run -e seeed_xiao_nrf52840_kit`
  - Wind: `pio run -e seeed_xiao_nrf52840_kit_wind`
  - Flash/RAM (`arm-none-eabi-size`, Flash = text + data) vorher und nachher ins DEVLOG.
- DEVLOG, DECISIONS, OPEN und PLAN aktualisieren, **ein** Commit pro Schritt.
- **Vor dem Flashen fragen**, erst nach „Board ist angeschlossen“.
- Zeitvergleiche immer vorzeichenlos (`(uint32_t)(millis() - t)`), wegen des `millis()`-Überlaufs.
