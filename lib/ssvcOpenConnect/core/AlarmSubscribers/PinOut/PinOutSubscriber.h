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

#ifndef SSVC_OPEN_CONNECT_PINOUTSUBSCRIBER_H
#define SSVC_OPEN_CONNECT_PINOUTSUBSCRIBER_H

#include <Arduino.h>
#include "core/IAlarmSubscriber/IAlarmSubscriber.h"
#include "core/SsvcConnector.h"
#include "core/AlarmMonitor/AlarmMonitor.h"

#if !PINOUT_USE_GPIO
#include "core/RelayPortCoordinator/RelayPortCoordinator.h"
#endif

/**
 * Alarm level → outputs (two mutually exclusive builds via PINOUT_USE_GPIO):
 * - 0: PCF8574 — outputs merged via `RelayPortCoordinator` (alarm bits + user rules).
 * - 1: GPIO only.
 */
class PinOutSubscriber final : public IAlarmSubscriber {
public:
  PinOutSubscriber();
  ~PinOutSubscriber() override;
  void onAlarm(const AlarmEvent& event) override;
  void forceResetAlarm() override;

private:
  static constexpr auto TAG = "PinOutSubscriber";
};

#endif // SSVC_OPEN_CONNECT_PINOUTSUBSCRIBER_H
