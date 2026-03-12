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

esp_err_t CoreDump::coreDump(PsychicRequest *request)
{
    size_t coredump_addr;
    size_t coredump_size;

    // 1. Пытаемся найти валидный дамп через официальное API
    esp_err_t err = esp_core_dump_image_get(&coredump_addr, &coredump_size);

    if (err != ESP_OK || coredump_size == 0) {
        ESP_LOGW(SVK_TAG, "Core dump not found or empty");
        request->reply(404, "application/json", "{\"status\":\"error\",\"message\":\"no core dump found\"}");
        return err;
    }

    ESP_LOGI(SVK_TAG, "Found coredump: size %u bytes at addr 0x%x", coredump_size, coredump_addr);

    // 2. Настраиваем HTTP ответ
    PsychicResponse response(request);
    response.setCode(200);
    response.setContentType("application/octet-stream");
    // Чтобы браузер сразу предлагал сохранить файл
    response.addHeader("Content-Disposition", "attachment; filename=core.bin");
    response.sendHeaders();

    // 3. Читаем и отправляем чанками
    const size_t chunk_size = 512; // Оптимально: не перегружает стек и быстро передает
    uint8_t *buffer = (uint8_t *)malloc(chunk_size);
    if (!buffer) return ESP_ERR_NO_MEM;

    size_t offset = 0;
    for (; offset < coredump_size; offset += chunk_size) {
        size_t read_len = (coredump_size - offset < chunk_size) ? (coredump_size - offset) : chunk_size;

        // Читаем напрямую из flash, зная адрес и смещение
        err = esp_flash_read(esp_flash_default_chip, buffer, coredump_addr + offset, read_len);
        if (err != ESP_OK) {
            ESP_LOGE(SVK_TAG, "Flash read failed at offset %u", offset);
            break;
        }

        if (response.sendChunk(buffer, read_len) != ESP_OK) {
            ESP_LOGE(SVK_TAG, "Chunk send failed");
            break;
        }
    }
    bool transfer_complete = (offset >= coredump_size);

    free(buffer);
    response.finishChunking();

    // 4. Очистка (Erase) — только при успешной передаче, чтобы сохранить дамп для повторной попытки
    if (transfer_complete) {
        uint32_t sectors_to_erase = (coredump_size + SPI_FLASH_SEC_SIZE - 1) / SPI_FLASH_SEC_SIZE;
        ESP_LOGI(SVK_TAG, "Erasing %u sectors of coredump", sectors_to_erase);

        err = esp_flash_erase_region(esp_flash_default_chip, coredump_addr, sectors_to_erase * SPI_FLASH_SEC_SIZE);
        if (err != ESP_OK) {
            ESP_LOGE(SVK_TAG, "Erase failed: %d", err);
        }
    } else {
        ESP_LOGW(SVK_TAG, "Transfer incomplete, coredump preserved for retry");
    }

    return err;
}