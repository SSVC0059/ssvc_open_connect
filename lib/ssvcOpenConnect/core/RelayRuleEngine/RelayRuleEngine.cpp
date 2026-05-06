#include "RelayRuleEngine.h"

#include "core/AlarmMonitor/AlarmMonitor.h"
#include "core/RelayPortCoordinator/RelayPortCoordinator.h"
#include "core/rectification/RectificationProcess.h"
#include "core/SsvcSettings/SsvcSettings.h"
#include <climits>
#include <cmath>
#include <cstring>
#include <esp_log.h>

namespace {
using FieldType = RelayRuleEngine::SsvcFieldValueType;
using FieldDesc = RelayRuleEngine::SsvcFieldDescriptor;

class EngineLock {
public:
  explicit EngineLock(SemaphoreHandle_t lock) : _lock(lock) {
    if (_lock != nullptr) {
      xSemaphoreTakeRecursive(_lock, portMAX_DELAY);
    }
  }

  ~EngineLock() {
    if (_lock != nullptr) {
      xSemaphoreGiveRecursive(_lock);
    }
  }

private:
  SemaphoreHandle_t _lock;
};

const FieldDesc kSsvcFieldDescriptors[] = {
    {"formula", "Формула скорости", FieldType::BOOL},
    {"auto_mode", "Автоматический режим", FieldType::BOOL},
    {"sound", "Звуковое оповещение", FieldType::BOOL},
    {"pressure", "Контроль давления", FieldType::BOOL},
    {"relay_inverted", "Инверсия реле", FieldType::BOOL},
    {"relay_autostart", "Автозапуск реле", FieldType::BOOL},
    {"autoresume", "Автовозобновление", FieldType::BOOL},
    {"hearts_pause", "Пауза тела", FieldType::BOOL},
    {"signal_tp1_control", "Контроль сигнала TP1", FieldType::BOOL},
    {"signal_inverted", "Инверсия сигнала", FieldType::BOOL},
    {"tp1_control_start", "Старт контроля TP1", FieldType::BOOL},
    {"stab_limit_finish", "Завершение по лимиту стабилизации", FieldType::BOOL},
    {"decrement", "Декремент", FieldType::INT},
    {"heads_timer", "Таймер голов", FieldType::INT},
    {"late_heads_timer", "Таймер подголовников", FieldType::INT},
    {"hearts_timer", "Таймер тела", FieldType::INT},
    {"start_delay", "Задержка старта", FieldType::INT},
    {"stab_limit_time", "Лимит стабилизации", FieldType::INT},
    {"release_timer", "Таймер сброса", FieldType::INT},
    {"tank_mmhg", "Давление в кубе (мм рт.ст.)", FieldType::FLOAT},
    {"hyst", "Гистерезис", FieldType::FLOAT},
    {"hearts_finish_temp", "Температура конца тела", FieldType::FLOAT},
    {"formula_start_temp", "Температура старта формулы", FieldType::FLOAT},
    {"tails_temp", "Температура хвостов", FieldType::FLOAT},
    {"release_speed", "Скорость сброса", FieldType::FLOAT},
    {"hearts_temp_shift", "Сдвиг температуры тела", FieldType::FLOAT},
    {"tp2_shift", "Сдвиг TP2", FieldType::FLOAT},
    {"tp1_control_temp", "Температура контроля TP1", FieldType::FLOAT},
};
} // namespace

const std::vector<RelayRuleEngine::SsvcFieldDescriptor>& RelayRuleEngine::getSsvcFieldDescriptors() {
  static const std::vector<SsvcFieldDescriptor> descriptors(
      std::begin(kSsvcFieldDescriptors), std::end(kSsvcFieldDescriptors));
  return descriptors;
}

RelayRuleEngine& RelayRuleEngine::getInstance() {
  static RelayRuleEngine instance;
  return instance;
}

RelayRuleEngine::RelayRuleEngine() : _lock(xSemaphoreCreateRecursiveMutex()) {}

void RelayRuleEngine::timerCallback(TimerHandle_t t) {
  (void)t;
  RelayRuleEngine::getInstance().recomputeAndApply();
}

void RelayRuleEngine::begin() {
  AlarmMonitor::getInstance().subscribe(this);
#if !PINOUT_USE_GPIO
  if (_timer == nullptr) {
    _timer = xTimerCreate(TIMER_NAME, pdMS_TO_TICKS(3000), pdTRUE, nullptr, timerCallback);
    if (_timer) {
      xTimerStart(_timer, 0);
    }
  }
#endif
  ESP_LOGI(TAG, "RelayRuleEngine started");
}

