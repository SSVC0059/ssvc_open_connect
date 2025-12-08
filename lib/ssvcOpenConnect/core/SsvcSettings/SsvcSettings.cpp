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

#include "SsvcSettings.h"

#include "core/StatefulServices/SensorDataService/SensorDataService.h"
#include <cstdlib>

SsvcSettings *SsvcSettings::_ssvcSettings = nullptr;

SsvcSettings &SsvcSettings::init() {
  if (_ssvcSettings == nullptr) {
    _ssvcSettings = new SsvcSettings();
  }
  return *_ssvcSettings;
}

SsvcSettings::SsvcSettings() = default;

bool SsvcSettings::load(const std::string &json) {
  ESP_LOGD("SsvcConnector", "Загрузка настроек контроллера");
  JsonDocument doc;

  const DeserializationError error = deserializeJson(doc, json);

  OpenConnectSettingsService* settingsService = OpenConnectSettingsService::getInstance();
  auto jsonObject = doc.as<JsonObject>();
  settingsService->update(jsonObject, OpenConnectSettings::update, "settings");

  if (error) {
    ESP_LOGV("SssvcController", "ошибка десериализации настроек: %s",
             error.c_str());
  }

  if (doc["settings"].is<JsonObject>()) {
    const JsonObject settings = doc["settings"];

    if (settings["heads"].is<JsonArray>()) {
      std::get<0>(heads) = settings["heads"][0].as<float>();
      std::get<1>(heads) = settings["heads"][1].as<int>();
      ESP_LOGV("SsvcSettings", "Обновлены heads: %f, %d", std::get<0>(heads),
               std::get<1>(heads));
    }

    if (settings["hearts"].is<JsonArray>()) {
      std::get<0>(hearts) = settings["hearts"][0].as<float>();
      std::get<1>(hearts) = settings["hearts"][1].as<int>();
      ESP_LOGV("SsvcSettings", "Обновлены hearts: %f, %d", std::get<0>(hearts),
               std::get<1>(hearts));
    }

    if (settings["late_heads"].is<JsonArray>()) {
      std::get<0>(late_heads) = settings["late_heads"][0].as<float>();
      std::get<1>(late_heads) = settings["late_heads"][1].as<int>();
      ESP_LOGV("SsvcSettings", "Обновлены late_heads: %f, %d",
               std::get<0>(late_heads), std::get<1>(late_heads));
    }

    if (settings["tails"].is<JsonArray>()) {
      std::get<0>(tails) = settings["tails"][0].as<float>();
      std::get<1>(tails) = settings["tails"][1].as<int>();
      ESP_LOGD("SsvcSettings", "Обновлены tails: %f, %d", std::get<0>(tails),
               std::get<1>(tails));
    }

    if (settings["tails_temp"].is<float>()) {
      tails_temp = settings["tails_temp"].as<float>();
      ESP_LOGV("SsvcSettings", "Обновлены tails: %d", tails_temp);
    }

    if (settings["valve_bw"].is<JsonArray>()) {
      valve_bw[0] = settings["valve_bw"][0].as<int>();
      valve_bw[1] = settings["valve_bw"][1].as<int>();
      valve_bw[2] = settings["valve_bw"][2].as<int>();
      ESP_LOGV("SsvcSettings", "Обновлены valve_bw: %d, %d, %d", valve_bw[0],
               valve_bw[1], valve_bw[2]);
    }

    hyst = settings["hyst"].as<float>();
    ESP_LOGV("SsvcSettings", "Обновлен hyst: %f", hyst);

    decrement = settings["decrement"].as<int>();
    ESP_LOGV("SsvcSettings", "Обновлен decrement: %d", decrement);

    formula = settings["formula"].as<bool>();
    ESP_LOGV("SsvcSettings", "Обновлен formula: %b", formula);

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
    ESP_LOGV("SsvcSettings", "Обновлен hearts_finish_temp: %f",
             hearts_finish_temp);

    formula_start_temp = settings["formula_start_temp"].as<float>();
    ESP_LOGV("SsvcSettings", "Обновлен formula_start_temp: %f",
             formula_start_temp);

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
      std::get<0>(parallel) = settings["parallel"][0].as<float>();
      std::get<1>(parallel) = settings["parallel"][1].as<int>();
      ESP_LOGV("SsvcSettings", "Обновлены parallel: %f, %d",
               std::get<0>(parallel), std::get<1>(parallel));
    }

    if (settings["parallel_v1"].is<JsonArray>()) {
      std::get<0>(parallel_v1) = settings["parallel_v1"][0].as<float>();
      std::get<1>(parallel_v1) = settings["parallel_v1"][1].as<int>();
      ESP_LOGV("SsvcSettings", "Обновлены parallel_v1: %f, %d",
               std::get<0>(parallel_v1), std::get<1>(parallel_v1));
    }

    if (settings["parallel_v3"].is<JsonArray>()) {
      const JsonArray parallel_v3_array = settings["parallel_v3"];

      // Очищаем текущий вектор и резервируем память для 4 элементов
      parallel_v3.clear();
      parallel_v3.reserve(4);

      // Проходим по всем 4 массивам в parallel_v3
      for (int i = 0; i < 4 && i < parallel_v3_array.size(); i++) {
        if (parallel_v3_array[i].is<JsonArray>()) {
          JsonArray inner_array = parallel_v3_array[i];

          // Создаем tuple для текущего внутреннего массива
          std::tuple<float, float, int> inner_tuple;

          // Заполняем tuple значениями из массива
          if (inner_array.size() > 0) std::get<0>(inner_tuple) = inner_array[0].as<float>();
          if (inner_array.size() > 1) std::get<1>(inner_tuple) = inner_array[1].as<float>();
          if (inner_array.size() > 2) std::get<2>(inner_tuple) = inner_array[2].as<int>();

          // Добавляем tuple в вектор
          parallel_v3.push_back(inner_tuple);

          ESP_LOGV("SsvcSettings", "parallel_v3[%d]: %f, %f, %d",
                   i, std::get<0>(inner_tuple), std::get<1>(inner_tuple), std::get<2>(inner_tuple));
        }
      }

      ESP_LOGV("SsvcSettings", "Обновлен parallel_v3, всего %d элементов", parallel_v3.size());
    }

    hearts_temp_shift = settings["hearts_temp_shift"].as<float>();
    ESP_LOGV("SsvcSettings", "Обновлен hearts_temp_shift: %f",
             hearts_temp_shift);

    hearts_pause = settings["hearts_pause"].as<int>() != 0;
    ESP_LOGV("SsvcSettings", "Обновлен hearts_pause: %d", hearts_pause);

    tp2_shift = settings["tp2_shift"].as<float>();
    ESP_LOGV("SsvcSettings", "Обновлен tp2_shift: %f", tp2_shift);

    tp_filter = settings["tp_filter"].as<int>() != 0;
    ESP_LOGV("SsvcSettings", "Обновлен tp_filter: %d", tp_filter);

    signal_tp1_control = settings["signal_tp1_control"].as<int>() != 0;
    ESP_LOGV("SsvcSettings", "Обновлен signal_tp1_control: %d",
             signal_tp1_control);

    signal_inverted = settings["signal_inverted"].as<int>() != 0;
    ESP_LOGV("SsvcSettings", "Обновлен signal_inverted: %d", signal_inverted);

    tp1_control_temp = settings["tp1_control_temp"].as<float>();
    ESP_LOGV("SsvcSettings", "Обновлен tp1_control_temp: %f", tp1_control_temp);

    tp1_control_start = settings["tp1_control_start"].as<int>() != 0;
    ESP_LOGV("SsvcSettings", "Обновлен tp1_control_start: %d",
             tp1_control_start);

    stab_limit_time = settings["stab_limit_time"].as<int>();
    ESP_LOGV("SsvcSettings", "Обновлен stab_limit_time: %d", stab_limit_time);

    stab_limit_finish = settings["stab_limit_finish"].as<int>() != 0;
    ESP_LOGV("SsvcSettings", "Обновлен stab_limit_finish: %d",
             stab_limit_finish);

    backlight = settings["backlight"].as<std::string>();
    ESP_LOGV("SsvcSettings", "Обновлен backlight: %s", backlight.c_str());

    if (settings["release_timer"].is<int>()) {
      release_timer = settings["release_timer"].as<int>();
      ESP_LOGV("SsvcSettings", "Обновлен release_timer: %d", release_timer);
    }

    if (settings["release_speed"].is<float>()) {
      release_speed = settings["release_speed"].as<float>();
      ESP_LOGV("SsvcSettings", "Обновлен release_speed: %f", release_speed);
    }
    if (settings["heads_final"].is<float>()) {
      heads_final = settings["heads_final"].as<float>();
      ESP_LOGV("SsvcSettings", "Обновлен heads_final: %f", heads_final);
    }
  }

  return true;
}

