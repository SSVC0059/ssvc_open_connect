#include "SsvcSettings.h"

// Инициализация статического члена класса
SsvcSettings* SsvcSettings::_ssvcSettings = nullptr;

SsvcSettings &SsvcSettings::init(SsvcConnector& connector) {
    if (_ssvcSettings == nullptr) {
        _ssvcSettings = new SsvcSettings(connector);
    }
    return *_ssvcSettings;
}

SsvcSettings::SsvcSettings(SsvcConnector &ssvcConnector) : _ssvcConnector(ssvcConnector) {

    ESP_LOGI("SsvcSettings", "Create reload task");
    xTaskCreatePinnedToCore(
            SsvcSettings::reload,            // Function that should be called
            "reload settings",     // Name of the task (for debugging)
            2048,                       // Stack size (bytes)
            this,                       // Pass reference to this class instance
            (tskIDLE_PRIORITY),     // task priority
            nullptr,                       // Task handle
            1 // Pin to application core
    );
}


[[noreturn]] void SsvcSettings::reload(void *pvParameters) {
    auto* self = static_cast<SsvcSettings*>(pvParameters);
    const char* taskName = pcTaskGetName(nullptr);
    ESP_LOGE("RectificationProcess", "Запуск задачи подготовки данных ректификации");

    while (true) {
        UBaseType_t stackWaterMark = uxTaskGetStackHighWaterMark(nullptr);
        ESP_LOGI("SsvcOpenConnect", "Task %s: Stack high water mark: %u", taskName, stackWaterMark);

        // Ожидание события
        EventBits_t bits = xEventGroupWaitBits(
                eventGroup,
                BIT1,
                pdTRUE,
                pdFALSE,
                portMAX_DELAY
        );
        if (bits & BIT1) {
            self->_ssvcConnector.sendGetSettingsCommand();
        }
    }
}