static AlarmLevel levelFromInt(int v) {
  switch (v) {
    case 0:
      return AlarmLevel::NORMAL;
    case 1:
      return AlarmLevel::MIN;
    case 2:
      return AlarmLevel::DANGEROUS;
    case 3:
      return AlarmLevel::CRITICAL;
    default:
      return AlarmLevel::NORMAL;
  }
}

bool RelayRuleEngine::parseSensorAlarmCondition(const JsonObject& cond, Rule& r) const {
  r.condKind = Rule::CondKind::SENSOR_ALARM;
  r.anySensorLevel = false;
  r.wantMin = r.wantDanger = r.wantCrit = false;
  if (cond["levels"].is<JsonArray>()) {
    for (JsonVariant lv : cond["levels"].as<JsonArray>()) {
      const AlarmLevel al = levelFromInt(lv.as<int>());
      if (al == AlarmLevel::MIN) {
        r.wantMin = true;
      }
      if (al == AlarmLevel::DANGEROUS) {
        r.wantDanger = true;
      }
      if (al == AlarmLevel::CRITICAL) {
        r.wantCrit = true;
      }
    }
  } else {
    r.wantDanger = true;
    r.wantCrit = true;
  }
  if (cond["sensorNameContains"].is<const char*>()) {
    r.sensorNameSubstr = cond["sensorNameContains"].as<const char*>();
  }
  return true;
}

bool RelayRuleEngine::parseHardwareFaultCondition(const JsonObject& cond, Rule& r) const {
  r.condKind = Rule::CondKind::HARDWARE_FAULT;
  r.hwCode = cond["code"] | -1;
  if (cond["roleContains"].is<const char*>()) {
    r.roleSubstr = cond["roleContains"].as<const char*>();
  }
  return true;
}

bool RelayRuleEngine::parseRectificationCondition(const JsonObject& cond, Rule& r) const {
  r.condKind = Rule::CondKind::RECTIFICATION;
  if (cond["stageEquals"].is<const char*>()) {
    r.stageEquals = cond["stageEquals"].as<const char*>();
  }
  return true;
}

bool RelayRuleEngine::parseSsvcSettingCondition(const JsonObject& cond, Rule& r) const {
  if (!cond["key"].is<const char*>()) {
    return false;
  }
  r.ssvcKey = cond["key"].as<const char*>();
  if (cond["boolEquals"].is<bool>()) {
    r.condKind = Rule::CondKind::SSVC_BOOL;
    r.ssvcBool = cond["boolEquals"].as<bool>();
    return true;
  }
  if (cond["intEquals"].is<int>() || cond["intEquals"].is<long>()) {
    r.condKind = Rule::CondKind::SSVC_INT;
    r.ssvcInt = cond["intEquals"].as<int>();
    return true;
  }
  if (cond["floatEquals"].is<float>() || cond["floatEquals"].is<double>()) {
    r.condKind = Rule::CondKind::SSVC_FLOAT;
    r.ssvcFloat = cond["floatEquals"].as<float>();
    return true;
  }
  return false;
}

bool RelayRuleEngine::parseCondition(const JsonObject& cond, Rule& r) const {
  const String t = cond["type"] | "sensor_alarm";
  if (t == "sensor_alarm") {
    return parseSensorAlarmCondition(cond, r);
  }
  if (t == "hardware_fault") {
    return parseHardwareFaultCondition(cond, r);
  }
  if (t == "rectification") {
    return parseRectificationCondition(cond, r);
  }
  if (t == "ssvc_setting") {
    return parseSsvcSettingCondition(cond, r);
  }
  return false;
}

void RelayRuleEngine::loadRules(const JsonObject& src) {
  EngineLock lock(_lock);
  _rules.clear();
  _schemaVersion = src["schemaVersion"] | 1;

  const JsonArray arr = src["rules"].as<JsonArray>();
  if (arr.isNull()) {
    ESP_LOGI(TAG, "loadRules: no rules array");
    recomputeAndApply();
    return;
  }

  for (JsonObject rj : arr) {
    Rule r;
    r.enabled = rj["enabled"] | true;
    r.targetBit = rj["targetBit"] | 2;
    r.actionEnergize = rj["actionEnergize"] | true;
    r.priority = rj["priority"] | 0;

    const JsonObject cond = rj["condition"].as<JsonObject>();
    if (cond.isNull()) {
      continue;
    }
    if (!parseCondition(cond, r)) {
      continue;
    }
    _rules.push_back(std::move(r));
  }

  ESP_LOGI(TAG, "loadRules: %u rules", static_cast<unsigned>(_rules.size()));
  recomputeAndApply();
}

