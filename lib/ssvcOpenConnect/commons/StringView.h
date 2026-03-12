/**
 * StringView - lightweight non-owning view over string data.
 * Extracted for unit testing on native platform.
 */
#ifndef SSVC_OPEN_CONNECT_STRINGVIEW_H
#define SSVC_OPEN_CONNECT_STRINGVIEW_H

#include <cstddef>
#include <string>

class StringView {
public:
    StringView(const char* data, size_t size) : data_(data), size_(size) {}
    StringView(const std::string& s) : data_(s.data()), size_(s.size()) {}

    const char* data() const { return data_; }
    size_t size() const { return size_; }

    bool empty() const { return size_ == 0; }

private:
    const char* data_;
    size_t size_;
};

#endif // SSVC_OPEN_CONNECT_STRINGVIEW_H
