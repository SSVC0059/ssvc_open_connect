/**
 *   ESP32 SvelteKit
 *
 *   A simple, secure and extensible framework for IoT projects for ESP32 platforms
 *   with responsive Sveltekit front-end built with TailwindCSS and DaisyUI.
 *   https://github.com/theelims/ESP32-sveltekit
 *
 *   Copyright (C) 2018 - 2023 rjwats
 *   Copyright (C) 2023 - 2025 theelims
 *
 *   All Rights Reserved. This software may be modified and distributed under
 *   the terms of the LGPL v3 license. See the LICENSE file for details.
 **/

#include <CoreDump.h>
#include <esp32-hal.h>

#include "esp_core_dump.h"
#include "esp_partition.h"
#include "esp_flash.h"
#include "esp_err.h"

CoreDump::CoreDump(AsyncWebServer *server,
                   SecurityManager *securityManager) : _server(server),
                                                       _securityManager(securityManager)
{
}

void CoreDump::begin()
{
    _server->on(CORE_DUMP_SERVICE_PATH,
                HTTP_GET,
                _securityManager->wrapRequest(std::bind(&CoreDump::coreDump, this, std::placeholders::_1),
                                              AuthenticationPredicates::IS_AUTHENTICATED));

    ESP_LOGV("CoreDump", "Registered GET endpoint: %s", CORE_DUMP_SERVICE_PATH);
}

void CoreDump::coreDump(AsyncWebServerRequest *request)
{
    size_t coredump_addr;
    size_t coredump_size;
    esp_err_t err = esp_core_dump_image_get(&coredump_addr, &coredump_size);
    if (err != ESP_OK)
    {
        request->send(500, "application/json", "{\"status\":\"error\",\"message\":\"core dump not available\"}");
        return;
    }
    ESP_LOGI(SVK_TAG, "Coredump is %u bytes", coredump_size);

    uint8_t *buffer = (uint8_t *)malloc(coredump_size);
    if (!buffer)
    {
        request->send(500, "text/plain", "coredump alloc failed");
        return;
    }
    err = esp_flash_read(esp_flash_default_chip, buffer, coredump_addr, coredump_size);
    if (err != ESP_OK)
    {
        ESP_LOGE(SVK_TAG, "Coredump read failed");
        free(buffer);
        request->send(500, "text/plain", "coredump read failed");
        return;
    }
    AsyncWebServerResponse *response = request->beginResponse(200, "application/octet-stream", buffer, coredump_size);
    response->addHeader("Content-Disposition", "attachment; filename=coredump.bin");
    response->addHeader("Cache-Control", "no-store");

    request->onDisconnect([buffer, coredump_size]() {
        // ESPAsyncWebServer has no responseCode(); erase after the client closes a 200 stream we started.
        const esp_err_t eraseErr = esp_core_dump_image_erase();
        if (eraseErr == ESP_OK) {
            ESP_LOGI(SVK_TAG, "Coredump erased from flash after download (%u bytes)", coredump_size);
        } else if (eraseErr != ESP_ERR_NOT_FOUND) {
            ESP_LOGW(SVK_TAG, "Coredump erase failed: %s", esp_err_to_name(eraseErr));
        }
        free(buffer);
    });

    request->send(response);
}