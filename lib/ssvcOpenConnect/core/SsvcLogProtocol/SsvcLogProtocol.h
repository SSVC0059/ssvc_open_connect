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

#if defined(ESP_PLATFORM) || defined(ARDUINO)
#include <esp_system.h>
#endif

// Нативные тесты подключают doctest, а он на Windows тянет windows.h, где IDLE
// и ERROR — числовые макросы. Без снятия макросов `enum class Status { IDLE }`
// не компилируется, а `Status::ERROR` превращается в `Status::0`.
#ifdef IDLE
#undef IDLE
#endif
#ifdef ERROR
#undef ERROR
#endif

namespace SsvcLogProtocol {

constexpr std::size_t GET_LOG_CMD_MAX_LEN = 30;
constexpr std::size_t FILE_CHUNK_RAW_MAX = 600;

// Пределы разбора `files`: ответ устройства не должен заставлять выделять
// память под произвольное число элементов и произвольную длину строки.
constexpr std::size_t LIST_FILES_MAX = 64;
constexpr std::size_t LIST_ENTRY_LEN_MAX = 64;

// Предел объёма одного журнала. 512 КБ во внутреннюю RAM ESP32-S3 не влезают:
// журнал собирается в std::string, поверх него создаётся копия для HTTP-ответа,
// плюс нужен запас на перевыделение. При сборке без обработки исключений
// bad_alloc превращается в abort, то есть в перезагрузку устройства. 64 КБ —
// это порядка тысячи строк CSV, типичный журнал процесса SSVC-0059.
constexpr std::size_t LOG_TOTAL_RAW_MAX = 64 * 1024;

// Минимум свободной кучи, который обязан остаться после выделения буфера:
// иначе приём журнала сам отнимает память у WiFi и HTTP-стека.
constexpr std::size_t LOG_HEAP_SAFETY_MARGIN = 32 * 1024;

/// Свободная внутренняя куча. На нативной платформе ограничения нет: там
/// заголовок используется тестами, а не реальным устройством.
inline std::size_t freeHeapBytes() {
#if defined(ESP_PLATFORM) || defined(ARDUINO)
  return static_cast<std::size_t>(esp_get_free_heap_size());
#else
  return std::numeric_limits<std::size_t>::max();
#endif
}

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
  std::vector<std::string> files;
  std::string fileName;
  std::string error;
  int totalChunks{0};
  int receivedChunks{0};
  unsigned long generation{0};
  int processNumber{0};
  /// Объём уже принятых данных: по нему HTTP-слой проверяет кучу до копирования.
  std::size_t dataBytes{0};
};

class Transfer {
public:
  bool beginList(unsigned long nowMs = 0);
  bool beginFile(int processNumber, unsigned long nowMs = 0);
  bool consume(const char* message, unsigned long nowMs = 0);

  Status status() const;
  /// Геттеры возвращают значения, а не ссылки: контейнеры меняются в RX-задаче,
  /// а читаются из HTTP-задачи. Ссылка на std::string в этот момент — гонка.
  std::vector<std::string> files() const;
  std::string fileName() const;
  std::string data() const;
  std::string error() const;
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

  /// Освобождает готовый результат, удерживаемый дольше holdMs: иначе буфер
  /// вплоть до LOG_TOTAL_RAW_MAX остаётся занятым, пока клиент не запросит иное.
  bool releaseExpiredResult(unsigned long nowMs, unsigned long holdMs);

  // Сессия занята (резерв, приём или дренирование) — обычные команды ждут.
  bool isBlocked() const;
  bool isFileSession() const;
  unsigned long generation() const;

  Snapshot snapshot() const;

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
  /// Переход в ERROR: ошибка, снятие признака занятости и освобождение буфера
  /// одной операцией, чтобы ни одно место не забыло освободить _data.
  void failLocked(const char* reason);
  void releaseDataLocked();
  bool consumeLocked(const char* message, unsigned long nowMs);

  /// Резервирует буфер под журнал известного объёма (total чанков по
  /// FILE_CHUNK_RAW_MAX). false — памяти не хватает, приём начинать нельзя.
  bool reserveForTotalLocked(int totalChunks);

