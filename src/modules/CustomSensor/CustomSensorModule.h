#pragma once

#include "configuration.h"

#if CUSTOM_SENSOR_TRAP

#include "concurrency/OSThread.h"
#include <stddef.h>
#include <stdint.h>

// Name used as prefix in log lines and (later) in mesh text messages.
#ifndef CUSTOM_SENSOR_NAME
#define CUSTOM_SENSOR_NAME "Falle"
#endif

// Longest time the thread sleeps without being woken by an interrupt.
#ifndef CUSTOM_SENSOR_IDLE_INTERVAL_MS
#define CUSTOM_SENSOR_IDLE_INTERVAL_MS (60 * 1000)
#endif

/**
 * Common base for our custom sensor modules (trap, later wind and fence).
 *
 * The subclass only does acquisition (pins, interrupts, debouncing, state text).
 * This base owns everything shared: thread lifecycle and, in later steps,
 * sending, the status schedule and the battery text.
 *
 * Threading: OSThread is cooperative. runOnce() is called from the main loop and
 * returns the delay (ms) until it wants to run again. Interrupt handlers must only
 * set volatile flags and wake the main loop; all real work happens in runOnce().
 */
class CustomSensorModule : protected concurrency::OSThread
{
  public:
    explicit CustomSensorModule(const char *threadName);

  protected:
    /// One-time hardware setup. Return false to disable the module.
    virtual bool setupSensor() = 0;

    /// Return true exactly once per debounced state change that must be reported.
    virtual bool takeChangeEvent() = 0;

    /// Write the current (last reported) state as short text, e.g. "ZU".
    virtual void formatState(char *buf, size_t len) = 0;

    /// Milliseconds until the sensor needs runOnce() again (e.g. debounce pending), INT32_MAX if nothing pending.
    virtual int32_t sensorWakeMs() { return INT32_MAX; }

    int32_t runOnce() override;

  private:
    bool firstTime = true;
};

#endif // CUSTOM_SENSOR_TRAP
