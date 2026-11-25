#pragma once

#include "CustomSensorModule.h"

#if CUSTOM_SENSOR_WIND

/*
 * Wind direction sensor (Acovmelst wind vane, RS485 / Modbus RTU), see docs/custom-sensor/WINDSENSOR.md.
 *
 * Wiring (default):
 *   - RS485-TTL module (MAX485, automatic direction control, no DE/RE pin) powered from XIAO 3V3.
 *     Module TX -> D7 (XIAO RX), module RX -> D6 (XIAO TX). These are the Serial1 (GPS UART) pins of
 *     the kit variant, so position.gps_mode must be NOT_PRESENT.
 *   - Wind vane powered by a 5 V step-up whose EN input is driven by WIND_EN_PIN (HIGH = on).
 *     Note: on the Wio-SX1262 for XIAO board, D0 has a 10 kOhm pull-up (R2) and the user button (K1).
 *
 * Measurement window before each report: EN on, warm-up, then read holding register 0x0000
 * (angle) every WIND_SAMPLE_MS for WIND_WINDOW_MS, EN off. Valid samples are averaged as vectors.
 */

// Arduino pin numbers (NOT nRF GPIO numbers).
#ifndef WIND_EN_PIN
#define WIND_EN_PIN D0
#endif

#if defined(BUTTON_PIN) && (BUTTON_PIN == WIND_EN_PIN)
#error "WIND_EN_PIN is also BUTTON_PIN in this variant; the button thread would fight over the pin"
#endif

// Modbus slave address of the vane (factory default 0x04 according to the datasheet).
#ifndef WIND_MODBUS_ADDR
#define WIND_MODBUS_ADDR 0x04
#endif

// Raw register units per degree. Datasheet table: 0.1 deg (=10), datasheet example: 1 deg (=1). Verify on hardware.
#ifndef WIND_RAW_PER_DEGREE
#define WIND_RAW_PER_DEGREE 10
#endif

#ifndef WIND_BAUD
#define WIND_BAUD 9600
#endif

// Time between switching the vane on and the first request.
#ifndef WIND_WARMUP_MS
#define WIND_WARMUP_MS 2000
#endif

// Length of the sampling part of the window and the time between two requests.
#ifndef WIND_WINDOW_MS
#define WIND_WINDOW_MS 120000
#endif
#ifndef WIND_SAMPLE_MS
#define WIND_SAMPLE_MS 2000
#endif

// How long to wait for the 7-byte answer of one request.
#ifndef WIND_RESPONSE_TIMEOUT_MS
#define WIND_RESPONSE_TIMEOUT_MS 200
#endif

// A window starts this often; the report is sent at its end.
#ifndef WIND_REPORT_INTERVAL_MS
#define WIND_REPORT_INTERVAL_MS (30 * 60 * 1000UL)
#endif

static_assert(WIND_SAMPLE_MS > WIND_RESPONSE_TIMEOUT_MS, "WIND_SAMPLE_MS must be longer than WIND_RESPONSE_TIMEOUT_MS");
static_assert(WIND_REPORT_INTERVAL_MS > WIND_WARMUP_MS + WIND_WINDOW_MS, "report interval must be longer than one window");

class WindSensorModule : public CustomSensorModule
{
  public:
    WindSensorModule();

  protected:
    bool setupSensor() override;
    bool takeChangeEvent() override { return false; } // no events, periodic reports only
    void formatState(char *buf, size_t len) override;
    uint32_t reportIntervalMs() override { return WIND_REPORT_INTERVAL_MS; }
    void startMeasurement() override;
    int32_t runMeasurement() override;
    void formatReport(char *buf, size_t len, const char *battery) override;

  private:
    enum class Phase { Warmup, Request, Wait };

    Phase phase = Phase::Warmup;
    uint32_t windowStartMs = 0; // millis() when EN was switched on
    uint32_t requestMs = 0;     // millis() when the current request was sent
    uint16_t sampleIndex = 0;   // number of requests sent in this window
    uint16_t validCount = 0;    // valid answers in this window
    float sumSin = 0, sumCos = 0;
    int16_t lastDirection = -2; // result of the last window: 0..359, -1 = ERR, -2 = none yet

    uint8_t rx[16];
    uint8_t rxLen = 0;

    void sendRequest();
    void evaluateAnswer();
    int32_t finishWindow();

    /// Modbus RTU CRC16 (poly 0xA001, init 0xFFFF). Sent low byte first.
    static uint16_t crc16(const uint8_t *data, size_t len);
    /// "04 03 02 ..." for log lines.
    static void hexDump(const uint8_t *data, size_t len, char *buf, size_t bufLen);
};

extern WindSensorModule *windSensorModule;

#endif // CUSTOM_SENSOR_WIND
