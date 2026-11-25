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
- Commit: `dbcac98f1`.

## 2026-11-24 – Schritt 3: Zeitplan
- `src/modules/CustomSensor/BerlinTime.h` (neu, header-only, ohne Firmware-Abhängigkeiten): Tage-Rechnung nach
  Howard Hinnant, `berlinOffsetSecs(utc)` nach EU-Regel (letzter Sonntag März/Oktober, 01:00 UTC).
  Auf dem PC gegen die Zeitzonen-Datenbank geprüft: 2024–2040 alle 15 min (596 160 Zeitpunkte), 0 Abweichungen,
  Umstellungen 2026 sekundengenau.
- `CustomSensorModule`: Startmeldung `Falle Start: …` 60 s nach dem Setup, Statusmeldung `Falle Status: …`
  06:00/18:00 Berlin (Slot-Logik, nichts nachholen), ohne gültige Zeit alle 6 h ab Startmeldung.
  Logs: `no valid time, fallback every 6 h` / `time valid, schedule active (06:00 and 18:00 Europe/Berlin)`.
- Neue Build-Flags mit Defaults: `CUSTOM_SENSOR_STATUS_HOUR_A/B` (mit `static_assert` A < B),
  `CUSTOM_SENSOR_FALLBACK_INTERVAL_MS`, `CUSTOM_SENSOR_BOOT_REPORT_DELAY_MS`.
- `TrapSensorModule` unverändert.
- Commit: `101ec07d0`.
- Ergebnis: Build SUCCESS, keine Warnungen in den neuen Dateien, clang-format sauber. Nicht geflasht.

  | | Schritt 2 | Schritt 3 | Differenz |
  |---|---|---|---|
  | Flash | 725 064 B (89,0 %) | 725 912 B (89,1 %) | **+848 B** |
  | RAM (statisch) | 102 204 B | 102 204 B | 0 B |

  Freier Flash: noch ca. 89,2 kB.

## 2026-11-24 – Board-Test Schritt 3 (nur USB, App anfangs nicht verbunden)
- **(1) Ohne Zeit:** Startmeldung bei Uptime 65 s (`Falle Start: … `), Log-Zeiten `??:??:??`. Zeile
  `no valid time, fallback every 6 h` im Start-Log nach dem Reset in (2) gesehen ✅.
- **(2) Zeit gültig, dann Reset:** App verbunden → `set_time_only` (Uptime 158 s) → `time valid, schedule active`
  um 13:54:57 (beim nächsten 60-s-Lauf) ✅. Nach einem Reset: `no valid time`, App verbindet neu, Startmeldung,
  `schedule active`, **keine** `Status:`-Meldung, obwohl 06:00 vorbei war ✅ (nichts nachgeholt).
- **(3) Zeitplan mit `HOUR_A=15`, `HOUR_B=16`** (nur per `PLATFORMIO_BUILD_FLAGS`, nicht eingecheckt; 14 war wegen
  Build-/Flash-Dauer zu knapp): `Falle Status:` um 15:00:57 und 16:00:57, je genau eine ✅.
- Rückflash auf 6/18 erfolgreich, Log-Bestätigung entfällt, weil direkt danach der Zustands-Fix kam (siehe unten).
- Ergebnis: Schritt 3 am Board bestätigt.

## 2026-11-24 – Fix: Zustände vertauscht
- **Die Zuordnung war vertauscht.** Richtig: Magnet am Reed = Kontakt zu = Pin LOW = Falle **OFFEN** (scharf,
  Normalzustand). Magnet weg = Kontakt offen = Pin HIGH = Falle **ZU (gefangen oder Kabelbruch)** = Alarm.
- `TRAP_CLOSED_MEANS_OPEN` Standard 0 → **1**. Texte: `OFFEN` bzw. `ZU (gefangen oder Kabelbruch)` (gilt für
  Ereignis-, Start- und Statusmeldung). Verdrahtungs-/Ruhestrom-Kommentar im Header angepasst
  (scharf/LOW ≈ 3 µA, gefangen/HIGH 0).