bool SsvcSettings::load(const std::string& json) {
    ESP_LOGV("SsvcConnector", "Загрузка настроек контроллера");
    JsonDocument doc;

    DeserializationError error = deserializeJson(doc, json);

    if (error) {
        ESP_LOGV("SssvcController", "ошибка десериализации настроек: %s", error.c_str());
        return false;
    }

    if (doc["settings"].is<JsonObject>()) {
        JsonObject settings = doc["settings"];

        if (settings["heads"].is<JsonArray>()) {
            heads[0] = settings["heads"][0].as<float>();
            heads[1] = settings["heads"][1].as<float>();
            ESP_LOGV("SsvcSettings", "Обновлены heads: %f, %f", heads[0], heads[1]);
        }

        if (settings["hearts"].is<JsonArray>()) {
            hearts[0] = settings["hearts"][0].as<float>();
            hearts[1] = settings["hearts"][1].as<float>();
            ESP_LOGV("SsvcSettings", "Обновлены hearts: %f, %f", hearts[0], hearts[1]);
        }

        if (settings["late_heads"].is<JsonArray>()) {
            late_heads[0] = settings["late_heads"][0].as<float>();
            late_heads[1] = settings["late_heads"][1].as<float>();
            ESP_LOGV("SsvcSettings", "Обновлены late_heads: %f, %f", late_heads[0], late_heads[1]);
        }

        if (settings["tails"].is<JsonArray>()) {
            tails[0] = settings["tails"][0].as<float>();
            tails[1] = settings["tails"][1].as<float>();
            ESP_LOGV("SsvcSettings", "Обновлены tails: %f, %f", tails[0], tails[1]);
        }

        if (settings["tails_temp"].is<float>()) {
            tails_temp = settings["tails_temp"].as<float>();
            ESP_LOGV("SsvcSettings", "Обновлены tails: %f, %f", tails[0], tails[1]);
        }

        if (settings["valve_bw"].is<JsonArray>()) {
            valve_bw[0] = settings["valve_bw"][0].as<int>();
            valve_bw[1] = settings["valve_bw"][1].as<int>();
            valve_bw[2] = settings["valve_bw"][2].as<int>();
            ESP_LOGV("SsvcSettings", "Обновлены valve_bw: %d, %d, %d", valve_bw[0], valve_bw[1], valve_bw[2]);
        }

        hyst = settings["hyst"].as<float>();
        ESP_LOGV("SsvcSettings", "Обновлен hyst: %f", hyst);

        decrement = settings["decrement"].as<int>();
        ESP_LOGV("SsvcSettings", "Обновлен decrement: %d", decrement);

        formula = settings["formula"].as<bool>();
        ESP_LOGV("SsvcSettings", "Обновлен formula: %d", formula);

        tank_mmhg = settings["tank_mmhg"].as<int>();
        ESP_LOGV("SsvcSettings", "Обновлен tank_mmhg: %d", tank_mmhg);

        heads_timer = settings["heads_timer"].as<int>();
        ESP_LOGV("SsvcSettings", "Обновлен heads_timer: %d", heads_timer);

        late_heads_timer = settings["late_heads_timer"].as<unsigned int>();
        ESP_LOGV("SsvcSettings", "Обновлен late_heads_timer: %d", late_heads_timer);

        hearts_timer = settings["hearts_timer"].as<int>();
        ESP_LOGV("SsvcSettings", "Обновлен hearts_timer: %d", hearts_timer);

        start_delay = settings["start_delay"].as<int>();
        ESP_LOGV("SsvcSettings", "Обновлен start_delay: %d", start_delay);

        hearts_finish_temp = settings["hearts_finish_temp"].as<float>();
        ESP_LOGV("SsvcSettings", "Обновлен hearts_finish_temp: %f", hearts_finish_temp);

        formula_start_temp = settings["formula_start_temp"].as<float>();
        ESP_LOGV("SsvcSettings", "Обновлен formula_start_temp: %f", formula_start_temp);

        sound = settings["sound"].as<int>() != 0;
        ESP_LOGV("SsvcSettings", "Обновлен sound: %d", sound);

        pressure = settings["pressure"].as<int>() != 0;
        ESP_LOGV("SsvcSettings", "Обновлен pressure: %d", pressure);

        relay_inverted = settings["relay_inverted"].as<int>() != 0;
        ESP_LOGV("SsvcSettings", "Обновлен relay_inverted: %d", relay_inverted);

        relay_autostart = settings["relay_autostart"].as<int>() != 0;
        ESP_LOGV("SsvcSettings", "Обновлен relay_autostart: %d", relay_autostart);

        autoresume = settings["autoresume"].as<int>() != 0;
        ESP_LOGV("SsvcSettings", "Обновлен autoresume: %d", autoresume);

        auto_mode = settings["auto_mode"].as<int>() != 0;
        ESP_LOGV("SsvcSettings", "Обновлен auto_mode: %d", auto_mode);

        if (settings["parallel"].is<JsonArray>()) {
            parallel[0] = settings["parallel"][0].as<float>();
            parallel[1] = settings["parallel"][1].as<float>();
            ESP_LOGV("SsvcSettings", "Обновлены parallel: %f, %f", parallel[0], parallel[1]);
        }

        hearts_temp_shift = settings["hearts_temp_shift"].as<float>();
        ESP_LOGV("SsvcSettings", "Обновлен hearts_temp_shift: %f", hearts_temp_shift);

        hearts_pause = settings["hearts_pause"].as<int>() != 0;
        ESP_LOGV("SsvcSettings", "Обновлен hearts_pause: %d", hearts_pause);

        tp2_shift = settings["tp2_shift"].as<float>();
        ESP_LOGV("SsvcSettings", "Обновлен tp2_shift: %f", tp2_shift);

        tp_filter = settings["tp_filter"].as<int>() != 0;
        ESP_LOGV("SsvcSettings", "Обновлен tp_filter: %d", tp_filter);

        signal_tp1_control = settings["signal_tp1_control"].as<int>() != 0;
        ESP_LOGV("SsvcSettings", "Обновлен signal_tp1_control: %d", signal_tp1_control);

        signal_inverted = settings["signal_inverted"].as<int>() != 0;
        ESP_LOGV("SsvcSettings", "Обновлен signal_inverted: %d", signal_inverted);

        tp1_control_temp = settings["tp1_control_temp"].as<float>();
        ESP_LOGV("SsvcSettings", "Обновлен tp1_control_temp: %f", tp1_control_temp);

        tp1_control_start = settings["tp1_control_start"].as<int>() != 0;
        ESP_LOGV("SsvcSettings", "Обновлен tp1_control_start: %d", tp1_control_start);

        stab_limit_time = settings["stab_limit_time"].as<int>();
        ESP_LOGV("SsvcSettings", "Обновлен stab_limit_time: %d", stab_limit_time);

        stab_limit_finish = settings["stab_limit_finish"].as<int>() != 0;
        ESP_LOGV("SsvcSettings", "Обновлен stab_limit_finish: %d", stab_limit_finish);

        backlight = settings["backlight"].as<std::string>();
        ESP_LOGV("SsvcSettings", "Обновлен backlight: %s", backlight.c_str());

        // Обновление release_timer и release_speed
        if (settings["release_timer"].is<int>()) {
            release_timer = settings["release_timer"].as<int>();
            ESP_LOGV("SsvcSettings", "Обновлен release_timer: %d", release_timer);
        }

        if (settings["release_speed"].is<float>()) {
            release_speed = settings["release_speed"].as<float>();
            ESP_LOGV("SsvcSettings", "Обновлен release_speed: %f", release_speed);
        }
    }

    return true;
}

