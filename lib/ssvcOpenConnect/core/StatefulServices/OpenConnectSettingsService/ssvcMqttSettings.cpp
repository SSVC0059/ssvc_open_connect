#include "ssvcMqttSettings.h"

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

SsvcMqttSettingsService* SsvcMqttSettingsService::_instance = nullptr;

void ssvcMqttSettings::read(const ssvcMqttSettings& settings, const JsonObject& root)
{
  const auto  _response = root["ssvcSettings"].to<JsonVariant>();
  SsvcSettings::init().fillSettings(_response);
}


SsvcMqttSettingsService::SsvcMqttSettingsService(
  PsychicHttpServer *server, ESP32SvelteKit *_esp32sveltekit)
    :_mqttEndpoint(
          ssvcMqttSettings::read,
          ssvcMqttSettings::update,
          this,
          _esp32sveltekit->getMqttClient(),
          OPEN_CONNECT_SETTINGS_PUB_TOPIC,
          ""
    )
{
  _instance = this;
}


