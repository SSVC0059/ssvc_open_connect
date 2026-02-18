/**
 * Логер расхода памяти при работе с JsonDocument (PSRAM vs internal).
 * Подключается в местах парсинга/сборки JSON для оценки эффективности переноса в PSRAM.
 *
 * Использование:
 *   JsonMemoryLogger logger("SsvcConnector", 20);  // тег, логировать раз в 20 вызовов (0 = каждый раз)
 *   logger.recordBefore();
 *   JsonDocument doc(&alloc);
 *   deserializeJson(doc, input);
 *   logger.logAfterParse(doc.overflowed(), alloc.usedInternalRam());
 *   // ... использование doc ...
 *   logger.logAfterDocDestroyed();
 */

#ifndef JSON_MEMORY_LOGGER_H
#define JSON_MEMORY_LOGGER_H

#include <esp_heap_caps.h>
#include <esp_log.h>
#include <cstddef>

class JsonMemoryLogger {
public:
    /**
     * @param tag       тег для логов (имя модуля)
     * @param interval  логировать раз в N вызовов (0 = при каждом вызове)
     */
    explicit JsonMemoryLogger(const char* tag, unsigned interval = 0)
        : _tag(tag ? tag : "JsonMem"), _interval(interval), _callCount(0),
          _freeIntBefore(0), _freePsramBefore(0) {}

    /** Записать состояние кучи до создания/парсинга документа. Вызывать до создания JsonDocument. */
    void recordBefore() {
        _freeIntBefore = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        _freePsramBefore = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        _callCount++;
    }

    /** Нужно ли писать лог в этот вызов (по интервалу). */
    bool shouldLog() const {
        return _interval == 0 || (_callCount % _interval == 0);
    }

    /**
     * Лог сразу после парсинга/сборки (документ ещё жив).
     * @param overflowed       doc.overflowed()
     * @param usedInternalRam  allocator.usedInternalRam()
     * @param serializedSize   размер сериализованного JSON (0 если не применимо)
     */
    void logAfterParse(bool overflowed, bool usedInternalRam, size_t serializedSize = 0) const {
        if (!shouldLog()) return;
        const size_t freeInt = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        const size_t freePsram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        const int deltaInt = static_cast<int>(freeInt - _freeIntBefore);
        const int deltaPsram = static_cast<int>(freePsram - _freePsramBefore);
        if (serializedSize != 0) {
            ESP_LOGV(_tag,
                "JSON: overflowed=%d alloc=%s size=%zu B | free internal %u->%u (delta=%d) PSRAM %u->%u (delta=%d)",
                overflowed ? 1 : 0, usedInternalRam ? "internal" : "PSRAM", serializedSize,
                (unsigned)_freeIntBefore, (unsigned)freeInt, deltaInt,
                (unsigned)_freePsramBefore, (unsigned)freePsram, deltaPsram);
        } else {
            ESP_LOGV(_tag,
                "JSON: overflowed=%d alloc=%s | free internal %u->%u (delta=%d) PSRAM %u->%u (delta=%d)",
                overflowed ? 1 : 0, usedInternalRam ? "internal" : "PSRAM",
                (unsigned)_freeIntBefore, (unsigned)freeInt, deltaInt,
                (unsigned)_freePsramBefore, (unsigned)freePsram, deltaPsram);
        }
    }

    /** Лог после уничтожения документа (освобождение памяти). */
    void logAfterDocDestroyed() const {
        if (!shouldLog()) return;
        const size_t freeInt = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        const size_t freePsram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        const int internalSaved = static_cast<int>(freeInt - _freeIntBefore);
        ESP_LOGV(_tag,
            "JSON after doc destroyed: free internal=%u PSRAM=%u (internal saved=%d vs before parse)",
            (unsigned)freeInt, (unsigned)freePsram, internalSaved);
    }

private:
    const char* _tag;
    unsigned _interval;
    unsigned _callCount;
    size_t _freeIntBefore;
    size_t _freePsramBefore;
};

#endif // JSON_MEMORY_LOGGER_H