bool SsvcSettings::getSettings(JsonVariant settings) {
    ESP_LOGV("SsvcSettings", "Получение настроек контроллера");

    // heads
    JsonArray headsArray = settings["heads"].to<JsonArray>();
    headsArray.add(heads[0]);
    headsArray.add(heads[1]);

    // hearts
    JsonArray heartsArray = settings["hearts"].to<JsonArray>();
    heartsArray.add(hearts[0]);
    heartsArray.add(hearts[1]);

    // late_heads
    JsonArray lateHeadsArray = settings["late_heads"].to<JsonArray>();
    lateHeadsArray.add(late_heads[0]);
    lateHeadsArray.add(late_heads[1]);

    // tails
    JsonArray tailsArray = settings["tails"].to<JsonArray>();
    tailsArray.add(tails[0]);
    tailsArray.add(tails[1]);

    // valve_bw
    JsonArray valveBwArray = settings["valve_bw"].to<JsonArray>();
    valveBwArray.add(valve_bw[0]);
    valveBwArray.add(valve_bw[1]);
    valveBwArray.add(valve_bw[2]);

    // Прочие параметры
    settings["hyst"] = hyst;
    settings["decrement"] = decrement;
    settings["formula"] = formula;
    settings["tank_mmhg"] = tank_mmhg;
    settings["heads_timer"] = heads_timer;
    settings["late_heads_timer"] = late_heads_timer;
    settings["hearts_timer"] = hearts_timer;
    settings["start_delay"] = start_delay;
    settings["hearts_finish_temp"] = hearts_finish_temp;
    settings["formula_start_temp"] = formula_start_temp;

    settings["sound"] = sound;
    settings["pressure"] = pressure;
    settings["relay_inverted"] = relay_inverted;
    settings["relay_autostart"] = relay_autostart;
    settings["autoresume"] = autoresume;
    settings["auto_mode"] = auto_mode;

    settings["hearts_temp_shift"] = hearts_temp_shift;
    settings["hearts_pause"] = hearts_pause;
    settings["tp2_shift"] = tp2_shift;
    settings["tp_filter"] = tp_filter;
    settings["signal_tp1_control"] = signal_tp1_control;
    settings["signal_inverted"] = signal_inverted;
    settings["tp1_control_temp"] = tp1_control_temp;
    settings["tp1_control_start"] = tp1_control_start;
    settings["stab_limit_time"] = stab_limit_time;
    settings["stab_limit_finish"] = stab_limit_finish;

    settings["backlight"] = backlight;

    // Release
    settings["release_timer"] = release_timer;
    settings["release_speed"] = release_speed;

    // Сериализация в строку
    std::string json;
    serializeJson(settings, json);

    ESP_LOGV("SsvcSettings", "Получение получены: %s", json.c_str());
    return true;
}

