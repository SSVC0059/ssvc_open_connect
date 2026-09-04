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

#ifndef SSVC_OPEN_CONNECT_APP_APPCONTEXT_H
#define SSVC_OPEN_CONNECT_APP_APPCONTEXT_H

// Forward declarations only — this header must stay light (no framework includes).
class AsyncWebServer;
class ESP32SvelteKit;
class SecurityManager;

/**
 * Bundle of platform dependencies injected into the composition root from main().
 *
 * Plain struct with an explicit constructor so it stays usable under older
 * C++ standards (no default-member-initializer aggregates).
 */
struct AppContext
{
  AppContext() = default;
  AppContext(AsyncWebServer* ser, ESP32SvelteKit* kit, SecurityManager* sec)
      : server(ser), sveltekit(kit), securityManager(sec)
  {
  }

  AsyncWebServer* server = nullptr;
  ESP32SvelteKit* sveltekit = nullptr;
  SecurityManager* securityManager = nullptr;
};

#endif // SSVC_OPEN_CONNECT_APP_APPCONTEXT_H