void SsvcSettings::fillSettings(JsonVariant settings) const
{
  ESP_LOGV("SsvcSettings", "Получение настроек контроллера");

  // heads
  const JsonArray headsArray = settings["heads"].to<JsonArray>();
  headsArray.add(std::get<0>(heads));
  headsArray.add(std::get<1>(heads));

  // hearts
  const JsonArray heartsArray = settings["hearts"].to<JsonArray>();
  heartsArray.add(std::get<0>(hearts));
  heartsArray.add(std::get<1>(hearts));

  // late_heads
  //    JsonArray lateHeadsArray = settings["late_heads"].to<JsonArray>();
  //    lateHeadsArray.add(std::get<0>(late_heads));
  //    lateHeadsArray.add(std::get<1>(late_heads));
  // tails
  const float late_heads_period = std::get<0>(late_heads);
  const int late_heads_open = std::get<1>(late_heads);
  if (late_heads_period > -1 || late_heads_open > -1) {
    const JsonArray lateHeadsArray = settings["late_heads"].to<JsonArray>();
    (void)lateHeadsArray.add(late_heads_period);
    (void)lateHeadsArray.add(late_heads_open);
  }

  // tails
  const float tails_period = std::get<0>(tails);
  const int tails_open = std::get<1>(tails);
  if (tails_period > -1 || tails_open > -1) {
    const JsonArray tailsArray = settings["tails"].to<JsonArray>();
    (void)tailsArray.add(tails_period);
    (void)tailsArray.add(tails_open);
  }

  if (tails_temp > 0)
  {
    settings["tails_temp"] = tails_temp;
  }

  // valve_bw
  const auto valveBwArray = settings["valve_bw"].to<JsonArray>();
  (void)valveBwArray.add(valve_bw[0]);
  (void)valveBwArray.add(valve_bw[1]);
  (void)valveBwArray.add(valve_bw[2]);

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
  settings["heads_final"] = heads_final;

  if (!is_tuple_empty(parallel)) {
    const auto _parallel = settings["parallel"].to<JsonArray>();
    _parallel.add(std::get<0>(parallel));
    _parallel.add(std::get<1>(parallel));
  }

  if (!is_tuple_empty(parallel_v1)) {
    const auto _parallel_v1 = settings["parallel_v1"].to<JsonArray>();
    _parallel_v1.add(std::get<0>(parallel_v1));
    _parallel_v1.add(std::get<1>(parallel_v1));
  }

  if (!parallel_v3.empty()) {
    const JsonArray parallel_v3_array = settings["parallel_v3"].to<JsonArray>();
    for (const auto& item : parallel_v3) {
      auto inner_array = parallel_v3_array.add<JsonArray>();
      inner_array.add(std::get<0>(item));
      inner_array.add(std::get<1>(item));
      inner_array.add(std::get<2>(item));
    }
  }

  // Сериализация в строку
  std::string json;
  serializeJson(settings, json);

  ESP_LOGV("SsvcSettings", "Получение получены: %s", json.c_str());
}

