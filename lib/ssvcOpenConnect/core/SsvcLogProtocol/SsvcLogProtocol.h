#ifndef SSVC_OPEN_CONNECT_SSVC_LOG_PROTOCOL_H
#define SSVC_OPEN_CONNECT_SSVC_LOG_PROTOCOL_H

#include <cstddef>
#include <cctype>
#include <cstring>
#include <cstdlib>
#include <limits>
#include <mutex>
#include <string>
#include <vector>

namespace SsvcLogProtocol {

constexpr std::size_t GET_LOG_CMD_MAX_LEN = 30;
constexpr std::size_t FILE_CHUNK_RAW_MAX = 600;
constexpr std::size_t LOG_TOTAL_RAW_MAX = 512 * 1024;

// Если во время передачи файла новые пакеты не приходят дольше этого интервала,
// передача считается зависшей и переводится в ошибку "timeout".
constexpr unsigned long TRANSFER_TIMEOUT_MS = 10000;

// GET_LOG появился в UART API 1.10; ниже этой версии скачивание журналов недоступно.
// Версия хранится целым кодом major*100+minor: "1.10" как float равно 1.1
// и оказалось бы меньше "1.7".
constexpr int GET_LOG_MIN_API_VERSION_CODE = 1 * 100 + 10;

enum class Status {
  IDLE,
  QUEUED_LIST,
  WAITING_LIST,
  LIST_RECEIVED,
  QUEUED_FILE,
  WAITING_START,
  RECEIVING,
  COMPLETED,
  ERROR,
  DRAINING
};

// Результат попытки зарезервировать сессию под новый запрос.
enum class ReserveResult {
  Accepted,     // резерв создан, команду можно ставить в очередь
  SameRequest,  // тот же запрос уже выполняется или готов — новую команду не ставить
  Busy,         // сессия занята другим запросом
  Unavailable   // запрос невозможен (например, некорректный номер процесса)
};

// Согласованный снимок состояния: HTTP читает его по значению и не держит
// ссылок на контейнеры, которые меняются в задачах RX и Queue.
struct Snapshot {
  Status status{Status::IDLE};
  bool pending{false};
  std::vector<std::string> files;
  std::string fileName;
  std::string error;
  int totalChunks{0};
  int receivedChunks{0};
  unsigned long generation{0};
  int processNumber{0};
};

class Transfer {
public:
  bool beginList(unsigned long nowMs = 0);
  bool beginFile(int processNumber, unsigned long nowMs = 0);
  bool consume(const char* message, unsigned long nowMs = 0);
  bool isPending() const;
  void markPending();
  void clearPending();

  Status status() const;
  const std::vector<std::string>& files() const;
  const std::string& fileName() const;
  const std::string& data() const;
  const std::string& error() const;
  int totalChunks() const;
  int receivedChunks() const;
  unsigned long lastActivityMs() const;

  // Резервирование сессии до постановки команды в очередь: под lock выставляются
  // generation, вид запроса и номер процесса, затем lock отпускается, и команда
  // ставится в очередь уже вне критической секции.
  ReserveResult reserveList(unsigned long nowMs = 0);
  ReserveResult reserveFile(int processNumber, unsigned long nowMs = 0);
  // QUEUED_* -> WAITING_*: вызывается перед записью в UART.
  void markSent();
  // Откат резерва с ошибкой: команда не поставлена или UART не принял запись.
  void abortReservation(const char* reason);
  // Снять резерв без ошибки: запрос отклонён (например, очередь полна).
  void releaseReservation();
  // Отмена: до отправки резерв снимается и команда инвалидируется, после
  // отправки — DRAINING до границы старого потока.
  void cancel();

  /// Завершение дренирования: asError=false — граница потока найдена и сессия
  /// освобождается; asError=true — фиксируется ошибка с указанной причиной.
  bool completeDrain(const char* reason, bool asError);
  bool isDraining() const;

  // Сессия занята (резерв, приём или дренирование) — обычные команды ждут.
  bool isBlocked() const;
  bool isFileSession() const;
  unsigned long generation() const;

