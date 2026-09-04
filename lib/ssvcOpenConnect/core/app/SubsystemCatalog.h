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

#ifndef SSVC_OPEN_CONNECT_APP_SUBSYSTEMCATALOG_H
#define SSVC_OPEN_CONNECT_APP_SUBSYSTEMCATALOG_H

class ServiceRegistry;

/**
 * Registers and starts the hardware/messenger subsystems in SubsystemManager.
 * Extracted from the old SsvcOpenConnect::subsystemManager(): one concern —
 * "which subsystems exist and in which order they boot".
 */
class SubsystemCatalog
{
public:
  void start(ServiceRegistry& services);
};

#endif // SSVC_OPEN_CONNECT_APP_SUBSYSTEMCATALOG_H
