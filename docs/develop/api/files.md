# API Файлов

Этот раздел описывает API для управления файлами на устройстве.

---

## Получение дерева файлов

Возвращает структуру файлов и каталогов в виде дерева.

**Эндпоинт:** `GET /rest/files`
**Метод:** `GET`
**Аутентификация:** Требуется

### Пример запроса (curl)

```bash
curl -X GET http://DEVICE_IP/rest/files \
     -H "Authorization: Bearer YOUR_AUTH_TOKEN"
```

### Ответы
*   **`200 OK`**: Успешный ответ со структурой файлов.
    ```json
    [
      {
        "name": "config",
        "type": "directory",
        "children": [
          {
            "name": "system.json",
            "type": "file",
            "size": 1024
          }
        ]
      },
      {
        "name": "boot.log",
        "type": "file",
        "size": 512
      }
    ]
    ```

---

## Скачивание файла

Позволяет скачать содержимое указанного файла.

**Эндпоинт:** `GET /rest/files/*`
**Метод:** `GET`
**Аутентификация:** Требуется

### Параметры URL
*   `*` (string) - Полный путь к файлу, который нужно скачать (например, `/rest/files/config/system.json`).

### Пример запроса (curl)

```bash
curl -X GET http://DEVICE_IP/rest/files/config/system.json \
     -H "Authorization: Bearer YOUR_AUTH_TOKEN" \
     --output system.json
```

### Ответы
*   **`200 OK`**: В теле ответа находится содержимое файла.
*   **`404 Not Found`**: Файл не найден.

---

## Удаление файла

Удаляет указанный файл с устройства.

**Эндпоинт:** `DELETE /rest/files/*`
**Метод:** `DELETE`
**Аутентификация:** Требуется

### Параметры URL
*   `*` (string) - Полный путь к файлу для удаления.

### Пример запроса (curl)

```bash
curl -X DELETE http://DEVICE_IP/rest/files/logs/old.log \
     -H "Authorization: Bearer YOUR_AUTH_TOKEN"
```

### Ответы
*   **`204 No Content`**: Файл успешно удален.
*   **`500 Internal Server Error`**: Не удалось удалить файл.

---

## Копирование файла

Копирует файл из одного места в другое.

**Эндпоинт:** `POST /rest/files/copy`
**Метод:** `POST`
**Аутентификация:** Требуется

### Пример запроса (curl)

```bash
curl -X POST http://DEVICE_IP/rest/files/copy \
     -H "Authorization: Bearer YOUR_AUTH_TOKEN" \
     -H "Content-Type: application/json" \
     -d '{
           "source": "/path/to/source/file.txt",
           "destination": "/path/to/destination/file.txt"
         }'
```

### Тело запроса
```json
{
  "source": "/path/to/source/file.txt",
  "destination": "/path/to/destination/file.txt"
}
```

### Ответы
*   **`201 Created`**: Файл успешно скопирован.
*   **`400 Bad Request`**: Отсутствуют необходимые поля в теле запроса.
*   **`404 Not Found`**: Исходный файл не найден.
*   **`500 Internal Server Error`**: Не удалось создать файл назначения.
