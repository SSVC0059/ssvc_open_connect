#ifndef SSVC_OPEN_CONNECT_SSVCSETTINGS_H
#define SSVC_OPEN_CONNECT_SSVCSETTINGS_H

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


#include <Arduino.h>
#include <memory>
#include <string>
#include <core/SsvcConnector.h>
#include <core/SsvcCommandsQueue.h>
#include "core/StatefulServices/OpenConnectSettingsService/OpenConnectSettings.h"
#include "core/profiles/IProfileObserver.h"

// Класс настроек SsvcSettings
class SsvcSettings : public IProfileObserver
{
public:
    static SsvcSettings& init();

    void fillSettings(JsonVariant json) const;

    bool load(const std::string& json);

    // --- Реализация контракта IProfileObserver ---
    const char* getProfileKey() const override { return "ssvcSettings"; }
    void onProfileSave(JsonObject& dest) override;
    void onProfileApply(const JsonObject& src) override;

    // GETTERS

    // Получает время включения и период для голов.
    std::tuple<float, int> getHeads() const;

    // Получает время включения и период для тела.
    std::tuple<float, int> getHearts() const;

    // Получает время включения и период для поздних голов.
    std::tuple<float, int> getLateHeads() const;

    // Получает время включения и период для хвостов.
    std::tuple<float, int> getTails() const;

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
    unsigned int getLateHeadsTimer() const;

    // Получает таймер для фиксации температуры отбора тела.
    unsigned int getHeartsTimer() const;

    // Получает время для отложенного пуска.
    unsigned int getStartDelay() const;

    // Получает температуру на ТД2 для завершения отбора тела.
    float getHeartsFinishTemp() const;

    // Получает начальную температуру для работы формулы.
    float getFormulaStartTemp() const;

    // Получает значение для звука (флаг для включения/выключения звуковых
    // сигналов).
    bool getSound() const;

    // Получает значение для давления (флаг для использования давления).
    bool getPressure() const;

    // Получает значение для инвертирования реле (флаг для инверсии реле).
    bool getRelayInverted() const;

    // Получает значение для автоматического старта реле (флаг для автоматического
    // старта).
    bool getRelayAutostart() const;

    // Получает значение для автозавершения (флаг для автоматического
    // возобновления).
    bool getAutoResume() const;

    // Получает значение для автоматического режима (флаг для включения
    // автоматического режима).
    bool getAutoMode() const;

    // Получает параметры для параллельного отбора (диапазон для параметра
    // параллели).
    std::tuple<float, int> getParallel() const;

    // Получает скорость параллельного отбора клапаном №1.
    std::tuple<float, int> getParallelV1() const;

    // Получает скорость параллельного отбора клапаном №3 для 4-х диапазонов.
    std::vector<std::tuple<float, float, int>> getParallelV3()  const;

    // Получает сдвиг температуры тела (может быть использован для калибровки).
    float getHeartsTempShift() const;

    // Получает паузу для тела (время паузы для тела).
    bool getHeartsPause() const;

    // Получает сдвиг температуры TP2.
    float getTp2Shift() const;

    // Получает сигнал управления TP1 (флаг для включения/выключения контроля
    // TP1).
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

    std::string getSsvcVersion() const;

    float getSsvcApiVersion() const;
    bool apiSsvcIsSupport() const;
    bool isSupportTails() const;

    // SETTERS

    bool setSsvcVersion(std::string _ssvcVersion);

    bool setSsvcApiVersion(float _ssvcApiVersion);

private:
    explicit SsvcSettings();

    // Версии подисистем модуля ssvc
    std::string ssvcVersion = "";
    float ssvcApiVersion = 0.0;
    bool isSupportApi = false;
    bool supportTails = false;

    // signed char или unsigned char.

    // Параметры присутствующие в обоих ветках
    std::tuple<float, int> heads = {0.0, 0}; // головы
    std::tuple<float, int> hearts = {0.0, 0.0}; // тело

    float heads_final = 0.0;
    float hyst = 0.0;
    unsigned char decrement = 0;
    bool formula = false;
    unsigned char tank_mmhg = 0;
    unsigned int heads_timer = 0;
    unsigned char hearts_timer = 0;
    unsigned int start_delay = 0;
    float hearts_finish_temp = 0.0;
    float formula_start_temp = 0.0;
    std::array<int, 3> valve_bw{};