// Getters

// Реализация геттеров
std::tuple<float, int> SsvcSettings::getHeads() const { return heads; }

std::tuple<float, int> SsvcSettings::getHearts() const { return hearts; }

std::tuple<float, int> SsvcSettings::getLateHeads() const { return late_heads; }

std::array<int, 3> SsvcSettings::getValveBw() const { return valve_bw; }

float SsvcSettings::getHysteresis() const { return hyst; }

unsigned char SsvcSettings::getDecrement() const { return decrement; }

bool SsvcSettings::getFormula() const { return formula; }

unsigned char SsvcSettings::getTank_mmhg() const { return tank_mmhg; }

unsigned int SsvcSettings::getHeadsTimer() const { return heads_timer; }

unsigned int SsvcSettings::getLateHeadsTimer() const {
  return late_heads_timer;
}

unsigned int SsvcSettings::getHeartsTimer() const { return hearts_timer; }

unsigned int SsvcSettings::getStartDelay() const { return start_delay; }

float SsvcSettings::getHeartsFinishTemp() const { return hearts_finish_temp; }

float SsvcSettings::getFormulaStartTemp() const { return formula_start_temp; }

std::tuple<float, int> SsvcSettings::getTails() const { return tails; }

// Получает значение для звука (флаг для включения/выключения звуковых
// сигналов).
bool SsvcSettings::getSound() const { return sound; }

// Получает значение для давления (флаг для использования давления).
bool SsvcSettings::getPressure() const { return pressure; }

// Получает значение для инвертирования реле (флаг для инверсии реле).
bool SsvcSettings::getRelayInverted() const { return relay_inverted; }

// Получает значение для автоматического старта реле (флаг для автоматического
// старта).
bool SsvcSettings::getRelayAutostart() const { return relay_autostart; }

// Получает значение для автозавершения (флаг для автоматического
// возобновления).
bool SsvcSettings::getAutoResume() const { return autoresume; }

// Получает значение для автоматического режима (флаг для включения
// автоматического режима).
bool SsvcSettings::getAutoMode() const { return auto_mode; }

// Получает параметры для параллельного отбора (диапазон для параметра
// параллели).
std::tuple<float, int> SsvcSettings::getParallel() const { return parallel; }