void RelayRuleEngine::serializeToJson(JsonObject& dest) const {
  EngineLock lock(_lock);
  dest["schemaVersion"] = _schemaVersion;
  JsonArray arr = dest["rules"].to<JsonArray>();
  for (const Rule& r : _rules) {
    JsonObject rj = arr.add<JsonObject>();
    rj["enabled"] = r.enabled;
    rj["targetBit"] = r.targetBit;
    rj["actionEnergize"] = r.actionEnergize;
    rj["priority"] = r.priority;
    JsonObject c = rj["condition"].to<JsonObject>();
    switch (r.condKind) {
      case Rule::CondKind::SENSOR_ALARM:
        c["type"] = "sensor_alarm";
        if (!r.anySensorLevel || r.wantMin || r.wantDanger || r.wantCrit) {
          JsonArray lv = c["levels"].to<JsonArray>();
          if (r.wantMin) {
            lv.add(1);
          }
          if (r.wantDanger) {
            lv.add(2);
          }
          if (r.wantCrit) {
            lv.add(3);
          }
        }
        if (!r.sensorNameSubstr.empty()) {
          c["sensorNameContains"] = r.sensorNameSubstr.c_str();
        }
        break;
      case Rule::CondKind::HARDWARE_FAULT:
        c["type"] = "hardware_fault";
        if (r.hwCode >= 0) {
          c["code"] = r.hwCode;
        }
        if (!r.roleSubstr.empty()) {
          c["roleContains"] = r.roleSubstr.c_str();
        }
        break;
      case Rule::CondKind::RECTIFICATION:
        c["type"] = "rectification";
        c["stageEquals"] = r.stageEquals.c_str();
        break;
      case Rule::CondKind::SSVC_BOOL:
        c["type"] = "ssvc_setting";
        c["key"] = r.ssvcKey.c_str();
        c["boolEquals"] = r.ssvcBool;
        break;
      case Rule::CondKind::SSVC_INT:
        c["type"] = "ssvc_setting";
        c["key"] = r.ssvcKey.c_str();
        c["intEquals"] = r.ssvcInt;
        break;
      case Rule::CondKind::SSVC_FLOAT:
        c["type"] = "ssvc_setting";
        c["key"] = r.ssvcKey.c_str();
        c["floatEquals"] = r.ssvcFloat;
        break;
    }
  }
}

void RelayRuleEngine::setManualOverride(const unsigned bit, const bool energized, const bool enableOverride) {
#if !PINOUT_USE_GPIO
  EngineLock lock(_lock);
  auto& coord = RelayPortCoordinator::getInstance();
  if (bit >= 64 || bit >= coord.totalRelayLines()) {
    return;
  }
  const uint64_t b = 1ULL << bit;
  if (coord.isAlarmReservedGlobal(bit)) {
    return;
  }
  if (!enableOverride) {
    _manualMask &= ~b;
    _manualEnergized &= ~b;
  } else {
    _manualMask |= b;
    if (energized) {
      _manualEnergized |= b;
    } else {
      _manualEnergized &= ~b;
    }
  }
  recomputeAndApply();
#endif
}

void RelayRuleEngine::clearAllManualOverrides() {
  EngineLock lock(_lock);
  _manualMask = 0;
  _manualEnergized = 0;
  recomputeAndApply();
}

#if !PINOUT_USE_GPIO
void RelayRuleEngine::appendCapabilitiesJson(JsonObject& root) const {
  const auto& coord = RelayPortCoordinator::getInstance();
  const std::vector<uint8_t> relayAddresses = coord.configuredAddresses();
  root["linesPerChip"] = SSVC_RELAY_PCF8574_LINES_PER_CHIP;
  JsonArray addrArr = root["relayPcf8574Addresses"].to<JsonArray>();
  for (uint8_t a : relayAddresses) {
    addrArr.add(a);
  }
  root["relayChipCount"] = coord.relayChipCount();
  root["relayLineCount"] = coord.totalRelayLines();
  if (!relayAddresses.empty()) {
    root["i2cAddress"] = relayAddresses[0];
  }
  root["alarmBitDangerous"] = SSVC_RELAY_PCF8574_BIT_DANGEROUS;
  root["alarmBitCritical"] = SSVC_RELAY_PCF8574_BIT_CRITICAL;
  root["alarmGlobalBitDangerous"] = SSVC_RELAY_PCF8574_BIT_DANGEROUS;
  root["alarmGlobalBitCritical"] = SSVC_RELAY_PCF8574_BIT_CRITICAL;
  root["firstChipAlarmReservedBits"] = coord.reservedAlarmBitsMask();
}

