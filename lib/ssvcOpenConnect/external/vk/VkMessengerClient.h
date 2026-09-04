#ifndef VK_MESSENGER_CLIENT_H
#define VK_MESSENGER_CLIENT_H

#include <Arduino.h>
#include "core/rectification/RectificationProcess.h"
#include "core/rectification/RectificationTypes.h"

class VkSettingsService;

/**
 * Outgoing VK community API (messages + optional wall) on a dedicated FreeRTOS task (core 1).
 */
class VkMessengerClient {
public:
    static VkMessengerClient& instance();

    bool init(VkSettingsService* settingsService);
    void shutoff();
    bool isInitialized() const { return _initialized; }

    /** Non-blocking; may drop if queue full */
    bool tryEnqueueAlert(const char* utf8Line);

    VkMessengerClient(const VkMessengerClient&) = delete;
    VkMessengerClient& operator=(const VkMessengerClient&) = delete;

private:
    VkMessengerClient();
    ~VkMessengerClient();

    static void workerTask(void* param);
    void runLoop();

    VkSettingsService* _settingsService = nullptr;
    TaskHandle_t _taskHandle = nullptr;
    SemaphoreHandle_t _httpMutex = nullptr;
    void* _alertQueue = nullptr;  // QueueHandle_t
    bool _initialized = false;

    int64_t _peer = 0;
    /** VK community tokens (vk1.a.…) are often 200+ chars; keep headroom for URL form + API. */
    char _token[512]{};
    char _apiVer[16]{};
    char _groupId[16]{};
    bool _liveEnabled = false;
    bool _alertsEnabled = false;
    bool _summaryEnabled = false;
    bool _wallPostEnabled = false;

    uint32_t _liveMessageId = 0;
    uint32_t _alertMessageId = 0;
    char _pendingAlertLine[144]{};
    bool _hasPendingAlert = false;
    /** Skip live edit/send until millis() reaches this (VK flood / rate limit). */
    uint32_t _liveVkBackoffUntilMs = 0;
    /** After flood/rate edit failure: retry edit once backoff ends (do not clear message_id). */
    bool _liveEditRetryAfterBackoff = false;
    RectificationTypes::ProcessState _lastRectState = RectificationTypes::ProcessState::IDLE;
    /** Last successfully captured rectification snapshot (worker task only); used on mutex-timeout. */
    RectificationProcess::Snapshot _lastRectSnapshot{};
    /** Wrap-safe: true while the flood/rate-limit backoff deadline has not been reached yet. */
    bool isVkBackoffActive(const uint32_t nowMs) const {
        return _liveVkBackoffUntilMs != 0 && static_cast<int32_t>(nowMs - _liveVkBackoffUntilMs) < 0;
    }

    // Large scratch buffers: only used from workerTask while _httpMutex is held (not stack).
    static constexpr size_t kFormCap = 10240;
    static constexpr size_t kRespCap = 2048;
    static constexpr size_t kLiveBodyCap = 1400;
    static constexpr size_t kSummaryCap = 4096;
    static constexpr size_t kFormatDataCap = 1024;
    char _formBuf[kFormCap]{};
    char _respBuf[kRespCap]{};
    char _liveBodyBuf[kLiveBodyCap]{};
    char _summaryBuf[kSummaryCap]{};
    /** VK messages.send/edit optional format_data JSON (UTF-16 offsets). */
    char _formatDataBuf[kFormatDataCap]{};
    /** Rectification summary format_data (VkMessenger task only; not concurrent with live body in same call). */
    char _summaryFmtScratch[kFormatDataCap]{};

    void reloadSettingsSnapshot();
    /** Best-effort rectification snapshot refresh; on failure keeps the last valid one. */
    bool refreshRectSnapshot();
    /** Cached HTTPS probe to api.vk.com (utils.getServerTime, no token). */
    static bool isVkApiReachable();
    bool vkFormPost(const char* method, const char* formBody, char* respBuf, size_t respCap);
    bool sendMessageToPeer(int64_t peer, const char* text, const char* keyboardJson, const char* formatDataJson,
                           uint32_t* outMessageId);
    bool deleteMessage(int64_t peer, uint32_t messageId);
    bool editLiveMessage(const char* text, const char* keyboardJson, const char* formatDataJson);
    /** Apply VK API backoff after flood / rate-limit errors (ms from now). */
    void applyLiveVkBackoff(uint32_t durationMs);
    void buildLiveStatusText(char* buf, size_t cap, char* fmtOut, size_t fmtCap);
    void flushAlertMessageIfDue(uint32_t nowMs);
    void runLiveCycle(uint32_t nowMs);
    void buildRectificationSummary(char* buf, size_t cap, char* fmtOut, size_t fmtCap) const;
    bool postWall(const char* text);

    static constexpr auto TAG = "VkMessenger";
};

#endif
