# API Безопасности (JWT)

Контроллер защищает REST-маршруты подписанным JWT (HMAC-SHA256, секрет `jwt_secret` из
`/config/securitySettings.json`). Токен передаётся в заголовке `Authorization: Bearer <jwt>`
(для GET-запросов допустим и параметр `?access_token=<jwt>`). Cookie-сессий нет.

Раздел актуален только для secure-сборки (`FT_SECURITY=1`). В обычной сборке (`FT_SECURITY=0`)
маршруты `/rest/signIn`, `/rest/generateToken` и `/rest/refreshToken` **не регистрируются** (404),
предикаты не проверяются, а все запросы считаются запросами администратора.

---

## Модель доступа

Пользователи хранятся в настройках безопасности: имя, пароль и флаг `admin`. Заводские значения —
`admin`/`admin` (администратор) и `guest`/`guest` (гость); их следует сменить при первом запуске.

| Предикат | Что означает | Кто проходит |
|---|---|---|
| `NONE_REQUIRED` | без аутентификации | любой клиент |
| `IS_AUTHENTICATED` | нужен валидный токен | `admin` и `guest` |
| `IS_ADMIN` | нужен валидный токен администратора | только `admin` |

Отказ по предикату — это **`401` без тела**, а не `403`. Поэтому гость, обратившийся к
admin-маршруту, получает ровно тот же ответ, что и клиент без токена.

Типичное распределение:

*   **Только `admin`:** `GET /rest/generateToken`, настройки (`/rest/securitySettings`,
    `/rest/wifiSettings`, `/rest/apSettings`, `/rest/ntpSettings`, `/rest/mqttSettings`),
    `POST /rest/restart`, `POST /rest/factoryReset`, `POST /rest/uploadFirmware`,
    `GET /rest/downloadUpdate`, `GET /rest/scanNetworks`, `GET /rest/listNetworks`,
    `POST /rest/time`, а также `/rest/telemetry`, `/rest/sensor`, `/rest/zones`, `/rest/alarms`
    и WebSocket `/ws/events` (он отдаёт те же admin-данные: `telemetry`, `stage`, `alarm`).
*   **Любой вошедший:** `/rest/systemStatus`, `/rest/wifiStatus`, `/rest/apStatus`,
    `/rest/ntpStatus`, `/rest/mqttStatus`, `/rest/settings`, `/rest/commands`,
    `/rest/subsystem`, конфигурация Telegram/VK, `/rest/oc/*`, `/rest/profiles*`,
    `/rest/files*`, `/rest/logs*`, `/rest/verifyAuthorization`, **`/rest/refreshToken`**.

---

## Выдача токена

### Вход по логину и паролю

**Эндпоинт:** `POST /rest/signIn`
**Метод:** `POST`
**Аутентификация:** не требуется

```bash
curl -X POST http://DEVICE_IP/rest/signIn \
     -H "Content-Type: application/json" \
     -d '{"username":"admin","password":"admin"}'
```

**`200 OK`:**

```json
{"access_token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJ1c2VybmFtZSI6ImFkbWluIiwiYWRtaW4iOnRydWUsImlhdCI6MTc2MzM5MDAwMCwiZXhwIjoxNzYzOTk0ODAwfQ.SIGNATURE"}
```

**`401 Unauthorized`** (без тела): неверная пара логин/пароль.

### Токен по имени пользователя (администратор)

**Эндпоинт:** `GET /rest/generateToken?username=<имя>`
**Метод:** `GET`
**Аутентификация:** `admin`

```bash
curl -X GET "http://DEVICE_IP/rest/generateToken?username=guest" \
     -H "Authorization: Bearer ADMIN_TOKEN"
```

*   **`200 OK`:** `{"token": "<jwt>"}`
*   **`400 Bad Request`:** параметр `username` не передан.
*   **`401 Unauthorized`:** пользователь с таким именем не найден (или вызывающий — не `admin`).

### Структура payload

```json
{"username": "admin", "admin": true, "iat": 1763390000, "exp": 1763994800}
```

| Поле | Тип | Когда присутствует | Назначение |
|---|---|---|---|
| `username` | string | всегда | имя пользователя, обязано совпасть с настройками |
| `admin` | bool | всегда | права пользователя, обязаны совпасть с настройками |
| `iat` | unixtime | только при синхронизированных часах | время выдачи |
| `exp` | unixtime | только при синхронизированных часах | `iat + JWT_TTL_SECONDS` |

`nbf` прошивка не выдаёт, но при проверке учитывает, если он есть в предъявленном токене.

---

## Срок жизни токена