void RelayRuleEngine::appendSsvcFieldsJson(JsonArray& arr) const {
  for (const auto& d : getSsvcFieldDescriptors()) {
    JsonObject o = arr.add<JsonObject>();
    o["key"] = d.key;
    o["label"] = d.label;
    switch (d.valueType) {
      case SsvcFieldValueType::BOOL:
        o["valueType"] = "bool";
        break;
      case SsvcFieldValueType::INT:
        o["valueType"] = "int";
        break;
      case SsvcFieldValueType::FLOAT:
        o["valueType"] = "float";
        break;
    }
  }
}
#endif

bool RelayRuleEngine::matchSensorAlarm(const Rule& r) const {
  for (const auto& kv : _sensorAlarmStates) {
    const AlarmLevel lv = kv.second.level;
    bool levelOk = false;
    if (r.wantMin && lv == AlarmLevel::MIN) {
      levelOk = true;
    }
    if (r.wantDanger && lv == AlarmLevel::DANGEROUS) {
      levelOk = true;
    }
    if (r.wantCrit && lv == AlarmLevel::CRITICAL) {
      levelOk = true;
    }
    if (!levelOk) {
      continue;
    }
    if (!r.sensorNameSubstr.empty() && kv.second.name.find(r.sensorNameSubstr) == std::string::npos) {
      continue;
    }
    return true;
  }
  return false;
}

bool RelayRuleEngine::matchHardwareFault(const Rule& r) const {
  // Query the live fault table from AlarmMonitor — not a cached single event.
  // This correctly handles faults that were raised before the rules were loaded,
  // as well as multiple simultaneous faults from different devices.
  const auto faults = AlarmMonitor::getInstance().getActiveHardwareFaults();
  for (const auto& f : faults) {
    if (r.hwCode >= 0 && static_cast<int>(f.code) != r.hwCode) {
      continue;
    }
    if (!r.roleSubstr.empty() && f.role.find(r.roleSubstr) == std::string::npos) {
      continue;
    }
    return true;
  }
  return false;
}

bool RelayRuleEngine::matchRectification(const Rule& r) const {
  if (r.stageEquals.empty()) {
    return false;
  }
  const std::string cur = RectificationProcess::rectController().getMetrics().type;
  return cur == r.stageEquals;
}

bool RelayRuleEngine::matchSsvc(const Rule& r) const {
  const SsvcSettings& s = SsvcSettings::init();
  if (r.ssvcKey == "formula") {
    return s.getFormula() == r.ssvcBool;
  }
  if (r.ssvcKey == "auto_mode") {
    return s.getAutoMode() == r.ssvcBool;
  }
  if (r.ssvcKey == "tank_mmhg") {
    if (r.condKind == Rule::CondKind::SSVC_FLOAT) {
      return fabsf(s.getTank_mmhg() - r.ssvcFloat) < 0.001f;
    }
    return static_cast<int>(s.getTank_mmhg() + 0.5f) == r.ssvcInt;
  }
  if (r.ssvcKey == "sound") {
    return s.getSound() == r.ssvcBool;
  }
  if (r.ssvcKey == "pressure") {
    return s.getPressure() == r.ssvcBool;
  }
  if (r.ssvcKey == "relay_inverted") {
    return s.getRelayInverted() == r.ssvcBool;
  }
  if (r.ssvcKey == "relay_autostart") {
    return s.getRelayAutostart() == r.ssvcBool;
  }
  if (r.ssvcKey == "autoresume") {
    return s.getAutoResume() == r.ssvcBool;
  }
  if (r.ssvcKey == "hearts_pause") {
    return s.getHeartsPause() == r.ssvcBool;
  }
  if (r.ssvcKey == "signal_tp1_control") {
    return s.getSignalTp1Control() == r.ssvcBool;
  }
  if (r.ssvcKey == "signal_inverted") {
    return s.getSignalInverted() == r.ssvcBool;
  }
  if (r.ssvcKey == "tp1_control_start") {
    return s.getTp1ControlStart() == r.ssvcBool;
  }
  if (r.ssvcKey == "stab_limit_finish") {
    return s.getStabLimitFinish() == r.ssvcBool;
  }
  if (r.ssvcKey == "decrement") {
    return static_cast<int>(s.getDecrement()) == r.ssvcInt;
  }
  if (r.ssvcKey == "heads_timer") {
    return static_cast<int>(s.getHeadsTimer()) == r.ssvcInt;
  }
  if (r.ssvcKey == "late_heads_timer") {
    return static_cast<int>(s.getLateHeadsTimer()) == r.ssvcInt;
  }
  if (r.ssvcKey == "hearts_timer") {
    return static_cast<int>(s.getHeartsTimer()) == r.ssvcInt;
  }
  if (r.ssvcKey == "start_delay") {
    return static_cast<int>(s.getStartDelay()) == r.ssvcInt;
  }
  if (r.ssvcKey == "stab_limit_time") {
    return s.getStabLimitTime() == r.ssvcInt;
  }
  if (r.ssvcKey == "release_timer") {
    return s.getReleaseTimeer() == r.ssvcInt;
  }
  if (r.ssvcKey == "hyst") {
    return fabsf(s.getHysteresis() - r.ssvcFloat) < 0.001f;
  }
  if (r.ssvcKey == "hearts_finish_temp") {
    return fabsf(s.getHeartsFinishTemp() - r.ssvcFloat) < 0.001f;
  }
  if (r.ssvcKey == "formula_start_temp") {
    return fabsf(s.getFormulaStartTemp() - r.ssvcFloat) < 0.001f;
  }
  if (r.ssvcKey == "tails_temp") {
    return fabsf(s.getTailsTemp() - r.ssvcFloat) < 0.001f;
  }
  if (r.ssvcKey == "release_speed") {
    return fabsf(s.getReleaseSpeed() - r.ssvcFloat) < 0.001f;
  }
  if (r.ssvcKey == "hearts_temp_shift") {
    return fabsf(s.getHeartsTempShift() - r.ssvcFloat) < 0.001f;
  }
  if (r.ssvcKey == "tp2_shift") {
    return fabsf(s.getTp2Shift() - r.ssvcFloat) < 0.001f;
  }
  if (r.ssvcKey == "tp1_control_temp") {
    return fabsf(s.getTp1ControlTemp() - r.ssvcFloat) < 0.001f;
  }
  return false;
}

