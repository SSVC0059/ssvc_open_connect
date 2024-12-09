#ifndef RECTIFICATION_PROCESS_H
#define RECTIFICATION_PROCESS_H

#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <Arduino.h>
#include <SsvcConnector.h>


#define TEMP_GRAPH_ARRAY_SIZE 8640
#define PERIOD_GRAPH_SEC 10

extern portMUX_TYPE ssvcMux;

class RectificationProcess {
public:
    // Получение единственного экземпляра класса
    static RectificationProcess* createRectification(SsvcConnector* ssvcConnector,
                                                     EventGroupHandle_t eventGroup);

    static String getRectificationTimeStart();
    static String getRectificationTimeEnd();
    JsonDocument getGraphTempData(size_t startIndex, size_t periodicity);

    JsonDocument* getSsvcSettings();

    JsonDocument getRectificationStatus();

private:
    // Приватный конструктор
    explicit RectificationProcess(SsvcConnector* ssvcConnector,
                                  EventGroupHandle_t eventGroup);

    // Статический экземпляр класса
    static RectificationProcess* _rectificationProcess;

//    Основные переменные класса
    int valveBandwidthHeads;
    int valveBandwidthHearts;
    int valveBandwidthTails;

//    Массивы хранения данных температуры
    static void addPointToTempGraphTask(void* pvParameters);
    float tp1Value;
    float tp2Value;
    int tempGraphCurrentIndex = 0;  // Индекс текущей позиции для вставки
    time_t timePoints[TEMP_GRAPH_ARRAY_SIZE]{};
    float temp1Values[TEMP_GRAPH_ARRAY_SIZE]{};
    float temp2Values[TEMP_GRAPH_ARRAY_SIZE]{};

    // Метод установки времени старта
    // Метод для обновления состояния
    static void update(void* pvParameters);

    void setRectificationStart();
    void setRectificationStop();
    void setStartTime();
    void setStopTime();


//    Обработчики Этапов
    void waitingTypeHandler(JsonDocument& message);
    void tp1TypeHandler(JsonDocument &message);
    void delayedStartHandler(JsonDocument &message);
    void headsTypeHandler(JsonDocument &message);
    void heartsTypeHandler(JsonDocument &message);
    void tailsTypeHandler(JsonDocument &message);

//    Обработчики событий
    void headsFinishedHandler(JsonDocument& message);
    void heartsFinishedHandler(JsonDocument& message);
    void tailsFinishedHandler(JsonDocument& message);
    void dcErrorHandler(JsonDocument& message);
    void dsErrorStopHandler(JsonDocument& message);
    void stabilizationLimitHandler(JsonDocument& message);
    void remoteStopHandler(JsonDocument& message);
    void manuallyClosedHandler(JsonDocument& message);
    void manuallyOpenedHandler(JsonDocument& message);
    void notFoundEvent(JsonDocument& message);

    // Поля класса
    JsonDocument ssvsTelemetry;
    JsonDocument* ssvcSettings;
    // Время запуска

    static char startTime[25];
    static char endTime[25];
    bool isRectificationStarted;

//    служебные методы
    static int getSelectedVolume(const int valveBandwidth, const double openValveTime);
    void setTime(char* timeBuffer);

    // Группа событий FreeRTOS
    SsvcConnector* _ssvcConnector;
    EventGroupHandle_t _eventGroup;

};

#endif // RECTIFICATION_PROCESS_H