// Getters

// Реализация геттеров
std::array<float, 2> SsvcSettings::getHeads() const {
    return heads;
}

std::array<float, 2>  SsvcSettings::getHearts() const {
    return hearts;
}

std::array<float, 2>  SsvcSettings::getLateHeads() const {
    return late_heads;
}

std::array<int, 3> SsvcSettings::getValveBw() const {
    return valve_bw;
}

float SsvcSettings::getHysteresis() const {
    return hyst;
}

unsigned char SsvcSettings::getDecrement() const {
    return decrement;
}

bool SsvcSettings::getFormula() const {
    return formula;
}

unsigned char SsvcSettings::getTank_mmhg() const {
    return tank_mmhg;
}

unsigned int SsvcSettings::getHeadsTimer() const {
    return heads_timer;
}

unsigned int SsvcSettings::getLateHeadsTimer() const {
    return late_heads_timer;
}

unsigned int SsvcSettings::getHeartsTimer() const {
    return hearts_timer;
}

unsigned int SsvcSettings::getStartDelay() const {
    return start_delay;
}

float SsvcSettings::getHeartsFinishTemp() const {
    return hearts_finish_temp;
}

float SsvcSettings::getFormulaStartTemp() const {
    return formula_start_temp;
}

std::array<float, 2>  SsvcSettings::getTails() const {
    return tails;
}

// Получает значение для звука (флаг для включения/выключения звуковых сигналов).
bool SsvcSettings::getSound() const {
    return sound;
}

// Получает значение для давления (флаг для использования давления).
bool SsvcSettings::getPressure() const {
    return pressure;
}

// Получает значение для инвертирования реле (флаг для инверсии реле).
bool SsvcSettings::getRelayInverted() const {
    return relay_inverted;
}

// Получает значение для автоматического старта реле (флаг для автоматического старта).
bool SsvcSettings::getRelayAutostart() const {
    return relay_autostart;
}

// Получает значение для автозавершения (флаг для автоматического возобновления).
bool SsvcSettings::getAutoResume() const {
    return autoresume;
}

// Получает значение для автоматического режима (флаг для включения автоматического режима).
bool SsvcSettings::getAutoMode() const {
    return auto_mode;
}

// Получает параметры для параллельного отбора (диапазон для параметра параллели).
std::array<float, 2> SsvcSettings::getParallel() const {
    return parallel;
}

// Получает скорость параллельного отбора клапаном №1.
std::array<float, 2> SsvcSettings::getParallelV1() const {
    return parallel_v1;
}

// Получает скорость параллельного отбора клапаном №3 для 4-х диапазонов.
std::array<float, 2> SsvcSettings::getParallelV3() const {
    return parallel_v3;
}

// Получает сдвиг температуры тела (может быть использован для калибровки).
float SsvcSettings::getHeartsTempShift() const {
    return hearts_temp_shift;
}

// Получает паузу для тела (время паузы для тела).
bool SsvcSettings::getHeartsPause() const {
    return hearts_pause;
}

// Получает сдвиг температуры TP2.
float SsvcSettings::getTp2Shift() const {
    return tp2_shift;
}