bool RelayRuleEngine::conditionMatches(const Rule& r) const {
  switch (r.condKind) {
    case Rule::CondKind::SENSOR_ALARM:
      return matchSensorAlarm(r);
    case Rule::CondKind::HARDWARE_FAULT:
      return matchHardwareFault(r);
    case Rule::CondKind::RECTIFICATION:
      return matchRectification(r);
    case Rule::CondKind::SSVC_BOOL:
    case Rule::CondKind::SSVC_INT:
    case Rule::CondKind::SSVC_FLOAT:
      return matchSsvc(r);
  }
  return false;
}

void RelayRuleEngine::recomputeAndApply() {
#if !PINOUT_USE_GPIO
  EngineLock lock(_lock);
  auto& coord = RelayPortCoordinator::getInstance();
  const unsigned n = coord.totalRelayLines();
  for (unsigned g = 0; g < n; ++g) {
    if (coord.isAlarmReservedGlobal(g)) {
      continue;
    }
    const uint64_t b = 1ULL << g;
    if (_manualMask & b) {
      const bool en = (_manualEnergized & b) != 0;
      coord.setUserLine(g, en);
      continue;
    }
    int bestPri = INT_MIN;
    bool bestEnergize = false;
    bool found = false;
    for (const Rule& r : _rules) {
      if (!r.enabled || r.targetBit != static_cast<int>(g)) {
        continue;
      }
      if (!conditionMatches(r)) {
        continue;
      }
      if (!found || r.priority > bestPri) {
        bestPri = r.priority;
        bestEnergize = r.actionEnergize;
        found = true;
      }
    }
    coord.setUserLine(g, found ? bestEnergize : false);
  }
  coord.flush();
#endif
}

void RelayRuleEngine::onAlarm(const AlarmEvent& event) {
  if (event.source_kind == AlarmSourceKind::SENSOR && event.sensor != nullptr) {
    EngineLock lock(_lock);
    const std::string sensorKey = SensorManager::addressToString(event.sensor->getAddress());
    if (event.level == AlarmLevel::NORMAL) {
      _sensorAlarmStates.erase(sensorKey);
    } else {
      SensorAlarmState& state = _sensorAlarmStates[sensorKey];
      state.level = event.level;
      state.name = event.sensor->getName();
    }
  }
  // Hardware faults are resolved by querying AlarmMonitor live state in matchHardwareFault,
  // so we don't cache hw events here — just trigger re-evaluation.
  recomputeAndApply();
}

void RelayRuleEngine::forceResetAlarm() {
  EngineLock lock(_lock);
  _sensorAlarmStates.clear();
  recomputeAndApply();
}
