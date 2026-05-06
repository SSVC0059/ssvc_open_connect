#include "Ds3231RtcCoordinator.h"

#include "Ds3231Rtc.h"

#include "components/subsystem/I2CBusSubsystem.h"
#include "core/StatefulServices/OpenConnectHardwareSettingsService/OpenConnectHardwareConfig.h"
#include "core/StatefulServices/OpenConnectHardwareSettingsService/OpenConnectHardwareSettingsService.h"

#include <Wire.h>
#include <cstdint>
#include <ctime>
#include <esp_log.h>
#include <freertos/task.h>
#include <sys/time.h>

extern "C" {
typedef void (*sntp_sync_time_cb_t)(struct timeval *tv);
void sntp_set_time_sync_notification_cb(sntp_sync_time_cb_t callback);
}

namespace {

constexpr const char* TAG = "Ds3231Rtc";

/** Treat MCU clock as unset before 2020-01-01 UTC when deciding to load from RTC. */
constexpr time_t kMinTrustedEpoch = 1577836800;

constexpr uint32_t kNotifyBootstrap = 1u << 0;
constexpr uint32_t kNotifySync = 1u << 1;

bool isLeapYear(int y) {
    return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}

/** 1-based month/day; returns day-of-year 1..366. */
int dayOfYear(int year, int month, int day) {
    static const int md[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    int doy = md[month - 1] + day;
    if (month > 2 && isLeapYear(year)) {
        doy++;
    }
    return doy;
}

/** Gregorian UTC → Unix epoch without setenv/TZ (no blocking env or long critical sections). */
time_t utcYmdhmsToEpoch(int year, int month, int day, int hour, int min, int sec) {
    if (month < 1 || month > 12 || day < 1 || day > 31 || hour < 0 || hour > 23 || min < 0 || min > 59 ||
        sec < 0 || sec > 60) {
        return static_cast<time_t>(-1);
    }
    int64_t days = 0;
    for (int yy = 1970; yy < year; ++yy) {
        days += isLeapYear(yy) ? 366 : 365;
    }
    days += static_cast<int64_t>(dayOfYear(year, month, day)) - 1;
    const int64_t secs = days * 86400LL + static_cast<int64_t>(hour) * 3600LL + static_cast<int64_t>(min) * 60LL +
                         static_cast<int64_t>(sec);
    return static_cast<time_t>(secs);
}

time_t utcTmToEpoch(const struct tm& utc) {
    return utcYmdhmsToEpoch(utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday, utc.tm_hour, utc.tm_min, utc.tm_sec);
}

void lwipSntpSyncNotifyCb(struct timeval* tv) {
    (void)tv;
    Ds3231RtcCoordinator::instance().queueSyncFromSystemUtc();
}

} // namespace

Ds3231RtcCoordinator& Ds3231RtcCoordinator::instance() {
    static Ds3231RtcCoordinator inst;
    return inst;
}

void Ds3231RtcCoordinator::workerTaskEntry(void* arg) {
    auto* self = static_cast<Ds3231RtcCoordinator*>(arg);
    for (;;) {
        uint32_t bits = 0;
        (void)xTaskNotifyWait(0, UINT32_MAX, &bits, portMAX_DELAY);
        if ((bits & kNotifyBootstrap) != 0) {
            self->performBootstrapFromRtc();
        }
        if ((bits & kNotifySync) != 0) {
            self->performSyncFromSystemUtc();
        }
    }
}

void Ds3231RtcCoordinator::ensureWorkerTask() {
    if (_worker != nullptr) {
        return;
    }
    constexpr uint32_t stackWords = 4096;
    const BaseType_t ok =
        xTaskCreatePinnedToCore(workerTaskEntry, "ds3231_rtc", stackWords, this, tskIDLE_PRIORITY + 2, &_worker, 1);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "Failed to create RTC worker task");
        _worker = nullptr;
    }
}

void Ds3231RtcCoordinator::registerSntpNotificationOnce() {
    if (_sntpHookRegistered) {
        return;
    }
    sntp_set_time_sync_notification_cb(lwipSntpSyncNotifyCb);
    _sntpHookRegistered = true;
    ESP_LOGI(TAG, "SNTP sync notification registered for DS3231");
}