// Получает сигнал управления TP1 (флаг для включения/выключения контроля TP1).
bool SsvcSettings::getSignalTp1Control() const {
    return signal_tp1_control;
}

// Получает инвертированный сигнал (флаг для инверсии сигнала).
bool SsvcSettings::getSignalInverted() const {
    return signal_inverted;
}

// Получает контроль температуры TP1 (значение температуры для контроля TP1).
float SsvcSettings::getTp1ControlTemp() const {
    return tp1_control_temp;
}

// Получает начало контроля температуры TP1 (флаг для начала контроля TP1).
bool SsvcSettings::getTp1ControlStart() const {
    return tp1_control_start;
}

// Получает время стабилизации (ограничение времени стабилизации).
int SsvcSettings::getStabLimitTime() const {
    return stab_limit_time;
}

// Получает завершение стабилизации (ограничение завершения стабилизации).
bool SsvcSettings::getStabLimitFinish() const {
    return stab_limit_finish;
}

// Получает подсветку (состояние подсветки).
std::string SsvcSettings::getBacklight() const {
    return backlight;
}

// Получает температуру завершения отбора хвостов.
float SsvcSettings::getTailsTemp() const {
    return tails_temp;
}

float SsvcSettings::getReleaseSpeed() const {
    return release_speed;
}

int SsvcSettings::getReleaseTimeer() const {
    return release_timer;
}

//SETTERS

//
// heads
//- Описание: Настройки скорости для "голов" (время открытого клапана и период цикла).
//- Синтаксис:
//"heads=[<время_включения>,<период>]"
//- "<время_включения>": вещественное число с одной десятичной цифрой (например, "99.0").
//- "<период>": целое число.
//- Ограничения:
//- время_включения ≤ 99.9
//- период ≤ 999
//- время_включения ≤ период
//- Пример:
//heads=[99.0,100]
SsvcSettings::Builder& SsvcSettings::Builder::setHeads(float timeTurnOn, int period) {
    validateAndSetValues(timeTurnOn, period, &settings->heads[0], &settings->heads[1]);
    return *this;
}

SsvcSettings::Builder& SsvcSettings::Builder::setHearts(float timeTurnOn, int period) {
    validateAndSetValues(timeTurnOn, period, &settings->hearts[0], &settings->hearts[1]);
    return *this;
}

SsvcSettings::Builder& SsvcSettings::Builder::setLateHeads(float timeTurnOn, int period) {
    validateAndSetValues(timeTurnOn, period, &settings->late_heads[0], &settings->late_heads[1]);
    return *this;
}

SsvcSettings::Builder& SsvcSettings::Builder::setTails(float timeTurnOn, int period) {
    validateAndSetValues(timeTurnOn, period, &settings->tails[0], &settings->tails[1]);
    return *this;
}

// hyst
//- Описание: Устанавливает значение гистерезиса
//- Синтаксис:
//"hyst=<значение>"
//- "<значение>" — число с плавающей точкой (например, 0.25).
//- Ограничения:
//- Значение должно быть положительным числом, но не более 50.0.
//- Пример:
//"hyst=0.25"
SsvcSettings::Builder& SsvcSettings::Builder::setHysteresis(float _hyst) {
    if (xSemaphoreTake(mutex, portMAX_DELAY)) {
        settings->hyst = std::min(std::max(_hyst, 0.0f), 50.0f);
        xSemaphoreGive(mutex);
    }
    return *this;
}

// Decrement
//- Описание: Устанавливает величину в процентах, на которую будет уменьшаться скорость отбора тела при "зелете" температуры.
//- Синтаксис:
//"decrement=<значение>"
//- "<значение>" — целое число (например, 20).
//- Ограничения:
//- Значение должно быть целым положительным числом от 0 до 100 включительно.
//- Пример:
//"decrement=20"
__attribute__((unused)) SsvcSettings::Builder& SsvcSettings::Builder::setDecrement(unsigned char  _decrement) {
    if (xSemaphoreTake(mutex, portMAX_DELAY)) {
        settings->decrement = std::min(std::max(_decrement, (unsigned char)0), (unsigned char)100);
        xSemaphoreGive(mutex);
    }
    return *this;
}

