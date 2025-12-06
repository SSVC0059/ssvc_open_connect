#include "TelegramBotClient.h"

/**
 *   SSVC Open Connect
 *
 *   A firmware for ESP32 to interface with SSVC 0059 distillation controller
 *   via UART protocol. Features a responsive SvelteKit web interface for
 *   monitoring and controlling the distillation process.
 *   https://github.com/SSVC0059/ssvc_open_connect
 *
 *   Copyright (C) 2024 SSVC Open Connect Contributors
 *
 *   This software is independent and not affiliated with SSVC0059 company.
 *   All Rights Reserved. This software may be modified and distributed under
 *   the terms of the LGPL v3 license. See the LICENSE file for details.
 *   
 *   Disclaimer: Use at your own risk. High voltage safety precautions required.
 **/


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
    return ""; // Возвращаем пустую строку, если кнопки не нужны
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


bool TelegramBotClient::init() {
    if (_initialized) {
        ESP_LOGW("TelegramBotClient", "Already initialized");
        return true;
    }

    vTaskDelay(pdMS_TO_TICKS(10000));

    while (!SsvcOpenConnect::getInstance().isOnline())
    {
        ESP_LOGD("TelegramBotClient", "Waiting for connection...");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }

    ESP_LOGI("TelegramBotClient", "Initializing TelegramBotClient");
    {
        token = GlobalConfig::config().get<String>(bootName, "token", "");
        _bot.setToken(token.c_str());
    }

    {
        chatID = GlobalConfig::config().get<int64_t>(bootName, "chat_id", 0);
        ESP_LOGI("TelegramBotClient", "Chat ID: %lld", chatID);
    }

    // _bot.setLimit(MESSAGE_LIMIT_BY_UPDATE);
    // ESP_LOGD("TelegramBotClient", "setPollMode");
    // _bot.setPollMode(fb::Poll::Long, 20000);
    // _bot.tick();

    sendHello();

    initTelemetryTaskSender();
    _initialized = true;
    return true;
}

void TelegramBotClient::shutoff() {
    if (!_initialized) return;

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
    chatID = 0;
    statusMessageID = 0;
    _initialized = false;
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
                vTaskDelay(pdMS_TO_TICKS(1000)); // Пауза перед повторной попыткой
            }
        }
    }
}

void TelegramBotClient::updateSensorInfo() {
    // 1. Получаем экземпляр сервиса данных датчиков
    SensorDataService* sensorService = SensorDataService::getInstance();

    if (sensorService == nullptr) {
        if (!cachedStatus.sensorZones.empty()) {
            cachedStatus.sensorZones.clear();
        }
        ESP_LOGE("TelegramBotClient", "SensorDataService is not initialized!");
        return;
    }

    sensorService->read([&](const SensorDataState& currentState) {

        // Проверяем, есть ли вообще данные
        if (currentState.readings_by_zone.empty()) {
            cachedStatus.sensorZones.clear();
            return; // Выходим из лямбды, если нет данных
        }

        // Временная структура для сравнения и хранения новых данных
        // Использовать 'value' вместо 'temperature' для обобщения
        std::map<std::string, std::vector<std::string>> newSensorZones;
        bool dataChanged = false;

        // 3. Итерируем по данным, сгруппированным по зонам
        for (const auto& zonePair : currentState.readings_by_zone) {
            const SensorZone zone = zonePair.first;
            const auto& readings = zonePair.second;

            // Получаем переведенное название зоны
            const std::string zoneName = SensorZoneHelper::toString(zone);

            // Создаем список для текстовых строк датчиков в этой зоне
            std::vector<std::string> sensorTexts;

            for (auto it = readings.rbegin(); it != readings.rend(); ++it) {
                const std::string& address = it->first; // Адрес датчика не используется в выводе
                const float sensorValue = it->second; // Использовать 'sensorValue' вместо 'temperature'

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

        // 5. Проверяем, изменились ли данные по сравнению с кэшем
        if (cachedStatus.sensorZones.size() != newSensorZones.size() || cachedStatus.sensorZones != newSensorZones) {
            dataChanged = true;
        }

        // 6. Обновляем кэш, только если есть изменения
        if (dataChanged) {
            cachedStatus.sensorZones = std::move(newSensorZones);
            cachedStatus.lastUpdateTime = millis();
        }

    }); // Нет необходимости в originId для метода read()
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
    strftime(time_buffer, sizeof(time_buffer), "%d.%m.%Y %H:%M:%S", &timeInfo);  // Форматируем

    buffer << "Время обновления: <b>" << time_buffer << "</b>\n\n";  // Можно изменить формат вывода

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


bool TelegramBotClient::setBotToken(const String& botToken) const
{
    if (botToken != "")
    {
        GlobalConfig::config().set(bootName, "token", botToken);
        return true;
    }
    return false;
}

String TelegramBotClient::getBotToken() const
{
    return token;
}

void TelegramBotClient::setChatID (const int64_t _chatID)
{
    GlobalConfig::config().set<int64_t>(bootName, "chat_id", _chatID);
    ESP_LOGD("TelegramBotClient", "setChatID: %lld", _chatID);
    chatID = _chatID;
}

int64_t TelegramBotClient::getChatId() const
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
    msg.chatID = chatID;
    (void)createControlKeyboard();

    while (!_bot.isPolling())
    {
        fb::Result result = _bot.sendMessage(msg);
        vTaskDelay(pdMS_TO_TICKS(100));
        return _bot.lastBotMessage();
    }
    return 0;
}

void TelegramBotClient::updateMessage(const std::string& message, uint32_t messageId)
{
    fb::TextEdit et;
    et.mode = fb::Message::Mode::HTML;
    et.text = message.c_str();
    et.chatID = chatID;
    et.messageID = messageId;

    _bot.editText(et);
}

void TelegramBotClient::sendHello() {
    static const String version = "v" + String(APP_VERSION);

    std::ostringstream msgText;
    msgText << "👋 <b>Привет! Это SSVC open connect</b>\n\n"
            << "📦 <b>Версия OpenConnect:</b> " << version.c_str() << "\n"
            << "🤖 <b>Версия SSVC:</b> " << SsvcSettings::init().getSsvcVersion() << "\n"
            << "🔌 <b>Версия API:</b> " << SsvcSettings::init().getSsvcApiVersion();

    if (SsvcSettings::init().apiSsvcIsSupport()) {
        msgText << " (✅ <b>Совместима</b>)\n";
    } else {
        msgText << " (❌ <b>Несовместима</b>)\n"
                << "    Требуется версия: <b>" << SSVC_SUPPORT_API_VERSION << "</b> или выше.\n"
                << "    <a href=\"https://smartmodule.ru/portfolio/0059_v2/\">Обновите прошивку на сайте производителя.</a>\n";
    }

    msgText << "🖥️ <b>Адрес:</b> http://" << WiFi.localIP().toString().c_str() << "\n";

    fb::Message msg;
    msg.mode = fb::Message::Mode::HTML;
    msg.text = msgText.str().c_str();
    msg.chatID = chatID;

    while (!_bot.isPolling()) {
        fb::Result result = _bot.sendMessage(msg);
        if (!result) {
            ESP_LOGE("TelegramBotClient", "Failed to send hello message");
        }
        return;
    }
}

bool TelegramBotClient::isReadiness()
{
    return true;
}
