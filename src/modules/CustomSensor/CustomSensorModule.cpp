#include "CustomSensorModule.h"

#if CUSTOM_SENSOR_TRAP

CustomSensorModule::CustomSensorModule(const char *threadName) : concurrency::OSThread(threadName) {}

int32_t CustomSensorModule::runOnce()
{
    char state[48];

    if (firstTime) {
        firstTime = false;
        if (!setupSensor()) {
            LOG_ERROR("%s: sensor setup failed, module disabled", CUSTOM_SENSOR_NAME);
            return disable();
        }
        formatState(state, sizeof(state));
        LOG_INFO("%s: init done, state=%s", CUSTOM_SENSOR_NAME, state);
    }

    if (takeChangeEvent()) {
        formatState(state, sizeof(state));
        // Step 1: log only. Sending to the mesh follows in step 2.
        LOG_INFO("%s: state changed -> %s (not sent yet)", CUSTOM_SENSOR_NAME, state);
    }

    // Ask the sensor last, right before returning, to keep the window small in which an
    // interrupt could fire after this check (OSThread::run() applies our return value afterwards).
    int32_t next = sensorWakeMs();
    return next < CUSTOM_SENSOR_IDLE_INTERVAL_MS ? next : CUSTOM_SENSOR_IDLE_INTERVAL_MS;
}

#endif // CUSTOM_SENSOR_TRAP
