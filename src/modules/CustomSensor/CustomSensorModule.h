#pragma once

#include "configuration.h"

#if CUSTOM_SENSOR_TRAP || CUSTOM_SENSOR_WIND

#if CUSTOM_SENSOR_TRAP && CUSTOM_SENSOR_WIND
#error "Only one custom sensor module per build: CUSTOM_SENSOR_TRAP or CUSTOM_SENSOR_WIND"
#endif

#include "concurrency/OSThread.h"
#include <stddef.h>
#include <stdint.h>

// Name used as prefix in log lines and in mesh text messages.
#ifndef CUSTOM_SENSOR_NAME
#if CUSTOM_SENSOR_WIND
#define CUSTOM_SENSOR_NAME "Wind"
#else
#define CUSTOM_SENSOR_NAME "Falle"
#endif
#endif

// 1 = only log what would be sent, never transmit (for first hardware tests). 0 = normal operation.
#ifndef CUSTOM_SENSOR_LOG_ONLY
#define CUSTOM_SENSOR_LOG_ONLY 0
#endif

// Longest time the thread sleeps without being woken by an interrupt.
#ifndef CUSTOM_SENSOR_IDLE_INTERVAL_MS
#define CUSTOM_SENSOR_IDLE_INTERVAL_MS (60 * 1000)
#endif

// Minimum gap between two event messages. Changes inside the gap are merged: after the gap
// only the then current (final) state is sent, and only if it differs from the last sent one.
#ifndef CUSTOM_SENSOR_MIN_EVENT_GAP_MS
#define CUSTOM_SENSOR_MIN_EVENT_GAP_MS 10000
#endif

// Daily status messages at these two full hours, Europe/Berlin local time (A < B).
#ifndef CUSTOM_SENSOR_STATUS_HOUR_A
#define CUSTOM_SENSOR_STATUS_HOUR_A 6
#endif
#ifndef CUSTOM_SENSOR_STATUS_HOUR_B
#define CUSTOM_SENSOR_STATUS_HOUR_B 18
#endif
static_assert(0 <= CUSTOM_SENSOR_STATUS_HOUR_A && CUSTOM_SENSOR_STATUS_HOUR_A < CUSTOM_SENSOR_STATUS_HOUR_B &&
                  CUSTOM_SENSOR_STATUS_HOUR_B <= 23,
              "CUSTOM_SENSOR_STATUS_HOUR_A/B must satisfy 0 <= A < B <= 23");

// Status interval while no valid time is known (counted from boot / start message).
#ifndef CUSTOM_SENSOR_FALLBACK_INTERVAL_MS
#define CUSTOM_SENSOR_FALLBACK_INTERVAL_MS (6 * 3600 * 1000UL)
#endif

// 1 = send all messages with want_ack. For broadcasts this means: up to 3 retransmissions until a
// neighbour is heard relaying the packet ("implicit ACK", first hop only, not end-to-end).
// 0 = send once, no retransmission.
#ifndef CUSTOM_SENSOR_WANT_ACK
#define CUSTOM_SENSOR_WANT_ACK 1
#endif

// The start message is sent this long after sensor setup, so the mesh has settled.
#ifndef CUSTOM_SENSOR_BOOT_REPORT_DELAY_MS
#define CUSTOM_SENSOR_BOOT_REPORT_DELAY_MS 60000
#endif

/**
 * Common base for our custom sensor modules (trap, later wind and fence).
 *
 * The subclass only does acquisition (pins, interrupts, debouncing, state text).
 * This base owns everything shared: thread lifecycle, sending (text message,
 * broadcast, primary channel), event rate limiting, the battery text, the start
 * message and the status schedule (06:00/18:00 Europe/Berlin, fallback every 6 h).
 *
 * Two report modes, chosen by the subclass via reportIntervalMs():
 *  - 0 (default, trap): change events + start message + status schedule.
 *  - > 0 (periodic, wind): every interval run a measurement (startMeasurement/runMeasurement)
 *    and send the text from formatReport(). No start message, no 06/18 schedule.
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

    /// Periodic mode: report interval in ms. 0 (default) = trap mode (events + status schedule).
    virtual uint32_t reportIntervalMs() { return 0; }

    /// Periodic mode: begin a measurement window (e.g. switch the sensor on).
    virtual void startMeasurement() {}

    /// Periodic mode: advance the measurement. Return ms until the next call, or 0 when finished.
    /// Must never block; long waits are expressed through the return value.
    virtual int32_t runMeasurement() { return 0; }

    /// Periodic mode: write the complete message text. battery is "87%" or "USB".
    virtual void formatReport(char *buf, size_t len, const char *battery)
    {
        (void)battery;
        if (len)
            buf[0] = '\0';
    }

    /// Battery charge as "87%", or "USB" if no battery is detected.
    void batteryPercentText(char *buf, size_t len);

    /// Send plain text as TEXT_MESSAGE_APP broadcast on the primary channel. Returns true if sent.
    bool sendText(const char *text);

    int32_t runOnce() override;

  private:
    bool firstTime = true;

    bool eventPending = false;    // a change is waiting for the minimum gap to pass
    bool eventSentOnce = false;   // false until the first event message was attempted
    uint32_t lastEventSentMs = 0; // millis() of the last event message attempt
    char lastSentState[48] = {0}; // state text of the last message that went out

    bool bootReportPending = false; // start message not sent yet
    uint32_t bootReportDueMs = 0;   // millis() when the start message is due
    uint32_t lastStatusMs = 0;      // millis() of the last start/status message (0 = boot)
    int32_t lastSlot = -1;          // last passed schedule slot, -1 = no valid time (yet)
    int8_t timeMode = -1;           // for logging only: -1 unknown, 0 fallback, 1 schedule

    bool measuring = false;    // periodic mode: a measurement window is running
    uint32_t nextCycleMs = 0;  // periodic mode: millis() when the next window starts
    uint32_t measureDueMs = 0; // periodic mode: millis() of the next runMeasurement() call

    /// Periodic mode: start windows on time, drive the measurement, send the result.
    void handlePeriodic();

    /// Start message and status schedule. Called on every runOnce().
    void handleSchedule();

    /// Send the current state as event message, unless it equals the last sent state.
    /// Returns false if nothing was attempted because the state is unchanged.
    bool sendEventIfChanged();

    /// Build "<NAME> <prefix> <state> | <battery>" and send it. Returns true if sent.
    bool sendReport(const char *prefix);
};

#endif // CUSTOM_SENSOR_TRAP || CUSTOM_SENSOR_WIND
