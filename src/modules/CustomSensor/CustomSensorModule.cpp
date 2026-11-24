#include "CustomSensorModule.h"

#if CUSTOM_SENSOR_TRAP

#include "MeshService.h"
#include "PowerStatus.h"
#include "Router.h"
#include "mesh/Channels.h"
#include <string.h>

CustomSensorModule::CustomSensorModule(const char *threadName) : concurrency::OSThread(threadName) {}

bool CustomSensorModule::sendText(const char *text)
{
    // Never broadcast sensor data on the public default channel (default PSK and preset name, e.g. "LongFast").
    if (channels.isDefaultChannel(0)) {
        LOG_WARN("%s: NOT sent - primary channel is the public default channel. Configure a private primary channel (own "
                 "name/PSK)!",
                 CUSTOM_SENSOR_NAME);
        return false;
    }

    // allocForSending() already sets from, to = broadcast, channel 0 (primary, zeroed), hop_limit and id.
    meshtastic_MeshPacket *p = router->allocForSending();
    p->decoded.portnum = meshtastic_PortNum_TEXT_MESSAGE_APP;
    p->want_ack = false;

    size_t size = strlen(text);
    if (size > meshtastic_Constants_DATA_PAYLOAD_LEN)
        size = meshtastic_Constants_DATA_PAYLOAD_LEN;
    memcpy(p->decoded.payload.bytes, text, size);
    p->decoded.payload.size = size;

    uint32_t id = p->id;
    service->sendToMesh(p, RX_SRC_LOCAL, true); // ccToPhone: also show it in the app
    LOG_INFO("%s: sent id=0x%08lx '%s'", CUSTOM_SENSOR_NAME, (unsigned long)id, text);
    return true;
}

bool CustomSensorModule::sendReport(const char *prefix)
{
    char state[sizeof(lastSentState)];
    formatState(state, sizeof(state));

    char battery[32];
    bool haveBattery = false;
#if defined(HAS_PMU) || defined(BATTERY_PIN)
    if (powerStatus && powerStatus->getHasBattery()) {
        int mv = powerStatus->getBatteryVoltageMv();
        snprintf(battery, sizeof(battery), "Akku %u%% %d.%02dV", (unsigned)powerStatus->getBatteryChargePercent(), mv / 1000,
                 (mv % 1000) / 10);
        haveBattery = true;
    }
#endif
    if (!haveBattery)
        snprintf(battery, sizeof(battery), "USB");

    char msg[120];
    snprintf(msg, sizeof(msg), "%s %s %s | %s", CUSTOM_SENSOR_NAME, prefix, state, battery);
    if (!sendText(msg))
        return false;

    strncpy(lastSentState, state, sizeof(lastSentState) - 1);
    lastSentState[sizeof(lastSentState) - 1] = '\0';
    return true;
}

bool CustomSensorModule::sendEventIfChanged()
{
    char state[sizeof(lastSentState)];
    formatState(state, sizeof(state));
    if (strcmp(state, lastSentState) == 0) {
        // Flapped back to what the mesh already knows: nothing to send.
        LOG_INFO("%s: state %s equals last sent state, no message", CUSTOM_SENSOR_NAME, state);
        return false;
    }
    // Also counts as attempt if sendText() refused (default channel), so the warning is rate limited too.
    sendReport("jetzt");
    return true;
}

int32_t CustomSensorModule::runOnce()
{
    char state[sizeof(lastSentState)];

    if (firstTime) {
        firstTime = false;
        if (!setupSensor()) {
            LOG_ERROR("%s: sensor setup failed, module disabled", CUSTOM_SENSOR_NAME);
            return disable();
        }
        formatState(state, sizeof(state));
        LOG_INFO("%s: init done, state=%s", CUSTOM_SENSOR_NAME, state);
        // Nothing is sent at boot in step 2 (start message follows in step 3). The boot state is only the
        // reference for comparing; the first change after boot is always sent immediately (no gap yet).
        strncpy(lastSentState, state, sizeof(lastSentState) - 1);
        lastSentState[sizeof(lastSentState) - 1] = '\0';
    }

    if (takeChangeEvent()) {
        formatState(state, sizeof(state));
        LOG_INFO("%s: state changed -> %s", CUSTOM_SENSOR_NAME, state);
        eventPending = true;
    }

    // Unsigned subtraction keeps this correct across the millis() wrap after ~49 days.
    uint32_t sinceLastEvent = millis() - lastEventSentMs;
    if (eventPending && (!eventSentOnce || sinceLastEvent >= CUSTOM_SENSOR_MIN_EVENT_GAP_MS)) {
        eventPending = false;
        if (sendEventIfChanged()) {
            // Start the gap only if a message was attempted.
            eventSentOnce = true;
            lastEventSentMs = millis();
        }
    }

    int32_t next = CUSTOM_SENSOR_IDLE_INTERVAL_MS;
    if (eventPending) {
        int32_t gapLeft = (int32_t)(CUSTOM_SENSOR_MIN_EVENT_GAP_MS - sinceLastEvent);
        if (gapLeft < next)
            next = gapLeft < 1 ? 1 : gapLeft;
    }

    // Ask the sensor last, right before returning, to keep the window small in which an
    // interrupt could fire after this check (OSThread::run() applies our return value afterwards).
    int32_t sensorNext = sensorWakeMs();
    return sensorNext < next ? sensorNext : next;
}

#endif // CUSTOM_SENSOR_TRAP
