#pragma once

#include "configuration.h"

#if CUSTOM_SENSOR_TRAP

#include "concurrency/OSThread.h"
#include <stddef.h>
#include <stdint.h>

// Name used as prefix in log lines and in mesh text messages.
#ifndef CUSTOM_SENSOR_NAME
#define CUSTOM_SENSOR_NAME "Falle"
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

    bool eventPending = false;    // a change is waiting for the minimum gap to pass
    bool eventSentOnce = false;   // false until the first event message was attempted
    uint32_t lastEventSentMs = 0; // millis() of the last event message attempt
    char lastSentState[48] = {0}; // state text of the last message that went out

    bool bootReportPending = false; // start message not sent yet
    uint32_t bootReportDueMs = 0;   // millis() when the start message is due
    uint32_t lastStatusMs = 0;      // millis() of the last start/status message (0 = boot)
    int32_t lastSlot = -1;          // last passed schedule slot, -1 = no valid time (yet)
    int8_t timeMode = -1;           // for logging only: -1 unknown, 0 fallback, 1 schedule

    /// Start message and status schedule. Called on every runOnce().
    void handleSchedule();

    /// Send the current state as event message, unless it equals the last sent state.
    /// Returns false if nothing was attempted because the state is unchanged.
    bool sendEventIfChanged();

    /// Build "<NAME> <prefix> <state> | <battery>" and send it. Returns true if sent.
    bool sendReport(const char *prefix);

    /// Send plain text as TEXT_MESSAGE_APP broadcast on the primary channel. Returns true if sent.
    bool sendText(const char *text);
};

#endif // CUSTOM_SENSOR_TRAP