  // mutable: снимки читаются из HTTP-задач, состояние меняется в RX/Queue.
  mutable std::mutex _mutex;
  Status _status{Status::IDLE};
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
  /// Момент перехода в COMPLETED: от него считается срок удержания результата.
  unsigned long _completedAtMs{0};
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

inline bool decodeBase64(const char* encoded, std::string& decoded,
                         const std::size_t maxDecodedBytes = FILE_CHUNK_RAW_MAX) {
  if (encoded == nullptr) {
    return false;
  }

  decoded.clear();
  const std::size_t length = std::strlen(encoded);
  // Длина обязана быть кратной 4: иначе кадр обрезан, и «успешное» декодирование
  // молча потеряло бы байты.
  // Пустая строка — валидный «нулевой» чанк, а не ошибка длины.
  if (length == 0) {
    return true;
  }
  if (length % 4 != 0) {
    return false;
  }
  // Верхняя оценка объёма известна до разбора: без этой проверки reserve()
  // выделял память под любой присланный размер, и лимит чанка применялся уже
  // после того, как куча была занята.
  if ((length / 4) * 3 > maxDecodedBytes) {
    return false;
  }

  decoded.reserve(length / 4 * 3);
  for (std::size_t index = 0; index < length; index += 4) {
    unsigned values[4] = {0, 0, 0, 0};
    unsigned padding = 0;
    for (std::size_t offset = 0; offset < 4; ++offset) {
      const char symbol = encoded[index + offset];
      if (symbol == '=') {
        // Padding допустим только в хвосте последней группы.
        if (index + 4 != length || offset < 2) {
          return false;
        }
        ++padding;
        continue;
      }
      if (padding != 0) {
        return false; // данные после '=' внутри группы
      }
      const int value = base64Value(symbol);
      if (value < 0) {
        return false; // символ вне алфавита
      }
      values[offset] = static_cast<unsigned>(value);
    }
    if (padding > 2) {
      return false;
    }

    const unsigned triple =
        (values[0] << 18) | (values[1] << 12) | (values[2] << 6) | values[3];
    decoded.push_back(static_cast<char>((triple >> 16) & 0xFF));
    if (padding < 2) {
      decoded.push_back(static_cast<char>((triple >> 8) & 0xFF));
    }
    if (padding == 0) {
      decoded.push_back(static_cast<char>(triple & 0xFF));
    }
  }

  return true;
}

inline bool validRequestLength(const std::string& request) {
  return request.size() + 1 <= GET_LOG_CMD_MAX_LEN;
}

/// Глагол команды из поля `request` ответа.
///
/// Контроллер повторяет в `request` всю команду вместе с параметрами
/// ("SET heads=[1,2]"), поэтому строгое сравнение с "GET_LOG" дало бы ложно
/// отрицательный результат, и кадр перестал бы продлевать сессию передачи.
inline std::string requestVerb(const std::string& request) {
  const std::size_t space = request.find_first_of(" \t");
  return space == std::string::npos ? request : request.substr(0, space);
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

/**
 * Разбор массива строк по ключу: `"files":["15.CSV","16.CSV"]`.
 *
 * Границы массива определяются по структуре (ключ → `:` → `[` → элементы через
 * запятую → `]`), а не по первой попавшейся скобке, и учитывается экранирование
 * внутри строк. Повреждённый массив возвращает false — вызывающий обязан выдать
 * ошибку, а не пустой список.
 */
inline bool parseStringArray(const std::string& json, const std::string& key,
                             std::vector<std::string>& out) {
  out.clear();
  const std::string marker = "\"" + key + "\"";
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
  if (cursor >= json.size() || json[cursor] != '[') {
    return false;
  }
  ++cursor;

  while (true) {
    while (cursor < json.size() && std::isspace(static_cast<unsigned char>(json[cursor]))) {
      ++cursor;
    }
    if (cursor >= json.size()) {
      return false;  // массив не закрыт
    }
    if (json[cursor] == ']') {
      return true;  // в том числе пустой массив
    }
    if (json[cursor] != '"') {
      return false;  // элемент обязан быть строкой
    }

    ++cursor;
    std::string value;
    bool closed = false;
    while (cursor < json.size()) {
      const char symbol = json[cursor++];
      if (symbol == '\\') {
        if (cursor >= json.size()) {
          break;  // экранирование оборвано
        }
        value.push_back(json[cursor++]);
        continue;
      }
      if (symbol == '"') {
        closed = true;
        break;
      }
      value.push_back(symbol);
    }
    if (!closed) {
      return false;
    }
    if (value.size() > LIST_ENTRY_LEN_MAX) {
      return false;  // имя файла не может быть такой длины
    }
    if (out.size() >= LIST_FILES_MAX) {
      return false;  // список больше, чем устройство способно отдать
    }
    out.push_back(std::move(value));

    while (cursor < json.size() && std::isspace(static_cast<unsigned char>(json[cursor]))) {
      ++cursor;
    }
    if (cursor >= json.size()) {
      return false;
    }
    if (json[cursor] == ',') {
      ++cursor;
      continue;
    }
    if (json[cursor] == ']') {
      return true;
    }
    return false;  // между элементами только ',' или конец массива
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
  return true;
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
  if (blockedLocked()) {
    return ReserveResult::Busy;
  }
  resetLocked();
  ++_generation;
  _request = RequestKind::File;
  _processNumber = processNumber;
  _fileName = std::to_string(processNumber) + ".CSV";
  _status = Status::QUEUED_FILE;
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

inline void Transfer::releaseDataLocked() {
  // clear() оставляет ёмкость: без shrink_to_fit десятки килобайт остаются
  // занятыми до следующего резерва и отнимают память у WiFi/HTTP.
  _data.clear();
  _data.shrink_to_fit();
}

inline void Transfer::failLocked(const char* reason) {
  _status = Status::ERROR;
  _error = reason == nullptr ? "aborted" : reason;
  releaseDataLocked();
}

inline void Transfer::abortLocked(const char* reason, const bool asError) {
  if (asError) {
    failLocked(reason);
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
  if (_status == Status::COMPLETED) {
    // Отмена готового результата: он освобождается, повторный GET того же
    // процесса потребует новой передачи.
    ++_generation;
    resetLocked();
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
    failLocked(reason == nullptr ? "drained" : reason);
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
  snap.files = _files;
  snap.fileName = _fileName;
  snap.error = _error;
  snap.totalChunks = _totalChunks;
  snap.receivedChunks = _receivedChunks;
  snap.generation = _generation;
  snap.processNumber = _processNumber;
  snap.dataBytes = _data.size();
  return snap;
}

inline bool Transfer::releaseExpiredResult(const unsigned long nowMs,
                                           const unsigned long holdMs) {
  std::lock_guard<std::mutex> lock(_mutex);
  if (_status != Status::COMPLETED || _completedAtMs == 0) {
    return false;
  }
  if (nowMs - _completedAtMs < holdMs) {
    return false;
  }
  // Буфер освобождается вместо бессрочного удержания: вызывающий (REST-слой)
  // видит факт по возвращаемому значению.
  ++_generation;
  resetLocked();
  return true;
}

inline bool Transfer::consume(const char* message, const unsigned long nowMs) {
  std::lock_guard<std::mutex> lock(_mutex);
  return consumeLocked(message, nowMs);
}

inline bool Transfer::consumeLocked(const char* message, const unsigned long nowMs) {
  if (message == nullptr) {
    return false;
  }

  const std::string json(message);
  std::string type;
  if (!detail::extractString(json, "type", type)) {
    return false;
  }

  // Deadline сессии продлевают только кадры самой сессии: посторонняя телеметрия
  // и повреждённые кадры не должны отодвигать таймаут передачи.
  std::string request;
  const bool isLogFrame =
      type == "file" ||
      (type == "response" && detail::extractString(json, "request", request) &&
       detail::requestVerb(request) == "GET_LOG");

  if (isLogFrame) {
    touchLocked(nowMs);
  }

  if (_status == Status::DRAINING) {
    // Идёт отмена: кадры старой передачи только отбрасываются, поздний START не
    // возрождает операцию. Границей потока считается последний чанк.
    if (type == "file") {
      int chunk = 0;
      if (detail::extractInt(json, "chunk", chunk) && _totalChunks > 0 &&
          chunk >= _totalChunks) {
        failLocked("cancelled");
      }
    }
    return false;
  }

  std::string result;
  if (isLogFrame && type == "response") {
    detail::extractString(json, "result", result);
    if (result == "LIST") {
      if (_request != RequestKind::List) {
        return false;  // список не запрашивали — кадр не для этой сессии
      }
      // Строгий разбор: повреждённый массив — это явная ошибка, а не «пустой
      // список» (раньше ручной обход скобок молча возвращал неполный результат).
      if (!detail::parseStringArray(json, "files", _files)) {
        failLocked("invalid_list");
        return false;
      }
      _status = Status::LIST_RECEIVED;
      return true;
    }

    if (result == "START") {
      if (_request != RequestKind::File) {
        return false;  // файл не запрашивали — кадр не для этой сессии
      }
      int total = 0;
      std::string file;
      // Отсутствие/нечитаемость полей — повреждённый кадр. А вот total == 0 —
      // это не повреждение: у пустого журнала ноль чанков, и раньше такой файл
      // было невозможно скачать (передача падала с invalid_start).
      if (!detail::extractInt(json, "total", total) ||
          !detail::extractString(json, "file", file) || total < 0) {
        failLocked("invalid_start");
        return false;
      }
      // Имя из START не заменяет зарезервированный process id: расхождение
      // означает ответ на чужой запрос, а не начало нашей передачи.
      const std::string expectedName = std::to_string(_processNumber) + ".CSV";
      if (file != expectedName) {
        failLocked("unexpected_file");
        return false;
      }
      _fileName = file;
      _totalChunks = total;
      _receivedChunks = 0;
      _data.clear();
      // Память резервируется по объявленному total до приёма первого чанка:
      // иначе рост std::string даёт пик 1.5× от объёма и bad_alloc посреди
      // передачи. Не хватило кучи — честная ошибка вместо abort.
      if (!reserveForTotalLocked(total)) {
        failLocked("log_heap_low");
        return false;
      }
      if (total == 0) {
        // Пустой журнал: чанков не будет, поэтому передача завершается сразу.
        _status = Status::COMPLETED;
        _completedAtMs = nowMs;
        return true;
      }
      _status = Status::RECEIVING;
      return true;
    }

    if (result.rfind("err_", 0) == 0) {
      failLocked(result.c_str());
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
    failLocked("invalid_chunk");
    return false;
  }

  std::string decoded;
  if (!detail::decodeBase64(encoded.c_str(), decoded, FILE_CHUNK_RAW_MAX) ||
      decoded.size() > FILE_CHUNK_RAW_MAX) {
    failLocked("invalid_data");
    return false;
  }

  if (_data.size() + decoded.size() > LOG_TOTAL_RAW_MAX) {
    failLocked("log_too_large");
    return false;
  }

  _data += decoded;
  _receivedChunks = chunk;
  if (_receivedChunks == _totalChunks) {
    _status = Status::COMPLETED;
    // Момент готовности: от него считается срок удержания результата в памяти.
    _completedAtMs = nowMs;
  }
  return true;
}

inline Status Transfer::status() const {
  std::lock_guard<std::mutex> lock(_mutex);
  return _status;
}

// Геттеры возвращают копии под lock: HTTP-задача читает их одновременно с
// приёмом в RX-задаче, а ссылка на std::string в этот момент — гонка и UB.
inline std::vector<std::string> Transfer::files() const {
  std::lock_guard<std::mutex> lock(_mutex);
  return _files;
}

inline std::string Transfer::fileName() const {
  std::lock_guard<std::mutex> lock(_mutex);
  return _fileName;
}

inline std::string Transfer::data() const {
  std::lock_guard<std::mutex> lock(_mutex);
  return _data;
}

inline std::string Transfer::error() const {
  std::lock_guard<std::mutex> lock(_mutex);
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

inline bool Transfer::reserveForTotalLocked(const int totalChunks) {
  const std::size_t declared =
      static_cast<std::size_t>(totalChunks) * FILE_CHUNK_RAW_MAX;
  const std::size_t wanted = declared < LOG_TOTAL_RAW_MAX ? declared : LOG_TOTAL_RAW_MAX;

  // Пик памяти — не только сам буфер, но и его копия для HTTP-ответа, поэтому
  // свободной кучи требуется вдвое больше объявленного объёма плюс запас.
  const std::size_t peak = wanted * 2;

  const std::size_t heap = freeHeapBytes();
  if (heap != std::numeric_limits<std::size_t>::max()) {
    const std::size_t usable =
        heap > LOG_HEAP_SAFETY_MARGIN ? heap - LOG_HEAP_SAFETY_MARGIN : 0;
    if (peak > usable) {
      return false;
    }
  }

  // Ёмкость выделяется один раз: последующее дописывание чанков не должно
  // приводить к перевыделению (и к bad_alloc/abort) в середине передачи.
  _data.reserve(wanted);
  return true;
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
  // Буфер недоступного результата освобождается здесь же: иначе после молчания
  // контролера память висела бы до следующего резерва.
  failLocked("timeout");
  return true;
}

inline void Transfer::reset() {
  std::lock_guard<std::mutex> lock(_mutex);
  resetLocked();
}

inline void Transfer::resetLocked() {
  _status = Status::IDLE;
  _request = RequestKind::None;
  _processNumber = 0;
  _files.clear();
  _fileName.clear();
  _data.clear();
  _error.clear();
  _totalChunks = 0;
  _receivedChunks = 0;
  _lastActivityMs = 0;
  _completedAtMs = 0;
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
