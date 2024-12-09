#include "RectificationProcess.h"

char RectificationProcess::startTime[25]          = "";
char RectificationProcess::endTime[25]            = "";


// Инициализация статического члена класса
RectificationProcess* RectificationProcess::_rectificationProcess = nullptr;

// Метод получения единственного экземпляра класса
RectificationProcess* RectificationProcess::createRectification(SsvcConnector* ssvcConnector,
                                                                EventGroupHandle_t eventGroup) {
    if (_rectificationProcess == nullptr) {
        _rectificationProcess = new RectificationProcess(ssvcConnector, eventGroup);
    }

    return _rectificationProcess;
}

// Приватный конструктор
RectificationProcess::RectificationProcess(SsvcConnector* ssvcConnector,
                                           EventGroupHandle_t eventGroup)
        : _ssvcConnector(ssvcConnector),
        _eventGroup(eventGroup),
        ssvcSettings(ssvcConnector->getSsvcSettings()){

//  Обнуляем значения
    valveBandwidthHeads = 0;
    valveBandwidthHearts = 0;
    valveBandwidthTails = 0;
    isRectificationStarted = false;

    xTaskCreatePinnedToCore(
            RectificationProcess::update,            // Function that should be called
            "SSVC Open Telemetry",     // Name of the task (for debugging)
            4096,                       // Stack size (bytes)
            this,                       // Pass reference to this class instance
            (configMAX_PRIORITIES - 10),     // task priority
            nullptr,                       // Task handle
            1 // Pin to application core
    );

}

// Метод установки времени старта
void RectificationProcess::setStartTime() {
    setTime(startTime);
}

// Метод установки времени остановки
void RectificationProcess::setStopTime() {
    setTime(endTime);
}


// Метод обновления состояния
void RectificationProcess::update(void* pvParameters) {
    auto* self = static_cast<RectificationProcess*>(pvParameters);
    while (true) {
        // Ожидание события
        EventBits_t bits = xEventGroupWaitBits(
                self->_eventGroup,
                BIT0,        // Ждём BIT0
                pdTRUE,      // Сбрасываем флаг после обработки
                pdFALSE,     // Любое событие
                portMAX_DELAY
        );

        if (bits & BIT0) {
            JsonDocument message = self->_ssvcConnector->getMessage();
            if (message["type"] == "response") {
                vTaskDelay(400);
            }

            if (message["event"]) {
                if  (message["event"] == "heads_finished" ) {
                    self->headsFinishedHandler(message);
                }else if  (message["event"] == "hearts_finished") {
                    self->heartsFinishedHandler(message);
                }else if  (message["event"] == "tails_finished") {
                    self->tailsFinishedHandler(message);
                }else if  (message["event"] == "ds_error") {
                    self->dcErrorHandler(message);
                }else if  (message["event"] == "ds_error_stop") {
                    self->dsErrorStopHandler(message);
                }else if  (message["event"] == "stabilization_limit") {
                    self->stabilizationLimitHandler(message);
                }else if  (message["event"] == "remote_stop") {
                    self->remoteStopHandler(message);
                }else if  (message["event"] == "manually_closed") {
                    self->manuallyClosedHandler(message);
                }else if  (message["event"] == "manually_opened") {
                    self->manuallyOpenedHandler(message);
                }else  {
                    self->notFoundEvent(message);
                }
            }

//            Первый тип сообщений при начале ректификации - delayed_start.
            if (message["type"] == "waiting") {
                self->waitingTypeHandler(message);
            } else if (message["type"] == "tp1_waiting" ) {
                self->tp1TypeHandler(message);
            }else if  (message["type"] == "delayed_start") {
                self->delayedStartHandler(message);
            }else if  (message["type"] == "heads") {
                self->headsTypeHandler(message);
            }else if  (message["type"] == "hearts") {
                self->heartsTypeHandler(message);
            }else if  (message["type"] == "tails") {
                self->tailsTypeHandler(message);
            }

            self -> ssvsTelemetry = message;
        }
    }
}

// Обработчики этапов
void RectificationProcess::waitingTypeHandler(JsonDocument& message) {
//    message.to<JsonObject>()["type"];
    if (strlen(startTime) != 0) {
        setRectificationStop();
    }
}

void RectificationProcess::tp1TypeHandler(JsonDocument& message) {
    setRectificationStart();
}

void RectificationProcess::delayedStartHandler(JsonDocument& message) {
    setRectificationStart();
}

void RectificationProcess::headsTypeHandler(JsonDocument& message) {
    JsonObject settings = (*ssvcSettings).as<JsonObject>();
    int valveBandwidth = settings["valve_bandwidth"][0];
    double openValveTime = message["v1"];

    if (!settings.isNull()) {
    #ifdef SSVC_DEBUG
        Serial.print("Отбор голов: ");
    #endif
        valveBandwidthHeads = getSelectedVolume(valveBandwidth, openValveTime);
    } else {
        Serial.println("Поле 'settings' отсутствует или не является объектом.");
    }
}

void RectificationProcess::heartsTypeHandler(JsonDocument& message) {
    JsonObject settings = (*ssvcSettings).as<JsonObject>();
    int valveBandwidth = settings["valve_bandwidth"][1];
    double openValveTime = message["v2"];

    if (!settings.isNull()) {
#ifdef SSVC_DEBUG
        Serial.print("Отбор тела: ");
#endif
        valveBandwidthHearts = getSelectedVolume(valveBandwidth, openValveTime);
    } else {
        Serial.println("Поле 'settings' отсутствует или не является объектом.");
    }
}