- HA-Beispiel: Alarm bei `ZU`, `OFFEN` = wieder scharf. CLAUDE.md, PLAN, DECISIONS, OPEN angepasst.
- **Hinweis zu den Board-Tests Schritt 1–3:** Sie liefen mit der **alten** Zuordnung. Wo dort `OFFEN`/`ZU` steht,
  ist der Pegel gemeint (OFFEN = Pin HIGH, ZU = Pin LOW). Erkennung, Entprellung, Mindestabstand und Kabelbruch
  hängen nicht von der Zuordnung ab und gelten weiter. Der Zeitplan-Test (15:00/16:00) ist unabhängig und gilt.
- Stunden geprüft: `CUSTOM_SENSOR_STATUS_HOUR_A/B` = 6/18 im Header, Test-Stunden nirgends eingecheckt.
- Build: SUCCESS, Flash 725 896 B (−16 B ggü. Schritt 3), RAM unverändert, clang-format sauber.
- Commit: `1bdc765d0`.

## 2026-11-24 – Board-Test Fix + 6/18 (ein Flash, nur USB)
- Flash OK. `Falle Start: OFFEN` bei Uptime 65 s (Magnet am Reed = scharf) ✅.
- App verbunden → `time valid, schedule active (06:00 and 18:00 Europe/Berlin)` um 16:24:16 ✅ (Stunden 6/18 bestätigt).
- Erster Magnet-Test: **keine Wechsel erkannt**, Pin blieb LOW. Ursache: Der **Logikanalyzer an D6** zog den
  1-MOhm-Pull-up herunter (genau der Fall aus OPEN.md). Analyzer abgeklemmt, Test wiederholt:
  - 16:26:47 Magnet weg → `Falle jetzt ZU (gefangen oder Kabelbruch)` sofort ✅
  - 16:27:07 Magnet ran → `Falle jetzt OFFEN` sofort ✅
  - 16:27:08/09 kurz ZU/OFFEN (Nachwackeln) in der Pause, 16:27:17 `equals last sent state, no message` ✅
- Ergebnis: Fix der Zuordnung am Board bestätigt.
- Commit: `a0fdfc0a6`.
- Nachtrag: Die Meldungen sind über Gateway → MQTT → HomeAssistant → **ntfy** angekommen ✅ (Ende-zu-Ende).

## 2026-11-24 – HA: Lebenszeichen-Alarm, Akku-Tests zurückgestellt
- ntfy bestätigt: 🚨 bei `ZU`, ✅ bei `OFFEN`. Die echte HA-Automatisierung ist umgestellt (Nutzer).
- Akku-Tests bis auf Weiteres zurückgestellt (kein Akku, nur USB-Betrieb). OPEN.md angepasst.
- `homeassistant-example.yaml`: Kommentare zur Fallen-Regel (ZU vor `|` = Alarm, auch in Status/Start;
  `Falle jetzt OFFEN` = ✅; sonst ℹ️) und zur Akku-Regel (Fallen-Knoten ausgeschlossen, mit echtem Akku Bedingung
  entfernen). Echte Knotennummer im Rechenbeispiel durch eine erfundene ersetzt.
- Neu `homeassistant-heartbeat-example.yaml` (HA-Package): `input_datetime` für letzte Meldung und letzten Alarm
  (übersteht HA-Neustart), Prüfung alle 15 min, Alarm bei >13 h, Wiederholung frühestens nach 6 h,
  "wieder da"-Meldung. YAML-Syntax geprüft, in HA nicht getestet.
- Commit: `a7ad952a1`.

## 2026-11-24 – Akku-Text: Idee verworfen
- Akku-Teil im Text abschaltbar/plausibilisiert machen: verworfen (kein Akku, Wert im USB-Betrieb nicht
  ausgewertet; bei echtem Akku neu bewerten). In DECISIONS.md unter "Verworfene Ideen", OPEN.md verweist darauf.
