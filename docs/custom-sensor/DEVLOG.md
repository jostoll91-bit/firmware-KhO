# Entwicklungs-Log

Ein Eintrag pro Schritt. Den Commit-Hash eines Schritts trägt immer der **nächste** Eintrag nach, weil der Hash
erst nach dem Commit feststeht.

## 2026-11-24 – Analyse und Plan
- Quelltext v2.7.26 analysiert: Detection-Modul, Pin-Map, Batterie, RTC/Zeit, nRF52-Sleep, Sendeweg, Rolle SENSOR,
  want_ack bei Broadcasts. Ergebnisse stehen in DECISIONS.md und OPEN.md.

## 2026-11-24 – Schritt 1: Gerüst
- Docs angelegt (`docs/custom-sensor/`).
- `src/modules/CustomSensor/CustomSensorModule.{h,cpp}`: Basis-Thread (Setup, Ereignis-Abfrage, Wake-Intervall).
- `src/modules/CustomSensor/TrapSensorModule.{h,cpp}`: Reed an D6, `INPUT` (externer Pull-up), Interrupt CHANGE,
  200 ms Debounce, GPS-Prüfung, Pin-Map-Log.
- Registrierung in `Modules.cpp` hinter `CUSTOM_SENSOR_TRAP`, Build-Flag im Env `seeed_xiao_nrf52840_kit`.
- Noch **kein Senden**: Zustandswechsel stehen nur im Log.
- Commit: `e1743618d`.
- Ergebnis: `pio run -e seeed_xiao_nrf52840_kit` SUCCESS, keine Warnungen in den neuen Dateien.
  RAM 41,1 %, Flash 88,9 %. Nicht geflasht.
- Größe im Vergleich zur Baseline (gleicher Code, Build mit `-UCUSTOM_SENSOR_TRAP` in separatem Build-Verzeichnis,
  in der Baseline-ELF keine TrapSensor-Symbole):

  | | Baseline | Schritt 1 | Differenz |
  |---|---|---|---|
  | Flash | 723 048 B (88,7 %) | 724 296 B (88,9 %) | **+1 248 B** |
  | RAM (statisch) | 102 196 B | 102 204 B | **+8 B** |

  Das Modul-Objekt selbst wird zur Laufzeit mit `new` angelegt (Heap). Es taucht in der statischen RAM-Zahl nicht auf.
  Freier Flash: noch ca. 90,8 kB.

## 2026-11-24 – Board-Test Schritt 1 (geflasht, Gerät: gps_mode NOT_PRESENT)
- Flash per `pio run -e seeed_xiao_nrf52840_kit -t upload --upload-port /dev/ttyACM0` OK.
- `pio device monitor` geht ohne interaktives Terminal nicht. Deshalb Log mit `tools/serlog.py` (pyserial).
- Init-Log: `TrapSensor: Arduino pin 6 -> nRF GPIO 43 (P1.11), debounce 200 ms, closed_means_open=0`, also D6 = P1.11
  bestätigt. Die GPS-Prüfung hat nicht abgeschaltet.
- **Test A, zwei Kabel von Hand** (Start auseinander, dann mehrmals zusammen und auseinander, Ende auseinander):
  Init `OFFEN (oder Kabelbruch)`, dann sauber abwechselnd ZU/OFFEN, Ende OFFEN. Trotz Prellen von Hand keine
  Doppelmeldungen.
- **Test B, Reed-Kontakt + Magnet** (Start Magnet am Reed):
  - Init `ZU` ✅.
  - Langsam weg und ran (5–6-s-Takt): genau ein Wechsel pro Bewegung, kein mechanisches Prellen sichtbar ✅.
  - Wedeln: **6 Wechsel in ca. 6 s** (jede Position war >200 ms stabil, also korrekt erkannt). Der Mindestabstand
    von 10 s aus Schritt 2 soll das im Mesh zu höchstens 2 Meldungen zusammenfassen.
  - Bei 76–81 s noch 4 Wechsel, vermutlich Kabel abziehen/anstecken (nicht bestätigt). Ende `ZU`, passt zu
    "Magnet am Ende am Reed" ✅.
- Ergebnis: Schritt 1 am Board abgenommen.

## 2026-11-24 – Docs vor /clear
- PLAN.md (Plan Schritt 2 und 3) und `tools/serlog.py` angelegt, DEVLOG/DECISIONS/OPEN ergänzt.
- Commit: `468262ab4`.

## 2026-11-24 – CLAUDE.md eingecheckt
- CLAUDE.md ergänzt: Einstieg über DEVLOG/PLAN, D6-Zuordnung bestätigt, gps_mode NOT_PRESENT als Pflicht,
  Log über `tools/serlog.py` statt `pio device monitor`, PLAN.md als vierte Doku-Datei. Erstmals versioniert.
- Commit: `0ac6e1bb4`.

