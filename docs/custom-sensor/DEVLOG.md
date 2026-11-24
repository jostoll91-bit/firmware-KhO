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
- Commit: dieser Commit (Hash trägt Schritt 2 nach).