// Получает скорость параллельного отбора клапаном №1.
std::tuple<float, int> SsvcSettings::getParallelV1() const {
  return parallel_v1;
}

// Получает скорость параллельного отбора клапаном №3 для 4-х диапазонов.
std::vector<std::tuple<float, float, int>> SsvcSettings::getParallelV3() const {
  return parallel_v3;
}

// Получает сдвиг температуры тела (может быть использован для калибровки).
float SsvcSettings::getHeartsTempShift() const { return hearts_temp_shift; }

// Получает паузу для тела (время паузы для тела).
bool SsvcSettings::getHeartsPause() const { return hearts_pause; }

// Получает сдвиг температуры TP2.
float SsvcSettings::getTp2Shift() const { return tp2_shift; }

// Получает сигнал управления TP1 (флаг для включения/выключения контроля TP1).
bool SsvcSettings::getSignalTp1Control() const { return signal_tp1_control; }

// Получает инвертированный сигнал (флаг для инверсии сигнала).
bool SsvcSettings::getSignalInverted() const { return signal_inverted; }

// Получает контроль температуры TP1 (значение температуры для контроля TP1).
float SsvcSettings::getTp1ControlTemp() const { return tp1_control_temp; }

// Получает начало контроля температуры TP1 (флаг для начала контроля TP1).
bool SsvcSettings::getTp1ControlStart() const { return tp1_control_start; }

// Получает время стабилизации (ограничение времени стабилизации).
int SsvcSettings::getStabLimitTime() const { return stab_limit_time; }

// Получает завершение стабилизации (ограничение завершения стабилизации).
bool SsvcSettings::getStabLimitFinish() const { return stab_limit_finish; }

// Получает подсветку (состояние подсветки).
std::string SsvcSettings::getBacklight() const { return backlight; }

// Получает температуру завершения отбора хвостов.
float SsvcSettings::getTailsTemp() const { return tails_temp; }

float SsvcSettings::getReleaseSpeed() const { return release_speed; }

int SsvcSettings::getReleaseTimeer() const { return release_timer; }

std::string SsvcSettings::getSsvcVersion() const
{
  if (ssvcVersion.empty()) {
    return "";
  }
  return ssvcVersion;
}

float SsvcSettings::getSsvcApiVersion() const
{
  return ssvcApiVersion;
}

bool SsvcSettings::apiSsvcIsSupport() const {
  return isSupportApi;
}

bool SsvcSettings::isSupportTails() const {
  return supportTails;
}

bool SsvcSettings::setSsvcVersion(std::string _ssvcVersion)
{
  this->ssvcVersion = std::move(_ssvcVersion);

  if (this->ssvcVersion.rfind("2.2", 0) == 0) {
    supportTails = true;
  } else {
    supportTails = false;
  }

  return true;
}

bool SsvcSettings::setSsvcApiVersion(const float _ssvcApiVersion)
{
  ESP_LOGD("SsvcSettings", "setSsvcApiVersion: %f", _ssvcApiVersion);
  this->ssvcApiVersion = _ssvcApiVersion;
#ifdef SSVC_SUPPORT_API_VERSION
  constexpr float supportVersion = SSVC_SUPPORT_API_VERSION;
  isSupportApi = this->ssvcApiVersion >= supportVersion;
#endif
  return true;
}

// SETTERS

//
// heads
//- Описание: Настройки скорости для "голов" (время открытого клапана и период
// цикла).
//- Синтаксис:
//"heads=[<время_включения>,<период>]"
//- "<время_включения>": вещественное число с одной десятичной цифрой (например,
//"99.0").
//- "<период>": целое число.
//- Ограничения:
//- время_включения ≤ 99.9
//- период ≤ 999
//- время_включения ≤ период
//- Пример:
// heads=[99.0,100]
SsvcSettings::Builder &SsvcSettings::Builder::setHeads(float timeTurnOn,
                                                       int period) {
  const float prevTime = std::get<0>(settings.heads);
  const int prevPeriod = std::get<1>(settings.heads);

  ESP_LOGV("SsvcSettings", "setHeads: timeTurnOn=%f, period=%d", timeTurnOn,
           period);
  SsvcSettings::Builder::validateAndSetValues(timeTurnOn, period,
                                              &std::get<0>(settings.heads),
                                              &std::get<1>(settings.heads));
  if (std::get<0>(settings.heads) != prevTime ||
       std::get<1>(settings.heads) != prevPeriod) {

    char buffer[50];
    std::snprintf(buffer, sizeof(buffer), "heads=[%.1f,%d]",
                  std::get<0>(settings.heads), std::get<1>(settings.heads));
    SsvcCommandsQueue::getQueue().set(buffer);
       }
  return *this;
}