// Formula
//- Описание: Использовать или нет формулу для снижения скорости отбора тела в зависимости от температуры на ТД2.
//- Синтаксис:
//"formula=[0|1]"
//- "[0|1]" — 0 - не использовать формулу, 1 - использовать.
//- Пример:
//"formula=1"
SsvcSettings::Builder& SsvcSettings::Builder::formulaEnable(bool enable) {
    if (xSemaphoreTake(mutex, portMAX_DELAY)) {
        settings->formula = enable;
        xSemaphoreGive(mutex);
    }
    return *this;
}

// tank_mmhg
//- Описание: Устанавливает давление в кубе в миллиметрах ртутного столба (мм рт. ст.) относительно атмосферного.
//- Синтаксис:
//"tank_mmhg=<значение>"
//- "<значение>" — целое число (например, 30).
//- Ограничения:
//- Значение должно быть положительным не более 50.
//- Пример:
//"tank_mmhg=50"
SsvcSettings::Builder& SsvcSettings::Builder::setTank_mmhg(unsigned char _tank_mmhg) {
    if (xSemaphoreTake(mutex, portMAX_DELAY)) {
        settings->tank_mmhg = std::min(std::max(_tank_mmhg, (unsigned char)0), (unsigned char)50);
        xSemaphoreGive(mutex);
    }
    return *this;
}

// Heads_timer
//- Описание: Таймер отбора голов (время в секундах).
//- Синтаксис:
//"heads_timer=<время>"
//- "<время>" — целое число (например, 36000).
//- Ограничения:
//- Значение должно быть положительным не более 86400, кратное 300 (соответствует 5 минутам).
//- Пример:
//"heads_timer=36000"
SsvcSettings::Builder& SsvcSettings::Builder::setHeadsTimer(unsigned int _headsTimer) {
    if (xSemaphoreTake(mutex, portMAX_DELAY)) {
        unsigned int headsTimer = std::min(std::max(_headsTimer, (unsigned int)0), (unsigned int)86400);
        settings->heads_timer = (headsTimer + 150) / 300 * 300;
        xSemaphoreGive(mutex);
    }
    return *this;
}

// late_heads_timer
//- Описание: Таймер отбора подголовников (время в секундах).
//- Синтаксис:
//"late_heads_timer=<время>"
//- "<время>" — целое число (например, 36000).
//- Ограничения:
//- Значение должно быть положительным не более 86400, кратное 300 (соответствует 5 минутам).
//- Пример:
//"late_heads_timer=36000"
SsvcSettings::Builder& SsvcSettings::Builder::setLateHeadsTimer(unsigned int _liteHeadsTimer) {
    if (xSemaphoreTake(mutex, portMAX_DELAY)) {
        unsigned int liteHeadsTimer = std::min(std::max(_liteHeadsTimer, (unsigned int)0), (unsigned int)86400);
        settings->late_heads_timer =  (liteHeadsTimer + 150) / 300 * 300;
        xSemaphoreGive(mutex);
    }
    return *this;
}

// Hearts_timer
//- Описание: Таймер фиксации температуры отбора тела, мин
//- Синтаксис:
//"hearts_timer=<время>"
//- "<время>" — целое число (например, 10).
//- Ограничения:
//- Значение должно быть положительным не более 30.
//- Пример:
//"hearts_timer=30"
SsvcSettings::Builder& SsvcSettings::Builder::setHeartsTimer(unsigned char _heartsTimer) {
    if (xSemaphoreTake(mutex, portMAX_DELAY)) {
        settings->heads_timer = std::min(std::max(_heartsTimer, (unsigned char)0), (unsigned char)30);
        xSemaphoreGive(mutex);
    }
    return *this;
}