void Ds3231RtcCoordinator::performSyncFromSystemUtc() {
    bool rtcOn = false;
    uint8_t addr = 0x68;
    OpenConnectHardwareSettingsService::instance().read([&](OpenConnectHardwareConfig& cfg) {
        rtcOn = cfg.rtcEnabled;
        addr = cfg.ds3231I2cAddress;
    });
    if (!rtcOn) {
        return;
    }

    TwoWire* w = I2CBusSubsystem::getInstance().getWire();
    if (!w) {
        ESP_LOGW(TAG, "No I²C bus — skip RTC write");
        return;
    }

    Ds3231Rtc rtc;
    rtc.setBus(w, addr);
    if (!rtc.probe()) {
        ESP_LOGW(TAG, "DS3231 not responding — skip RTC write");
        return;
    }

    const time_t now = time(nullptr);
    struct tm utc {};
    gmtime_r(&now, &utc);
    if (!rtc.writeUtcTm(utc)) {
        ESP_LOGW(TAG, "Failed to write DS3231 from system UTC");
        return;
    }
    ESP_LOGI(TAG, "DS3231 updated from system UTC");
}

void Ds3231RtcCoordinator::queueSyncFromSystemUtc() {
    bool rtcOn = false;
    OpenConnectHardwareSettingsService::instance().read(
        [&](OpenConnectHardwareConfig& cfg) { rtcOn = cfg.rtcEnabled; });
    if (!rtcOn || _worker == nullptr) {
        return;
    }
    (void)xTaskNotify(_worker, kNotifySync, eSetBits);
}

void Ds3231RtcCoordinator::performBootstrapFromRtc() {
    bool rtcOn = false;
    uint8_t addr = 0x68;
    OpenConnectHardwareSettingsService::instance().read([&](OpenConnectHardwareConfig& cfg) {
        rtcOn = cfg.rtcEnabled;
        addr = cfg.ds3231I2cAddress;
    });

    if (!rtcOn) {
        return;
    }

    TwoWire* w = I2CBusSubsystem::getInstance().getWire();
    if (!w) {
        ESP_LOGW(TAG, "RTC enabled but I²C bus inactive");
        return;
    }

    Ds3231Rtc rtc;
    rtc.setBus(w, addr);
    if (!rtc.probe()) {
        ESP_LOGW(TAG, "DS3231 not found at 0x%02X", static_cast<unsigned>(addr));
        return;
    }

    const time_t sysNow = time(nullptr);
    const bool sysUntrusted = sysNow < kMinTrustedEpoch;
    bool osf = false;
    if (rtc.readOscillatorStoppedFlag(osf) && osf) {
        if (sysUntrusted) {
            ESP_LOGW(TAG, "DS3231 oscillator-stop flag set while system time is untrusted — check battery");
        } else {
            ESP_LOGI(TAG, "DS3231 oscillator-stop flag set (will be cleared on next successful sync)");
        }
    }

    if (sysUntrusted) {
        struct tm utc {};
        if (!rtc.readUtcTm(&utc)) {
            ESP_LOGW(TAG, "Failed to read DS3231");
            return;
        }
        const time_t rtcEpoch = utcTmToEpoch(utc);
        if (rtcEpoch == static_cast<time_t>(-1)) {
            ESP_LOGW(TAG, "RTC time conversion failed");
            return;
        }
        struct timeval tv = {.tv_sec = rtcEpoch, .tv_usec = 0};
        settimeofday(&tv, nullptr);
        ESP_LOGI(TAG, "System time restored from DS3231 (UTC)");
        return;
    }

    performSyncFromSystemUtc();
}

void Ds3231RtcCoordinator::bootstrapAfterI2c() {
    bool rtcOn = false;
    OpenConnectHardwareSettingsService::instance().read(
        [&](OpenConnectHardwareConfig& cfg) { rtcOn = cfg.rtcEnabled; });

    if (!rtcOn) {
        return;
    }

    ensureWorkerTask();
    registerSntpNotificationOnce();

    if (_worker == nullptr) {
        return;
    }
    (void)xTaskNotify(_worker, kNotifyBootstrap, eSetBits);
}
