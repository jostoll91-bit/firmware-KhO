Projekt: Eigene Sensor-Firmware für Meshtastic (XIAO nRF52840 + Wio-SX1262 Kit).
Repo ~/projects/meshtastic-firmware, Branch custom-sensor-module (Basis Tag
v2.7.26.54e0d8d, eigener Fork). Zentral- und Heimknoten bleiben unangetastet.

Stand: Fallenmelder fertig und am Board getestet. Gemeinsame Basis
src/modules/CustomSensor/CustomSensorModule.* plus TrapSensorModule.*.
Doku: CLAUDE.md und docs/custom-sensor/ (DEVLOG, DECISIONS, OPEN, PLAN).

AUFGABE: Windsensor-Modul (WindSensorModule) als eigenes Board. Die
Sensor-Klärung ist abgeschlossen, Details unten. Bitte zuerst CLAUDE.md und
docs/custom-sensor/ lesen, dann die folgenden Daten als neue Datei
docs/custom-sensor/WINDSENSOR.md ablegen (erster Commit, nur Doku).

=== Sensor ===
- Acovmelst Direction Sensor: NUR Windrichtung (Windfahne), keine Geschwindigkeit
- RS485 / Modbus RTU, Versorgung DC 5–24 V, max. 0,15 W, Ansprechzeit ≤ 0,5 s
- Montage: Pfeil am Sockel nach geografisch Nord

=== Modbus (Werkseinstellung laut Datenblatt) ===
- 9600 Baud, 8N1
- Adresse 0x04 (nicht 1!)
- Funktion 0x03 (Register lesen)
- Register 0x0000: Winkel 0–3600, Einheit 0,1°  ← dieses verwenden
  (0x0001 = 8 Stufen, 0x0002 = 16 Stufen, werden nicht gebraucht)
- Beispiel aus Datenblatt: 04 03 00 00 00 02 C4 5E (CRC nachgerechnet, stimmt)
- CRC jeder Antwort selbst prüfen (Modbus CRC16, Poly 0xA001, Low-Byte zuerst).
  Die Beispielantwort im Datenblatt hat einen falschen CRC (Druckfehler).
- WIDERSPRUCH: Tabelle sagt 0,1°, das Beispiel liest Rohwert 90 als 90° (Ost).
  Skalierung als eine Konstante bauen; erster Hardwaretest: Fahne nach Ost,
  Rohwert loggen (900 oder 90?), dann Konstante festlegen.

=== Hardware ===
- RS485-TTL-Modul: MAX485-Aufdruck, „VCC 3,3–30 V“, Anschlüsse VCC/GND/TX/RX,
  automatische Richtungsumschaltung -> kein DE/RE-Pin.
  Modul-VCC an 3,3 V des XIAO. Modul-TX -> D7 (XIAO RX), Modul-RX -> D6 (XIAO TX).
- Versorgung später Solar + Akku. Fahne braucht >= 5 V: Step-up 3,7 V -> 5 V mit
  EN-Eingang, EN wird von einem XIAO-Pin geschaltet.
  -> In der Variante seeed_xiao_nrf52840_kit prüfen, welcher Pin dafür frei ist
     (nicht durch LoRa/SX1262, Akku-Messung, LED etc. belegt) und mir vorschlagen.
- D6/D7 sind GPS-UART (Serial1). Nur nutzen, wenn position.gps_mode = NOT_PRESENT,
  sonst deutlich loggen und nicht messen. Prüfen, wie Serial1 sauber für 9600 8N1
  geöffnet wird, ohne mit der GPS-Logik zu kollidieren.

=== Verhalten ===
- Regelmeldung alle 30 Minuten. KEINE Zusatzmeldungen um 06/18 Uhr, keine
  Schwellen/Ereignisse.
- Messfenster vor jeder Meldung: EN an, ~2 s Anlaufzeit, dann 2 min lang alle
  2 s Register 0x0000 lesen (~60 Werte), EN aus. Zwischen den Fenstern Sensor aus.
- Mittelung als Vektor-Mittel (sin/cos), damit 350° und 10° -> 0° ergibt.
  Ungültige Antworten (Timeout, CRC falsch, Wert außerhalb Bereich) verwerfen.
- Messen darf den Meshtastic-Thread nicht blockieren (Zustandsmaschine im
  OSThread, kein langes delay).

=== Textformat (festgelegt, Home Assistant parst per MQTT) ===
  WIND d=270 bat=87%
  WIND d=ERR bat=87%     (keine einzige gültige Antwort im Fenster)
- d = Herkunftsrichtung, ganze Grad 0–359, 0 = Nord, im Uhrzeigersinn
- Batterietext wie beim Fallenmelder aus der Basis
- Textnachricht, Broadcast, Primary-Channel (privat). Kein Senden auf dem
  öffentlichen Default-Kanal. Keine Protobuf-Änderungen.

=== Architektur-Frage ===
Die Basis CustomSensorModule ist auf Änderungs-Ereignisse (takeChangeEvent) und
den 06/18-Zeitplan ausgelegt. Prüfen und mir vorschlagen, wie ein periodischer
Bericht (festes Intervall) sauber unterstützt wird, OHNE das Verhalten des
Fallenmelders zu ändern. Den Fallenmelder nach der Änderung erneut bauen.
Auswahl, welches Modul gebaut wird, wie bisher (Build-Flag o. ä.).

=== Randbedingungen ===
- Nur Pin-Symbole (D6/D7/Dx), nie GPIO-Nummern.
- Environment ausschließlich seeed_xiao_nrf52840_kit (nicht _btb, nicht _i2c).
- Flash ist zu ca. 89 % belegt (~90 kB frei). Belegung vor und nach jedem
  Schritt messen und im DEVLOG notieren.

=== Arbeitsweise ===
- Erst lesen und analysieren (Plan-Modus), Plan zur Freigabe vorlegen.
- Dann kleine Schritte, je ein Commit inkl. Doku-Update (DEVLOG/DECISIONS/OPEN).
- Build nach jedem Schritt. Flash nur nach meinem „Board ist angeschlossen“.
- Sinnvoller erster Hardwareschritt: nur Modbus-Abfrage + Rohwert loggen
  (Skalierung klären, Verdrahtung TX/RX und A/B testen), noch ohne Senden.
- Erklärungen auf Deutsch, einfach gehalten (ich bin kein C++-Profi).
