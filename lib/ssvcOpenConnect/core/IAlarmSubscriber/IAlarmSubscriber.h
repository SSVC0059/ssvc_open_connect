#ifndef SSVC_OPEN_CONNECT_IALARMSUBSCRIBER_H
#define SSVC_OPEN_CONNECT_IALARMSUBSCRIBER_H

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

#include "components/sensors/AbstractSensor/AbstractSensor.h"
#include "core/StatefulServices/AlarmThresholdService/AlarmThresholdService.h" // Для AlarmLevel

enum class AlarmSourceKind : uint8_t {
  SENSOR = 0,
  HARDWARE_FAULT = 1,
};

enum class HardwareFaultCode : uint8_t {
  NONE = 0,
  I2C_NACK = 1,
  I2C_BUS_DOWN = 2,
  DEVICE_NOT_PRESENT = 3,
  UART_LINK_LOST = 4,
};

struct AlarmEvent {
  AlarmSourceKind source_kind = AlarmSourceKind::SENSOR;
  const AbstractSensor* sensor = nullptr;
  float current_value = 0.f;
  float threshold_value = 0.f;
  AlarmLevel level = AlarmLevel::NORMAL;
  time_t timestamp = 0;
  /** Valid when source_kind == HARDWARE_FAULT */
  HardwareFaultCode hw_code = HardwareFaultCode::NONE;
  uint8_t hw_i2c_address = 0;
  const char* hw_device_role = nullptr;
};

class IAlarmSubscriber {
public:
  virtual ~IAlarmSubscriber() = default;
  virtual void onAlarm(const AlarmEvent& event) = 0;

  virtual void forceResetAlarm() {}
};

#endif //SSVC_OPEN_CONNECT_IALARMSUBSCRIBER_H
