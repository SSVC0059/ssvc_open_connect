# API Профили

Этот раздел описывает API для управления профилями настроек.

---

## Получение списка профилей

Возвращает список всех сохраненных профилей с их метаданными.

**Эндпоинт:** `GET /rest/profiles`
**Метод:** `GET`
**Аутентификация:** Требуется

### Пример запроса (curl)

```bash
curl -X GET http://DEVICE_IP/rest/profiles \
     -H "Authorization: Bearer YOUR_AUTH_TOKEN"
```

### Ответы
*   **`200 OK`**: Успешный ответ со списком профилей.
    ```json
    [
      {"id": "0", "name": "Default Profile", "createdAt": "2024-01-01T12:00:00Z", "isApplied": true},
      {"id": "1", "name": "Profile 1", "createdAt": "2024-01-02T10:30:00Z", "isApplied": false}
    ]
    ```

---

## Получение активного профиля

Возвращает ID активного в данный момент профиля.

**Эндпоинт:** `GET /rest/profiles/active`
**Метод:** `GET`
**Аутентификация:** Требуется

### Пример запроса (curl)

```bash
curl -X GET http://DEVICE_IP/rest/profiles/active \
     -H "Authorization: Bearer YOUR_AUTH_TOKEN"
```

### Ответы
*   **`200 OK`**:
    ```json
    {"id": "0"}
    ```
*   **`404 Not Found`**: Активный профиль не найден.
    ```json
    {"error": "No active profile found"}
    ```

---

## Получение содержимого профиля

Возвращает полное содержимое (все настройки) указанного профиля.

**Эндпоинт:** `GET /rest/profiles/content`
**Метод:** `GET`
**Аутентификация:** Требуется

### Тело запроса
```json
{
  "id": "идентификатор_профиля"
}
```

### Пример запроса (curl)

```bash
curl -X GET http://DEVICE_IP/rest/profiles/content \
     -H "Authorization: Bearer YOUR_AUTH_TOKEN" \
     -H "Content-Type: application/json" \
     -d '{"id": "0"}'
```

### Ответы
*   **`200 OK`**:
    ```json
    {
      "setting_key_1": "значение",
      "another_setting": 456
    }
    ```
*   **`400 Bad Request`**: Неверный JSON или отсутствует поле `id`.
    ```json
    {"error": "Missing or invalid 'id' field in request body"}
    ```
*   **`404 Not Found`**: Профиль не найден или не удалось получить содержимое.
    ```json
    {"error": "Profile content not found or failed to retrieve"}
    ```

---

## Создание нового профиля

Создает новый профиль из текущих настроек системы. ID профиля генерируется автоматически.

**Эндпоинт:** `POST /rest/profiles`
**Метод:** `POST`
**Аутентификация:** Требуется

### Тело запроса
```json
{
  "name": "Имя нового профиля"
}
```

### Пример запроса (curl)

```bash
curl -X POST http://DEVICE_IP/rest/profiles \
     -H "Authorization: Bearer YOUR_AUTH_TOKEN" \
     -H "Content-Type: application/json" \
     -d '{"name": "Мой новый профиль"}'
```

### Ответы
*   **`201 Created`**: Профиль успешно создан.
    ```json
    {"success": true, "id": "1"}
    ```
*   **`400 Bad Request`**: Неверный JSON или отсутствует поле `name`.
    ```json
    {"error": "Missing or invalid 'name' field"}
    ```
*   **`500 Internal Server Error`**: Не удалось сохранить профиль.
    ```json
    {"error": "Failed to save profile"}
    ```

---

## Удаление профиля

Удаляет указанный профиль.

**Эндпоинт:** `DELETE /rest/profiles/delete`
**Метод:** `DELETE`
**Аутентификация:** Требуется

### Тело запроса
```json
{
  "id": "идентификатор_профиля"
}
```

### Пример запроса (curl)

```bash
curl -X DELETE http://DEVICE_IP/rest/profiles/delete \
     -H "Authorization: Bearer YOUR_AUTH_TOKEN" \
     -H "Content-Type: application/json" \
     -d '{"id": "1"}'
```

### Ответы
*   **`200 OK`**: Профиль успешно удален.
    ```json
    {"success": "Profile deleted"}
    ```
*   **`400 Bad Request`**: Неверный JSON или отсутствует поле `id`.
    ```json
    {"error": "Missing or invalid 'id' field in request body"}
    ```
*   **`404 Not Found`**: Профиль не найден или не удалось удалить.
    ```json
    {"error": "Profile not found or failed to delete"}
    ```

---

## Копирование профиля

