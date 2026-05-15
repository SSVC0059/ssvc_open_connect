#include "TelegramBotClient.h"
#include "core/SsvcConnector.h"
#include "core/SubsystemManager/SubsystemManager.h"
#include "core/StatefulServices/SensorDataService/SensorDataService.h"
#include "core/rectification/RectificationProcess.h"
#include "core/SsvcSettings/SsvcSettings.h"

namespace {

constexpr time_t kMinValidSystemTime = 1704067200;  // 2024-01-01 UTC

bool waitWifiConnected(uint32_t timeoutMs) {
    const uint32_t deadline = millis() + timeoutMs;
    uint32_t lastLogMs = 0;
    while (!WiFi.isConnected() && millis() < deadline) {
        const uint32_t now = millis();
        if (now - lastLogMs >= 30000) {
            ESP_LOGV("TelegramBotClient", "Waiting for WiFi...");
            lastLogMs = now;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    return WiFi.isConnected();
}

bool waitSystemTime(uint32_t timeoutMs) {
    const uint32_t deadline = millis() + timeoutMs;
    uint32_t lastLogMs = 0;
    while (time(nullptr) < kMinValidSystemTime && millis() < deadline) {
        const uint32_t now = millis();
        if (now - lastLogMs >= 30000) {
            ESP_LOGV("TelegramBotClient", "Waiting for SNTP time sync...");
            lastLogMs = now;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    const bool ok = time(nullptr) >= kMinValidSystemTime;
    if (!ok) {
        ESP_LOGW("TelegramBotClient", "System time not synced — TLS may still work (setInsecure)");
    }
    return ok;
}

}  // namespace

namespace {

bool waitTelegramSendWindow(FastBot2& bot, uint32_t maxWaitMs) {
    const uint32_t start = millis();
    while (bot.isPolling()) {
        if (millis() - start >= maxWaitMs) {
            ESP_LOGW("TelegramBotClient",
                     "Timeout waiting for Telegram HTTP slot (isPolling)");
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    return true;
}

void logTelegramSendFailure(const char* ctx, fb::Result& result) {
    if (result.isError()) {
        String err;
        String code;
        result.getError().toString(err);
        result.getErrorCode().toString(code);
        ESP_LOGE("TelegramBotClient", "%s: Telegram API error (code=%s): %s", ctx,
                 code.length() ? code.c_str() : "?",
                 err.length() ? err.c_str() : "(no description)");
    } else {
        ESP_LOGE(
            "TelegramBotClient",
            "%s: no Telegram API payload (TLS/HTTP failure or timeout). WiFi=%d IP=%s",
            ctx, static_cast<int>(WiFi.status()),
            WiFi.localIP().toString().c_str());
    }
}

}  // namespace

bool TelegramBotClient::probeTelegramApi() {
    if (token.isEmpty()) {
        ESP_LOGW("TelegramBotClient", "probeTelegramApi: empty token");
        return false;
    }

    if (!waitTelegramSendWindow(_bot, MAX_SEND_TIME_MS)) {
        ESP_LOGE("TelegramBotClient", "probeTelegramApi: HTTP slot busy");
        return false;
    }

    fb::Result result = _bot.sendCommand(tg_cmd::getMe, true);
    if (result && !result.isError()) {
        ESP_LOGI("TelegramBotClient", "probeTelegramApi: getMe OK");
        return true;
    }
    logTelegramSendFailure("probeTelegramApi", result);
    return false;
}

TelegramBotClient& TelegramBotClient::bot() {
    static TelegramBotClient instance;
    return instance;
}

TelegramBotClient::TelegramBotClient() = default;

static constexpr auto CALLBACK_RECT_STOP = "/rect_stop";

String TelegramBotClient::createControlKeyboard() const {
    if (cachedStatus.showControlButtons) {
        fb::InlineMenu keyboard;
        // Кнопка, которая при нажатии отправит CALLBACK_RECT_STOP
        keyboard.addButton("❌ Остановить процесс", CALLBACK_RECT_STOP);
        // Можно добавить дополнительные кнопки, например:
        // keyboard.addButton("🔄 Перезапустить", "/rect_restart");
    }
    return "";
}

void TelegramBotClient::initTelemetryTaskSender()
{
    if (_telemetryTaskHandle != nullptr) {
        ESP_LOGW("TelegramBotClient", "Telemetry task already running");
        return;
    }

    xTaskCreatePinnedToCore(
        statusMessageSender,
        "TelegramBotClient",
        5120,
        this,
        tskIDLE_PRIORITY,
        &_telemetryTaskHandle,
        1
    );
}

TelegramBotClient::~TelegramBotClient() {
    shutoff();
}


bool TelegramBotClient::init(TelegramSettingsService* settingsService) {
    return startBoot(settingsService);
}

bool TelegramBotClient::startBoot(TelegramSettingsService* settingsService, BootDoneFn onDone,
                                  void* onDoneCtx) {
    if (_initialized) {
        ESP_LOGW("TelegramBotClient", "Already initialized");
        if (onDone) onDone(true, onDoneCtx);
        return true;
    }
    if (_bootTaskHandle != nullptr) {
        ESP_LOGW("TelegramBotClient", "Boot already in progress");
        if (onDone) onDone(false, onDoneCtx);
        return false;
    }

    _settingsService = settingsService;
    if (!_settingsService) {
        ESP_LOGE("TelegramBotClient", "Settings service is not provided!");
        if (onDone) onDone(false, onDoneCtx);
        return false;
    }

    _bootDoneFn  = onDone;
    _bootDoneCtx = onDoneCtx;
    _bootAbort   = false;

    constexpr uint32_t kBootStack = 8192;
    const BaseType_t   ok         = xTaskCreatePinnedToCore(
        bootTaskEntry, "tg_boot", kBootStack, this, tskIDLE_PRIORITY + 1, &_bootTaskHandle, 1);

    if (ok != pdPASS) {
        ESP_LOGE("TelegramBotClient", "Failed to create boot task");
        _bootTaskHandle = nullptr;
        _bootDoneFn     = nullptr;
        _bootDoneCtx    = nullptr;
        if (onDone) onDone(false, onDoneCtx);
        return false;
    }

    ESP_LOGI("TelegramBotClient", "Boot task started (non-blocking)");
    return true;
}

void TelegramBotClient::bootTaskEntry(void* param) {
    auto* self = static_cast<TelegramBotClient*>(param);
    const bool ok = self->runBootSequence();
    self->_bootTaskHandle = nullptr;
    self->finishBoot(ok);
    vTaskDelete(nullptr);
}

void TelegramBotClient::finishBoot(bool ok) {
    if (_bootDoneFn) {
        BootDoneFn fn  = _bootDoneFn;
        void*        ctx = _bootDoneCtx;
        _bootDoneFn      = nullptr;
        _bootDoneCtx     = nullptr;
        fn(ok, ctx);
    } else if (!ok) {
        SubsystemManager::instance().disableSubsystem("telegram_bot");
    }
}

bool TelegramBotClient::runBootSequence() {
    if (_bootAbort) {
        return false;
    }

    constexpr uint32_t kWifiWaitMs = 30000;
    if (!waitWifiConnected(kWifiWaitMs)) {
        ESP_LOGE("TelegramBotClient", "WiFi not connected after %u ms", kWifiWaitMs);
        return false;
    }

    // Дать стеку/TCP стабилизироваться после DHCP.
    vTaskDelay(pdMS_TO_TICKS(3000));
    if (_bootAbort) return false;

    (void)waitSystemTime(15000);

    _settingsService->read([this](const TelegramSettings& settings) {
        token  = settings.botToken;
        chatID = settings.chatId;
    });

    if (token.isEmpty()) {
        ESP_LOGE("TelegramBotClient", "Bot token is empty");
        return false;
    }

    ESP_LOGI("TelegramBotClient", "Chat ID: %s, token length: %u chars, heap=%u",
             chatID.c_str(), static_cast<unsigned>(token.length()),
             static_cast<unsigned>(ESP.getFreeHeap()));

    _bot.setToken(token.c_str());
    _bot.setTimeout(12000);

    constexpr int kProbeAttempts = 3;
    bool          probeOk        = false;
    for (int attempt = 1; attempt <= kProbeAttempts && !probeOk && !_bootAbort; ++attempt) {
        ESP_LOGI("TelegramBotClient", "Probing Telegram API via FastBot2 (attempt %d/%d)...",
                 attempt, kProbeAttempts);
        ESP_LOGI("TelegramBotClient", "Free heap before probe: %u",
                 static_cast<unsigned>(ESP.getFreeHeap()));

        if (_botMutex != nullptr) {
            MutexLocker lock(_botMutex);
            probeOk = probeTelegramApi();
        } else {
            probeOk = probeTelegramApi();
        }

        if (!probeOk && attempt < kProbeAttempts) {
            vTaskDelay(pdMS_TO_TICKS(2000));
        }
    }

    if (_bootAbort || !probeOk) {
        ESP_LOGE("TelegramBotClient", "Telegram API unreachable, boot aborted");
        return false;
    }

    _apiReady = true;
    sendHello();
    initTelemetryTaskSender();
    _initialized = true;
    ESP_LOGI("TelegramBotClient", "Initialized successfully");
    return true;
}

void TelegramBotClient::shutoff() {
    _bootAbort = true;

    if (_bootTaskHandle != nullptr) {
        ESP_LOGI("TelegramBotClient", "Waiting for boot task to stop...");
        for (int i = 0; i < 50 && _bootTaskHandle != nullptr; ++i) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    if (!_initialized && !_apiReady && _bootTaskHandle == nullptr) {
        _bootAbort = false;
        return;
    }

    ESP_LOGI("TelegramBotClient", "Deinitializing TelegramBotClient");

    if (_telemetryTaskHandle != nullptr) {
        vTaskDelete(_telemetryTaskHandle);
        _telemetryTaskHandle = nullptr;
    }

    if (_botMutex != nullptr) {
        vSemaphoreDelete(_botMutex);
        _botMutex = nullptr;
    }

    token = "";
    chatID = "";
    statusMessageID = 0;
    _apiReady    = false;
    _initialized = false;
    _bootAbort   = false;
    _bootDoneFn  = nullptr;
    _bootDoneCtx = nullptr;
}

void TelegramBotClient::statusMessageSender(void* params) {
    const auto manager = static_cast<TelegramBotClient*>(params);
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(5000));

        if (manager->_botMutex != nullptr) {
            try {
                MutexLocker lock(manager->_botMutex);

                std::string msg = manager->createStatusMessage();
                if (manager->statusMessageID != 0)
                {
                    manager->updateMessage(msg, manager->statusMessageID);
                }else
                {
                    if (!WiFi.isConnected()) {
                        ESP_LOGE("TelegramBotClient", "No network!");
                        continue;
                    }
                    manager->statusMessageID = manager->sendMessage(msg);
                }
            } catch (...) {
                ESP_LOGE("TelegramBotClient", "Exception in statusMessageSender");
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
        }
    }
}

void TelegramBotClient::updateSensorInfo() {
    SensorDataService* sensorService = SensorDataService::getInstance();

    if (sensorService == nullptr) {
        if (!cachedStatus.sensorZones.empty()) {
            cachedStatus.sensorZones.clear();
        }
        ESP_LOGE("TelegramBotClient", "SensorDataService is not initialized!");
        return;
    }

    sensorService->read([&](const SensorDataState& currentState) {

        if (currentState.readings_by_zone.empty()) {
            cachedStatus.sensorZones.clear();
            return;
        }

        std::map<std::string, std::vector<std::string>> newSensorZones;
        bool dataChanged = false;

        for (const auto& zonePair : currentState.readings_by_zone) {
            const SensorZone zone = zonePair.first;
            const auto& readings = zonePair.second;
            const std::string zoneName = SensorZoneHelper::toString(zone);
            std::vector<std::string> sensorTexts;

            for (auto it = readings.rbegin(); it != readings.rend(); ++it) {
                const std::string& address = it->first;
                const float sensorValue = it->second;

                std::string shortAddress;
                if (address.length() > 4) {
                    shortAddress = address.substr(address.length() - 4);
                } else {
                    shortAddress = address;
                }

                char tempBuffer[64];
                snprintf(tempBuffer, sizeof(tempBuffer), "  %s: <b>%.2f°C</b>\n",
                             shortAddress.c_str(), sensorValue);

                sensorTexts.emplace_back(tempBuffer);
            }

            if (!sensorTexts.empty()) {
                 newSensorZones[zoneName] = sensorTexts;
            }
        }

        if (cachedStatus.sensorZones.size() != newSensorZones.size() || cachedStatus.sensorZones != newSensorZones) {
            dataChanged = true;
        }

        if (dataChanged) {
            cachedStatus.sensorZones = std::move(newSensorZones);
            cachedStatus.lastUpdateTime = millis();
        }

    });
}

void TelegramBotClient::initializeMessageStructure() {
    if (messageStructureInitialized) return;
    cachedStatus.header = STATUS_HEADER;
    messageStructureInitialized = true;
}


void TelegramBotClient::updateRectificationInfo() {
    const RectificationProcess::Metrics& metrics = RectificationProcess::rectController().getMetrics();

    struct LastValidData {
        float tp1 = 0;
        float tp2 = 0;
        std::string type;
        uint32_t lastValidTime = 0;
    };

    static LastValidData lastValidData;

    const bool hasTp1Data = (metrics.tp1_sap != 0 || metrics.common.tp1 != 0);
    const bool hasTp2Data = (metrics.tp2_sap != 0 || metrics.common.tp2 != 0);
    const bool hasTypeData = !metrics.type.empty();

    if (hasTp1Data) {
        lastValidData.tp1 = (metrics.tp1_sap != 0) ? metrics.tp1_sap : metrics.common.tp1;
    }
    if (hasTp2Data) {
        lastValidData.tp2 = (metrics.tp2_sap != 0) ? metrics.tp2_sap : metrics.common.tp2;
    }
    if (hasTypeData) {
        lastValidData.type = RectificationProcess::translateRectificationStage(metrics.type);
        lastValidData.lastValidTime = millis();
    }

    const bool hasAnyData = hasTp1Data || hasTp2Data || hasTypeData;
    const bool cacheValid = (millis() - lastValidData.lastValidTime < 30000) &&
                          (!lastValidData.type.empty() || lastValidData.tp1 != 0 || lastValidData.tp2 != 0);

    cachedStatus.showControlButtons = hasAnyData || cacheValid;

    if (!hasAnyData && !cacheValid) {
        cachedStatus.rectificationInfo.clear();
        return;
    }

    std::ostringstream buffer;
    cachedStatus.rectificationInfo.clear();

    const time_t now = time(nullptr);
    struct tm timeInfo{};
    localtime_r(&now, &timeInfo);

    char time_buffer[64];
    strftime(time_buffer, sizeof(time_buffer), "%d.%m.%Y %H:%M:%S", &timeInfo);

    buffer << "Время обновления: <b>" << time_buffer << "</b>\n\n";

    if (hasTypeData || !lastValidData.type.empty()) {
        buffer << "<b>"
               << (hasTypeData ? RectificationProcess::translateRectificationStage(metrics.type)
                               : lastValidData.type)
               << "</b>\n";
    }

    if (hasTp1Data || (cacheValid && lastValidData.tp1 != 0)) {
        const float value = hasTp1Data ?
                          (metrics.tp1_sap != 0 ? metrics.tp1_sap : metrics.common.tp1)
                          : lastValidData.tp1;
        buffer << "<b>Колонна:</b> " << std::fixed << std::setprecision(2) << value << "°C\n";
    }

    if (hasTp2Data || (cacheValid && lastValidData.tp2 != 0)) {
        const float value = hasTp2Data ?
                          (metrics.tp2_sap != 0 ? metrics.tp2_sap : metrics.common.tp2)
                          : lastValidData.tp2;
        buffer << "<b>Куб:</b> " << std::fixed << std::setprecision(2) << value << "°C\n";
    }

    if ((!hasTp1Data && lastValidData.tp1 != 0) ||
        (!hasTp2Data && lastValidData.tp2 != 0) ||
        (!hasTypeData && !lastValidData.type.empty())) {
        buffer << "<i>(данные из кэша)</i>\n";
    }
    cachedStatus.rectificationInfo = buffer.str();
}

std::string TelegramBotClient::createStatusMessage() {
    initializeMessageStructure();
    updateRectificationInfo();
    updateSensorInfo();

    std::ostringstream text;
    text << cachedStatus.header;

    if (SsvcConnector::getConnector().uartCommunicationError) {
        text << "⚠️ <b>Нет связи с SSVC</b>\n"
             << "Проверьте подключение по UART и включено ли устройство.\n\n";
    }

    if (!cachedStatus.rectificationInfo.empty()) {
        text << cachedStatus.rectificationInfo;
    }

    if (!cachedStatus.sensorZones.empty()) {
        text << "\n<b>Температуры датчиков:</b>\n";
        for (const auto& zone_sensors : cachedStatus.sensorZones) {
            text << "\n<b>" << SensorZoneHelper::translateZone(zone_sensors.first) << "</b>:\n";
            for (const auto& sensorText : zone_sensors.second) {
                text << sensorText;
            }
        }
    }

    if (text.str() == cachedStatus.header) {
        text << "<i>Нет данных для отображения</i>\n";
    }

    return text.str();
}


void TelegramBotClient::setBotToken(const String& botToken) {
    if (botToken != "" && _settingsService) {
        _settingsService->update([&](TelegramSettings& settings) {
            settings.botToken = botToken;
            return StateUpdateResult::CHANGED;
        }, "api");
        this->token = botToken;
        _bot.setToken(this->token.c_str());
    }
}

String TelegramBotClient::getBotToken() const
{
    return token;
}

void TelegramBotClient::setChatID(const String& _chatID)
{
    if (_chatID.length() > 0 && _settingsService) {
        _settingsService->update([&](TelegramSettings& settings) {
            settings.chatId = _chatID;
            return StateUpdateResult::CHANGED;
        }, "api");
        this->chatID = _chatID;
    }
}

String TelegramBotClient::getChatId() const
{
    return chatID;
}

void TelegramBotClient::setPullMode(int _pullMode)
{
    pollMode = _pullMode;
}

uint32_t TelegramBotClient::sendMessage(const std::string& message)
{
    fb::Message msg;
    msg.mode = fb::Message::Mode::HTML;
    msg.text = message.c_str();
    msg.chatID = fb::ID(chatID);
    (void)createControlKeyboard();

    if (!waitTelegramSendWindow(_bot, MAX_SEND_TIME_MS)) {
        return 0;
    }

    fb::Result result = _bot.sendMessage(msg);
    if (!result || result.isError()) {
        logTelegramSendFailure("sendMessage", result);
        return 0;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
    return _bot.lastBotMessage();
}

void TelegramBotClient::updateMessage(const std::string& message, uint32_t messageId)
{
    fb::TextEdit et;
    et.mode = fb::Message::Mode::HTML;
    et.text = message.c_str();
    et.chatID = fb::ID(chatID);
    et.messageID = messageId;

    _bot.editText(et);
}

void TelegramBotClient::sendHello() {
    static const String version = "v" + String(APP_VERSION);

    float apiVersin = SsvcSettings::init().getSsvcApiVersion();
    std::ostringstream msgText;
    msgText << "👋 <b>Привет! Это SSVC open connect</b>\n\n"
            << "📦 <b>Версия OpenConnect:</b> " << version.c_str() << "\n"
            << "🤖 <b>Версия SSVC:</b> " << SsvcSettings::init().getSsvcVersion() << "\n";

    if (apiVersin != 0.0) {
        msgText << "🔌 <b>Версия API:</b> " << SsvcSettings::init().getSsvcApiVersion();
        if (SsvcSettings::init().apiSsvcIsSupport()) {
            msgText << " (✅ <b>Совместима</b>)\n";
        } else {
            msgText << " (❌ <b>Несовместима</b>)\n"
                    << "    Требуется версия: <b>" << SSVC_SUPPORT_API_VERSION << "</b> или выше.\n"
                    << "    <a href=\"https://smartmodule.ru/portfolio/0059_v2/\">Обновите прошивку на сайте производителя.</a>\n";
        }
    } else {
        msgText << "🔌 <b>Версия API не загружена</b>\n";
    }

    msgText << "🖥️ <b>Адрес:</b> http://" << WiFi.localIP().toString().c_str() << "\n";

    const std::string helloBody = msgText.str();

    fb::Message msg;
    msg.mode = fb::Message::Mode::HTML;
    msg.text = helloBody.c_str();
    msg.chatID = fb::ID(chatID);

    constexpr int kHelloAttempts = 3;
    for (int attempt = 1; attempt <= kHelloAttempts; ++attempt) {
        if (!waitTelegramSendWindow(_bot, MAX_SEND_TIME_MS)) {
            ESP_LOGE("TelegramBotClient",
                     "sendHello: no HTTP slot after waiting (attempt %d/%d)",
                     attempt, kHelloAttempts);
            continue;
        }
        fb::Result result = _bot.sendMessage(msg);
        if (result && !result.isError()) {
            if (attempt > 1) {
                ESP_LOGI("TelegramBotClient", "Hello sent OK on attempt %d", attempt);
            }
            return;
        }
        logTelegramSendFailure("sendHello", result);
        if (attempt < kHelloAttempts) {
            vTaskDelay(pdMS_TO_TICKS(2000));
        }
    }
}

bool TelegramBotClient::isReadiness()
{
    return bot()._apiReady;
}
