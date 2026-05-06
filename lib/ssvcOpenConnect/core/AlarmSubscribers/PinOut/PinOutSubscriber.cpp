/**
*   SSVC Open Connect
 *
 *   A firmware for ESP32 to interface with SSVC 0059 distillation controller
 *   via UART protocol. Features a responsive Sveltekit web interface for
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

#include "PinOutSubscriber.h"

#if !PINOUT_USE_GPIO
#include "core/RelayPortCoordinator/RelayPortCoordinator.h"
#endif

PinOutSubscriber::PinOutSubscriber() {
  AlarmMonitor::getInstance().subscribe(this);
  ESP_LOGI(TAG, "subscribed to AlarmMonitor");

#if !PINOUT_USE_GPIO
  RelayPortCoordinator::getInstance().resetAlarmLines();
  RelayPortCoordinator::getInstance().flush();
  ESP_LOGI(TAG,
           "alarm outputs via PCF8574 @ 0x%02X role=%s bits D=%u C=%u (active-LOW), lines/chip=%u",
           static_cast<unsigned>(SSVC_RELAY_PCF8574_I2C_ADDR),
           SSVC_RELAY_PCF8574_DEVICE_ROLE,
           static_cast<unsigned>(SSVC_RELAY_PCF8574_BIT_DANGEROUS),
           static_cast<unsigned>(SSVC_RELAY_PCF8574_BIT_CRITICAL),
           static_cast<unsigned>(SSVC_RELAY_PCF8574_LINES_PER_CHIP));
#else
  pinMode(SSVC_PINOUT_ALARM_GPIO_DANGEROUS, OUTPUT);
  pinMode(SSVC_PINOUT_ALARM_GPIO_CRITICAL, OUTPUT);
  digitalWrite(SSVC_PINOUT_ALARM_GPIO_DANGEROUS, HIGH);
  digitalWrite(SSVC_PINOUT_ALARM_GPIO_CRITICAL, HIGH);
  ESP_LOGI(TAG, "alarm GPIO DANGEROUS=%d CRITICAL=%d (no PCF8574 in this build)",
           static_cast<int>(SSVC_PINOUT_ALARM_GPIO_DANGEROUS),
           static_cast<int>(SSVC_PINOUT_ALARM_GPIO_CRITICAL));
#endif
}

PinOutSubscriber::~PinOutSubscriber() {
  AlarmMonitor::getInstance().unsubscribe(this);
}

void PinOutSubscriber::forceResetAlarm() {
#if !PINOUT_USE_GPIO
  RelayPortCoordinator::getInstance().resetAlarmLines();
  RelayPortCoordinator::getInstance().flush();
#else
  digitalWrite(SSVC_PINOUT_ALARM_GPIO_DANGEROUS, HIGH);
  digitalWrite(SSVC_PINOUT_ALARM_GPIO_CRITICAL, HIGH);
#endif
}

void PinOutSubscriber::onAlarm(const AlarmEvent& event) {
  if (event.source_kind == AlarmSourceKind::HARDWARE_FAULT) {
    return;
  }

#if !PINOUT_USE_GPIO
  RelayPortCoordinator::getInstance().setAlarmLevel(event.level);
  RelayPortCoordinator::getInstance().flush();
#else
  digitalWrite(SSVC_PINOUT_ALARM_GPIO_DANGEROUS, HIGH);
  digitalWrite(SSVC_PINOUT_ALARM_GPIO_CRITICAL, HIGH);

  if (event.level == AlarmLevel::DANGEROUS) {
    digitalWrite(SSVC_PINOUT_ALARM_GPIO_DANGEROUS, LOW);
  } else if (event.level == AlarmLevel::CRITICAL) {
    digitalWrite(SSVC_PINOUT_ALARM_GPIO_CRITICAL, LOW);
  }
#endif
}
