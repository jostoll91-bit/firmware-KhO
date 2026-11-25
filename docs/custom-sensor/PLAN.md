# Plan Schritt 2 und 3

**Status:** Schritt 1–3 und der Fix der Zuordnung (`TRAP_CLOSED_MEANS_OPEN=1`) sind am Board bestätigt, Firmware mit
6/18 läuft; 06:00-Statusmeldung und HA-Lebenszeichen-Alarm im Betrieb bestätigt (2026-11-25). `want_ack=true` gebaut, Board-Test offen (OPEN.md, Zustellsicherheit). Offen: Akku/USB-Text (Test mit echtem Akku), weitere Punkte in OPEN.md. Für weitere Schritte gibt es
noch keinen freigegebenen Plan.

Freigegebener Plan, so aufgeschrieben, dass man ihn ohne den Chatverlauf umsetzen kann. Stand nach Schritt 1
(Commit `e1743618d`). Alle Fundstellen gelten für v2.7.26. Vor der Benutzung trotzdem im Quelltext gegenprüfen.

## Stand nach Schritt 1 (was schon da ist)
- `src/modules/CustomSensor/CustomSensorModule.{h,cpp}`: Basis `class CustomSensorModule : protected concurrency::OSThread`
  mit den virtuellen Hooks `setupSensor()`, `takeChangeEvent()`, `formatState(buf,len)`, `sensorWakeMs()`.
  `runOnce()` ruft beim ersten Lauf `setupSensor()` auf und loggt danach Wechsel nur (`(not sent yet)`). Die Rückgabe
  ist `min(sensorWakeMs(), CUSTOM_SENSOR_IDLE_INTERVAL_MS = 60 s)`. `CUSTOM_SENSOR_NAME` ist standardmäßig `"Falle"`.
- `src/modules/CustomSensor/TrapSensorModule.{h,cpp}`: D6, `INPUT`, ISR auf CHANGE, 200 ms Debounce über
  `volatile lastEdgeMs` und `volatile edgeCount` plus `handledEdgeCount`, GPS-Prüfung und Pin-Map-Log.
  `formatState()` liefert `"OFFEN"` (Pin LOW, scharf) oder `"ZU (gefangen oder Kabelbruch)"` (Pin HIGH).
  Zuordnung korrigiert nach Schritt 3 (`TRAP_CLOSED_MEANS_OPEN=1`), siehe DECISIONS.md.
- Registrierung: `src/modules/Modules.cpp` hinter `#if CUSTOM_SENSOR_TRAP` (Include und
  `trapSensorModule = new TrapSensorModule();` nach GenericThreadModule).
  Build-Flag `-DCUSTOM_SENSOR_TRAP=1` in `variants/nrf52840/seeed_xiao_nrf52840_kit/platformio.ini`.
- Die Sensor-Klasse bleibt in Schritt 2 und 3 **unverändert**. Alles Neue kommt in die Basis.

## Ablauf in `CustomSensorModule::runOnce()` (Ziel nach Schritt 3)
1. **Erster Lauf:** `setupSensor()`. Bei false → `disable()`. Danach wird die Startmeldung für in 60 s geplant
   (`bootReportDueMs = millis() + 60000`, Flag `bootReportPending`).
2. **Ereignis:** `takeChangeEvent()` gibt true zurück → `eventPending = true`.
   - Sind seit der letzten Ereignis-Meldung mindestens `CUSTOM_SENSOR_MIN_EVENT_GAP_MS` (10 000) vergangen, wird
     **sofort** gesendet: `sendReport("jetzt")`, danach `lastEventSentMs = millis()` und `eventPending = false`.
   - Sonst bleibt `eventPending` gesetzt und das Modul wacht genau dann auf, wenn die Pause um ist. Gesendet wird der
     **dann aktuelle** gemeldete Zustand (= Endzustand). Hat der Zustand inzwischen zurückgewechselt auf den zuletzt
     **gesendeten**, wird nichts gesendet. Dafür den zuletzt gesendeten Zustandstext oder einen Vergleichswert merken,
     z. B. `char lastSentState[48]` mit `strcmp`.
