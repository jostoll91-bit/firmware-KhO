#include "WindSensorModule.h"

#if CUSTOM_SENSOR_WIND

#include "NodeDB.h"
#include <Arduino.h>
#include <math.h>

WindSensorModule *windSensorModule;

static constexpr float kPi = 3.14159265f;

WindSensorModule::WindSensorModule() : CustomSensorModule("WindSensor") {}

uint16_t WindSensorModule::crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++)
            crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
    }
    return crc;
}

void WindSensorModule::hexDump(const uint8_t *data, size_t len, char *buf, size_t bufLen)
{
    size_t pos = 0;
    if (bufLen)
        buf[0] = '\0';
    for (size_t i = 0; i < len && pos + 4 <= bufLen; i++)
        pos += snprintf(buf + pos, bufLen - pos, i ? " %02x" : "%02x", data[i]);
}

bool WindSensorModule::setupSensor()
{
    // D6/D7 are the GPS UART (Serial1) on this variant. Only usable if no GPS driver owns them.
    if (config.position.gps_mode != meshtastic_Config_PositionConfig_GpsMode_NOT_PRESENT) {
        LOG_ERROR("Wind: DISABLED - D6/D7 are the GPS UART (Serial1) on this variant and GPS is not NOT_PRESENT "
                  "(gps_mode=%d). Set position.gps_mode = NOT_PRESENT and reboot!",
                  config.position.gps_mode);
        return false;
    }

    // Sensor off between windows.
    pinMode(WIND_EN_PIN, OUTPUT);
    digitalWrite(WIND_EN_PIN, LOW);

    // Log the Arduino-pin -> nRF GPIO mapping so the wiring can be checked on the board.
    uint32_t en = g_ADigitalPinMap[WIND_EN_PIN];
    uint32_t tx = g_ADigitalPinMap[PIN_SERIAL1_TX];
    uint32_t rxPin = g_ADigitalPinMap[PIN_SERIAL1_RX];
    LOG_INFO("Wind: EN pin %u -> P%lu.%02lu, Serial1 TX pin %u -> P%lu.%02lu, RX pin %u -> P%lu.%02lu", (unsigned)WIND_EN_PIN,
             (unsigned long)(en / 32), (unsigned long)(en % 32), (unsigned)PIN_SERIAL1_TX, (unsigned long)(tx / 32),
             (unsigned long)(tx % 32), (unsigned)PIN_SERIAL1_RX, (unsigned long)(rxPin / 32), (unsigned long)(rxPin % 32));
    LOG_INFO("Wind: addr 0x%02x, %u baud, raw/deg %u, window %lu s every %lu min", (unsigned)WIND_MODBUS_ADDR,
             (unsigned)WIND_BAUD, (unsigned)WIND_RAW_PER_DEGREE, (unsigned long)(WIND_WINDOW_MS / 1000),
             (unsigned long)(WIND_REPORT_INTERVAL_MS / 60000UL));
    return true;
}

void WindSensorModule::startMeasurement()
{
    digitalWrite(WIND_EN_PIN, HIGH);
    Serial1.begin(WIND_BAUD); // 8N1, pins D6/D7 from the variant
    windowStartMs = millis();
    sampleIndex = 0;
    validCount = 0;
    sumSin = 0;
    sumCos = 0;
    phase = Phase::Warmup;
}

void WindSensorModule::sendRequest()
{
    // Read holding register 0x0000 (angle), count 1.
    uint8_t req[8] = {WIND_MODBUS_ADDR, 0x03, 0x00, 0x00, 0x00, 0x01, 0, 0};
    uint16_t crc = crc16(req, 6);
    req[6] = crc & 0xFF;
    req[7] = crc >> 8;

    while (Serial1.available()) // drop noise from power-up or a late answer
        Serial1.read();
    rxLen = 0;
    Serial1.write(req, sizeof(req));
    requestMs = millis();
    sampleIndex++;

    if (sampleIndex == 1) {
        char hex[32];
        hexDump(req, sizeof(req), hex, sizeof(hex));
        LOG_INFO("Wind: request %s", hex);
    }
}

