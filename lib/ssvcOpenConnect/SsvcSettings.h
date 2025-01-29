//
// Created by demoncat on 29.12.2024.
//

#ifndef SSVC_OPEN_CONNECT_SSVCSETTINGS_H
#define SSVC_OPEN_CONNECT_SSVCSETTINGS_H

#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <Arduino.h>
#include <SsvcConnector.h>

extern SemaphoreHandle_t mutex;
extern EventGroupHandle_t eventGroup;

// Класс настроек SsvcSettings
class SsvcSettings {
public:
    static SsvcSettings& init(SsvcConnector& connector);
    bool getSettings(JsonVariant json);

    bool load(const std::string& json);

    [[noreturn]] [[noreturn]] static void reload(void* pvParameters);

    // GETTERS

    // Получает время включения и период для голов.
    std::array<float, 2> getHeads() const;

    // Получает время включения и период для тела.
    std::array<float, 2> getHearts() const;

    // Получает время включения и период для поздних голов.
    std::array<float, 2> getLateHeads() const;

    // Получает время включения и период для хвостов.
    std::array<float, 2> getTails() const;

    // Получает пропускную способность клапанов.
    std::array<int, 3> getValveBw() const;

    // Получает значение гистерезиса.
    float getHysteresis() const;

    // Получает величину уменьшения скорости отбора.
    unsigned char getDecrement() const;

    // Получает значение для использования формулы.
    bool getFormula() const;

    // Получает давление в кубе (мм рт. ст.).
    unsigned char getTank_mmhg() const;

    // Получает таймер для отбора голов.
    unsigned int getHeadsTimer() const;

    // Получает таймер для отбора подголовников.
    unsigned int  getLateHeadsTimer() const;

    // Получает таймер для фиксации температуры отбора тела.
    unsigned int getHeartsTimer() const;

    // Получает время для отложенного пуска.
    unsigned int getStartDelay() const;

    // Получает температуру на ТД2 для завершения отбора тела.
    float getHeartsFinishTemp() const;

    // Получает начальную температуру для работы формулы.
    float getFormulaStartTemp() const;

    // Получает значение для звука (флаг для включения/выключения звуковых сигналов).
    bool getSound() const;

    // Получает значение для давления (флаг для использования давления).
    bool getPressure() const;

    // Получает значение для инвертирования реле (флаг для инверсии реле).
    bool getRelayInverted() const;

    // Получает значение для автоматического старта реле (флаг для автоматического старта).
    bool getRelayAutostart() const;

    // Получает значение для автозавершения (флаг для автоматического возобновления).
    bool getAutoResume() const;

    // Получает значение для автоматического режима (флаг для включения автоматического режима).
    bool getAutoMode() const;

    // Получает параметры для параллельного отбора (диапазон для параметра параллели).
    std::array<float, 2> getParallel() const;

    // Получает скорость параллельного отбора клапаном №1.
    std::array<float, 2> getParallelV1() const;

    // Получает скорость параллельного отбора клапаном №3 для 4-х диапазонов.
    std::array<float, 2> getParallelV3() const;

    // Получает сдвиг температуры тела (может быть использован для калибровки).
    float getHeartsTempShift() const;

    // Получает паузу для тела (время паузы для тела).
    bool getHeartsPause() const;

    // Получает сдвиг температуры TP2.
    float getTp2Shift() const;

    // Получает сигнал управления TP1 (флаг для включения/выключения контроля TP1).
    bool getSignalTp1Control() const;

    // Получает инвертированный сигнал (флаг для инверсии сигнала).
    bool getSignalInverted() const;

    // Получает контроль температуры TP1 (значение температуры для контроля TP1).
    float getTp1ControlTemp() const;

    // Получает начало контроля температуры TP1 (флаг для начала контроля TP1).
    bool getTp1ControlStart() const;

    // Получает время стабилизации (ограничение времени стабилизации).
    int getStabLimitTime() const;

    // Получает завершение стабилизации (ограничение завершения стабилизации).
    bool getStabLimitFinish() const;

    // Получает подсветку (состояние подсветки).
    std::string getBacklight() const;

    // Получает температуру завершения отбора хвостов.
    float getTailsTemp() const;

    float getReleaseSpeed() const;

    // Получает температуру завершения отбора хвостов.
    int getReleaseTimeer() const;

private:
    explicit SsvcSettings(SsvcConnector &ssvcConnector);

    SsvcConnector& _ssvcConnector;

    // signed char или unsigned char.

    // Параметры присутствующие в обоих ветках
    std::array<float, 2> heads = {0.0, 0.0}; // головы
    std::array<float, 2> hearts = {0.0, 0.0}; // тело

    float heads_final = 0.0;
    float hyst = 0.0;
    unsigned char decrement = 0;
    bool formula = false;
    unsigned char tank_mmhg = 0;
    unsigned char heads_timer = 0;
    unsigned int hearts_timer = 0;
    unsigned int start_delay = 0;
    float hearts_finish_temp = 0.0;
    float formula_start_temp = 0.0;
    std::array<int, 3> valve_bw{};

