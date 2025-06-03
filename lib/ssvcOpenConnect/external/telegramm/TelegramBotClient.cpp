#include "TelegramBotClient.h"

TelegramBotClient::TelegramBotClient() = default;

void TelegramBotClient::initTelemetryTaskSender()
{
    xTaskCreatePinnedToCore(
        statusMessageSender,
        "TelegramBotClient",
        8192,
        this,
        tskIDLE_PRIORITY,
        nullptr,
        1
    );
}

TelegramBotClient::~TelegramBotClient() {

}


void TelegramBotClient::init() {
    vTaskDelay(pdMS_TO_TICKS(10000)); // Пауза для предотвращения перегрузки
    while (!SsvcOpenConnect::getInstance().isOnline())
    {
        ESP_LOGD("TelegramBotClient", "Waiting for connection...");
        vTaskDelay(pdMS_TO_TICKS(5000)); // Пауза для предотвращения перегрузки
    }
    ESP_LOGI("TelegramBotClient", "Initializing TelegramBotClient");
    {
        token = GlobalConfig::config().get<String>(bootName, "token", "");
        ESP_LOGI("TelegramBotClient", "Token found: %s", token);
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
}

[[noreturn]] void TelegramBotClient::statusMessageSender(void* params) {
    const auto manager = static_cast<TelegramBotClient*>(params);
    while (true) {
        ESP_LOGD("TelegramBotClient", "statusMessageSender - tick");
        ESP_LOGI("TelegramBotClient", "Free stack: %u", uxTaskGetStackHighWaterMark(nullptr));
        vTaskDelay(pdMS_TO_TICKS(5000));

        if (manager->_botMutex != nullptr) {
            try {
                MutexLocker lock(manager->_botMutex);

                std::string msg = manager->createStatusMessage();
                if (manager->statusMessageID != 0)
                {
                    const uint32_t start = millis();
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
    if (!SubsystemManager::instance().isSubsystemEnabled(ThermalSubsystem::getName())) {
        if (!cachedStatus.sensorZones.empty()) {
            cachedStatus.sensorZones.clear();
        }
        return;
    }

    const auto& currentTemperatures = ThermalSensors::thermalController().getTemperatures();
    if (currentTemperatures.empty()) {
        if (!cachedStatus.sensorZones.empty()) {
            cachedStatus.sensorZones.clear(); // Очищаем кэш если нет данных
        }
        return;
    }

    static std::vector<SensorTemperatureData> lastTemperatures;
    bool dataChanged = false;

    if (lastTemperatures.size() != currentTemperatures.size()) {
        dataChanged = true;
    } else {
        for (size_t i = 0; i < lastTemperatures.size(); ++i) {
            if (lastTemperatures[i].address != currentTemperatures[i].address ||
                fabs(lastTemperatures[i].temperature - currentTemperatures[i].temperature) > 0.1f) {
                dataChanged = true;
                break;
                }
        }
    }

    if (!dataChanged) {
        return;
    }

    lastTemperatures = currentTemperatures;
    cachedStatus.sensorZones.clear();

    // Группируем датчики по зонам
    for (const SensorTemperatureData& sensor : currentTemperatures) {
        // Пропускаем невалидные данные
        if (sensor.temperature == -127.0f || sensor.address.empty()) {
            continue;
        }

        char tempBuffer[64];
        snprintf(tempBuffer, sizeof(tempBuffer), "  • %s: <b>%.2f°C</b>\n",
                sensor.address.c_str(), sensor.temperature);

        cachedStatus.sensorZones[sensor.zone].emplace_back(tempBuffer);
    }
    cachedStatus.lastUpdateTime = millis();
}

void TelegramBotClient::initializeMessageStructure() {
    if (messageStructureInitialized) return;
    cachedStatus.header = STATUS_HEADER;
    messageStructureInitialized = true;
}


void TelegramBotClient::updateRectificationInfo() {
    const RectificationProcess::Metrics& metrics = RectificationProcess::rectController().getMetrics();

    // Статическая переменная для хранения последних корректных значений
    struct LastValidData {
        float tp1 = 0;
        float tp2 = 0;
        std::string type;
        uint32_t lastValidTime = 0;
    };

    static LastValidData lastValidData;

    // Проверяем наличие данных для каждого параметра отдельно
    const bool hasTp1Data = (metrics.tp1_sap != 0 || metrics.common.tp1 != 0);
    const bool hasTp2Data = (metrics.tp2_sap != 0 || metrics.common.tp2 != 0);
    const bool hasTypeData = !metrics.type.empty();

    // Обновляем кэш только для тех параметров, которые есть в текущих данных
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

    // Проверяем, есть ли вообще какие-то данные для отображения
    const bool hasAnyData = hasTp1Data || hasTp2Data || hasTypeData;
    const bool cacheValid = (millis() - lastValidData.lastValidTime < 30000) &&
                          (!lastValidData.type.empty() || lastValidData.tp1 != 0 || lastValidData.tp2 != 0);

    if (!hasAnyData && !cacheValid) {
        // Нет ни текущих, ни валидных кэшированных данных
        cachedStatus.rectificationInfo.clear();
        return;
    }

    // Формируем сообщение
    std::ostringstream buffer;
    cachedStatus.rectificationInfo.clear();

    // Тип процесса (из кэша или текущих данных)
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
            text << "\n<b>" << ThermalSensorZoneHelper::translateZone(zone_sensors.first) << "</b>:\n";
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
    while (!_bot.isPolling())
    {
        fb::Result result = _bot.sendMessage(msg);
        ESP_LOGD("TelegramBotClient", "result - %s", result.getRaw().c_str());
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
            << "📌 <b>Версия:</b> " << version.c_str() << "\n"
            << "🖥️ <b>Адрес:</b> http://" << WiFi.localIP().toString().c_str() << "\n";

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
