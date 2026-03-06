# API Датчики

Этот раздел описывает API для управления датчиками.

---

## Обновление зоны датчика

Обновляет информацию о зоне, к которой привязан датчик.

**Эндпоинт:** `PUT /rest/sensors/zone`

**Метод:** `PUT`

**Аутентификация:** Требуется

### Пример запроса (curl)

```bash
curl -X PUT http://DEVICE_IP/rest/sensors/zone \
     -H "Authorization: Bearer YOUR_AUTH_TOKEN" \
     -H "Content-Type: application/json" \
     -d '{
           "sensorId": "temp_sensor_1",
           "zone": "living_room"
         }'
```

### Тело запроса

JSON-объект, содержащий идентификатор датчика и новую информацию о его зоне.

**Пример тела запроса:**

```json
{
  "sensorId": "temp_sensor_1",
  "zone": "living_room"
}
```

### Ответы

*   **`200 OK`**: Зона датчика успешно обновлена.
*   **`400 Bad Request`**: Ошибка в формате запроса или неверные данные.
*   **`401 Unauthorized`**: Ошибка аутентификации.
*   **`404 Not Found`**: Датчик с указанным ID не найден.
*   **`500 Internal Server Error`**: Внутренняя ошибка сервера.

---
