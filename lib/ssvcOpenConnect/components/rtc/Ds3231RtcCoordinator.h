#ifndef DS3231_RTC_COORDINATOR_H
#define DS3231_RTC_COORDINATOR_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

/**
 * Optional DS3231 on I²C: restore UTC on boot when system time invalid, push NTP time to RTC when synced.
 * All I²C and multi-byte RTC work runs on a dedicated FreeRTOS task (non-blocking for caller).
 * Config: OpenConnectHardwareSettingsService (rtcEnabled, ds3231I2cAddress).
 */
class Ds3231RtcCoordinator {
public:
    static Ds3231RtcCoordinator& instance();

    /** Schedules bootstrap on the RTC worker (returns immediately — no I²C on caller task). */
    void bootstrapAfterI2c();

    /** Notify worker to copy system UTC → DS3231 (safe from SNTP tcpip thread). */
    void queueSyncFromSystemUtc();

private:
    Ds3231RtcCoordinator() = default;

    static void workerTaskEntry(void* arg);

    void ensureWorkerTask();
    void registerSntpNotificationOnce();
    void performBootstrapFromRtc();
    void performSyncFromSystemUtc();

    TaskHandle_t _worker = nullptr;
    bool _sntpHookRegistered = false;
};

#endif