## 2026-11-24 – Schritt 2: Senden
- `CustomSensorModule` (Basis) sendet jetzt: `sendText()` (TEXT_MESSAGE_APP, Broadcast, Primary-Channel,
  `want_ack=false`, ccToPhone), `sendReport()` (`"<NAME> <prefix> <state> | <Akku ..% x.xxV | USB>"`),
  Kanal-Sperre bei öffentlichem Standardkanal (LOG_WARN, nichts gesendet).
- Ereignis-Meldung `Falle jetzt ...` sofort, danach 10 s Mindestabstand (`CUSTOM_SENSOR_MIN_EVENT_GAP_MS`).
  Wechsel innerhalb der Pause werden zusammengefasst: Nach der Pause kommt nur der Endzustand, und nur wenn er
  sich vom zuletzt gesendeten unterscheidet.
- `TrapSensorModule` unverändert. Noch keine Start- und Statusmeldung (Schritt 3).
- Commit: `5b29950b4`.
- Ergebnis: `pio run -e seeed_xiao_nrf52840_kit` SUCCESS, keine Warnungen in den neuen Dateien, clang-format
  (`.trunk/configs/.clang-format`) sauber. Nicht geflasht.

  | | Schritt 1 | Schritt 2 | Differenz |
  |---|---|---|---|
  | Flash | 724 296 B (88,9 %) | 725 064 B (89,0 %) | **+768 B** |
  | RAM (statisch) | 102 204 B | 102 204 B | 0 B |

  Freier Flash: noch ca. 90,0 kB.

## 2026-11-24 – Korrektur Doku Schritt 2
- Schritt 2 abgenommen. Falsche Aussage in PLAN.md und im Code-Kommentar korrigiert: Ein Wechsel kurz nach dem
  Boot bleibt **nicht** unbemerkt. Der erste Wechsel geht immer sofort raus, der Boot-Zustand ist nur
  Vergleichswert. Keine Code-Logik geändert (nur Kommentar), daher kein Neubau nötig.
- Commit: `230a83a62`.

## 2026-11-24 – Board-Test Schritt 2 (geflasht, nur USB, kein Akku)
- Erster Flash: Board blieb im Bootloader (USB `2886:0045` + Laufwerk `XIAO-SENSE`, kein Log). Zweiter Flash
  ("Device programmed") OK, danach USB `239a:810b`, Firmware läuft.
- Gerätekonfiguration: Das **eingebaute Detection Sensor Module war aktiv** (alte Testeinstellung,
  `Testfalle2 detected` alle 15 s). In der App ausgeschaltet.
- Kanal: Unser Knoten sendet auf Hash `0xec`, ein Nachbar hat unsere Pakete auf `0xec` weitergesendet. Der Kanal passt.
- **Test 1, Wechsel** (langsam weg/ran): je eine Meldung, sofort ✅.
- **Test 2b, Wedeln, Ende = zuletzt gesendet**: 3 Wechsel, 1 Meldung, danach `equals last sent state, no message` ✅.
- **Test 2a, Wedeln ca. 10 s**: 22 Wechsel, 2 Meldungen (erste sofort, Endzustand ZU). Nach Pausenende war der
  Zustand zufällig gleich dem gesendeten (keine Meldung, keine neue Pause), das abschließende ZU kam deshalb sofort ✅.
- **Test 3, Kabelbruch**: Kabel ab → `OFFEN (oder Kabelbruch)` sofort, Kabel dran → `ZU` sofort ✅.
  10-s-Pause auch hier gesehen (13:02:12 OFFEN, ZU 13:02:20 erkannt, 13:02:22 gesendet).
- **Akku/USB ❌**: Nur am USB ohne Akku steht trotzdem `Akku ..% x.xxV`, der Wert springt zwischen 49 % / 3,71 V
  und 94 % / 4,11 V (`battery hardware detected`, `usbPower=0`). Fehlmessung des offenen VBAT-Teilers, nicht
  unser Code. Siehe OPEN.md.
- Ankunft: Das Gateway (Zuhause-Knoten) hat die Meldungen bekommen ✅. HomeAssistant hat nicht reagiert, die
  Automatisierung muss erst an den neuen Text angepasst werden (Nutzer, außerhalb der Firmware).
- Ergebnis: Schritt 2 am Board bestätigt. Akku/USB bleibt offen bis zum Test mit echtem Akku.
- Commit: `8c7a6d358`.

## 2026-11-24 – Nachtrag Board-Test Schritt 2
- Ankunft am Gateway bestätigt, HomeAssistant-Automatisierung als offenen Punkt aufgenommen.
- Commit: `f776f4485`.

## 2026-11-24 – HomeAssistant-Beispiel
- `homeassistant-example.yaml`: Akkustand-Regel ohne Fallen-Knoten, Fallen-Regel (Typ `text`, Text beginnt mit
  `Falle `, OFFEN-Prüfung nur auf den Teil vor `|`, `mode: queued`). Platzhalter statt Knotennummern und Kanal.
- OPEN.md: Lebenszeichen-Alarm (>13 h ohne Meldung) für nach Schritt 3.
- Commit: dieser Commit.
