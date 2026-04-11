/**
 * Общий аллокатор ArduinoJson для размещения JsonDocument в PSRAM (SPIRAM).
 * Снижает использование внутренней RAM при парсинге/сборке крупных JSON.
 *
 * Использование:
 *   JsonSpiRamAllocator alloc;
 *   JsonDocument doc(&alloc);
 *   deserializeJson(doc, input);
 *
 * При отсутствии PSRAM аллокатор выделяет память из внутренней кучи (fallback).
 */

#ifndef JSON_SPIRAM_ALLOCATOR_H
#define JSON_SPIRAM_ALLOCATOR_H

#include <ArduinoJson.h>
#include <esp_heap_caps.h>
#include <esp_log.h>

#ifndef JSON_SPIRAM_ALLOCATOR_LOG_TAG
#define JSON_SPIRAM_ALLOCATOR_LOG_TAG "JsonPsram"
#endif

class JsonSpiRamAllocator : public ArduinoJson::Allocator {
public:
    /** Вызвать перед созданием каждого нового JsonDocument */
    void reset() { _loggedForDoc = false; }

    /** Тег для логов (модуль), опционально */
    void setTag(const char* tag) { _tag = tag ? tag : JSON_SPIRAM_ALLOCATOR_LOG_TAG; }

    void* allocate(size_t size) override {
        _usedInternal = false;
        void* p = nullptr;
#if CONFIG_SPIRAM_USE_MALLOC || CONFIG_SPIRAM
        p = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
        if (p != nullptr) {
            _logFirstAlloc(size, true);
            return p;
        }
#endif
        _usedInternal = true;
        p = heap_caps_malloc(size, MALLOC_CAP_INTERNAL);
        _logFirstAlloc(size, false);
        return p;
    }

    void deallocate(void* pointer) override {
        if (pointer != nullptr) heap_caps_free(pointer);
    }

    void* reallocate(void* ptr, size_t new_size) override {
#if CONFIG_SPIRAM_USE_MALLOC || CONFIG_SPIRAM
        if (!_usedInternal)
            return heap_caps_realloc(ptr, new_size, MALLOC_CAP_SPIRAM);
#endif
        return heap_caps_realloc(ptr, new_size, MALLOC_CAP_INTERNAL);
    }

    bool usedInternalRam() const { return _usedInternal; }

private:
    void _logFirstAlloc(size_t size, bool fromPsram) {
        if (_loggedForDoc) return;
        _loggedForDoc = true;
        const size_t freeInt = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        const size_t freePsram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        const char* tag = _tag ? _tag : JSON_SPIRAM_ALLOCATOR_LOG_TAG;
        ESP_LOGV(tag,
            "JSON doc: first alloc from %s (%u B) | free internal=%u PSRAM=%u",
            fromPsram ? "PSRAM" : "internal",
            (unsigned)size, (unsigned)freeInt, (unsigned)freePsram);
    }

    mutable bool _usedInternal = false;
    mutable bool _loggedForDoc = false;
    const char* _tag = nullptr;
};

#endif // JSON_SPIRAM_ALLOCATOR_H