SsvcSettings::Builder &SsvcSettings::Builder::setHearts(float timeTurnOn,
                                                        int period) {
  const float prevTime = std::get<0>(settings.hearts);
  const int prevPeriod = std::get<1>(settings.hearts);

  validateAndSetValues(timeTurnOn, period, &std::get<0>(settings.hearts),
                       &std::get<1>(settings.hearts));
  if (std::get<0>(settings.hearts) != prevTime ||
       std::get<1>(settings.hearts) != prevPeriod) {
    char buffer[50];
    std::snprintf(buffer, sizeof(buffer), "hearts=[%.1f,%d]",
                  std::get<0>(settings.hearts), std::get<1>(settings.hearts));
    SsvcCommandsQueue::getQueue().set(buffer);
  }
  return *this;
}

SsvcSettings::Builder &SsvcSettings::Builder::setLateHeads(float timeTurnOn,
                                                           int period) {
  const float prevTime = std::get<0>(settings.late_heads);
  const int prevPeriod = std::get<1>(settings.late_heads);

  validateAndSetValues(timeTurnOn, period, &std::get<0>(settings.late_heads),
                       &std::get<1>(settings.late_heads));
  if (std::get<0>(settings.late_heads) != prevTime ||
       std::get<1>(settings.late_heads) != prevPeriod) {
    char buffer[50];
    std::snprintf(buffer, sizeof(buffer), "late_heads=[%.1f,%d]",
                  std::get<0>(settings.late_heads),
                  std::get<1>(settings.late_heads));
    SsvcCommandsQueue::getQueue().set(buffer);
  }
  return *this;
}