  Snapshot snapshot() const;
  // Копия собранного CSV под lock — без ссылок на внутренний буфер.
  std::string dataCopy() const;

  // Переводит зависшую передачу (RECEIVING без активности дольше timeoutMs) в ERROR.
  // Возвращает true, если таймаут сработал. nowMs передаётся снаружи (millis()),
  // чтобы заголовок оставался тестируемым на нативной платформе.
  bool checkTimeout(unsigned long nowMs, unsigned long timeoutMs = TRANSFER_TIMEOUT_MS);

  void reset();

private:
  enum class RequestKind { None, List, File };

  void resetLocked();
  void touchLocked(unsigned long nowMs);
  bool blockedLocked() const;
  void abortLocked(const char* reason, bool asError);
  bool consumeLocked(const char* message, unsigned long nowMs);

  // mutable: снимки читаются из HTTP-задач, состояние меняется в RX/Queue.
  mutable std::mutex _mutex;
  Status _status{Status::IDLE};
  bool _pending{false};
  RequestKind _request{RequestKind::None};
  unsigned long _generation{0};
  int _processNumber{0};
  std::vector<std::string> _files;
  std::string _fileName;
  std::string _data;
  std::string _error;
  int _totalChunks{0};
  int _receivedChunks{0};
  unsigned long _lastActivityMs{0};
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

inline bool Transfer::beginList(const unsigned long nowMs) {
  std::lock_guard<std::mutex> lock(_mutex);
  resetLocked();
  touchLocked(nowMs);
  _status = Status::WAITING_LIST;
  _request = RequestKind::List;
  ++_generation;
  _pending = true;
  return true;
}

inline bool Transfer::beginFile(const int processNumber, const unsigned long nowMs) {
  std::string request;
  if (!formatFileRequest(processNumber, request)) {
    return false;
  }
  std::lock_guard<std::mutex> lock(_mutex);
  resetLocked();
  touchLocked(nowMs);
  _status = Status::RECEIVING;
  _request = RequestKind::File;
  _processNumber = processNumber;
  ++_generation;
  _pending = true;
  return true;
}

inline bool Transfer::isPending() const {
  std::lock_guard<std::mutex> lock(_mutex);
  return _pending;
}

inline void Transfer::markPending() {
  std::lock_guard<std::mutex> lock(_mutex);
  _pending = true;
}

inline void Transfer::clearPending() {
  std::lock_guard<std::mutex> lock(_mutex);
  _pending = false;
}

// Состояния, в которых сессия занята: обычные команды ждут освобождения.
inline bool Transfer::blockedLocked() const {
  switch (_status) {
  case Status::QUEUED_LIST:
  case Status::WAITING_LIST:
  case Status::QUEUED_FILE:
  case Status::WAITING_START:
  case Status::RECEIVING:
  case Status::DRAINING:
    return true;
  default:
    return false;
  }
}

inline ReserveResult Transfer::reserveList(const unsigned long nowMs) {
  std::lock_guard<std::mutex> lock(_mutex);
  if (_request == RequestKind::List &&
      (_status == Status::QUEUED_LIST || _status == Status::WAITING_LIST)) {
    return ReserveResult::SameRequest;
  }
  if (blockedLocked()) {
    return ReserveResult::Busy;
  }
  resetLocked();
  ++_generation;
  _request = RequestKind::List;
  _status = Status::QUEUED_LIST;
  _pending = true;
  touchLocked(nowMs);
  return ReserveResult::Accepted;
}

inline ReserveResult Transfer::reserveFile(const int processNumber, const unsigned long nowMs) {
  std::string request;
  if (!formatFileRequest(processNumber, request)) {
    return ReserveResult::Unavailable;
  }
  std::lock_guard<std::mutex> lock(_mutex);
  const bool sameFile = _request == RequestKind::File && _processNumber == processNumber;
  if (sameFile && (_status == Status::QUEUED_FILE || _status == Status::WAITING_START ||
                   _status == Status::RECEIVING || _status == Status::COMPLETED)) {
    // Повторный запрос того же файла: новая команда не отправляется, клиент
    // получает текущий прогресс или уже собранный CSV.
    return ReserveResult::SameRequest;
  }
  if (blockedLocked() || _status == Status::COMPLETED) {
    return ReserveResult::Busy;
  }
  resetLocked();
  ++_generation;
  _request = RequestKind::File;
  _processNumber = processNumber;
  _fileName = std::to_string(processNumber) + ".CSV";
  _status = Status::QUEUED_FILE;
  _pending = true;
  touchLocked(nowMs);
  return ReserveResult::Accepted;
}

inline void Transfer::markSent() {
  std::lock_guard<std::mutex> lock(_mutex);
  if (_status == Status::QUEUED_LIST) {
    _status = Status::WAITING_LIST;
  } else if (_status == Status::QUEUED_FILE) {
    _status = Status::WAITING_START;
  }
}

inline void Transfer::abortLocked(const char* reason, const bool asError) {
  _pending = false;
  if (asError) {
    _status = Status::ERROR;
    _error = reason == nullptr ? "aborted" : reason;
    return;
  }
  resetLocked();
}

inline void Transfer::abortReservation(const char* reason) {
  std::lock_guard<std::mutex> lock(_mutex);
  if (_status == Status::QUEUED_LIST || _status == Status::WAITING_LIST ||
      _status == Status::QUEUED_FILE || _status == Status::WAITING_START) {
    abortLocked(reason, true);
  }
}

inline void Transfer::releaseReservation() {
  std::lock_guard<std::mutex> lock(_mutex);
  if (_status == Status::QUEUED_LIST || _status == Status::QUEUED_FILE) {
    abortLocked(nullptr, false);
  }
}

inline void Transfer::cancel() {
  std::lock_guard<std::mutex> lock(_mutex);
  if (_status == Status::QUEUED_LIST || _status == Status::QUEUED_FILE) {
    // Команда ещё не отправлена. Поколение инкрементируется: без этого уже
    // лежащая в очереди команда с прежним generation дошла бы до UART.
    ++_generation;
    abortLocked(nullptr, false);
    return;
  }
  if (blockedLocked()) {
    // Отправка уже началась: не пускаем новые запросы до границы потока.
    _status = Status::DRAINING;
    _error.clear();
  }
}

inline bool Transfer::completeDrain(const char* reason, const bool asError) {
  std::lock_guard<std::mutex> lock(_mutex);
  if (_status != Status::DRAINING) {
    return false;
  }
  if (asError) {
    _status = Status::ERROR;
    _error = reason == nullptr ? "drained" : reason;
    _pending = false;
    return true;
  }
  resetLocked();
  return true;
}

inline bool Transfer::isDraining() const {
  std::lock_guard<std::mutex> lock(_mutex);
  return _status == Status::DRAINING;
}

inline bool Transfer::isBlocked() const {
  std::lock_guard<std::mutex> lock(_mutex);
  return blockedLocked();
}

inline bool Transfer::isFileSession() const {
  std::lock_guard<std::mutex> lock(_mutex);
  return _request == RequestKind::File && blockedLocked();
}

inline unsigned long Transfer::generation() const {
  std::lock_guard<std::mutex> lock(_mutex);
  return _generation;
}

inline Snapshot Transfer::snapshot() const {
  std::lock_guard<std::mutex> lock(_mutex);
  Snapshot snap;
  snap.status = _status;
  snap.pending = _pending;
  snap.files = _files;
  snap.fileName = _fileName;
  snap.error = _error;
  snap.totalChunks = _totalChunks;
  snap.receivedChunks = _receivedChunks;
  snap.generation = _generation;
  snap.processNumber = _processNumber;
  return snap;
}

inline std::string Transfer::dataCopy() const {
  std::lock_guard<std::mutex> lock(_mutex);
  return _data;
}

inline bool Transfer::consume(const char* message, const unsigned long nowMs) {
  std::lock_guard<std::mutex> lock(_mutex);
  return consumeLocked(message, nowMs);
}

inline bool Transfer::consumeLocked(const char* message, const unsigned long nowMs) {
  if (message == nullptr) {
    return false;
  }
  touchLocked(nowMs);

  const std::string json(message);
  std::string type;
  if (!detail::extractString(json, "type", type)) {
    return false;
  }

  if (_status == Status::DRAINING) {
    // Идёт отмена: кадры старой передачи только отбрасываются, поздний START не
    // возрождает операцию. Границей потока считается последний чанк.
    if (type == "file") {
      int chunk = 0;
      if (detail::extractInt(json, "chunk", chunk) && _totalChunks > 0 &&
          chunk >= _totalChunks) {
        _status = Status::ERROR;
        _error = "cancelled";
        _pending = false;
      }
    }
    return false;
  }

  std::string request;
  std::string result;
  if (type == "response" && detail::extractString(json, "request", request) &&
      request == "GET_LOG") {
    detail::extractString(json, "result", result);
    if (result == "LIST") {
      if (_request != RequestKind::List) {
        return false;  // список не запрашивали — кадр не для этой сессии
      }
      _files.clear();
      detail::extractFiles(json, _files);
      _status = Status::LIST_RECEIVED;
      return true;
    }

    if (result == "START") {
      if (_request != RequestKind::File) {
        return false;  // файл не запрашивали — кадр не для этой сессии
      }
      int total = 0;
      std::string file;
      if (!detail::extractInt(json, "total", total) ||
          !detail::extractString(json, "file", file) || total <= 0) {
        _status = Status::ERROR;
        _error = "invalid_start";
        return false;
      }
      // Имя из START не заменяет зарезервированный process id: расхождение
      // означает ответ на чужой запрос, а не начало нашей передачи.
      const std::string expectedName = std::to_string(_processNumber) + ".CSV";
      if (file != expectedName) {
        _status = Status::ERROR;
        _error = "unexpected_file";
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

  if (_data.size() + decoded.size() > LOG_TOTAL_RAW_MAX) {
    _status = Status::ERROR;
    _error = "log_too_large";
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
  std::lock_guard<std::mutex> lock(_mutex);
  return _status;
}

// Ссылочные геттеры читает только владелец сессии (RX-задача) и нативные тесты.
// HTTP-слой обязан использовать snapshot()/dataCopy(), а не ссылки: контейнеры
// меняются в другой задаче.
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
  std::lock_guard<std::mutex> lock(_mutex);
  return _totalChunks;
}

inline int Transfer::receivedChunks() const {
  std::lock_guard<std::mutex> lock(_mutex);
  return _receivedChunks;
}

inline unsigned long Transfer::lastActivityMs() const {
  std::lock_guard<std::mutex> lock(_mutex);
  return _lastActivityMs;
}

inline void Transfer::touchLocked(const unsigned long nowMs) {
  // 0 означает "время не отслеживается" — вызовы без метки времени не сбрасывают её.
  if (nowMs != 0) {
    _lastActivityMs = nowMs;
  }
}

inline bool Transfer::checkTimeout(const unsigned long nowMs, const unsigned long timeoutMs) {
  std::lock_guard<std::mutex> lock(_mutex);
  // Deadline действует для любого незавершённого состояния: ожидание списка,
  // ожидание START, приём файла и дренирование. Иначе потерянный LIST/START
  // оставлял бы сессию занятой навсегда.
  if (_lastActivityMs == 0 || !blockedLocked()) {
    return false;
  }
  if (nowMs - _lastActivityMs < timeoutMs) {
    return false;
  }
  _status = Status::ERROR;
  _error = "timeout";
  // Снимаем признак занятости: иначе после молчания контролера лог-эндпоинты
  // останутся заблокированными навсегда.
  _pending = false;
  return true;
}

inline void Transfer::reset() {
  std::lock_guard<std::mutex> lock(_mutex);
  resetLocked();
}

inline void Transfer::resetLocked() {
  _status = Status::IDLE;
  _pending = false;
  _request = RequestKind::None;
  _processNumber = 0;
  _files.clear();
  _fileName.clear();
  _data.clear();
  _error.clear();
  _totalChunks = 0;
  _receivedChunks = 0;
  _lastActivityMs = 0;
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
