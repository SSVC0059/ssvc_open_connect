#ifndef SSVC_OPEN_CONNECT_RELAY_RULE_ENGINE_H
#define SSVC_OPEN_CONNECT_RELAY_RULE_ENGINE_H

#include "core/IAlarmSubscriber/IAlarmSubscriber.h"
#include "core/SsvcConnector.h"
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/timers.h>
#include <cstdint>
#include <map>
#include <vector>
#include <string>

/**
 * Evaluates profile-driven rules for PCF8574 lines not reserved for AlarmManager (bits 2–7 by default).
 */
class RelayRuleEngine final : public IAlarmSubscriber {
public:
  enum class SsvcFieldValueType : uint8_t {
    BOOL = 0,
    INT = 1,
    FLOAT = 2
  };

  struct SsvcFieldDescriptor {
    const char* key;
    const char* label;
    SsvcFieldValueType valueType;
  };

  static RelayRuleEngine& getInstance();
  static const std::vector<SsvcFieldDescriptor>& getSsvcFieldDescriptors();

  void begin();

  void loadRules(const JsonObject& src);
  void serializeToJson(JsonObject& dest) const;

  /** Manual override for one user bit; enableOverride=false clears manual for that bit. */
  void setManualOverride(unsigned bit, bool energized, bool enableOverride);
  void clearAllManualOverrides();

#if !PINOUT_USE_GPIO
  void appendCapabilitiesJson(JsonObject& root) const;
  void appendSsvcFieldsJson(JsonArray& arr) const;
#endif

  void onAlarm(const AlarmEvent& event) override;
  void forceResetAlarm() override;

private:
  RelayRuleEngine();

  void recomputeAndApply();
  static void timerCallback(TimerHandle_t t);

  struct Rule {
    bool enabled = true;
    int targetBit = 2;
    bool actionEnergize = true;
    int priority = 0;
    enum class CondKind {
      SENSOR_ALARM,
      HARDWARE_FAULT,
      RECTIFICATION,
      SSVC_BOOL,
      SSVC_INT,
      SSVC_FLOAT
    } condKind = CondKind::SENSOR_ALARM;
    // sensor_alarm
    bool anySensorLevel = true;
    bool wantMin = false, wantDanger = false, wantCrit = false;
    std::string sensorNameSubstr;
    // hardware_fault
    int hwCode = -1;
    std::string roleSubstr;
    // rectification
    std::string stageEquals;
    // ssvc
    std::string ssvcKey;
    bool ssvcBool = false;
    int ssvcInt = 0;
    float ssvcFloat = 0.f;
  };

  struct SensorAlarmState {
    AlarmLevel level = AlarmLevel::NORMAL;
    std::string name;
  };

  bool conditionMatches(const Rule& r) const;
  bool parseSensorAlarmCondition(const JsonObject& cond, Rule& r) const;
  bool parseHardwareFaultCondition(const JsonObject& cond, Rule& r) const;
  bool parseRectificationCondition(const JsonObject& cond, Rule& r) const;
  bool parseSsvcSettingCondition(const JsonObject& cond, Rule& r) const;
  bool parseCondition(const JsonObject& cond, Rule& r) const;
  bool matchSensorAlarm(const Rule& r) const;
  bool matchHardwareFault(const Rule& r) const;
  bool matchRectification(const Rule& r) const;
  bool matchSsvc(const Rule& r) const;

  std::vector<Rule> _rules;
  int _schemaVersion = 1;

  std::map<std::string, SensorAlarmState> _sensorAlarmStates;

  uint64_t _manualMask = 0;
  uint64_t _manualEnergized = 0;
  mutable SemaphoreHandle_t _lock = nullptr;

#if !PINOUT_USE_GPIO
  TimerHandle_t _timer = nullptr;
  static constexpr const char* TIMER_NAME = "relayRules";
#endif

  static constexpr auto TAG = "RelayRuleEngine";
};

#endif