3. **Startmeldung** fällig → `sendReport("Start:")`.
4. **Statusmeldung** fällig (Zeitplan oder Fallback, siehe unten) → `sendReport("Status:")`.
5. **Rückgabe:** Minimum aus `sensorWakeMs()`, verbleibender Zeit bis zur Ereignis-Pause (falls `eventPending`),
   verbleibender Zeit bis zur Startmeldung und 60 s.
   - Die Abfrage `sensorWakeMs()` bleibt die **letzte** Anweisung vor dem `return`. Hintergrund: `OSThread::run()`
     (OSThread.cpp:96) setzt das Intervall nach `runOnce()` neu. Ein ISR genau dazwischen würde sonst bis zu 60 s
     verschluckt.
- Zeitvergleiche immer vorzeichenlos (`(uint32_t)(millis() - t) < x`, wie `Throttle::isWithinTimespanMs`,
  src/mesh/Throttle.cpp:32). Dann ist der `millis()`-Überlauf nach 49 Tagen kein Problem.

## Schritt 2: Senden
### `bool sendText(const char *text)` (private, Basis)
- **Kanal-Sperre zuerst:** `if (channels.isDefaultChannel(0))` (src/mesh/Channels.cpp:376: Standard-PSK `0x01`
  **und** Kanalname = Preset-Name, z. B. "LongFast") →
  `LOG_WARN("%s: NOT sent - primary channel is the public default channel. Configure a private primary channel (own name/PSK)!", CUSTOM_SENSOR_NAME); return false;`
- `meshtastic_MeshPacket *p = router->allocForSending();` (src/mesh/Router.cpp:192). Das setzt schon
  `to = NODENUM_BROADCAST`, `channel = 0` (Primary), hop_limit, id und `from`.
- `p->decoded.portnum = meshtastic_PortNum_TEXT_MESSAGE_APP; p->want_ack = false;`
- Payload: `size = min(strlen(text), meshtastic_Constants_DATA_PAYLOAD_LEN)` (233), dann
  `memcpy(p->decoded.payload.bytes, text, size)`.
- `service->sendToMesh(p, RX_SRC_LOCAL, true);` (src/mesh/MeshService.h:167, ccToPhone=true, damit es in der App
  erscheint).
- `LOG_INFO("%s: sent id=0x%08x '%s'", ...)`.
- Includes: `"MeshService.h"` (`service`), `"mesh/Router.h"` bzw. `"Router.h"` (`router`), `"mesh/Channels.h"`
  (`channels`), `"PowerStatus.h"` (`powerStatus`), `"gps/RTC.h"`. Pfade so wie in
  `src/modules/DetectionSensorModule.cpp` (nur lesen!).

### `void sendReport(const char *prefix)` (private, Basis)
- Text: `"<NAME> <prefix> <state> | <battery>"`, ASCII, keine Umlaute.
- Batterie (src/PowerStatus.h:50–65, `powerStatus` wird alle 20 s vom Power-Thread aktualisiert):
  - `powerStatus && powerStatus->getHasBattery()` → `"Akku %u%% %d.%02dV"` aus `getBatteryChargePercent()` und
    `getBatteryVoltageMv()`.
  - sonst `"USB"`.
- Beispiele:
  - `Falle jetzt ZU (gefangen oder Kabelbruch) | Akku 87% 4.05V`  (Alarm)
  - `Falle jetzt OFFEN | USB`  (wieder scharf)
  - `Falle Status: OFFEN | Akku 87% 4.05V`
  - `Falle Start: OFFEN | Akku 87% 4.05V`
- Puffer z. B. `char msg[120]` mit `snprintf`.

### Umsetzung Schritt 2 (so gebaut, kleine Präzisierungen zum Plan)
- `lastSentState` wird beim Init mit dem Boot-Zustand gefüllt (es wird beim Boot noch nichts gesendet). Das ist
  nur der Vergleichswert. Der **erste** Wechsel nach dem Boot wird immer sofort gesendet (vor der ersten Meldung
  gibt es keine Pause). Nach dem Boot bleibt also kein Wechsel unbemerkt.