    // Дополнительно из JSON
    bool sound = false; // звук (флаг для включения/выключения звуковых сигналов)
    bool pressure = false; // давление (флаг для использования давления)
    bool relay_inverted = false; // инвертирование реле (флаг для инверсии реле)
    bool relay_autostart = false; // автоматический старт реле (флаг для автоматического старта)
    bool autoresume = false; // автозавершение (флаг для автоматического возобновления)
    bool auto_mode = false; // автоматический режим (флаг для включения автоматического режима)
    std::array<float, 2> parallel = {0.0, 0.0}; // параллель (диапазон для параметра параллели)
    std::array<float, 2> parallel_v1 = {0.0, 0.0};           // Скорость параллельного отбора клапаном №1
    std::array<float, 2> parallel_v3 = {0.0, 0.0}; // Скорость параллельного отбора клапаном №3 для 4-х диапазонов
    float hearts_temp_shift = 0.0; // сдвиг температуры тела (может быть использован для калибровки)
    bool hearts_pause = false; // пауза для тела (время паузы для тела)
    float tp2_shift = 0.0; // сдвиг температуры TP2
    bool tp_filter = false; // фильтр для температуры TP
    bool signal_tp1_control = false; // сигнал управления TP1 (флаг для включения/выключения контроля TP1)
    bool signal_inverted = false; // инвертированный сигнал (флаг для инверсии сигнала)
    float tp1_control_temp = 0.0; // контроль температуры TP1 (значение температуры для контроля TP1)
    bool tp1_control_start = false; // начало контроля температуры TP1 (флаг для начала контроля TP1)
    int stab_limit_time = 0; // время стабилизации (ограничение времени стабилизации)
    bool stab_limit_finish = false; // завершение стабилизации (ограничение завершения стабилизации)
    std::string backlight; // подсветка (состояние подсветки)

//    Актуально в firmware 2.2.*
    std::array<float, 2> tails = {0.0, 0.0};
    float tails_temp = 0.0;

//    Актуально в firmware 2.3.*
    std::array<float, 2> late_heads = {0.0, 0.0};
    unsigned int  late_heads_timer = 0;
    float release_speed{};
    int release_timer{};

    std::string lastSettingsSrc;

    static SsvcSettings* _ssvcSettings;


public:
    // Метод для отображения данных (для тестирования)

    // Класс билдера
    class Builder {
    private:
        std::unique_ptr<SsvcSettings> settings;

    public:

// Устанавливает настройки для "голов" (время включения и период).
        Builder& setHeads(float timeTurnOn, int period);

        // Устанавливает настройки для "сердец" (время включения и период).
        Builder& setHearts(float timeTurnOn, int period);

        // Устанавливает настройки для "поздних голов" (время включения и период).
        Builder& setLateHeads(float timeTurnOn, int period);

        // Устанавливает настройки для "хвостов" (время включения и период).
        Builder& setTails(float timeTurnOn, int period);

        // Устанавливает значение гистерезиса.
        Builder& setHysteresis(float _hyst);

        // Устанавливает величину уменьшения скорости отбора при "зелете" температуры.
        __attribute__((unused)) Builder& setDecrement(unsigned char _decrement);

        // Включает или выключает использование формулы для снижения скорости отбора.
        Builder& formulaEnable(bool enable);

        // Устанавливает давление в кубе (мм рт. ст.) относительно атмосферного.
        Builder& setTank_mmhg(unsigned char _tank_mmhg);

        // Устанавливает таймер для отбора голов.
        Builder& setHeadsTimer(unsigned int _headsTimer);

        // Устанавливает таймер для отбора подголовников.
        Builder& setLateHeadsTimer(unsigned int _liteHeadsTimer);

        // Устанавливает таймер для фиксации температуры отбора тела.
        Builder& setHeartsTimer(unsigned char _heartsTimer);

        // Устанавливает завершение отбора хвостов (температура).
        Builder& setTailsTemp(float _tailsTemp);

        // Устанавливает время для отложенного пуска.
        Builder& setStartDelay(unsigned int _start_delay);

        // Устанавливает температуру на ТД2, при которой завершается отбор тела.
        Builder& setHeartsFinishTemp(float _heartsFinishTemp);

        // Устанавливает начальную температуру для начала работы формулы.
        Builder& setFormulaStartTemp(float _formulaStartTemp);

        // Устанавливает пропускную способность клапанов.
        Builder& setValveBw(int v1, int v2 , int v3);

        // Другие методы установки параметров...
        SsvcSettings build();
        

        // Метод для общей проверки и установки значений
        static void validateAndSetValues(float &timeTurnOn, int &period, float* targetTime, float* targetPeriod) {

            if (xSemaphoreTake(mutex, portMAX_DELAY)) {
                // Проверка и корректировка значения timeTurnOn
                timeTurnOn = std::min(std::max(timeTurnOn, 0.0f), 99.9f);

                // Проверка и корректировка значения period
                period = std::min(std::max(period, 0), 999);

                // Назначение значений
                *targetTime = timeTurnOn;
                *targetPeriod = period;
                xSemaphoreGive(mutex);
            }
        }

    };

};


#endif //SSVC_OPEN_CONNECT_SSVCSETTINGS_H
