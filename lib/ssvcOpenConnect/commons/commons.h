#ifndef COMMONS_H
#define COMMONS_H

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

class StringView {
public:
    StringView(const char* data, size_t size) : data_(data), size_(size) {}
    StringView(const std::string& s) : data_(s.data()), size_(s.size()) {}

    const char* data() const { return data_; }
    size_t size() const { return size_; }

private:
    const char* data_;
    size_t size_;
};

// RAII-обертка для мьютекса
class MutexLocker {
    SemaphoreHandle_t mutex;
public:
    explicit MutexLocker(SemaphoreHandle_t m) : mutex(m) {
        if (mutex) xSemaphoreTake(mutex, portMAX_DELAY);
    }
    ~MutexLocker() {
        if (mutex) xSemaphoreGive(mutex);
    }
};

#if __cplusplus < 201402L
#include <memory>

namespace std
{
    /**
     * @brief Реализация make_unique для C++11
     */
    template <typename T, typename... Args>
    unique_ptr<T> make_unique(Args&&... args)
    {
        return unique_ptr<T>(new T(std::forward<Args>(args)...));
    }
} // namespace std
#endif

#endif //COMMONS_H
