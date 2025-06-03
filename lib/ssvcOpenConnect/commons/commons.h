//
// Created by demoncat on 19.04.25.
//

#ifndef COMMONS_H
#define COMMONS_H


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
