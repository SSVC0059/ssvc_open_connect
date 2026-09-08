#ifndef SSVC_OPEN_CONNECT_SSVC_LOG_PROTOCOL_H
#define SSVC_OPEN_CONNECT_SSVC_LOG_PROTOCOL_H

#include <cstddef>
#include <cctype>
#include <cstring>
#include <cstdlib>
#include <limits>
#include <string>
#include <vector>

namespace SsvcLogProtocol {

constexpr std::size_t GET_LOG_CMD_MAX_LEN = 30;
constexpr std::size_t FILE_CHUNK_RAW_MAX = 600;

enum class Status {
  IDLE,
  LIST_RECEIVED,
  RECEIVING,
  COMPLETED,
  ERROR
};

class Transfer {
public:
  bool beginList();
  bool beginFile(int processNumber);
  bool consume(const char* message);

  Status status() const;
  const std::vector<std::string>& files() const;
  const std::string& fileName() const;
  const std::string& data() const;
  const std::string& error() const;
  int totalChunks() const;
  int receivedChunks() const;

private:
  void reset();

  Status _status{Status::IDLE};
  std::vector<std::string> _files;
  std::string _fileName;
  std::string _data;
  std::string _error;
  int _totalChunks{0};
  int _receivedChunks{0};
};

Transfer& getTransfer();

bool formatListRequest(std::string& request);
bool formatFileRequest(int processNumber, std::string& request);

namespace detail {

inline const char* base64Alphabet() {
  return "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
}

inline int base64Value(const char c) {
  const char* value = std::strchr(base64Alphabet(), c);
  return value == nullptr ? -1 : static_cast<int>(value - base64Alphabet());
}

inline bool decodeBase64(const char* encoded, std::string& decoded) {
  if (encoded == nullptr) {
    return false;
  }

  decoded.clear();
  int accumulator = 0;
  int bits = -8;
  std::size_t padding = 0;
  std::size_t length = 0;
  for (const char* cursor = encoded; *cursor != '\0'; ++cursor) {
    ++length;
    if (*cursor == '=') {
      ++padding;
      continue;
    }
    if (padding != 0) {
      return false;
    }
    const int value = base64Value(*cursor);
    if (value < 0) {
      return false;
    }
    accumulator = (accumulator << 6) | value;
    bits += 6;
    if (bits >= 0) {
      decoded.push_back(static_cast<char>((accumulator >> bits) & 0xff));
      bits -= 8;
    }
  }
  if (padding > 2 || length % 4 == 1 || (padding != 0 && length % 4 != 0)) {
    return false;
  }
  return padding == 0 || !decoded.empty();
}

inline bool validRequestLength(const std::string& request) {
  return request.size() + 1 <= GET_LOG_CMD_MAX_LEN;
}

inline bool extractString(const std::string& json, const char* key, std::string& value) {
  const std::string marker = "\"" + std::string(key) + "\"";
  const std::size_t keyPosition = json.find(marker);
  if (keyPosition == std::string::npos) {
    return false;
  }
  const std::size_t colon = json.find(':', keyPosition + marker.size());
  if (colon == std::string::npos) {
    return false;
  }
  const std::size_t openingQuote = json.find('"', colon + 1);
  if (openingQuote == std::string::npos) {
    return false;
  }
  const std::size_t closingQuote = json.find('"', openingQuote + 1);
  if (closingQuote == std::string::npos) {
    return false;
  }
  value = json.substr(openingQuote + 1, closingQuote - openingQuote - 1);
  return true;
}

inline bool extractInt(const std::string& json, const char* key, int& value) {
  const std::string marker = "\"" + std::string(key) + "\"";
  const std::size_t keyPosition = json.find(marker);
  if (keyPosition == std::string::npos) {
    return false;
  }
  const std::size_t colon = json.find(':', keyPosition + marker.size());
  if (colon == std::string::npos) {
    return false;
  }
  std::size_t cursor = colon + 1;
  while (cursor < json.size() && std::isspace(static_cast<unsigned char>(json[cursor]))) {
    ++cursor;
  }
  char* end = nullptr;
  const long parsed = std::strtol(json.c_str() + cursor, &end, 10);
  if (end == json.c_str() + cursor) {
    return false;
  }
  if (parsed < std::numeric_limits<int>::min() ||
      parsed > std::numeric_limits<int>::max()) {
    return false;
  }
  value = static_cast<int>(parsed);
  return true;
}

inline void extractFiles(const std::string& json, std::vector<std::string>& files) {
  const std::string marker = "\"files\"";
  const std::size_t keyPosition = json.find(marker);
  if (keyPosition == std::string::npos) {
    return;
  }
  const std::size_t openingBracket = json.find('[', keyPosition + marker.size());
  if (openingBracket == std::string::npos) {
    return;
  }
  const std::size_t closingBracket = json.find(']', openingBracket + 1);
  if (closingBracket == std::string::npos) {
    return;
  }
  std::size_t cursor = openingBracket + 1;
  while (cursor < closingBracket) {
    const std::size_t openingQuote = json.find('"', cursor);
    if (openingQuote == std::string::npos || openingQuote >= closingBracket) {
      break;
    }
    const std::size_t closingQuote = json.find('"', openingQuote + 1);
    if (closingQuote == std::string::npos || closingQuote > closingBracket) {
      break;
    }
    files.emplace_back(json.substr(openingQuote + 1, closingQuote - openingQuote - 1));
    cursor = closingQuote + 1;
  }
}

} // namespace detail

inline Transfer& getTransfer() {
  static Transfer transfer;
  return transfer;
}

inline bool Transfer::beginList() {
  reset();
  return true;
}

inline bool Transfer::beginFile(const int processNumber) {
  std::string request;
  if (!formatFileRequest(processNumber, request)) {
    return false;
  }
  reset();
  _status = Status::RECEIVING;
  return true;
}

inline bool Transfer::consume(const char* message) {
  if (message == nullptr) {
    return false;
  }

  const std::string json(message);
  std::string type;
  if (!detail::extractString(json, "type", type)) {
    return false;
  }

  std::string request;
  std::string result;
  if (type == "response" && detail::extractString(json, "request", request) &&
      request == "GET_LOG") {
    detail::extractString(json, "result", result);
    if (result == "LIST") {
      _files.clear();
      detail::extractFiles(json, _files);
      _status = Status::LIST_RECEIVED;
      return true;
    }

    if (result == "START") {
      int total = 0;
      std::string file;
      if (!detail::extractInt(json, "total", total) ||
          !detail::extractString(json, "file", file) || total <= 0) {
        _status = Status::ERROR;
        _error = "invalid_start";
        return false;
      }
      _fileName = file;
      _totalChunks = total;
      _receivedChunks = 0;
      _data.clear();
      _status = Status::RECEIVING;
      return true;
    }

    if (result.rfind("err_", 0) == 0) {
      _status = Status::ERROR;
      _error = result;
      return true;
    }
    return false;
  }

  if (type != "file" || _status != Status::RECEIVING) {
    return false;
  }

  int chunk = 0;
  std::string encoded;
  if (!detail::extractInt(json, "chunk", chunk) ||
      !detail::extractString(json, "data", encoded) ||
      chunk != _receivedChunks + 1) {
    _status = Status::ERROR;
    _error = "invalid_chunk";
    return false;
  }

  std::string decoded;
  if (!detail::decodeBase64(encoded.c_str(), decoded) ||
      decoded.size() > FILE_CHUNK_RAW_MAX) {
    _status = Status::ERROR;
    _error = "invalid_data";
    return false;
  }

  _data += decoded;
  _receivedChunks = chunk;
  if (_receivedChunks == _totalChunks) {
    _status = Status::COMPLETED;
  }
  return true;
}

inline Status Transfer::status() const {
  return _status;
}

inline const std::vector<std::string>& Transfer::files() const {
  return _files;
}

inline const std::string& Transfer::fileName() const {
  return _fileName;
}

inline const std::string& Transfer::data() const {
  return _data;
}

inline const std::string& Transfer::error() const {
  return _error;
}

inline int Transfer::totalChunks() const {
  return _totalChunks;
}

inline int Transfer::receivedChunks() const {
  return _receivedChunks;
}

inline void Transfer::reset() {
  _status = Status::IDLE;
  _files.clear();
  _fileName.clear();
  _data.clear();
  _error.clear();
  _totalChunks = 0;
  _receivedChunks = 0;
}

inline bool formatListRequest(std::string& request) {
  request = "GET_LOG\n";
  return detail::validRequestLength(request);
}

inline bool formatFileRequest(const int processNumber, std::string& request) {
  if (processNumber <= 0) {
    return false;
  }
  request = "GET_LOG " + std::to_string(processNumber) + "\n";
  return detail::validRequestLength(request);
}

} // namespace SsvcLogProtocol

#endif