Создает копию существующего профиля с новым ID.

**Эндпоинт:** `POST /rest/profiles/copy`
**Метод:** `POST`
**Аутентификация:** Требуется

### Тело запроса
```json
{
  "sourceId": "идентификатор_исходного_профиля",
  "newName": "Имя копии профиля"
}
```

### Пример запроса (curl)

```bash
curl -X POST http://DEVICE_IP/rest/profiles/copy \
     -H "Authorization: Bearer YOUR_AUTH_TOKEN" \
     -H "Content-Type: application/json" \
     -d '{"sourceId": "0", "newName": "Копия дефолтного"}'
```

### Ответы
*   **`201 Created`**: Копия успешно создана.
    ```json
    {"success": true, "id": "2"}
    ```
*   **`400 Bad Request`**: Неверный JSON или отсутствуют поля `sourceId` или `newName`.
    ```json
    {"error": "Missing or invalid 'sourceId' field in request body"}
    ```
*   **`409 Conflict`**: Не удалось скопировать профиль (например, исходный профиль не найден).
    ```json
    {"error": "Failed to copy profile. source not found "}
    ```

---

## Обновление метаданных профиля

Обновляет метаданные профиля, например, его имя.

**Эндпоинт:** `PUT /rest/profiles/meta`
**Метод:** `PUT`
**Аутентификация:** Требуется

### Тело запроса
```json
{
  "id": "идентификатор_профиля",
  "name": "Новое имя профиля"
}
```

### Пример запроса (curl)

```bash
curl -X PUT http://DEVICE_IP/rest/profiles/meta \
     -H "Authorization: Bearer YOUR_AUTH_TOKEN" \
     -H "Content-Type: application/json" \
     -d '{"id": "2", "name": "Переименованная копия"}'
```

### Ответы
*   **`200 OK`**: Метаданные успешно обновлены.
    ```json
    {"success": "Profile updated"}
    ```
*   **`400 Bad Request`**: Неверный JSON или отсутствуют поля `id` или `name`.
    ```json
    {"error": "Missing or invalid 'id' field in request body"}
    ```
*   **`404 Not Found`**: Профиль не найден или не удалось обновить.
    ```json
    {"error": "Profile not found or failed to update"}
    ```

---

## Установка активного профиля и его применение

Устанавливает указанный профиль как активный и немедленно применяет его настройки к системе.

**Эндпоинт:** `POST /rest/profiles/set-active`
**Метод:** `POST`
**Аутентификация:** Требуется

### Тело запроса
```json
{
  "id": "идентификатор_профиля"
}
```

### Пример запроса (curl)

```bash
curl -X POST http://DEVICE_IP/rest/profiles/set-active \
     -H "Authorization: Bearer YOUR_AUTH_TOKEN" \
     -H "Content-Type: application/json" \
     -d '{"id": "2"}'
```

### Ответы
*   **`200 OK`**: Профиль успешно установлен как активный и применен.
    ```json
    {"success": "Profile set as active and applied"}
    ```
*   **`400 Bad Request`**: Неверный JSON или отсутствует поле `id`.
    ```json
    {"error": "Missing or invalid 'id' field in request body"}
    ```
*   **`404 Not Found`**: Профиль не найден или не удалось установить как активный.
    ```json
    {"error": "Profile not found or failed to set as active"}
    ```
*   **`500 Internal Server Error`**: Не удалось применить профиль.
    ```json
    {"error": "Failed to set active and apply profile"}
    ```

---

## Сохранение текущих настроек в профиль

Сохраняет текущие настройки системы в указанный профиль, перезаписывая его содержимое.

**Эндпоинт:** `POST /rest/profiles/save`
**Метод:** `POST`
**Аутентификация:** Требуется

### Тело запроса
```json
{
  "id": "идентификатор_профиля"
}
```

### Пример запроса (curl)

```bash
curl -X POST http://DEVICE_IP/rest/profiles/save \
     -H "Authorization: Bearer YOUR_AUTH_TOKEN" \
     -H "Content-Type: application/json" \
     -d '{"id": "2"}'
```

### Ответы
*   **`200 OK`**: Текущие настройки успешно сохранены в профиль.
    ```json
    {"success": true, "message": "Current settings saved to profile."}
    ```
*   **`400 Bad Request`**: Неверный JSON или отсутствует поле `id`.
    ```json
    {"error": "Missing or invalid 'id' field in request body"}
    ```
*   **`404 Not Found`**: Профиль с указанным ID не найден или произошла ошибка при сохранении.
    ```json
    {"success": false, "message": "Profile not found or failed to save current settings."}
    ```