*   `JWT_TTL_SECONDS` — **604800 секунд (7 суток)** по умолчанию.
*   Константа переопределяется **при сборке**:

    ```cpp
    #ifndef JWT_TTL_SECONDS
    #define JWT_TTL_SECONDS (7 * 24 * 60 * 60)
    #endif
    ```

    Например, для проверки на устройстве: `-DJWT_TTL_SECONDS=30`.
*   `exp` и `iat` вычисляются по одному и тому же моменту `time(nullptr)`, поэтому
    `exp - iat` в токене **точно** равно `JWT_TTL_SECONDS`.
*   Отзыва токенов нет: выданный токен живёт до `exp`. Единственный способ инвалидировать все
    токены сразу — сменить `jwt_secret` в `POST /rest/securitySettings` (смена пароля
    пользователя уже выданные токены **не** отзывает).

---

## Почему без NTP не ставится `exp`

До синхронизации часов `time(nullptr)` на ESP32 возвращает эпоху 1970 года. Если выдать токен
с `exp = 1970 + 7 суток`, он родится уже просроченным, и вход сломается до первого NTP-ответа.
Поэтому `iat`/`exp` добавляются **только когда `time(nullptr) > 1700000000`**
(14.11.2023, 22:13:20 UTC). До этого payload содержит ровно `username` и `admin`, как в
прошивках до 0.2.9.2.

Симметрично на проверке: пока часы не синхронизированы, `exp`/`nbf` **не проверяются**. Иначе
после перезагрузки до NTP устройство отвергало бы все токены, включая только что выданные.

Практическое следствие: после перезагрузки без NTP вход работает, а уже истёкшие токены
временно принимаются — до первой синхронизации часов. Это осознанный компромисс: без
достоверного времени проверить срок жизни невозможно.

---

## Проверка токена и обратная совместимость

Проверка идёт в два шага:

1.  Подпись HMAC-SHA256 должна совпасть с текущим `jwt_secret` (иначе payload не разбирается).
2.  Payload сравнивается с пользователем из настроек **по подмножеству**, а не целиком:

    *   `username` должен быть строкой и совпасть с именем пользователя;
    *   `admin` должен быть bool и совпасть с флагом пользователя;
    *   `exp`, если присутствует, требует `now < exp` — только при синхронизированных часах;
    *   `nbf`, если присутствует, требует `now >= nbf` — только при синхронизированных часах.

    Любые другие поля payload игнорируются.

Поэтому токены, выданные **до обновления прошивки** (без `iat`/`exp`), продолжают работать:
отсутствие `exp` не является ошибкой. Никого не разлогинивает и обновление устройства.

| Токен | Часы синхронизированы | Результат |
|---|---|---|
| без `exp` | да | **принят** (обратная совместимость) |
| без `exp` | нет | **принят** |
| `exp` в будущем | да | **принят** |
| `exp` в прошлом | да | **401** |
| `exp` в прошлом | нет | **принят** (время недостоверно) |
| `nbf` в будущем | да | **401** |
| `nbf` в будущем | нет | **принят** |

Проверить токен, не обращаясь к данным, можно через `GET /rest/verifyAuthorization`:
`200` — токен валиден, `401` — нет (оба ответа без тела).

---

## Обновление токена

**Эндпоинт:** `GET /rest/refreshToken`
**Метод:** `GET`
**Аутентификация:** `IS_AUTHENTICATED` (любой вошедший: и `admin`, и `guest`)

Выдаёт новый токен для того же пользователя, что предъявил текущий. Текущий токен при этом
должен быть **валидным**: `wrapRequest` проверяет его до вызова обработчика и отдаёт `401`,
если токен просрочен или подпись неверна.

```bash
curl -X GET http://DEVICE_IP/rest/refreshToken \
     -H "Authorization: Bearer YOUR_AUTH_TOKEN"
```

**`200 OK` (часы синхронизированы):**

```json
{"token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJ1c2VybmFtZSI6ImFkbWluIiwiYWRtaW4iOnRydWUsImlhdCI6MTc2MzM5MDEwMCwiZXhwIjoxNzYzOTk0OTAwfQ.SIGNATURE", "exp": 1763994900}
```

*   `token` — новый JWT с новыми `iat`/`exp`.
*   `exp` — ровно тот же `exp`, что внутри токена (оба значения считаются от одного мгновения).

**`200 OK` (часы не синхронизированы):**

```json
{"token": "<jwt без iat/exp>", "exp": null}
```

*   **`401 Unauthorized`** (без тела): токена нет, подпись неверна, пользователь удалён или
    токен просрочен.

> Важно: обновить **уже истёкший** токен нельзя — refresh сам требует валидный токен.
> `refreshToken` продлевает сессию, пока она ещё жива; после истечения нужен повторный вход.

