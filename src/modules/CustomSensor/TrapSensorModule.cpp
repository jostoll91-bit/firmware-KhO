#include "TrapSensorModule.h"

#if CUSTOM_SENSOR_TRAP

#include "NodeDB.h"
#include "concurrency/OSThread.h"
#include "main.h"
#include <Arduino.h>

TrapSensorModule *trapSensorModule;

volatile uint32_t TrapSensorModule::lastEdgeMs = 0;
volatile uint32_t TrapSensorModule::edgeCount = 0;

TrapSensorModule::TrapSensorModule() : CustomSensorModule("TrapSensor") {}

bool TrapSensorModule::pinIsHigh()
{
    return digitalRead(TRAP_SENSOR_PIN) == HIGH;
}

/**
 * Interrupt handler (any edge). Keep it tiny: remember when the edge happened, then
 * wake the main loop so our thread runs once the debounce time has passed.
 * Same wake-up pattern as the button handlers in src/input/InputBroker.cpp.
 */
void TrapSensorModule::isr()
{
    lastEdgeMs = millis();
    edgeCount = edgeCount + 1;
    if (trapSensorModule)
        trapSensorModule->setIntervalFromNow(TRAP_DEBOUNCE_MS);
    runASAP = true;
    BaseType_t higherPriorityTaskWoken = 0;
    concurrency::mainDelay.interruptFromISR(&higherPriorityTaskWoken);
}

bool TrapSensorModule::setupSensor()
{
    // D6 is GPS_TX_PIN on the default kit variant. If the GPS driver is active, it owns the pin (Serial1).
    if (config.position.gps_mode != meshtastic_Config_PositionConfig_GpsMode_NOT_PRESENT) {
        LOG_ERROR("TrapSensor: DISABLED - pin D6 is GPS_TX on this variant and GPS is not NOT_PRESENT (gps_mode=%d). "
                  "Set position.gps_mode = NOT_PRESENT and reboot!",
                  config.position.gps_mode);
        return false;
    }

    // Log the Arduino-pin -> nRF GPIO mapping so the wiring can be checked on the board.
    uint32_t gpio = g_ADigitalPinMap[TRAP_SENSOR_PIN];
    LOG_INFO("TrapSensor: Arduino pin %u -> nRF GPIO %lu (P%lu.%02lu), debounce %u ms, closed_means_open=%d",
             (unsigned)TRAP_SENSOR_PIN, (unsigned long)gpio, (unsigned long)(gpio / 32), (unsigned long)(gpio % 32),
             (unsigned)TRAP_DEBOUNCE_MS, TRAP_CLOSED_MEANS_OPEN);

    // External 1 MOhm pull-up is fitted, so plain INPUT (no internal pull-up, saves ~250 uA when closed).
    pinMode(TRAP_SENSOR_PIN, INPUT);
    reportedPinHigh = pinIsHigh();
    handledEdgeCount = edgeCount;
    attachInterrupt(TRAP_SENSOR_PIN, TrapSensorModule::isr, CHANGE);
    return true;
}

bool TrapSensorModule::takeChangeEvent()
{
    // Read the counter before the timestamp: if an edge arrives in between, the timestamp is
    // newer and we simply wait another debounce period.
    uint32_t count = edgeCount;
    if (count == handledEdgeCount)
        return false;
    if ((uint32_t)(millis() - lastEdgeMs) < TRAP_DEBOUNCE_MS)
        return false; // still bouncing

    bool high = pinIsHigh();
    if (edgeCount != count)
        return false; // a new edge arrived while reading, wait for it to settle

    handledEdgeCount = count;
    if (high == reportedPinHigh)
        return false; // bounced back to the reported state, nothing to report

    reportedPinHigh = high;
    return true;
}

int32_t TrapSensorModule::sensorWakeMs()
{
    if (edgeCount == handledEdgeCount)
        return INT32_MAX;
    uint32_t elapsed = millis() - lastEdgeMs;
    return elapsed >= TRAP_DEBOUNCE_MS ? 1 : (int32_t)(TRAP_DEBOUNCE_MS - elapsed);
}

void TrapSensorModule::formatState(char *buf, size_t len)
{
    bool open = (reportedPinHigh == pinHighMeansOpen());
    // A broken wire looks like an open contact (pin pulled HIGH by the external pull-up).
    // Default wiring: pin LOW -> "OFFEN", pin HIGH -> "ZU (gefangen oder Kabelbruch)".
    if (open)
        snprintf(buf, len, "OFFEN%s", reportedPinHigh ? " (oder Kabelbruch)" : "");
    else
        snprintf(buf, len, "ZU (gefangen%s)", reportedPinHigh ? " oder Kabelbruch" : "");
}

#endif // CUSTOM_SENSOR_TRAP
