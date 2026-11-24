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
- Commit: dieser Commit.
- Ergebnis: `pio run -e seeed_xiao_nrf52840_kit` SUCCESS, keine Warnungen in den neuen Dateien.
  RAM 41,1 %, Flash 88,9 %. Nicht geflasht.
