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

#include "core/app/SubsystemCatalog.h"

#include <Arduino.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "core/SubsystemManager/SubsystemManager.h"
#include "core/app/ServiceRegistry.h"

#include "core/StatefulServices/OpenConnectHardwareSettingsService/OpenConnectHardwareSettingsService.h"
#include "core/StatefulServices/OpenConnectHardwareSettingsService/OpenConnectHardwareConfig.h"

#include "components/subsystem/SettingsSubsystem.h"
#include "components/subsystem/ThermalSubsystem.h"
#include "components/subsystem/I2CBusSubsystem.h"
#include "components/subsystem/AtmosphericSubsystem.h"
#include "components/subsystem/Lcd1602Subsystem.h"

#ifndef FT_TELEGRAM_BOT
#define FT_TELEGRAM_BOT 0
#endif
#if FT_TELEGRAM_BOT
#include "components/subsystem/TelegramBotSubsystem.h"
#endif
#ifndef FT_VK_BOT
#define FT_VK_BOT 0
#endif
#if FT_VK_BOT
#include "components/subsystem/VkBotSubsystem.h"
#endif

#include "components/rtc/Ds3231RtcCoordinator.h"
#include "core/SsvcCommandsQueue.h"

#define TAG "SubsystemCatalog"

void SubsystemCatalog::start(ServiceRegistry& services)
{
  ESP_LOGI(TAG, "[SUBSYSTEM_MANAGER] Initializing subsystem manager");

  auto& subsystemManager = SubsystemManager::instance();
  ESP_LOGD(TAG, "[SUBSYSTEM_MANAGER] SubsystemManager instance obtained");

  subsystemManager.registerSubsystem<SettingsSubsystem>();
  subsystemManager.registerSubsystem<ThermalSubsystem>();
  subsystemManager.registerSingleton<I2CBusSubsystem>();

  bool pressureHardwareOn = true;
  bool lcd1602HardwareOn = false;
  OpenConnectHardwareSettingsService::instance().read([&](OpenConnectHardwareConfig& cfg) {
    pressureHardwareOn = cfg.pressureSensorEnabled;
    lcd1602HardwareOn = cfg.lcd1602Enabled;
  });

  if (pressureHardwareOn) {
    subsystemManager.registerSubsystem<AtmosphericSubsystem>();
  }
  if (lcd1602HardwareOn) {
    subsystemManager.registerSubsystem<Lcd1602Subsystem>();
  }

#if FT_TELEGRAM_BOT
  subsystemManager.registerSubsystem<TelegramBotSubsystem>();
#endif
#if FT_VK_BOT
  subsystemManager.registerSubsystem<VkBotSubsystem>();
#endif
  ESP_LOGD(TAG, "[SUBSYSTEM_MANAGER] Subsystems registered");

  subsystemManager.setInitialState("settings", true);
  subsystemManager.setInitialState("thermal", true);
  subsystemManager.setInitialState("i2c_bus", true);
  if (pressureHardwareOn) {
    subsystemManager.setInitialState("atm_sensor", true);
  }
  if (lcd1602HardwareOn) {
    subsystemManager.setInitialState("lcd1602_display", true);
  }

  // Messengers (telegram_bot, vk_bot): state only from NVS via UI toggle — no setInitialState.

  ESP_LOGD(TAG, "[SUBSYSTEM_MANAGER] Starting subsystem manager...");
  subsystemManager.begin();
  ESP_LOGI(TAG, "[SUBSYSTEM_MANAGER] Initialization complete");

  Ds3231RtcCoordinator::instance().bootstrapAfterI2c();

  services.ensurePinOutSubscriber();

  bool ipShow = false;
  while (!ipShow) {
    if (WiFi.isConnected()) {
      ipShow = true;
      SsvcCommandsQueue::getQueue().status(WiFi.localIP().toString().c_str());
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