- Gleicht der Zustand nach der Pause dem zuletzt gesendeten, wird nichts gesendet, und die 10-s-Pause startet
  **nicht** neu. Der nächste echte Wechsel geht dann sofort raus.
- Die Pause startet auch, wenn die Kanal-Sperre greift. So kommt die Warnung höchstens alle 10 s.
- In Schritt 3 die Rückgabe in `runOnce()` um die Startmeldung erweitern, `sensorWakeMs()` bleibt zuletzt.

### Umfang Schritt 2
- `sendText`, `sendReport`, Kanal-Sperre, Ereignis-Meldung mit 10-s-Mindestabstand und Endzustand, Akku/USB-Text.
- Neues Build-Flag mit Default im Header: `CUSTOM_SENSOR_MIN_EVENT_GAP_MS 10000`.
- Startmeldung und Zeitplan kommen erst in Schritt 3.

### Test Schritt 2 (am Board, vorher fragen)
- Primary-Channel ist privat → Wechsel erscheinen als Text in der App und beim Zuhause-Knoten.
- Wedeln (vorher 6 Wechsel in 6 s im Log) → im Mesh höchstens 2 Meldungen: die erste sofort, der Endzustand nach
  ≥10 s. Endet das Wedeln im Ausgangszustand, kommt nur die erste.
- Mit Akku ohne USB → `Akku ..%`. Nur am USB ohne Akku → `USB`.

## Schritt 3: Zeitplan
### Build-Flags (Defaults im Header)
`CUSTOM_SENSOR_STATUS_HOUR_A 6`, `CUSTOM_SENSOR_STATUS_HOUR_B 18` (Bedingung A < B),
`CUSTOM_SENSOR_FALLBACK_INTERVAL_MS (6 * 3600 * 1000UL)`, `CUSTOM_SENSOR_BOOT_REPORT_DELAY_MS 60000`.

### Zeit holen
- `uint32_t utc = getValidTime(RTCQualityFromNet);` (src/gps/RTC.h:53). Das ist **0**, solange die Zeit nicht
  gültig ist. `local` immer `false` lassen, wir rechnen selbst. Mesh-Zeit kommt nur aus Positions-Paketen mit
  `location_source >= LOC_INTERNAL` (PositionModule.cpp:130–146). Bei uns schickt die der Solarknoten mit GPS.

### Berlin-Zeit (`static int32_t berlinOffsetSecs(uint32_t utc)`)
- MEZ = +3600. MESZ = +7200 vom **letzten Sonntag im März 01:00 UTC** bis zum **letzten Sonntag im Oktober
  01:00 UTC**.
- Umsetzung: `gmtime_r` für das Jahr, dann für den 31.03. bzw. 31.10. 01:00 UTC per `mktime`-freier Rechnung
  (Tage seit Epoch) den Wochentag bestimmen und auf Sonntag zurückgehen. Kein `setenv("TZ")`, keine globale
  Änderung. `config.device.tzdef` wird **nicht** benutzt (im Gerät steht zudem `M3.4.0`, siehe OPEN.md).
- `local = utc + berlinOffsetSecs(utc)`, `day = local / 86400`, `sec = local % 86400`.

### Slot-Logik
- `A = HOUR_A*3600`, `B = HOUR_B*3600`.
- `sec >= B` → slot `day*2+1`; `sec >= A` → slot `day*2`; sonst slot `(day-1)*2+1`. Das ist jeweils der **zuletzt
  vergangene** Termin.
- `int32_t lastSlot = -1` (-1 heißt: Zeit war noch nie gültig, oder wir kommen aus dem Fallback).
  - Gültige Zeit und `lastSlot == -1` → `lastSlot = slot` (nur merken, **nichts nachholen**), Log
    `"time valid, schedule active"`.
  - `slot > lastSlot` → `sendReport("Status:")`, `lastSlot = slot`, `lastStatusMs = millis()`.
  - `slot < lastSlot` (Uhr sprang zurück) → nur `lastSlot = slot`.
- Sommer/Winterzeit-Umstellung (02/03 Uhr) berührt 06/18 nicht.
- Weckintervall bleibt maximal 60 s. Der Termin wird also höchstens ~60 s zu spät erkannt, ein genauer Wecker ist
  nicht nötig.

