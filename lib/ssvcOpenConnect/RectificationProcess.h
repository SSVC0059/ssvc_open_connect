#ifndef RECTIFICATION_PROCESS_H
#define RECTIFICATION_PROCESS_H

#include <map>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <Arduino.h>
#include <SsvcConnector.h>
#include "SsvcSettings.h"


#define TEMP_GRAPH_ARRAY_SIZE 720
#define PERIOD_GRAPH_SEC 20

extern portMUX_TYPE ssvcMux;

class RectificationProcess {
public:
    // Получение единственного экземпляра класса
    static RectificationProcess &createRectification(SsvcConnector& connector, SsvcSettings& settings);

    enum class ProcessState {
        IDLE, // Процесс еще не запускался
        RUNNING,     // Процесс запущен
        PAUSED,
        FINISHED,     // Процесс завершен
        SKIPPED,
        STOPPED,
        ERROR
    };

    ProcessState getCurrentState() const;
    std::string getTelemetry();
    bool getStatus(JsonVariant temperBuffer);

private:
    // Приватный конструктор
    explicit RectificationProcess(SsvcConnector& connector, SsvcSettings& settings);
    static void update(void* pvParameters);

    enum class RectificationStage {
        EMPTY,
        WAITING, // Дежурный режим
        TP1_WAITING, // Ожидание нагрева колонны
        DELAYED_START, // Отложенный старт
        HEADS, // Отбор голов
        LATE_HEADS, // Отбор подголовников
        HEARTS, // Отбор тела
        TAILS, // Отбор хвостов
        ERROR // Ошибка разбора этапов
    };

    enum class RectificationEvent {
        EMPTY,
        HEADS_FINISHED,        // завершен этап Головы
        HEARTS_FINISHED,       // завершен этап Тело (ректификация завершена для firmware 2.3.*)
        TAILS_FINISHED,        // завершен этап Хвосты (ректификация завершена для firmware 2.2.*)
        DS_ERROR,              // ошибка датчика температуры
        DS_ERROR_STOP,         // выключение оборудования (реле) из-за ошибки датчика
        STABILIZATION_LIMIT,   // превышен лимит времени стабилизации
        REMOTE_STOP,           // получена удаленная команда остановки, процесс остановлен
        MANUALLY_CLOSED,       // включено ручное управление клапаном текущего этапа, клапан закрыт
        MANUALLY_OPENED,        // включено ручное управление клапаном текущего этапа, клапан открыт
        ERROR
    };

    struct Common {
        unsigned int mmhg;
        float tp1;
        float tp2;
        bool relay;
        bool signal;
    };

    struct Metrics {
        std::string type;
        float tp1_target;
        float tp2_target;
        std::string countdown;
        Common common;
        std::string release;
        std::string time;
        float open;
        unsigned int period;
        unsigned int tank_mmhg;
        float tp1_sap;
        float tp2_sap;
        float hysteresis;
        unsigned int v1;
        unsigned int v2;
        unsigned int v3;
        float alc;
        bool stop;
        unsigned char stops;
        RectificationEvent event;
    };

    char16_t pid;
    RectificationStage currentStage;
    RectificationStage previousStage;
    std::map<RectificationStage, int> flowVolumeValves;
    std::map<RectificationStage, ProcessState> rectificationStageStates;

    char startTime[25]{};
    char endTime[25]{};

    Metrics metric;

    ProcessState currentProcessStatus;

    SsvcConnector& _ssvcConnector;
    SsvcSettings&  _ssvcSettings;
    static RectificationProcess* _rectificationProcess;

    bool isRectificationStarted();

//    void setRectificationStage(RectificationStage stage);
//    void setRectificationEvent(RectificationEvent event);
//    void setRectificationStageState(RectificationStage stage, ProcessState state);


    static RectificationStage stringToRectificationStage(const std::string& stageStr);
    static std::string rectificationEventToString(RectificationEvent event);
    static RectificationEvent stringToRectificationEvent(std::string& eventString);
    static std::string rectificationEventToDescription(RectificationEvent event);
    static std::string stageToString(RectificationStage stage);
    static std::string stateToString(ProcessState state);

    bool eventReceived = false;
    void startEventHandler(const std::string&);
    void endEventHandler(std::string currentEvent);


    bool isHeatingOn();
    bool isOverclockingOn();

    void recalculateFlowVolume(int v1, int v2, int v3);

    bool calculateVolumeSpeed(int valveOpen, int& volumeSpeed);

    void initStageState();

};

#endif // RECTIFICATION_PROCESS_H
