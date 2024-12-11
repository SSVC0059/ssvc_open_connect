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

// Сбор телеметрии
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
            (tskIDLE_PRIORITY),     // task priority
            nullptr,                       // Task handle
            1 // Pin to application core
    );

    xTaskCreatePinnedToCore(
            RectificationProcess::addPointToTempGraphTask,            // Function that should be called
            "Temp Graph handler",     // Name of the task (for debugging)
            4096,                       // Stack size (bytes)
            this,                       // Pass reference to this class instance
            (tskIDLE_PRIORITY),     // task priority
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

            if (message["common"]) {
                if (message["tp1_sap"]) {
                    self->tp1Value = message["tp1_sap"];
                }else {
                    self->tp1Value = message["common"]["tp1"];
                }
                if (message["tp2_sap"]) {
                    self->tp2Value = message["tp2_sap"];
                }else {
                    self->tp2Value = message["common"]["tp2"];
                }
            }

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

            if (xSemaphoreTake(mutex, portMAX_DELAY)) {
                self -> ssvsTelemetry = message;
                xSemaphoreGive(mutex);
            }
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
        memset(RectificationProcess::endTime, 0, sizeof(RectificationProcess::endTime));

        // Устанавливаем время начала
        this->setStartTime();

        // Логируем начало ректификации
        Serial.print("Начало ректификации: ");
        Serial.println(RectificationProcess::getRectificationTimeStart());
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
        snprintf(buffer, sizeof(buffer), "Версия SSVC %s не поддерживается. Используйте версию - %s и выше",
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

void RectificationProcess::addPointToTempGraphTask(void *pvParameters) {
    auto* self = static_cast<RectificationProcess*>(pvParameters);
    while (true) {
        time_t now = time(nullptr);

        // Если массив заполнен, сдвигаем элементы влево
        if (self->tempGraphCurrentIndex >= TEMP_GRAPH_ARRAY_SIZE) {
            // Сдвигаем все элементы массива влево, начиная с второго элемента
            portENTER_CRITICAL(&ssvcMux);
            for (size_t i = 1; i < TEMP_GRAPH_ARRAY_SIZE; ++i) {
                self->timePoints[i - 1] = self->timePoints[i];
                self->temp1Values[i - 1] = self->temp1Values[i];
                self->temp2Values[i - 1] = self->temp2Values[i];
            }
            self->tempGraphCurrentIndex = TEMP_GRAPH_ARRAY_SIZE - 1; // Устанавливаем индекс на последний элемент
            portEXIT_CRITICAL(&ssvcMux);
        }

        // Добавляем новые данные в конец массива
        portENTER_CRITICAL(&ssvcMux);
        self->timePoints[self->tempGraphCurrentIndex] = now;
        self->temp1Values[self->tempGraphCurrentIndex] = self->tp1Value;
        self->temp2Values[self->tempGraphCurrentIndex] = self->tp2Value;
        portEXIT_CRITICAL(&ssvcMux);

        // Инкрементируем индекс для следующей записи, используя остаток от деления
        self->tempGraphCurrentIndex = (self->tempGraphCurrentIndex + 1) % TEMP_GRAPH_ARRAY_SIZE;

        // Задержка 1 секунда
        vTaskDelay(pdMS_TO_TICKS(1000 * PERIOD_GRAPH_SEC));  // Задержка PERIOD_GRAPH_SEC секунд
    }
}


// Метод для создания JSON документа с данными
JsonDocument RectificationProcess::getGraphTempData(size_t point, size_t periodicity) {
    JsonDocument doc;  // Создаем JSON документ (выбираем подходящий размер)
    JsonArray data = doc["data"].to<JsonArray>();

    // Начинаем с startIndex и отбираем данные с шагом periodicity
    for (size_t i = point; i < TEMP_GRAPH_ARRAY_SIZE; i += periodicity) {
        if (timePoints[i] == 0 && temp1Values[i] == 0 && temp2Values[i] == 0) {
            return doc;
        }

        JsonDocument entry;
        entry["time"] = RectificationProcess::timePoints[i];
        entry["tp1"] = RectificationProcess::temp1Values[i];
        entry["tp2"] = RectificationProcess::temp2Values[i];
        data.add(entry);
    }

    return doc;
}