    // Дополнительно из JSON
    bool sound = false; // звук (флаг для включения/выключения звуковых сигналов)
    bool pressure = false; // давление (флаг для использования давления)
    bool relay_inverted = false; // инвертирование реле (флаг для инверсии реле)
    bool relay_autostart =
        false; // автоматический старт реле (флаг для автоматического старта)
    bool autoresume =
        false; // автозавершение (флаг для автоматического возобновления)
    bool auto_mode =
        false; // автоматический режим (флаг для включения автоматического режима)
    std::tuple<float, int> parallel = {
        0.0, 0
    }; // параллель (диапазон для параметра параллели)
    std::tuple<float, int> parallel_v1 = {
        0.0, 0
    }; // Скорость параллельного отбора клапаном №1
    std::vector<std::tuple<float, float, int>> parallel_v3; // Скорость параллельного отбора клапаном №3 для 4-х диапазонов
    float hearts_temp_shift =
        0.0; // сдвиг температуры тела (может быть использован для калибровки)
    bool hearts_pause = false; // пауза для тела (время паузы для тела)
    float tp2_shift = 0.0; // сдвиг температуры TP2
    bool tp_filter = false; // фильтр для температуры TP
    bool signal_tp1_control = false; // сигнал управления TP1 (флаг для
    // включения/выключения контроля TP1)
    bool signal_inverted =
        false; // инвертированный сигнал (флаг для инверсии сигнала)
    float tp1_control_temp =
        0.0; // контроль температуры TP1 (значение температуры для контроля TP1)
    bool tp1_control_start =
        false; // начало контроля температуры TP1 (флаг для начала контроля TP1)
    int stab_limit_time =
        0; // время стабилизации (ограничение времени стабилизации)
    bool stab_limit_finish =
        false; // завершение стабилизации (ограничение завершения стабилизации)
    std::string backlight; // подсветка (состояние подсветки)

    //    Актуально в firmware 2.2.*
    std::tuple<float, int> tails = {-1.0, -1};
    float tails_temp = 0.0;

    //    Актуально в firmware 2.3.*
    std::tuple<float, int> late_heads = {-1.0, -1};
    unsigned int late_heads_timer = 0;
    float release_speed = 0.0;
    unsigned int release_timer = 0;

    std::string lastSettingsSrc;

    static SsvcSettings* _ssvcSettings;

    static bool is_tuple_empty(const std::tuple<float, int>& t)
    {
        return std::get<0>(t) == float{} && // Проверка float на 0.0f
            std::get<1>(t) == int{}; // Проверка int на 0
    }

public:
    // Метод для отображения данных (для тестирования)

    // Класс билдера
    class Builder
    {
    private:
        SsvcSettings& settings; // Ссылка на существующий экземпляр
    public:
        bool hasChanges = false;
        explicit Builder() : settings(SsvcSettings::init())
        {
        }

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

        // Устанавливает величину уменьшения скорости отбора при "зелете"
        // температуры.
        Builder& setDecrement(unsigned char _decrement);

        // Включает или выключает использование формулы для снижения скорости
        // отбора.HG
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
        Builder& setValveBw(int v1, int v2, int v3);

        Builder& setReleaseSpeed(float _release_speed);

        Builder& setReleaseTimer(unsigned int _release_timer);

        Builder& setHeadsFinal(float _heartsFinishTemp);

        // Обработчики для установки оперативных параметров

        Builder& setStepTemp(float stepTemp);

        Builder& setStepHysteresis(float stepHysteresis);

        Builder& setStepSpeed(float timeTurnOn, int period);

        Builder& setStepDecrement(unsigned char _decrement);

        Builder& setStepTimer(unsigned int _timer);

        // Другие методы установки параметров...
        SsvcSettings build() const;

        static void validateAndSetValues(float& timeTurnOn, int& period,
                                         float* targetTimeTurnOn,
                                         int* targetPeriod);
    };
};

#endif // SSVC_OPEN_CONNECT_SSVCSETTINGS_H