void RectificationProcess::tailsTypeHandler(JsonDocument& message) {
    JsonObject settings = (*ssvcSettings).as<JsonObject>();
    int valveBandwidth = settings["valve_bandwidth"][2];
    double openValveTime = message["v3"];

    if (!settings.isNull()) {
        #ifdef SSVC_DEBUG
                Serial.print("Отбор хвостов: ");
        #endif
        valveBandwidthTails = getSelectedVolume(valveBandwidth, openValveTime);
    } else {
        Serial.println("Поле 'settings' отсутствует или не является объектом.");
    }
}

void RectificationProcess::setRectificationStart() {
    if (!this->isRectificationStarted) {
        Serial.println("Начало ректификации");
        this->isRectificationStarted = true;

        // Обнуляем статические переменные
        this->valveBandwidthHeads = 0;
        this->valveBandwidthHearts = 0;
        this->valveBandwidthTails = 0;

        // Очищаем время завершения
        memset(this->endTime, 0, sizeof(this->endTime));

        // Устанавливаем время начала
        this->setStartTime();

        // Логируем начало ректификации
        Serial.print("Начало ректификации: ");
        Serial.println(this->getRectificationTimeStart());
    }
}


void RectificationProcess::setRectificationStop() {
    if (this->isRectificationStarted) {
        this->isRectificationStarted = false;
        this->setStopTime();
        Serial.print("Окончание ректификации: ");
        Serial.println(getRectificationTimeStart());
    }
}


String RectificationProcess::getRectificationTimeStart() {
    return startTime;
}

String RectificationProcess::getRectificationTimeEnd() {
    return endTime;
}


// Event Handler
void RectificationProcess::manuallyOpenedHandler(JsonDocument& message) {

}

void RectificationProcess::headsFinishedHandler(JsonDocument& message) {

}

void RectificationProcess::heartsFinishedHandler(JsonDocument& message) {

}

void RectificationProcess::tailsFinishedHandler(JsonDocument& message) {

}

void RectificationProcess::dcErrorHandler(JsonDocument& message) {

}

void RectificationProcess::dsErrorStopHandler(JsonDocument& message) {

}

void RectificationProcess::stabilizationLimitHandler(JsonDocument& message) {

}

void RectificationProcess::remoteStopHandler(JsonDocument& message) {

}

void RectificationProcess::manuallyClosedHandler(JsonDocument& message) {

}

void RectificationProcess::notFoundEvent(JsonDocument& message) {

}

// Auxiliary methods
// Вспомогательный метод для установки времени
void RectificationProcess::setTime(char* timeBuffer) {
    time_t now = time(nullptr);
    struct tm* timeInfo = localtime(&now);
    if (timeInfo) {
        strftime(timeBuffer, 25, "%Y-%m-%d %H:%M:%S", timeInfo);  // Размер буфера всегда 25
    } else {
//        timeBuffer[24] = '\0'; // Гарантируем завершение строки
    }
}

int RectificationProcess::getSelectedVolume(int valveBandwidth, double openValveTime) {
    int volumeMl = (valveBandwidth / 3600) * openValveTime;
//#ifdef SSVC_DEBUG
    Serial.print("Пропускная способность клапан: ");
    Serial.println(valveBandwidth);
    Serial.print("Отобранный объем: ");
    Serial.print(volumeMl);
    Serial.print(" мл");
//#endif
    return volumeMl;
}

// Основной метод, который должен возвращать данные по работе подписчикам
JsonDocument RectificationProcess::getRectificationStatus() {
    JsonDocument _rectificationStatus = ssvsTelemetry;
    if (_rectificationStatus == nullptr) {
        _rectificationStatus["uartCommunicationError"] = true;
        _rectificationStatus["info"] = "Ошибка связи. Перезагрузите SSVC";
    }

    if (strlen(startTime) != 0) {
        _rectificationStatus["rectificationStart"] = startTime;
    }
    if (strlen(endTime) != 0) {
        _rectificationStatus["rectificationEnd"] = endTime;
    }
    if (valveBandwidthHeads != 0) {
        _rectificationStatus["valveBandwidthHeads"] = valveBandwidthHeads;
    }
    if (valveBandwidthHearts != 0) {
        _rectificationStatus["valveBandwidthHearts"] = valveBandwidthHearts;
    }
    if (valveBandwidthTails != 0) {
        _rectificationStatus["valveBandwidthTails"] = valveBandwidthTails;
    }

    _rectificationStatus["info"] = "";
    bool isSupportSSVCVersion = _ssvcConnector->isSupportSSVCVersion;
    ESP_LOGV("isSupportSSVCVersion::isSupportSSVCVersion: ", isSupportSSVCVersion);
    if (!isSupportSSVCVersion) {
        char buffer[256];
        snprintf(buffer, sizeof(buffer), "Версия %s не поддерживается. Минимально поддерживаемая - %s",
                 SSVC_MIN_SUPPORT_VERSION,
                 _ssvcConnector->getSsvcVersion().c_str());
        _rectificationStatus["info"] = buffer;
    }
    _rectificationStatus["ssvcVersionValid"] = _ssvcConnector->isSupportSSVCVersion;

    return _rectificationStatus;
}

JsonDocument* RectificationProcess::getSsvcSettings() {
    return _ssvcConnector->getSsvcSettings();
}