SsvcSettings::Builder &SsvcSettings::Builder::setTails(float timeTurnOn,
                                                       int period) {
  const float prevTime = std::get<0>(settings.tails);
  const int prevPeriod = std::get<1>(settings.tails);

  validateAndSetValues(timeTurnOn, period, &std::get<0>(settings.tails),
                       &std::get<1>(settings.tails));
  if (std::get<0>(settings.tails) != prevTime ||
       std::get<1>(settings.tails) != prevPeriod) {
    char buffer[50];
    std::snprintf(buffer, sizeof(buffer), "tails=[%.1f,%d]",
                  std::get<0>(settings.tails), std::get<1>(settings.tails));
    SsvcCommandsQueue::getQueue().set(buffer);
       }
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
SsvcSettings::Builder &SsvcSettings::Builder::setHysteresis(float _hyst) {
  const float prevHyst = settings.hyst;
  settings.hyst = std::min(std::max(_hyst, 0.0f), 50.0f);
  if (settings.hyst != prevHyst) {
    char buffer[50];
    std::snprintf(buffer, sizeof(buffer), "hyst=%.2f", settings.hyst);
    ESP_LOGD("SsvcSettings", "setHysteresis: %s", buffer);
    SsvcCommandsQueue::getQueue().set(buffer);
  }
  return *this;
}

// Decrement
//- Описание: Устанавливает величину в процентах, на которую будет уменьшаться
// скорость отбора тела при "зелете" температуры.
//- Синтаксис:
//"decrement=<значение>"
//- "<значение>" — целое число (например, 20).
//- Ограничения:
//- Значение должно быть целым положительным числом от 0 до 100 включительно.
//- Пример:
//"decrement=20"
SsvcSettings::Builder &
SsvcSettings::Builder::setDecrement(unsigned char _decrement) {
  const unsigned char prevDecrement = settings.decrement;
  settings.decrement =
      std::min(std::max(_decrement, static_cast<unsigned char>(0)), static_cast<unsigned char>(100));
  if (settings.decrement != prevDecrement) {
    char buffer[50];
    std::snprintf(buffer, sizeof(buffer), "decrement=%d", settings.decrement);
    ESP_LOGD("SsvcSettings", "setDecrement: %s", buffer);
    SsvcCommandsQueue::getQueue().set(buffer);
  }
  return *this;
}

// Formula
//- Описание: Использовать или нет формулу для снижения скорости отбора тела в
// зависимости от температуры на ТД2.
//- Синтаксис:
//"formula=[0|1]"
//- "[0|1]" — 0 - не использовать формулу, 1 - использовать.
//- Пример:
//"formula=1"
SsvcSettings::Builder &SsvcSettings::Builder::formulaEnable(const bool enable) {
  const bool prevFormula = settings.formula;
  settings.formula = enable;
  if (settings.formula != prevFormula) {
    char buffer[50];
    std::snprintf(buffer, sizeof(buffer), "formula=%d", settings.formula);
    ESP_LOGD("SsvcSettings", "formulaEnable: %s", buffer);
    SsvcCommandsQueue::getQueue().set(buffer);
  }
  return *this;
}

// tank_mmhg
//- Описание: Устанавливает давление в кубе в миллиметрах ртутного столба (мм
// рт. ст.) относительно атмосферного.
//- Синтаксис:
//"tank_mmhg=<значение>"
//- "<значение>" — целое число (например, 30).
//- Ограничения:
//- Значение должно быть положительным не более 50.
//- Пример:
//"tank_mmhg=50"
SsvcSettings::Builder &
SsvcSettings::Builder::setTank_mmhg(unsigned char _tank_mmhg) {
  const unsigned char prevTank_mmhg = settings.tank_mmhg;
  settings.tank_mmhg =
      std::min(std::max(_tank_mmhg, static_cast<unsigned char>(0)), static_cast<unsigned char>(50));
  if (settings.tank_mmhg != prevTank_mmhg) {
    char buffer[50];
    std::snprintf(buffer, sizeof(buffer), "tank_mmhg=%d", settings.tank_mmhg);
    ESP_LOGD("SsvcSettings", "tank_mmhg: %s", buffer);
    SsvcCommandsQueue::getQueue().set(buffer);
  }
  return *this;
}

// Heads_timer
//- Описание: Таймер отбора голов (время в секундах).
//- Синтаксис:
//"heads_timer=<время>"
//- "<время>" — целое число (например, 36000).
//- Ограничения:
//- Значение должно быть положительным не более 86400, кратное 300
//(соответствует 5 минутам).
//- Пример:
//"heads_timer=36000"
SsvcSettings::Builder &
SsvcSettings::Builder::setHeadsTimer(const unsigned int _headsTimer) {
  const unsigned int prevHeadsTimer = settings.heads_timer;
  ESP_LOGD("SsvcSettings", "_headsTimer: %d", _headsTimer);
  const unsigned int headsTimer =
      std::min(std::max(_headsTimer,
        static_cast<unsigned int>(0)),
        static_cast<unsigned int>(86400));
  ESP_LOGD("SsvcSettings", "headsTimer min|max: %d", _headsTimer);
  settings.heads_timer = headsTimer;
  if (settings.heads_timer != prevHeadsTimer) {
    char buffer[50];
    std::snprintf(buffer, sizeof(buffer), "heads_timer=%d", settings.heads_timer);
    ESP_LOGD("SsvcSettings", "heads_timer: %s", buffer);
    SsvcCommandsQueue::getQueue().set(buffer);
  }
  return *this;
}

// late_heads_timer
//- Описание: Таймер отбора подголовников (время в секундах).
//- Синтаксис:
//"late_heads_timer=<время>"
//- "<время>" — целое число (например, 36000).
//- Ограничения:
//- Значение должно быть положительным не более 86400, кратное 300
//(соответствует 5 минутам).
//- Пример:
//"late_heads_timer=36000"
SsvcSettings::Builder &
SsvcSettings::Builder::setLateHeadsTimer(const unsigned int _liteHeadsTimer) {
  const unsigned int prevLateHeadsTimer = settings.late_heads_timer;
  const unsigned int liteHeadsTimer =
      std::min(std::max(_liteHeadsTimer, static_cast<unsigned int>(0)), static_cast<unsigned int>(86400));
  settings.late_heads_timer = liteHeadsTimer;
  if (settings.late_heads_timer != prevLateHeadsTimer) {
    char buffer[50];
    std::snprintf(buffer, sizeof(buffer), "late_heads_timer=%d",
                  settings.late_heads_timer);
    ESP_LOGD("SsvcSettings", "late_heads_timer: %s", buffer);
    SsvcCommandsQueue::getQueue().set(buffer);
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
SsvcSettings::Builder &
SsvcSettings::Builder::setHeartsTimer(const unsigned char _heartsTimer) {
  const unsigned char prevHeartsTimer = settings.hearts_timer;
  settings.hearts_timer =
      std::min(std::max(_heartsTimer, static_cast<unsigned char>(0)), static_cast<unsigned char>(30));
  if (settings.hearts_timer != prevHeartsTimer) {
    char buffer[50];
    std::snprintf(buffer, sizeof(buffer), "hearts_timer=%d",
                  settings.hearts_timer);
    ESP_LOGD("SsvcSettings", "hearts_timer: %s", buffer);
    SsvcCommandsQueue::getQueue().set(buffer);
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
SsvcSettings::Builder &SsvcSettings::Builder::setTailsTemp(const float _tailsTemp) {
  const float prevTailsTemp = settings.tails_temp;
  settings.tails_temp = std::min(std::max(_tailsTemp, 0.0f), 110.0f);
  if (settings.tails_temp != prevTailsTemp) {
    char buffer[50];
    std::snprintf(buffer, sizeof(buffer), "tails_temp=%0.1f",
                  settings.tails_temp);
    ESP_LOGD("SsvcSettings", "tails_temp: %s", buffer);
    SsvcCommandsQueue::getQueue().set(buffer);
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
SsvcSettings::Builder &
SsvcSettings::Builder::setStartDelay(const unsigned int _start_delay) {
  const unsigned int prevStartDelay = settings.start_delay;
  settings.start_delay =
      std::min(std::max(_start_delay,
        static_cast<unsigned int>(0)),
        static_cast<unsigned int>(18000));
  if (settings.start_delay != prevStartDelay) {
    char buffer[50];
    std::snprintf(buffer, sizeof(buffer), "start_delay=%d", settings.start_delay);
    ESP_LOGD("SsvcSettings", "Start_delay: %s", buffer);
    SsvcCommandsQueue::getQueue().set(buffer);
  }
  return *this;
}

// Hearts_finish_temp
//- Описание: Температура на ТД2, при которой завершается отбор тела, в градусах
// Цельсия.
//- Синтаксис:
//"hearts_finish_temp=<температура>"
//- "<температура>" — число с плавающей точкой (например, 93.0).
//- Ограничения:
//- Температура должна быть положительным числом не более 110.0.
//- Пример:
//"hearts_finish_temp=93.0"
SsvcSettings::Builder &
SsvcSettings::Builder::setHeartsFinishTemp(const float _heartsFinishTemp) {
  const float prevHeartsFinishTemp = settings.hearts_finish_temp;
  settings.hearts_finish_temp =
      std::min(std::max(_heartsFinishTemp, 0.0f), 110.0f);
  if (settings.hearts_finish_temp != prevHeartsFinishTemp) {
    char buffer[50];
    std::snprintf(buffer, sizeof(buffer), "hearts_finish_temp=%0.1f",
                  settings.hearts_finish_temp);
    ESP_LOGD("SsvcSettings", "Start_delay: %s", buffer);
    SsvcCommandsQueue::getQueue().set(buffer);
  }
  return *this;
}

// Formula_start_temp
//- Описание: Начальная температура для начала работы формулы, в градусах
// Цельсия.
//- Синтаксис:
//"formula_start_temp=<температура>"
//- "<температура>" — число с плавающей точкой (например, 84.0).
//- Ограничения:
//- Температура должна быть положительным числом не более 100.0 и не менее 84.0.
//- Пример:
//"formula_start_temp=84.0"
SsvcSettings::Builder &
SsvcSettings::Builder::setFormulaStartTemp(const float _formulaStartTemp) {
  const float prevFormulaStartTemp = settings.formula_start_temp;
  settings.formula_start_temp =
      std::min(std::max(_formulaStartTemp, 84.0f), 100.0f);
  if (settings.formula_start_temp != prevFormulaStartTemp) {
    char buffer[50];
    std::snprintf(buffer, sizeof(buffer), "formula_start_temp=%0.1f",
                  settings.formula_start_temp);
    ESP_LOGD("SsvcSettings", "formula_start_temp: %s", buffer);
    SsvcCommandsQueue::getQueue().set(buffer);
  }
  return *this;
}

// Valve_bw
//- Описание: Устанавливает пропускные способности клапанов в мл/ч.
//- Синтаксис:
// valve_bw=[<скорость1>, <скорость2>, <скорость3>]
//- Каждая скорость "<скоростьN>" — целое число.
//- Ограничения:
//- Все скорости ≤ 20000.
//- Пример:
// valve speeds=[10000,20000,15000]
SsvcSettings::Builder &SsvcSettings::Builder::setValveBw(const int v1, const int v2,
                                                         const int v3) {
  const int prevV1 = settings.valve_bw[0];
  const int prevV2 = settings.valve_bw[1];
  const int prevV3 = settings.valve_bw[2];

  const int valve_bw_0 = std::min(std::max(v1, 0), 20000);
  const int valve_bw_1 = std::min(std::max(v2, 0), 20000);
  const int valve_bw_2 = std::min(std::max(v3, 0), 20000);

  settings.valve_bw[0] = valve_bw_0;
  settings.valve_bw[1] = valve_bw_1;
  settings.valve_bw[2] = valve_bw_2;

  if (settings.valve_bw[0] != prevV1 || settings.valve_bw[1] != prevV2 || settings.valve_bw[2] != prevV3) {
    char buffer[50];
    std::snprintf(buffer, sizeof(buffer), "valve_bw=[%d,%d,%d]", valve_bw_0,
                  valve_bw_1, valve_bw_2);
    SsvcCommandsQueue::getQueue().set(buffer);
  }

  return *this;
}

SsvcSettings::Builder &
SsvcSettings::Builder::setReleaseSpeed(const float _release_speed) {
  const float prevReleaseSpeed = settings.release_speed;
  settings.release_speed = std::min(std::max(_release_speed, 0.0f), 99.9f);
  if (settings.release_speed != prevReleaseSpeed) {
    char buffer[50];
    std::snprintf(buffer, sizeof(buffer), "release_speed=%0.1f",
                  settings.release_speed);
    ESP_LOGD("SsvcSettings", "release_speed: %s", buffer);
    SsvcCommandsQueue::getQueue().set(buffer);
  }
  return *this;
}

SsvcSettings::Builder &
SsvcSettings::Builder::setReleaseTimer(const unsigned int _release_timer) {
  const unsigned int prevReleaseTimer = settings.release_timer;
  settings.release_timer =
      std::min(std::max(_release_timer, static_cast<unsigned int>(0)), static_cast<unsigned int>(1200));
  if (settings.release_timer != prevReleaseTimer) {
    char buffer[50];
    std::snprintf(buffer, sizeof(buffer), "start_delay=%d",
                  settings.release_timer);
    ESP_LOGD("SsvcSettings", "release_speed: %s", buffer);
    SsvcCommandsQueue::getQueue().set(buffer);
  }
  return *this;
}

SsvcSettings::Builder &
SsvcSettings::Builder::setHeadsFinal(const float _heartsFinishTemp) {
  const float prevHeadsFinal = settings.heads_final;
  settings.heads_final = std::min(std::max(_heartsFinishTemp, 0.0f), 99.9f);
  if (settings.heads_final != prevHeadsFinal) {
    char buffer[50];
    std::snprintf(buffer, sizeof(buffer), "heads_final=%0.1f",
                  settings.heads_final);
    ESP_LOGD("SsvcSettings", "heads_final: %s", buffer);
    SsvcCommandsQueue::getQueue().set(buffer);
  }
  return *this;
}

SsvcSettings::Builder& SsvcSettings::Builder::setStepTemp(const float stepTemp){
  char buffer[50];
  std::snprintf(buffer, sizeof(buffer), "s_temp=%0.1f",
                stepTemp);
  ESP_LOGD("SsvcSettings", "s_temp: %s", buffer);
  SsvcCommandsQueue::getQueue().set(buffer);
  return *this;
}

SsvcSettings::Builder& SsvcSettings::Builder::setStepHysteresis(const float stepHysteresis) {
  char buffer[50];
  std::snprintf(buffer, sizeof(buffer), "s_hyst=%.2f", stepHysteresis);
  ESP_LOGD("SsvcSettings", "s_hyst: %s", buffer);
  SsvcCommandsQueue::getQueue().set(buffer);
  return *this;
}

SsvcSettings::Builder &SsvcSettings::Builder::setStepSpeed(const float timeTurnOn,
                                                       const int period) {
  char buffer[50];
  std::snprintf(buffer, sizeof(buffer), "s_speed=[%.1f,%d]",
                timeTurnOn, period);
  SsvcCommandsQueue::getQueue().set(buffer);
  return *this;
}

SsvcSettings::Builder &
SsvcSettings::Builder::setStepDecrement(const unsigned char _decrement) {
  char buffer[50];
  std::snprintf(buffer, sizeof(buffer), "s_decrement=%d", _decrement);
  ESP_LOGD("SsvcSettings", "s_decrement: %s", buffer);
  SsvcCommandsQueue::getQueue().set(buffer);
  return *this;
}

SsvcSettings::Builder &
SsvcSettings::Builder::setStepTimer(const unsigned int _timer) {
  char buffer[50];
  std::snprintf(buffer, sizeof(buffer), "s_timer=%d",
                _timer);
  ESP_LOGI("SsvcSettings", "s_timer: %s", buffer);
  SsvcCommandsQueue::getQueue().set(buffer);
  return *this;
}

SsvcSettings SsvcSettings::Builder::build() const { return settings; }

void SsvcSettings::Builder::validateAndSetValues(float &timeTurnOn, int &period,
                                                 float *targetTimeTurnOn,
                                                 int *targetPeriod) {
  if (!targetTimeTurnOn || !targetPeriod) {
    ESP_LOGE("Settings", "Invalid pointers!");
    return;
  }
  // Проверка и корректировка значения timeTurnOn
  timeTurnOn = std::min(std::max(timeTurnOn, 0.0f), 99.9f);

  // Проверка и корректировка значения period
  period = std::min(std::max(period, 0), 999);

  // Назначение значений
  *targetTimeTurnOn = timeTurnOn;
  *targetPeriod = period; // Без приведения к float!
}