---

## Как проверить на устройстве

Ниже — проверка сокращённого срока жизни (`JWT_TTL_SECONDS=30`). `platformio.ini` править не
нужно: PlatformIO добавляет переменную окружения `PLATFORMIO_BUILD_FLAGS` к `build_flags`
выбранного окружения.

```powershell
$env:PATH='C:\msys64\ucrt64\bin;'+$env:PATH
$env:PYTHONUTF8='1'
$env:PLATFORMIO_BUILD_FLAGS='-DJWT_TTL_SECONDS=30'
pio run -e esp32-s3-devkitc-1-8m-secure -t upload
```

Дальше — с компьютера в той же сети (`DEVICE_IP` — адрес контроллера).

**1. Дождаться синхронизации часов.** `GET /rest/ntpStatus` (нужен любой токен, поэтому проще
сначала выполнить вход):

```bash
curl -s -H "Authorization: Bearer $TOKEN" http://DEVICE_IP/rest/ntpStatus
```

`utc_time` должен показывать текущую дату, а не `1970-01-01T00:00:xxZ`. Если NTP выключен, время
можно выставить вручную (только при выключенном SNTP):

```bash
curl -X POST http://DEVICE_IP/rest/time \
     -H "Authorization: Bearer $ADMIN_TOKEN" \
     -H "Content-Type: application/json" \
     -d '{"local_time":"2026-11-17T20:00:00"}'
```

**2. Войти и разобрать payload.**

```bash
TOKEN=$(curl -s -X POST http://DEVICE_IP/rest/signIn \
  -H "Content-Type: application/json" \
  -d '{"username":"admin","password":"admin"}' \
  | python -c "import sys,json;print(json.load(sys.stdin)['access_token'])")

python - "$TOKEN" <<'PY'
import base64, json, sys
payload = sys.argv[1].split('.')[1]
payload += '=' * (-len(payload) % 4)
claims = json.loads(base64.urlsafe_b64decode(payload))
print(claims)
print('exp - iat =', claims['exp'] - claims['iat'])
PY
```

Ожидается `exp - iat = 30` (при `JWT_TTL_SECONDS=30`) и `exp` примерно на 30 секунд больше
текущего времени контроллера.

**3. Обновить токен, пока он ещё валиден.**

```bash
curl -s -H "Authorization: Bearer $TOKEN" http://DEVICE_IP/rest/refreshToken
```

В ответе — новый `token` и `exp`, совпадающий с `exp` внутри нового токена; после декодирования
нового токена снова `exp - iat = 30`.

**4. Убедиться, что просроченный токен отвергается.** Подождать больше 30 секунд:

```bash
curl -s -o /dev/null -w '%{http_code}\n' \
  -H "Authorization: Bearer $TOKEN" http://DEVICE_IP/rest/verifyAuthorization   # 401
curl -s -o /dev/null -w '%{http_code}\n' \
  -H "Authorization: Bearer $TOKEN" http://DEVICE_IP/rest/refreshToken          # 401
```

Повторный `POST /rest/signIn` снова выдаёт рабочий токен.

**5. Проверить старый токен (без `exp`).** Сохранить токен, выданный прошивкой **до** обновления
(в нём нет `iat`/`exp`), обновить прошивку и выполнить:

```bash
curl -s -o /dev/null -w '%{http_code}\n' \
  -H "Authorization: Bearer $OLD_TOKEN" http://DEVICE_IP/rest/verifyAuthorization   # 200
```

**6. Проверить поведение без NTP.** Выключить NTP (полный объект из `GET /rest/ntpSettings`,
поле `enabled: false`), перезагрузить устройство и убедиться, что `utc_time` = 1970 год, затем:

*   `signIn` → в payload только `username`/`admin`, полей `iat`/`exp` нет;
*   `refreshToken` → `{"token": "...", "exp": null}`;
*   старый токен с `exp` в прошлом всё ещё принимается (`verifyAuthorization` → `200`).

После включения NTP и синхронизации такой токен начнёт отвергаться с `401`.

---

## Что делает веб-интерфейс

Фронтенд хранит токен в localStorage (`user.bearer_token`) и шлёт его как `Bearer`. Если
`apiFetch` получает `401` (кроме самих `/rest/signIn` и `/rest/refreshToken`), он один раз
вызывает `GET /rest/refreshToken` с текущим токеном, сохраняет новый токен в стор тем же
способом, что и при входе, и повторяет исходный запрос. Если обновление не удалось, возвращается
исходная ошибка `401` — без рекурсии и бесконечных повторов. Отдельного экрана разлогина нет:
при неудаче интерфейс показывает ошибку, как и раньше, а пользователь входит заново.
