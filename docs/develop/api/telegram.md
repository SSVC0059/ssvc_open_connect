# API Telegram Бота

Этот раздел описывает API для управления настройками Telegram бота.

---

## Обновление настроек Telegram бота

Обновляет конфигурацию Telegram бота, например, токен доступа или ID чата.

**Эндпоинт:** `PUT /rest/telegram/config`

**Метод:** `PUT`

**Аутентификация:** Требуется

### Пример запроса (curl)

```bash
curl -X PUT http://DEVICE_IP/rest/telegram/config \
     -H "Authorization: Bearer YOUR_AUTH_TOKEN" \
     -H "Content-Type: application/json" \
     -d '{
           "botToken": "YOUR_NEW_BOT_TOKEN",
           "chatId": "YOUR_CHAT_ID"
         }'
```

### Тело запроса

JSON-объект с обновленными параметрами конфигурации.

**Пример тела запроса:**

```json
{
  "botToken": "YOUR_NEW_BOT_TOKEN",
  "chatId": "YOUR_CHAT_ID"
}
```

### Ответы

*   **`200 OK`**: Настройки успешно обновлены.
*   **`400 Bad Request`**: Ошибка в формате запроса.
*   **`401 Unauthorized`**: Ошибка аутентификации.
*   **`500 Internal Server Error`**: Внутренняя ошибка сервера.

---

## Получение настроек Telegram бота

Возвращает текущую конфигурацию Telegram бота.

**Эндпоинт:** `GET /rest/telegram/config`

**Метод:** `GET`

**Аутентификация:** Требуется

### Пример запроса (curl)

```bash
curl -X GET http://DEVICE_IP/rest/telegram/config \
     -H "Authorization: Bearer YOUR_AUTH_TOKEN"
```

### Ответы

*   **`200 OK`**: Успешный ответ с текущими настройками.

    **Пример ответа:**

    ```json
    {
      "botToken": "YOUR_CURRENT_BOT_TOKEN",
      "chatId": "YOUR_CHAT_ID"
    }
    ```
*   **`401 Unauthorized`**: Ошибка аутентификации.
*   **`500 Internal Server Error`**: Внутренняя ошибка сервера.

---
