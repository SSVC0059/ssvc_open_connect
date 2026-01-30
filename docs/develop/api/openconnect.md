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
      "version": "1.0.0",
      "buildDate": "2023-10-27",
      "wifiStatus": "connected",
      "ipAddress": "192.168.1.100"
    }
    ```
*   **`401 Unauthorized`**: Ошибка аутентификации.
*   **`500 Internal Server Error`**: Внутренняя ошибка сервера.

---