### Fallback ohne gültige Zeit
- `utc == 0` → `lastSlot = -1`. Sind seit `lastStatusMs` (Startwert: Boot = 0) mindestens 6 h vergangen,
  → `sendReport("Status:")` und `lastStatusMs = millis()`.
- Log einmalig beim Wechsel: `"no valid time, fallback every 6 h"`.

### Startmeldung
- 60 s nach `setupSensor()` einmal `sendReport("Start:")`, damit das Mesh sich vorher eingeschwungen hat.
  `lastStatusMs = millis()` setzen, damit der 6-h-Fallback ab dort zählt.

### Umsetzung Schritt 3 (so gebaut, Präzisierungen zum Plan)
- Datumsrechnung in `BerlinTime.h` statt `gmtime_r` (header-only, auf dem PC testbar). Ergebnis siehe DEVLOG.
- Reihenfolge in `runOnce()`: Setup → Ereignis → `handleSchedule()` (Start, dann Zeitplan/Fallback) → Rückgabe.
  `sensorWakeMs()` bleibt die letzte Abfrage.
- **Zeitquelle:** Gültig ist die Zeit auch, wenn die **App** verbunden war (`set_time_only` → `RTCQualityNTP`,
  AdminModule.cpp:442), nicht nur durch GPS-Positionen vom Solarknoten. Für den Test heißt das: Wird die App
  verbunden, ist die Zeit sofort gültig. Den Fallback-Test deshalb **ohne App-Verbindung** machen.
- Status- und Startmeldung setzen ebenfalls `lastSentState`. Läuft gerade eine 10-s-Pause und die
  Statusmeldung enthält schon den neuen Zustand, kommt danach keine doppelte Ereignis-Meldung.

### Test Schritt 3 (am Board, vorher fragen)
- Log nach dem Boot: `no valid time, fallback every 6 h`, nach 60 s die Startmeldung im Mesh.
- Warten auf eine GPS-Position vom Solarknoten → `time valid, schedule active`. Danach sind die Log-Zeitstempel
  nicht mehr `??:??:??`.
- **Zeitplan-Test:** Einmal mit `-DCUSTOM_SENSOR_STATUS_HOUR_A=<nächste volle Stunde Berlin>` und
  `-DCUSTOM_SENSOR_STATUS_HOUR_B=<übernächste>` bauen. Das Flag nur **temporär** auf der Kommandozeile setzen, z. B.
  `PLATFORMIO_BUILD_FLAGS="-DCUSTOM_SENSOR_STATUS_HOUR_A=14 -DCUSTOM_SENSOR_STATUS_HOUR_B=15" pio run -e seeed_xiao_nrf52840_kit -t upload --upload-port /dev/ttyACM0`.
  Erwartet: Statusmeldung ≤60 s nach der vollen Stunde, und zwar genau eine. Danach normal ohne die Flags neu flashen.
- Fallback-Test optional mit `-DCUSTOM_SENSOR_FALLBACK_INTERVAL_MS=300000` (5 min) ohne Zeitquelle.

## Allgemeines Vorgehen
- Nach jedem Schritt: `pio run -e seeed_xiao_nrf52840_kit`, Flash-/RAM-Zahlen ins DEVLOG (Vergleich zum Vorschritt;
  die Baseline steht dort), DEVLOG, DECISIONS, OPEN und PLAN aktualisieren, **ein** Commit.
- **Vor dem Flashen fragen.** Log mitschneiden mit `docs/custom-sensor/tools/serlog.py`
  (`pio device monitor` geht nicht ohne interaktives Terminal).
- Baseline-Messung ohne Modul: `PLATFORMIO_BUILD_DIR=<scratch> PLATFORMIO_BUILD_FLAGS="-UCUSTOM_SENSOR_TRAP" pio run -e seeed_xiao_nrf52840_kit`.
  Das eigene Build-Verzeichnis sorgt dafür, dass `.pio/build` unberührt bleibt.
- Gerätekonfiguration für Tests: `position.gps_mode = NOT_PRESENT` (Pflicht), Rolle SENSOR,
  `power.is_power_saving = false`, Primary-Channel privat.
