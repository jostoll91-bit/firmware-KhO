# Entscheidungen – Custom Sensor-Modul (Fallenmelder v1)

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
  Kontakt ~250 µA ziehen. Mit 1 MOhm: Falle zu ≈ 3 µA, offen = 0.
- **Logik: Pin HIGH = Kontakt offen = Falle OFFEN (oder Kabelbruch). Pin LOW = Falle ZU.** Ein Kabelbruch zeigt
  sich also als "offen" und fällt auf. Umkehren mit `-DTRAP_CLOSED_MEANS_OPEN=1`.
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
  flatternder Kontakt das Mesh nicht flutet.
- **Kanal-Sperre:** Ist der Primary-Channel der öffentliche Standardkanal (Standard-PSK + Preset-Name, z. B.
  "LongFast"), wird nicht gesendet. Es gibt eine deutliche Warnung im Log.
- **`want_ack=false` in v1.** Bei Broadcasts würde `want_ack=true` nur bis zum ersten Hop wiederholen
  (Details in OPEN.md, Zustellsicherheit). Kandidat für v1.1.
- **Batterie** aus `powerStatus` (wird alle 20 s aktualisiert). Ohne erkannten Akku steht "USB" im Text.

## Zeit
- **Zeit nur aus dem Mesh:** v2.7.26 übernimmt Mesh-Zeit nur aus Positions-Paketen mit echtem GPS
  (`location_source >= LOC_INTERNAL`). Der Solarknoten hat GPS.
- **Europe/Berlin wird im Code aus UTC berechnet** (EU-Sommerzeitregel). So hängt es nicht von `config.device.tzdef`
  ab und ändert keine globalen Einstellungen. Bestätigt durch den Board-Test: Im Gerät steht
  `CET-1CEST,M3.4.0/2,M10.4.0/3` (4. statt letzter Sonntag). Das wäre in manchen Jahren eine Woche falsch.
- **Statusmeldung 06:00 und 18:00.** Verpasste Termine werden nicht nachgeholt.
- **Fallback ohne gültige Zeit:** Statusmeldung alle 6 h ab Boot.

## Betrieb
- **Zielrolle SENSOR, zum Testen `is_power_saving=false`.** Die Rolle beeinflusst das Senden von Textnachrichten
  nicht. Mit power_saving macht der nRF52 "Deep Sleep" als `delay()` + Reset. Dann laufen keine Module, und die
  Uhrzeit ist danach weg.
