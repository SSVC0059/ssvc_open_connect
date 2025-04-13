//
// Created by demoncat on 23.03.2025.
//

#ifndef SSVCOPENCONNECT_SETTINGSHANDLERS_H
#define SSVCOPENCONNECT_SETTINGSHANDLERS_H

#include "ArduinoJson.h"
#include "core/SsvcSettings.h"
#include <WString.h>
#include <climits>
#include <map>
#include <memory>

class ParamHandler {
public:
  virtual ~ParamHandler() = default;

  virtual bool handle(SsvcSettings::Builder &builder,
                      const JsonVariant &value) const = 0;
};

// Обработчик для параметров с двумя значениями (массив из float и int)
// (например, heads=[99.0, 100])
class TwoValueHandler : public ParamHandler {
public:
  using Setter = std::function<void(SsvcSettings::Builder &, float, int)>;

  explicit TwoValueHandler(Setter setter) : _setter(std::move(setter)) {}

  bool handle(SsvcSettings::Builder &builder,
              const JsonVariant &value) const override {
    if (!value.is<JsonArray>() || value.size() != 2)
      return false;
    ESP_LOGI("TwoValueHandler", "value: %s", value.as<String>().c_str());

    const auto timeTurnOn = value[0].as<float>();
    const int period = value[1].as<int>();

    _setter(builder, timeTurnOn, period);
    return true;
  }

private:
  Setter _setter;
};

// Обработчик для параметров с одним float значением (например, hyst=0.25)
class SingleFloatHandler : public ParamHandler {
public:
  using Setter = std::function<void(SsvcSettings::Builder &, float)>;

  explicit SingleFloatHandler(Setter setter) : _setter(std::move(setter)) {}

  bool handle(SsvcSettings::Builder &builder,
              const JsonVariant &value) const override {
    if (!value.is<float>())
      return false;
    ESP_LOGI("SingleFloatHandler", "value: %s", value.as<String>().c_str());

    const auto val = value.as<float>();

    _setter(builder, val);
    return true;
  }

private:
  Setter _setter;
};

// Обработчик для параметра decrement (целое число 0-100)
class CharHandler : public ParamHandler {
public:
  using Setter = std::function<void(SsvcSettings::Builder &, char)>;

  explicit CharHandler(Setter setter) : _setter(std::move(setter)) {}

  bool handle(SsvcSettings::Builder &builder,
              const JsonVariant &value) const override {
    if (!value.is<int>())
      return false;

    // 2. Получаем значение как int
    const char val = value.as<int>();

    // 3. Проверяем диапазон char
    if (static_cast<int>(val) < CHAR_MIN || static_cast<int>(val) > CHAR_MAX) {
      ESP_LOGE("DecrementHandler", "Value %d is out of range char", val);
      return false;
    }

    // 4. Приводим к unsigned char и передаём
    _setter(builder, static_cast<unsigned char>(val));
    return true;
  }

private:
  Setter _setter;
};

// Обработчик для параметра decrement (целое число 0-100)
class UnsignedCharHandler : public ParamHandler {
public:
  using Setter = std::function<void(SsvcSettings::Builder &, unsigned char)>;

  explicit UnsignedCharHandler(Setter setter) : _setter(std::move(setter)) {}

  bool handle(SsvcSettings::Builder &builder,
              const JsonVariant &value) const override {
    if (!value.is<int>())
      return false;

    // 2. Получаем значение как int
    const char val = value.as<int>();

    //        if (val < 0 || static_cast<unsigned int>(val) > UCHAR_MAX) {
    if (static_cast<int>(val) < 0 || static_cast<int>(val) > UCHAR_MAX) {
      ESP_LOGE("DecrementHandler", "Value %d is out of range (0-%u)", val,
               UCHAR_MAX);
      return false;
    }

    // 4. Приводим к unsigned char и передаём
    _setter(builder, static_cast<unsigned char>(val));
    return true;
  }

private:
  Setter _setter;
};

// Обработчик для параметра типа int
class IntHandler : public ParamHandler {
public:
  using Setter = std::function<void(SsvcSettings::Builder &, int)>;

  explicit IntHandler(Setter setter) : _setter(std::move(setter)) {}

  bool handle(SsvcSettings::Builder &builder,
              const JsonVariant &value) const override {
    if (!value.is<int>()) {
      ESP_LOGE("IntHandler", "Value is not an integer");
      return false;
    }
    _setter(builder, value.as<int>());
    return true;
  }

private:
  Setter _setter;
};

// Обработчик для unsigned int (с проверкой на отрицательные значения)
class UnsignedIntHandler : public ParamHandler {
public:
  using Setter = std::function<void(SsvcSettings::Builder &, unsigned int)>;

  explicit UnsignedIntHandler(Setter setter) : _setter(std::move(setter)) {}

  bool handle(SsvcSettings::Builder &builder,
              const JsonVariant &value) const override {
    if (!value.is<int>())
      return false;

    const int val = value.as<int>();
    if (val < 0 || static_cast<unsigned int>(val) > UINT_MAX) {
      ESP_LOGE("UnsignedIntHandler", "Value %d invalid (must be 0..%u)", val,
               UINT_MAX);
      return false;
    }

    _setter(builder, static_cast<unsigned int>(val));
    return true;
  }

private:
  Setter _setter;
};

// Для параметров вида "param": [int1, int2, int3]
class ThreeIntHandler : public ParamHandler {
public:
  using Setter = std::function<void(SsvcSettings::Builder &, int, int, int)>;

  explicit ThreeIntHandler(Setter setter) : _setter(std::move(setter)) {}

  bool handle(SsvcSettings::Builder &builder,
              const JsonVariant &value) const override {
    if (!value.is<JsonArray>() || value.size() != 3)
      return false;

    bool allValid = true;
    allValid &= value[0].is<int>();
    allValid &= value[1].is<int>();
    allValid &= value[2].is<int>();

    if (!allValid) {
      ESP_LOGE("ThreeIntHandler", "Array must contain 3 integers");
      return false;
    }

    _setter(builder, value[0].as<int>(), value[1].as<int>(),
            value[2].as<int>());
    return true;
  }

private:
  Setter _setter;
};

class BooleanHandler : public ParamHandler {
public:
  using Setter = std::function<void(SsvcSettings::Builder &, bool)>;

  explicit BooleanHandler(Setter setter) : _setter(std::move(setter)) {}

  bool handle(SsvcSettings::Builder &builder,
              const JsonVariant &value) const override {
    // Проверяем, что значение является boolean или числом (0/1)
    if (!value.is<bool>() && !value.is<int>()) {
      ESP_LOGE("BooleanHandler", "Value must be boolean or 0/1");
      return false;
    }

    // Преобразуем значение к bool
    bool boolValue;
    if (value.is<bool>()) {
      boolValue = value.as<bool>();
    } else {
      int intValue = value.as<int>();
      if (intValue != 0 && intValue != 1) {
        ESP_LOGE("BooleanHandler", "Numeric value must be 0 or 1, got %d",
                 intValue);
        return false;
      }
      boolValue = (intValue != 0);
    }

    _setter(builder, boolValue);
    return true;
  }

private:
  Setter _setter;
};

using ParamHandlerMap = std::map<String, std::unique_ptr<ParamHandler>>;

extern const ParamHandlerMap PARAM_HANDLERS;

#endif // SSVCOPENCONNECT_SETTINGSHANDLERS_H