// Tails_temp
//- Описание: Устанавливает завершения отбора хвостов, в градусах Цельсия.
//- Синтаксис:
//"tails_temp=<температура>"
//- "<температура>" — число с плавающей точкой (например, 109.9).
//- Ограничения:
//- Температура должна быть положительным числом не более 110.0.
//- Пример:
//"tails_temp=109.9"
SsvcSettings::Builder& SsvcSettings::Builder::setTailsTemp(float _tailsTemp) {
    if (xSemaphoreTake(mutex, portMAX_DELAY)) {
        settings->tails_temp = std::min(std::max(_tailsTemp, 0.0f), 110.0f);
        xSemaphoreGive(mutex);
    }
    return *this;
}

// Start_delay
//- Описание: Отложенный пуск (время в секундах).
//- Синтаксис:
//"start_delay=<время>"
//- "<время>" — целое число (например, 6000).
//- Ограничения:
//- Значение должно быть положительным не более 18000.
//- Пример:
//"start_delay=18000"
SsvcSettings::Builder& SsvcSettings::Builder::setStartDelay(unsigned int _start_delay){
    if (xSemaphoreTake(mutex, portMAX_DELAY)) {
        settings->start_delay = std::min(std::max(_start_delay, (unsigned int)0), (unsigned int)18000);
        xSemaphoreGive(mutex);
    }
    return *this;
}

// Hearts_finish_temp
//- Описание: Температура на ТД2, при которой завершается отбор тела, в градусах Цельсия.
//- Синтаксис:
//"hearts_finish_temp=<температура>"
//- "<температура>" — число с плавающей точкой (например, 93.0).
//- Ограничения:
//- Температура должна быть положительным числом не более 110.0.
//- Пример:
//"hearts_finish_temp=93.0"
SsvcSettings::Builder& SsvcSettings::Builder::setHeartsFinishTemp(float _heartsFinishTemp) {
    if (xSemaphoreTake(mutex, portMAX_DELAY)) {
        settings->hearts_finish_temp = std::min(std::max(_heartsFinishTemp, 0.0f), 110.0f);
        xSemaphoreGive(mutex);
    }
    return *this;
}

// Formula_start_temp
//- Описание: Начальная температура для начала работы формулы, в градусах Цельсия.
//- Синтаксис:
//"formula_start_temp=<температура>"
//- "<температура>" — число с плавающей точкой (например, 84.0).
//- Ограничения:
//- Температура должна быть положительным числом не более 100.0 и не менее 84.0.
//- Пример:
//"formula_start_temp=84.0"
SsvcSettings::Builder& SsvcSettings::Builder::setFormulaStartTemp(float _formulaStartTemp) {
    if (xSemaphoreTake(mutex, portMAX_DELAY)) {
        settings->formula_start_temp =  std::min(std::max(_formulaStartTemp, 0.0f), 110.0f);
        xSemaphoreGive(mutex);
    }
    return *this;
}

// Valve_bw
//- Описание: Устанавливает пропускные способности клапанов в мл/ч.
//- Синтаксис:
//valve_bw=[<скорость1>, <скорость2>, <скорость3>]
//- Каждая скорость "<скоростьN>" — целое число.
//- Ограничения:
//- Все скорости ≤ 20000.
//- Пример:
//valve speeds=[10000,20000,15000]
SsvcSettings::Builder& SsvcSettings::Builder::setValveBw(int v1, int v2 , int v3) {
    if (xSemaphoreTake(mutex, portMAX_DELAY)) {
        settings->valve_bw[0] = std::min(std::max(v1, 0), 20000);
        settings->valve_bw[2] = std::min(std::max(v2, 0), 20000);
        settings->valve_bw[3] = std::min(std::max(v3, 0), 20000);
        xSemaphoreGive(mutex);
    }
    return *this;
}

SsvcSettings SsvcSettings::Builder::build() {
    return *settings;
}
