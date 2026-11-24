#pragma once

#include "CustomSensorModule.h"

#if CUSTOM_SENSOR_TRAP

/*
 * Trap sensor (reed contact) wiring, default configuration:
 *   - Reed contact between TRAP_SENSOR_PIN and GND.
 *   - External 1 MOhm pull-up from TRAP_SENSOR_PIN to 3V3 (internal pull-up is NOT used).
 *   - Magnet at the reed -> contact closed -> pin LOW  -> trap OPEN (armed, nothing caught).
 *     This is the normal resting state. Quiescent current ~3.3 uA (3.3 V / 1 MOhm).
 *   - Magnet away        -> contact open   -> pin HIGH -> trap CLOSED (caught) or broken wire.
 *     Quiescent current ~0. A broken wire therefore raises the same alarm as a catch.
 */

// Arduino pin number (NOT the nRF GPIO number). D6 maps to P1.11 (= GPIO 43) via g_ADigitalPinMap.
#ifndef TRAP_SENSOR_PIN
#define TRAP_SENSOR_PIN D6
#endif

// A new state must be stable this long before it counts.
#ifndef TRAP_DEBOUNCE_MS
#define TRAP_DEBOUNCE_MS 200
#endif

// 1 (default): contact closed (pin LOW) = trap open/armed, pin HIGH = trap closed (caught).
// 0: inverted, pin HIGH = trap open.
#ifndef TRAP_CLOSED_MEANS_OPEN
#define TRAP_CLOSED_MEANS_OPEN 1
#endif

class TrapSensorModule : public CustomSensorModule
{
  public:
    TrapSensorModule();

  protected:
    bool setupSensor() override;
    bool takeChangeEvent() override;
    void formatState(char *buf, size_t len) override;
    int32_t sensorWakeMs() override;

  private:
    static void isr();
    static bool pinIsHigh();
    static bool pinHighMeansOpen() { return !TRAP_CLOSED_MEANS_OPEN; }

    bool reportedPinHigh = false;  // last debounced (reported) pin level
    uint32_t handledEdgeCount = 0; // edgeCount value already processed by the thread

    // Written by the ISR, read by the thread. A counter instead of a bool flag:
    // the thread never clears it, so an edge arriving during processing cannot be lost.
    static volatile uint32_t lastEdgeMs;
    static volatile uint32_t edgeCount;
};

extern TrapSensorModule *trapSensorModule;

#endif // CUSTOM_SENSOR_TRAP
