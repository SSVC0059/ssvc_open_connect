# API OpenConnect

Этот раздел описывает API для получения информации о состоянии OpenConnect.

---

## Получение информации о состоянии OpenConnect

Запрашивает общую информацию о состоянии модуля OpenConnect.

**Эндпоинт:** `GET /rest/oc/info`

**Метод:** `GET`

**Аутентификация:** Требуется

### Пример запроса (curl)

```bash
curl -X GET http://DEVICE_IP/rest/oc/info \
     -H "Authorization: Bearer YOUR_AUTH_TOKEN"
```

### Ответы

*   **`200 OK`**: Успешный ответ с информацией о состоянии.

    **Пример ответа:**

    ```json
    {
      "ssvc": {
        "version": "1.10",
        "api": "1.10",
        "api_code": 110,
        "mode": "late_heads"
      },
      "oc": {
        "version": "0.2.9.1",
        "is_support_api": true,
        "is_support_release": false,
        "api_min": "1.7",
        "api_target": "1.10",
        "api_compatibility": "supported",
        "api_features": [
          { "name": "get_log", "min_api": "1.10", "available": true }
        ]
      }
    }
    ```
*   **`401 Unauthorized`**: Ошибка аутентификации.
*   **`500 Internal Server Error`**: Внутренняя ошибка сервера.

---

## Агрегированный снимок состояния (snapshot)

Один запрос вместо четырёх: `info`, текущая телеметрия, статус, показания
датчиков и аппаратные сбои в одном согласованном ответе. Нужен клиентам (в том
числе MCP-серверу), которым важен срез на один момент: телеметрия обновляется
раз в 2 с, и четыре отдельных запроса дают данные из разных моментов.

**Эндпоинт:** `GET /rest/oc/snapshot`

**Метод:** `GET`

**Аутентификация:** только администратор. `/rest/telemetry` и `/rest/sensor` в
прошивке доступны только администратору, поэтому снимок не должен становиться
обходом этих прав для гостя.

### Пример запроса (curl)

```bash
curl -X GET http://DEVICE_IP/rest/oc/snapshot \
     -H "Authorization: Bearer YOUR_AUTH_TOKEN"
```

### Формат ответа (`200`)

| Поле | Тип | Что содержит |
|------|-----|--------------|
| `observed_at` | number | `millis()` устройства на момент сборки ответа |
| `info` | object | ровно ответ `GET /rest/oc/info`: `ssvc` и `oc` (включая `api_features`) |
| `telemetry` | object | поле `telemetry` из состояния `TelemetryService` |
| `status` | object | поле `status` того же состояния, включая `uartConnectionError` |
| `lastUpdate` | number | `millis()` последнего изменения телеметрии, **не epoch** |
| `sensor` | object | ровно ответ `GET /rest/sensor` |
| `faults` | object | ровно ответ `GET /rest/oc/hardware-faults` |
| `partial` | array of string | имена частей, которые не удалось получить; пустой массив — всё хорошо |

**Пример ответа (сокращён):**

```json
{
  "observed_at": 123456,
  "info": {
    "ssvc": { "version": "1.10", "api": "1.10", "api_code": 110, "mode": "late_heads" },
    "oc": { "version": "0.2.9.1", "is_support_api": true, "api_features": [] }
  },
  "telemetry": { "temperature": 78.4 },
  "status": { "stage": "heads", "uartConnectionError": false },
  "lastUpdate": 123000,
  "sensor": {},
  "faults": { "faults": [], "count": 0, "i2cBusActive": true, "i2cScan": [72] },
  "partial": []
}
```

`info`, `telemetry`, `sensor` и `faults` содержат все поля одноимённых одиночных
маршрутов; в примере они сокращены.

### Части и поле `partial`

Части собираются независимо и не роняют ответ. Если сервис телеметрии ещё не
создан или её состояние пустое/повреждено, `telemetry` и `status` приходят
пустыми объектами, а имена `"telemetry"` / `"status"` попадают в `partial`. Если
не создан сервис датчиков — пустой `sensor` и `"sensor"` в `partial`. Ответ при
этом остаётся `200`: снимок нужен, чтобы видеть доступные части, а не падать
из-за одной неготовой.

`lastUpdate` — `millis()` устройства (время с загрузки), а не Unix-время: NTP
может быть не настроен. Возраст телеметрии — разность `observed_at - lastUpdate`
(оба значения в millis); сравнивать их с epoch нельзя.

### Ответы

*   **`200 OK`**: снимок состояния; недоступные части перечислены в `partial`.
*   **`401 Unauthorized`**: нет токена, токен неверен или принадлежит гостю
    (маршрут только для администратора).

---
