# Windsensor: Acovmelst Direction Sensor (Windfahne)

Stand: 2026-11-25. Sensor-Klärung abgeschlossen (Datenblatt-Fotos, Nutzerangaben). Eigenes Board,
Modul `WindSensorModule` auf der gemeinsamen Basis `CustomSensorModule`.

## Sensor
- **Nur Windrichtung** (Windfahne), keine Geschwindigkeit
- Version RS485 / Modbus RTU
- Versorgung DC 5–24 V, Leistung ≤ 0,15 W, Ansprechzeit ≤ 0,5 s, Betrieb −40 bis +60 °C
- Montage: Pfeil am Sockel nach **geografisch Nord** ausrichten

## Modbus (Werkseinstellung laut Datenblatt)
- 9600 Baud, 8N1
- **Adresse 0x04** (nicht 1!)
- Funktion 0x03 (Register lesen); 0x06 nur zum Umkonfigurieren (wird nicht benutzt)
- Register (16 Bit, High-Byte zuerst):
  - **0x0000: Winkel 0–3600, Einheit 0,1°** ← wird verwendet
  - 0x0001: Richtung in 8 Stufen, 0x0002: 16 Stufen (nicht gebraucht)
- Anfrage für 1 Register ab 0x0000 wird im Code samt CRC gebaut. Beispiel aus dem Datenblatt (2 Register):
  `04 03 00 00 00 02 C4 5E` (CRC nachgerechnet: stimmt)
- **CRC jeder Antwort selbst prüfen** (Modbus CRC16, Polynom 0xA001, Low-Byte zuerst). Die Beispielantwort im
  Datenblatt hat CRC `5B E1`, richtig wäre `0E E1` (Druckfehler).

## Widerspruch im Datenblatt (am Sensor klären)
- Tabelle: 0,1° (0–3600). Beispiel: Rohwert 0x005A = 90 wird als 90° (Ost) gelesen.
- Im Code eine Konstante für die Skalierung. **Erster Hardwaretest:** Fahne nach Ost, Rohwert loggen
  (900 oder 90?), dann Konstante festlegen.

## RS485-TTL-Modul
- Aufdruck MAX485, Platine „VCC 3,3–30 V“, Anschlüsse VCC/GND/TX/RX → automatische Richtungsumschaltung,
  **kein DE/RE-Pin**
- Modul-VCC an **3,3 V des XIAO** (nicht an Akku/5 V), damit TX des Moduls nicht über 3,3 V liegt.
  Vor dem Anschluss: Modul mit 3,3 V versorgen, Spannung an TX gegen GND messen (≤ 3,3 V).
- Verdrahtung: Modul-TX → **D7** (XIAO RX), Modul-RX → **D6** (XIAO TX). Antwortet nichts: TX/RX tauschen
  (bei 3,3 V unkritisch). A/B zum Sensor; antwortet nichts: A/B tauschen.
- Ruhestrom des Moduls messen; eine Power-LED (oft 1–3 mA Dauerstrom) ggf. auslöten.
- D6/D7 sind in der Kit-Variante GPS-UART (`Serial1`). Nur nutzen, wenn `position.gps_mode = NOT_PRESENT`,
  sonst deutlich loggen und nicht messen. Auf dem Wio-SX1262-Board sind D6/D7 nicht belegt (Schaltplan).

## Versorgung (Solar + Akku)
- Fahne braucht ≥ 5 V → Step-up 3,7 V → 5 V mit **EN-Eingang**, EN von einem XIAO-Pin geschaltet
  (alternativ P-MOSFET-Highside vor dem Step-up).
- Dauerbetrieb wäre ~3,6 Wh/Tag → zu viel für den Akku, daher Messfenster.
- **Pin für EN:** Am XIAO-Header ist nur **D0** frei (D1–D5 und D8–D10 LoRa/SPI, D6/D7 UART). D0 ist in der
  Variante `PIN_GPS_STANDBY` (nur benutzt, wenn GPS angelegt wird, bei NOT_PRESENT nicht).
  **Achtung, Schaltplan Wio-SX1262 for XIAO V1.0:** An D0 hängen **R2 = 10 kΩ Pull-up nach 3V3** und der
  **User-Taster K1 nach GND**. Entschieden: EN an **D0** (DECISIONS.md). Ob R2 ausgelötet wird, ist offen
  (OPEN.md, Schritt 6).

## Verhalten (festgelegt)
- Regelmeldung **alle 30 Minuten**. Keine Zusatzmeldungen um 06/18 Uhr, keine Schwellen/Ereignisse.
- Messfenster vor jeder Meldung: EN an, ~2 s Anlaufzeit, dann 2 min lang alle 2 s Register 0x0000 lesen
  (~60 Werte), EN aus. Zwischen den Fenstern ist der Sensor aus.
- Mittelung als **Vektor-Mittel** (sin/cos), damit 350° und 10° → 0° ergibt.
- Ungültige Antworten (Timeout, CRC falsch, Wert außerhalb des Bereichs) werden verworfen.
- Messen blockiert den Meshtastic-Thread nicht (Zustandsmaschine im OSThread, kein langes `delay`).

## Textformat (festgelegt, Home Assistant parst per MQTT)
```
WIND d=270 bat=87%
WIND d=ERR bat=87%     (keine einzige gültige Antwort im Fenster)
```
- `d` = Herkunftsrichtung, ganze Grad 0–359, 0 = Nord, im Uhrzeigersinn
- Batterie aus derselben Quelle wie beim Fallenmelder (Basis); ohne Akku `bat=USB`
- Textnachricht, Broadcast, Primary-Channel (privat). Kein Senden auf dem öffentlichen Default-Kanal.
  Keine Protobuf-Änderungen.