void WindSensorModule::evaluateAnswer()
{
    char hex[52];
    hexDump(rx, rxLen, hex, sizeof(hex));

    if (rxLen >= 5 && rx[0] == WIND_MODBUS_ADDR && rx[1] == (0x03 | 0x80)) {
        LOG_WARN("Wind: Modbus exception: %s", hex);
        return;
    }
    if (rxLen < 7) {
        LOG_WARN("Wind: timeout, got %u bytes: %s", (unsigned)rxLen, hex);
        return;
    }
    uint16_t crc = crc16(rx, 5);
    if (rx[5] != (crc & 0xFF) || rx[6] != (crc >> 8)) {
        LOG_WARN("Wind: CRC bad: %s", hex);
        return;
    }
    if (rx[0] != WIND_MODBUS_ADDR || rx[1] != 0x03 || rx[2] != 2) {
        LOG_WARN("Wind: unexpected answer: %s", hex);
        return;
    }

    uint16_t raw = ((uint16_t)rx[3] << 8) | rx[4];
    if (raw > 360 * WIND_RAW_PER_DEGREE) {
        LOG_WARN("Wind: out of range raw=%u", (unsigned)raw);
        return;
    }
    LOG_INFO("Wind: raw=%u (0x%04x)", (unsigned)raw, (unsigned)raw);

    float rad = (raw / (float)WIND_RAW_PER_DEGREE) * kPi / 180.0f;
    sumSin += sinf(rad);
    sumCos += cosf(rad);
    validCount++;
}

int32_t WindSensorModule::finishWindow()
{
    digitalWrite(WIND_EN_PIN, LOW);
    Serial1.end();

    if (validCount == 0) {
        lastDirection = -1;
    } else {
        // Vector mean: 350 and 10 degrees give 0, not 180.
        float deg = atan2f(sumSin, sumCos) * 180.0f / kPi;
        if (deg < 0)
            deg += 360.0f;
        lastDirection = (int16_t)((int)(deg + 0.5f) % 360);
    }
    LOG_INFO("Wind: window done, %u/%u valid, d=%d", (unsigned)validCount, (unsigned)sampleIndex, (int)lastDirection);
    return 0;
}

int32_t WindSensorModule::runMeasurement()
{
    switch (phase) {
    case Phase::Warmup:
        phase = Phase::Request;
        return WIND_WARMUP_MS;

    case Phase::Request:
        sendRequest();
        phase = Phase::Wait;
        return 20;

    case Phase::Wait:
    default: {
        while (Serial1.available() && rxLen < sizeof(rx))
            rx[rxLen++] = (uint8_t)Serial1.read();

        bool exception = rxLen >= 5 && (rx[1] & 0x80);
        if (rxLen < 7 && !exception && (uint32_t)(millis() - requestMs) < WIND_RESPONSE_TIMEOUT_MS)
            return 20; // keep waiting

        evaluateAnswer();

        // Next request on a fixed raster from the end of the warm-up.
        uint32_t nextOffset = (uint32_t)WIND_WARMUP_MS + (uint32_t)sampleIndex * WIND_SAMPLE_MS;
        if ((uint32_t)sampleIndex * WIND_SAMPLE_MS >= WIND_WINDOW_MS)
            return finishWindow();
        phase = Phase::Request;
        int32_t wait = (int32_t)(windowStartMs + nextOffset - millis());
        return wait < 1 ? 1 : wait;
    }
    }
}

void WindSensorModule::formatState(char *buf, size_t len)
{
    if (lastDirection >= 0)
        snprintf(buf, len, "d=%d", (int)lastDirection);
    else if (lastDirection == -1)
        snprintf(buf, len, "d=ERR");
    else
        snprintf(buf, len, "d=?");
}

void WindSensorModule::formatReport(char *buf, size_t len, const char *battery)
{
    if (lastDirection >= 0)
        snprintf(buf, len, "WIND d=%d bat=%s", (int)lastDirection, battery);
    else
        snprintf(buf, len, "WIND d=ERR bat=%s", battery);
}

#endif // CUSTOM_SENSOR_WIND
