# Entscheidungen – Custom Sensor-Modul (Fallenmelder v1, Windsensor)

Kurz und mit Begründung. Basis: Firmware v2.7.26, Board `seeed_xiao_nrf52840_kit`.

## Grundsätzliches
- **Eigenes Modul statt Detection Sensor Module patchen.** Das eingebaute Modul hat offene Bugs
  (#7541, #8977, Discussion #10924) und pollt alle 100 ms. Unser Modul liegt getrennt in `src/modules/CustomSensor/`
  und lässt Upstream-Dateien in Ruhe (einfacher Rebase).
- **Keine Protobuf- oder Wire-Änderung.** Das bestehende Netz soll unverändert weiterlaufen.
- **Konfiguration per Build-Flags**, keine neue Config-Struktur. Eingeschaltet wird mit `-DCUSTOM_SENSOR_TRAP=1`
  im Env `seeed_xiao_nrf52840_kit`.
- **Trennung:** `CustomSensorModule` (gemeinsam: Thread, Senden, Zeitplan, Batterie) und `TrapSensorModule`
  (nur Erfassung des Reed-Kontakts). Wind- und Weidezaunsensor sollen später dieselbe Basis nutzen.
- **Reiner `OSThread` statt `SinglePortModule`.** Wir senden nur. Mit Port TEXT_MESSAGE_APP würde ein
  SinglePortModule jede eingehende Textnachricht "wollen".

## Hardware / Pin
- **D6 statt "43".** `g_ADigitalPinMap[6] = 43` = P1.11. An `pinMode`/`attachInterrupt` gehört die
  Arduino-Nummer (D6). `pinMode(43)` macht nichts, weil 43 >= `PINS_COUNT` (33) ist.
- **GPS muss `NOT_PRESENT` sein.** D6 ist in der Default-Variante `GPS_TX_PIN`. Bei NOT_PRESENT wird kein GPS
  angelegt (main.cpp:890). Sonst schaltet sich das Modul mit einer Fehlermeldung ab.
- **Externer 1-MOhm-Pull-up nach 3V3, `pinMode(INPUT)`.** Der interne Pull-up (~13 kOhm) würde bei geschlossenem
  Kontakt ~250 µA ziehen. Mit 1 MOhm: Falle scharf (Kontakt zu, Pin LOW) ≈ 3 µA, gefangen (Pin HIGH) = 0.
- **Logik (korrigiert, `TRAP_CLOSED_MEANS_OPEN=1` ist Standard): Magnet am Reed = Kontakt geschlossen = Pin LOW =
  Falle OFFEN (scharf). Magnet weg = Kontakt offen = Pin HIGH = Falle ZU (gefangen) oder Kabelbruch.**
  Texte: `OFFEN` bzw. `ZU (gefangen oder Kabelbruch)`. Ein Kabelbruch löst also denselben Alarm aus wie ein Fang
  und fällt sicher auf. Umkehren mit `-DTRAP_CLOSED_MEANS_OPEN=0`. Bis Schritt 3 war die Zuordnung vertauscht
  (Standard 0), siehe DEVLOG.
- **Interrupt (CHANGE) + 200 ms Debounce** statt Polling. Ein Zustand zählt erst, wenn er 200 ms stabil war.
  Eine Meldung gibt es nur, wenn er sich vom zuletzt gemeldeten Zustand unterscheidet.
- **Flankenzähler `edgeCount` statt `bool edgePending`** zwischen ISR und Thread. Bei einem bool müsste der Thread
  das Flag zurücksetzen, und eine Flanke genau in diesem Moment ginge verloren. Beim Zähler schreibt nur der ISR.
  Der Thread merkt sich `handledEdgeCount`. `edgeCount` und `lastEdgeMs` sind `volatile`.
- **Restrisiko, bewusst akzeptiert:** `OSThread::run()` setzt nach `runOnce()` das Intervall (OSThread.cpp:96).
  Ein ISR in den paar Takten zwischen `sensorWakeMs()` und diesem Setzen würde erst beim nächsten Weckzeitpunkt
  bearbeitet (≤60 s). Der Zustand geht nicht verloren, die Meldung kommt nur verspätet.

## Senden
- **Textnachricht (TEXT_MESSAGE_APP), Broadcast, Primary-Channel**, damit der Zuhause-Knoten ohne Anpassung
  weiterleitet.
- **Mindestabstand 10 s** zwischen Ereignis-Meldungen. Danach wird nur der Endzustand gemeldet, damit ein
  flatternder Kontakt das Mesh nicht flutet. Verglichen wird mit dem **zuletzt gesendeten** Zustandstext
  (`lastSentState`, beim Boot = Boot-Zustand). Endet das Flattern im schon gemeldeten Zustand, wird nichts
  gesendet und die Pause startet nicht neu.
- **Kanal-Sperre:** Ist der Primary-Channel der öffentliche Standardkanal (Standard-PSK + Preset-Name, z. B.
  "LongFast"), wird nicht gesendet. Es gibt eine deutliche Warnung im Log.
- **`want_ack=true` für alle Meldungen (seit 2026-11-25, Flag `CUSTOM_SENSOR_WANT_ACK`, Standard 1).** Bei
  Broadcasts wiederholt der Router bis zu 3-mal, bis ein Nachbar das Paket per LoRa weitersendet (implicit ACK,
  ReliableRouter.cpp:42–62). Bestätigt ist nur der erste Hop, nicht die Ankunft beim Zuhause-Knoten. Gilt auch für
  Start- und Statusmeldungen, weil die ebenfalls `ZU` (Alarm) enthalten können. Kostet im Normalfall keine
  Sendezeit, nur wenn kein Nachbar weitersendet. Mit `-DCUSTOM_SENSOR_WANT_ACK=0` wieder wie v1. Am Board bestätigt (DEVLOG 2026-11-25).
- **Pause startet nur bei einem Sendeversuch.** Bestätigt am Board (Test 2a): Endet die Pause mit dem schon
  gesendeten Zustand, geht der nächste echte Wechsel sofort raus statt bis zu 10 s später.
- **Kanal-Sperre zählt als Sendeversuch** für den Mindestabstand, damit die Warnung das Log nicht flutet.
- **Batterie** aus `powerStatus` (wird alle 20 s aktualisiert). Ohne erkannten Akku steht "USB" im Text.
  Prozent nur, wenn das Board `BATTERY_PIN` oder `HAS_PMU` hat (sonst gibt es `getBatteryChargePercent()` nicht);
  das Kit hat `BATTERY_PIN`.

## Zeit
- **Zeit nur aus dem Mesh:** v2.7.26 übernimmt Mesh-Zeit nur aus Positions-Paketen mit echtem GPS
  (`location_source >= LOC_INTERNAL`). Der Solarknoten hat GPS.
- **Europe/Berlin wird im Code aus UTC berechnet** (EU-Sommerzeitregel). So hängt es nicht von `config.device.tzdef`
  ab und ändert keine globalen Einstellungen. Bestätigt durch den Board-Test: Im Gerät steht
  `CET-1CEST,M3.4.0/2,M10.4.0/3` (4. statt letzter Sonntag). Das wäre in manchen Jahren eine Woche falsch.
- **Berlin-Rechnung in eigener Header-Datei (`BerlinTime.h`)**, ohne `gmtime`/`mktime` und ohne Firmware-Abhängigkeit.
  So lässt sie sich auf dem PC gegen die Zeitzonen-Datenbank prüfen (2024–2040 fehlerfrei).
- **Statusmeldung 06:00 und 18:00.** Verpasste Termine werden nicht nachgeholt. Erkennung über "zuletzt vergangener
  Termin" (Slot), geprüft bei jedem Lauf (≤60 s), kein eigener Wecker.
- **Zeit gilt auch aus der App** (beim Verbinden gesetzt). Wir nehmen alles ab `RTCQualityFromNet`.
- **Startmeldung 60 s nach dem Boot**, danach zählt der 6-h-Fallback ab dort.
- **Fallback ohne gültige Zeit:** Statusmeldung alle 6 h ab Boot.

## Betrieb
- **Zielrolle SENSOR, zum Testen `is_power_saving=false`.** Die Rolle beeinflusst das Senden von Textnachrichten
  nicht. Mit power_saving macht der nRF52 "Deep Sleep" als `delay()` + Reset. Dann laufen keine Module, und die
  Uhrzeit ist danach weg.

## Windsensor (Plan freigegeben 2026-11-25, Details in WINDSENSOR.md und PLAN.md)
- **Taktmodus in der Basis statt eigener Basisklasse.** Der neue Hook `reportIntervalMs()` liefert standardmäßig 0,
  dann läuft der Fallenmodus wie bisher (Start, 06/18, Fallback). Ein Wert > 0 schaltet auf "alle N ms messen und
  melden". Die Messung selbst steckt im Sensormodul (`startMeasurement`/`runMeasurement`), ebenso der Text
  (`formatReport`). Zeitplan und Senden bleiben in der Basis. Der Fallenmelder überschreibt die neuen Hooks nicht.
- **Kein Startbericht, keine 06/18-Meldung beim Windsensor.** Das erste Messfenster beginnt 60 s nach dem Boot,
  danach alle 30 min, gezählt ab Fensterbeginn (der Takt verschiebt sich nicht).
- **Modbus selbst implementiert** (eine Anfrage, eine Antwort, CRC16), keine Bibliothek. Das spart Flash, und es wird
  nur ein Register gelesen.
- **Serial1 nur im Messfenster offen** (`begin`/`end`). Ein offener UART zieht auf dem nRF52 ~900 µA.
- **Vektor-Mittel** (sin/cos) statt arithmetischem Mittel, damit 350° und 10° → 0° ergibt.
- **Auswahl per eigenem Environment `seeed_xiao_nrf52840_kit_wind`** (`extends` auf das Kit-Env, nur Modul-Flags:
  `-UCUSTOM_SENSOR_TRAP -DCUSTOM_SENSOR_WIND=1`). Gleiche Hardware-Variante, keine Pin-Änderungen. Das Fallen-Env
  bleibt unverändert. Beide Module zusammen verhindert ein `static_assert`.
- **EN des Step-ups an D0.** Am Header ist nur D0 frei (D1–D5, D8–D10 LoRa/SPI, D6/D7 UART). Prüfung, ob die
  Firmware D0 als Taster benutzt: **nein.** In der Standard-Kit-Variante ist `BUTTON_PIN` nicht definiert
  (variant.h:112 auskommentiert, nur `_i2c`/`_btb` setzen ihn), also gibt es keinen `UserButtonThread`
  (InputBroker.cpp:148/172). `config.device.button_gpio` wirkt nur innerhalb von `#ifdef BUTTON_PIN`. D0 ist sonst
  nur `PIN_GPS_STANDBY`, das nur das GPS-Objekt benutzt (bei NOT_PRESENT nicht vorhanden). Abschalten ist also nicht
  nötig. Als Absicherung gibt es im Wind-Build ein `#error`, falls `BUTTON_PIN == WIND_EN_PIN`.
  Hardware-Folgen von R2 (10 kΩ Pull-up) und Taster K1 auf dem Wio-Board: siehe OPEN.md.
- **Erster Hardwaretest nur mit Log** (`CUSTOM_SENSOR_LOG_ONLY=1`): N/O/S/W-Rohwerte klären Skalierung (Datenblatt
  widersprüchlich: 0,1° vs. 1°), Drehsinn und Nullpunkt, bevor etwas ins Mesh geht.

## Verworfene Ideen
- **Akku-Teil im Meldungstext abschaltbar oder plausibilisiert** (z. B. Build-Flag `SHOW_BATTERY`, oder springende
  Werte als "Akku ?" ausgeben): **nicht umgesetzt.** Grund: Es ist kein Akku verfügbar, alles läuft im USB-Betrieb.
  Der Akkuwert ist dort Rauschen, wird aber nirgends ausgewertet (HA wertet nur den Teil vor `|` aus, der
  Fallen-Knoten ist von der Akku-Regel ausgeschlossen). Das Rauschen stört nicht. **Bei echtem Akku neu bewerten.**