- Commit: `e988a7499`.

## 2026-11-24 – HA: Lebenszeichen eingerichtet, Idee mehrere Fallen
- Lebenszeichen-Alarm in der echten HA-Instanz eingerichtet (Helfer in `configuration.yaml`, Automationen in
  `automations.yaml`). Dabei zwei Fehler in der HA-Konfiguration behoben (Nutzer): verirrte Topic-Zeilen in der
  Fallen-Automation, Template-Sensor `S001 Akkustand` stand unter `device_tracker:` statt `sensor:`
  (Knotennummer dort auf 409011304 korrigiert). Konfigurationsprüfung grün, vier Automationen sichtbar.
  Alarm selbst noch nicht getestet.
- OPEN.md: Vorgehen für mehrere Fallen notiert (eine Automation mit Liste, Lebenszeichen als Blueprint).
- Commit: `b5458fcb4`.

## 2026-11-25 – Lebenszeichen-Alarm und 06:00-Status im Betrieb bestätigt
- HA-Lebenszeichen-Alarm hat ausgelöst und nach 6 h wie vorgesehen erneut gemeldet ✅.
- Heute 06:00 kam die Statusmeldung des Fallenmelders (Zeitplan im echten Betrieb über Nacht) ✅,
  danach "wieder da" in HA.
- Testauslösungen (ZU/OFFEN) kommen weiter per ntfy an ✅.
- Keine Code-Änderung. Commit: `322732134`.

## 2026-11-25 – Offene Punkte nachgetragen
- `tzdef` laut Nutzer in der App korrigiert (Gegenprüfung im nächsten Boot-Log).
- Test der Kanal-Sperre zurückgestellt (öffentlicher Standardkanal ist auf den eigenen Geräten nicht eingerichtet).
- Commit: `5bfd67139`.

## 2026-11-25 – want_ack für alle Meldungen
- Neues Flag `CUSTOM_SENSOR_WANT_ACK` (Standard 1) in `CustomSensorModule.h`, `sendText()` setzt
  `p->want_ack` daraus. Wirkung bei Broadcast: bis zu 3 Wiederholungen bis zum implicit ACK (erster Hop).
- Geprüft: `router` ist in v2.7.26 ein `ReliableRouter` (main.cpp:734), `NUM_RELIABLE_RETX = 3`.
- Build: SUCCESS, Flash 725 896 B (unverändert), RAM unverändert, clang-format (Projekt-Stil) sauber.
- Am Board noch nicht getestet.
- Commit: `a66368c6c`.

## 2026-11-25 – Board-Test want_ack (nur USB)
- Flash OK (`2.7.26.5d66681`, Build vor dem Umschreiben = Inhalt von `a66368c6c`). Mitschnitt mit `serlog.py`, Uptime in Sekunden:
  - 64 `Falle Start: OFFEN` → 69 `Generate implicit ack`, `Received a ACK ..., stopping retransmissions` ✅
  - 98 `Falle jetzt ZU (gefangen oder Kabelbruch)` → 103 implicit ACK ✅
  - 111 `Falle jetzt OFFEN` → nach 7,35 s kein Weitersenden gehört → 118 `Sending retransmission ... tries left=2`
    → 120 implicit ACK (Nachbar `relay=0x40`) ✅. Die Wiederholung hat also einmal echt gegriffen.
- Ergebnis: want_ack funktioniert wie geplant.
- Commit: `666182cf4`.

## 2026-11-25 – Commit-Adresse auf GitHub-Noreply umgestellt
- Push wurde abgelehnt (GitHub-E-Mail-Schutz). Im Repo `user.email` auf die GitHub-Noreply-Adresse gesetzt und
  die 5 noch nicht gepushten Commits umgeschrieben (Inhalt gleich, neue Hashes). Hashes oben korrigiert.
- Commit: dieser Commit.
