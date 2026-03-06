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

#define MIN(a, b) ({ __typeof__ (a) _a = (a); __typeof__ (b) _b = (b); _a < _b ? _a : _b; })

CoreDump::CoreDump(PsychicHttpServer *server,
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

esp_err_t CoreDump::coreDump(PsychicRequest* request) {
    httpd_resp_set_status(request->request(), "200 OK");
    PsychicResponse response(request);
    response.setCode(200);
    response.setContentType("application/octet-stream");
    response.addHeader("Content-Disposition", "attachment;filename=core.bin");
    response.sendHeaders();

    esp_partition_iterator_t partition_iterator = esp_partition_find(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_COREDUMP, "coredump");

    const esp_partition_t* partition = esp_partition_get(partition_iterator);

    int file_size = 65536;
    int chunk_size = 1024;
    int i = 0;
    for (i = 0; i < (file_size / chunk_size); i++) {
        uint8_t store_data[chunk_size];
        ESP_ERROR_CHECK(esp_partition_read(partition, i * chunk_size, store_data, chunk_size));
        response.sendChunk(store_data, chunk_size);
    }
    uint16_t pending_size = file_size - (i * chunk_size);
    uint8_t pending_data[pending_size];
    if (pending_size > 0) {
        ESP_ERROR_CHECK(esp_partition_read(partition, i * chunk_size, pending_data, pending_size));
        response.sendChunk(pending_data, pending_size);
    }
    return response.finishChunking();
}