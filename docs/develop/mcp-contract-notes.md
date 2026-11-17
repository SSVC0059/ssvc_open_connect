# REST-контракт OpenConnect: заметки разведки по исходному коду

Рабочий справочник для разработчиков MCP-сервера и мок-устройства. Снят **с кода**, а не с RFC.
Каждое утверждение сопровождается ссылкой `файл:строка`; всё, что не удалось подтвердить чтением
кода, помечено «не подтверждено» и вынесено в § N.

**Срез, по которому снимался контракт**

| Что | Значение | Источник |
|---|---|---|
| Версия прошивки OpenConnect | `0.2.9.1` | `platformio.ini:29` |
| Целевая версия UART API | `1.10` | `lib/ssvcOpenConnect/core/SsvcApiCapabilities/SsvcApiCapabilities.h:29` |
| Минимальная версия UART API | `1.7` | `.../SsvcApiCapabilities.h:33` |
| Сборки | `-secure` = `FT_SECURITY=1`, обычная = `FT_SECURITY=0` | `platformio.ini:120,131,152,163,200,211` |
| Фичи | `FT_MQTT/NTP/UPLOAD_FIRMWARE/DOWNLOAD_FIRMWARE/ANALYTICS/COREDUMP/TELEGRAM_BOT/VK_BOT=1`, `FT_SLEEP/FT_BATTERY/FT_ETHERNET=0` | `features.ini:2-12` |
| `ENABLE_CORS` | закомментирован → CORS-заголовков нет | `platformio.ini:36` |
| `PINOUT_USE_GPIO` | `1` по умолчанию; `kincony-kc868-a6` → `0` | `lib/ssvcOpenConnect/core/SsvcConnector.h:59-61`, `platformio.ini:176` |

Два следствия, которые надо держать в голове постоянно:

1. Все `HttpEndpoint<T>`-сервисы регистрируют **только GET и POST** — PUT/PATCH/DELETE для них не
   существуют (`lib/framework/HttpEndpoint.h:62-114`). Метод, которого нет, даёт не 405, а
   **404 text/plain «Not Found»** из catch-all (`WebHandlers.cpp:313` в `.pio/libdeps/.../ESPAsyncWebServer/src`).
2. Провал любого предиката (`IS_ADMIN` в том числе) отдаётся как **401 без тела**, не 403
   (`lib/framework/SecuritySettingsService.cpp:129-155`).

---

## A. Таблица маршрутов

Предикат — то, что реально передано в `wrapRequest`/`wrapCallback` или отсутствует вовсе.
В сборке `FT_SECURITY=0` все предикаты игнорируются: `wrapRequest`/`wrapCallback` возвращают
обработчик как есть, `authenticateRequest` всегда отдаёт admin-пользователя
(`SecuritySettingsService.cpp:191-214`), а `/rest/signIn` и `/rest/verifyAuthorization`
**не регистрируются** (весь `AuthenticationService.cpp` под `#if FT_ENABLED(FT_SECURITY)`, строка 18)
→ на не-secure сборке это 404.

### A.1 Фреймворк (ESP32-SvelteKit)

| № | Метод | Путь | Предикат | Назначение |
|---|---|---|---|---|
| 1 | POST | `/rest/signIn` | без аутентификации (`NONE_REQUIRED`) | выдать JWT (`AuthenticationService.cpp:44`) |
| 2 | GET | `/rest/verifyAuthorization` | обработчик без обёртки, токен проверяется внутри | 200/401 без тела (`AuthenticationService.cpp:48-51`) |
| 3 | GET | `/rest/features` | **нет предиката вообще** (публичный) | карта фич (`FeaturesService.cpp:23-29`) |
| 4 | GET | `/rest/systemStatus` | `IS_AUTHENTICATED` | heap/uptime/чип (`SystemStatus.cpp:123-126`) |
| 5 | POST | `/rest/restart` | `IS_ADMIN` | перезагрузка (`RestartService.cpp:24-27`) |
| 6 | POST | `/rest/factoryReset` | `IS_ADMIN` | сброс (`FactoryResetService.cpp:29-33`) |
| 7 | GET | `/rest/coreDump` | `IS_AUTHENTICATED` | дамп паники (`CoreDump.cpp:31-34`) |
| 8 | GET | `/rest/generateToken?username=` | `IS_ADMIN` | JWT по имени (`SecuritySettingsService.cpp:31-34,157-178`) |
| 9 | GET | `/rest/securitySettings` | `IS_ADMIN` (по умолчанию в `HttpEndpoint`) | пользователи/jwtSecret |
| 10 | POST | `/rest/securitySettings` | `IS_ADMIN` | запись тех же настроек |
| 11 | GET | `/rest/wifiSettings` | `IS_ADMIN` | настройки STA |
| 12 | POST | `/rest/wifiSettings` | `IS_ADMIN` | запись |
| 13 | GET | `/rest/apSettings` | `IS_ADMIN` | настройки AP |
| 14 | POST | `/rest/apSettings` | `IS_ADMIN` | запись |
| 15 | GET | `/rest/ntpSettings` | `IS_ADMIN` | NTP |
| 16 | POST | `/rest/ntpSettings` | `IS_ADMIN` | запись |
| 17 | GET | `/rest/mqttSettings` | `IS_ADMIN` | MQTT |
| 18 | POST | `/rest/mqttSettings` | `IS_ADMIN` | запись |
| 19 | GET | `/rest/wifiStatus` | `IS_AUTHENTICATED` | статус Wi-Fi (`WiFiStatus.cpp:25-28`) |
| 20 | GET | `/rest/apStatus` | `IS_AUTHENTICATED` | статус AP (`APStatus.cpp:26-29`) |
| 21 | GET | `/rest/ntpStatus` | `IS_AUTHENTICATED` | статус NTP (`NTPStatus.cpp:24-27`) |
| 22 | GET | `/rest/mqttStatus` | `IS_AUTHENTICATED` | статус MQTT (`MqttStatus.cpp:27-30`) |
| 23 | GET | `/rest/scanNetworks` | `IS_ADMIN` | скан Wi-Fi (`WiFiScanner.cpp:25-28`) |
| 24 | GET | `/rest/listNetworks` | `IS_ADMIN` | список сохранённых сетей (`WiFiScanner.cpp:32-35`) |
| 25 | POST | `/rest/time` | `IS_ADMIN` | установка времени (`NTPSettingsService.cpp:49-53`) |
| 26 | POST | `/rest/uploadFirmware` | `IS_ADMIN` | OTA-загрузка (`UploadFirmwareService.cpp:63,116`) |
| 27 | GET | `/rest/downloadUpdate` | `IS_ADMIN` | OTA из GitHub (`DownloadFirmwareService.cpp:202-206`) |
| — | — | `/rest/sleep` | — | **не зарегистрирован** (`FT_SLEEP=0`, `features.ini:7`; регистрация — `SleepService.cpp:32,42`) |
| — | — | `/rest/ethernetSettings`/`Status` | — | **не зарегистрирован** (`FT_ETHERNET=0`) |
| — | — | `/rest/analytics*` | — | **нет маршрута**: `AnalyticsService` только header, `.cpp` отсутствует |

### A.2 Прикладные маршруты OpenConnect

| № | Метод | Путь | Предикат | Назначение |
|---|---|---|---|---|
| 28 | GET | `/rest/telemetry` | **`IS_ADMIN`** (умолчание `HttpEndpoint`) | `{telemetry,status,lastUpdate}` (`TelemetryService.cpp:52-61`, `HttpEndpoint.h:36`) |
| 29 | GET | `/rest/sensor` | **`IS_ADMIN`** | датчики по зонам (`SensorDataService.cpp:67-76`) |
| 30 | POST | `/rest/sensor` | **`IS_ADMIN`** | no-op: `update()` всегда `UNCHANGED` (`SensorDataService.cpp:71`, формат — `HttpEndpoint.h:78-114`) |
| 31 | GET | `/rest/zones` | **`IS_ADMIN`** | зоны (`SensorConfigService.h:66-74`) |
| 32 | POST | `/rest/zones` | **`IS_ADMIN`** | карта зон |
| 33 | GET | `/rest/alarms` | **`IS_ADMIN`** | пороги (`AlarmThresholdService.h:89-97`) |
| 34 | POST | `/rest/alarms` | **`IS_ADMIN`** | карта порогов |
| 35 | GET | `/rest/settings` | `IS_AUTHENTICATED` | `{ssvcSettings:{...}}` (`HandlerRegistrator.cpp:63-68`) |
| 36 | PUT | `/rest/settings` | `IS_AUTHENTICATED` | плоское частичное обновление (`HandlerRegistrator.cpp:56-61`) |
| 37 | POST | `/rest/commands` | `IS_AUTHENTICATED` | команда SSVC (`HandlerRegistrator.cpp:73-78`) |
| 38 | PUT | `/rest/sensors/zone?address=&zone=` | `IS_AUTHENTICATED` | одна привязка (`HandlerRegistrator.cpp:83-88`) |
| 39 | GET | `/rest/subsystem` | `IS_AUTHENTICATED` | карта подсистем (`HandlerRegistrator.cpp:140-145`) |
| 40 | PUT | `/rest/subsystem` | `IS_AUTHENTICATED` | `{name:bool}` (`HandlerRegistrator.cpp:147-154`) |
| 41 | GET | `/rest/telegram/config` | `IS_AUTHENTICATED` | `{enable,subsystem_name,status,token,chat_id}` (`HandlerRegistrator.cpp:103-108`) |
| 42 | PUT | `/rest/telegram/config` | `IS_AUTHENTICATED` | `{token?,chat_id?}` (`HandlerRegistrator.cpp:95-100`) |
| 43 | GET | `/rest/vk/config` | `IS_AUTHENTICATED` (только `FT_VK_BOT`) | настройки VK (`HandlerRegistrator.cpp:122-127`) |
| 44 | PUT | `/rest/vk/config` | `IS_AUTHENTICATED` (только `FT_VK_BOT`) | запись (`HandlerRegistrator.cpp:115-120`) |
| 45 | POST | `/rest/vk/peers` | `IS_AUTHENTICATED` (только `FT_VK_BOT`) | список диалогов VK (`HandlerRegistrator.cpp:129-134`) |
| 46 | GET | `/rest/oc/info` | `IS_AUTHENTICATED` | версии + реестр возможностей (`HandlerRegistrator.cpp:159-164`) |
| 47 | GET | `/rest/oc/hardware-config` | `IS_AUTHENTICATED` | конфиг+пробы (`HandlerRegistrator.cpp:166-171`) |
| 48 | PUT | `/rest/oc/hardware-config` | `IS_AUTHENTICATED` | частичная запись (`HandlerRegistrator.cpp:173-178`) |
| 49 | GET | `/rest/oc/hardware-faults` | `IS_AUTHENTICATED` | активные сбои (`HandlerRegistrator.cpp:180-185`) |
| 50 | GET | `/rest/oc/xkc25/status` | `IS_AUTHENTICATED` | слоты XKC-25 (`HandlerRegistrator.cpp:187-192`) |
| 51 | GET | `/rest/oc/xkc25/verify-status` | `IS_AUTHENTICATED` | результат `verify_return` (`HandlerRegistrator.cpp:196-201`) |
| 52 | POST | `/rest/oc/xkc25/calibrate` | `IS_AUTHENTICATED` | шаг калибровки (`HandlerRegistrator.cpp:203-208`) |
| 53 | GET | `/rest/oc/relay/capabilities` | `IS_AUTHENTICATED` | возможности PCF8574 (`HandlerRegistrator.cpp:210-215`) |
| 54 | GET | `/rest/oc/relay/metadata` | `IS_AUTHENTICATED` | метаданные линий реле (`HandlerRegistrator.cpp:216-221`) |
| 55 | GET | `/rest/oc/relay/state` | `IS_AUTHENTICATED` | теневые байты (`HandlerRegistrator.cpp:223-228`) |
| 56 | POST | `/rest/oc/relay/override` | `IS_AUTHENTICATED` | ручное управление (`HandlerRegistrator.cpp:230-235`) |
| 57 | GET | `/rest/profiles` | `IS_AUTHENTICATED` | список метаданных профилей (`HandlerRegistrator.cpp:257-261`) |
| 58 | POST | `/rest/profiles` | `IS_AUTHENTICATED` | создание (`HandlerRegistrator.cpp:294-297`) |
| 59 | GET | `/rest/profiles/active` | `IS_AUTHENTICATED` | `{id}` или 404 (`HandlerRegistrator.cpp:245-248`) |
| 60 | GET | `/rest/profiles/content?id=` | `IS_AUTHENTICATED` | полный профиль (`HandlerRegistrator.cpp:251-254`) |
| 61 | POST | `/rest/profiles/content` | `IS_AUTHENTICATED` | обновление содержимого (`HandlerRegistrator.cpp:270-273`) |
| 62 | POST | `/rest/profiles/set-active` | `IS_AUTHENTICATED` | сделать активным и применить (`HandlerRegistrator.cpp:276-279`) |
| 63 | POST | `/rest/profiles/copy` | `IS_AUTHENTICATED` | копия (`HandlerRegistrator.cpp:282-285`) |
| 64 | POST | `/rest/profiles/save` | `IS_AUTHENTICATED` | сохранить текущие настройки (`HandlerRegistrator.cpp:288-291`) |
| 65 | PUT | `/rest/profiles/meta` | `IS_AUTHENTICATED` | переименование (`HandlerRegistrator.cpp:300-303`) |
| 66 | DELETE | `/rest/profiles/delete` | `IS_AUTHENTICATED` | удаление, **тело JSON в DELETE** (`HandlerRegistrator.cpp:306-309`) |
| 67 | GET | `/rest/files` | `IS_AUTHENTICATED` | дерево LittleFS (`FileHandler.cpp:24`) |
| 68 | GET | `/rest/files/*` | `IS_AUTHENTICATED` | скачивание файла — **перекрыт маршрутом 67**, см. § 8.1 (`FileHandler.cpp:48`) |
| 69 | DELETE | `/rest/files/*` | `IS_AUTHENTICATED` | удаление файла (`FileHandler.cpp:68`) |
| 70 | POST | `/rest/files/copy` | `IS_AUTHENTICATED` | `{source,destination}` (`FileHandler.cpp:106`) |
| 71 | GET | `/rest/logs` | `IS_AUTHENTICATED` | сессия журналов, exact-матчер (`HandlerRegistrator.cpp:347`) |
| 72 | GET | `/rest/logs/{id}` | `IS_AUTHENTICATED` | 202/200 text/csv (`HandlerRegistrator.cpp:432`) |
| 73 | DELETE | `/rest/logs` | `IS_AUTHENTICATED` | отмена (`HandlerRegistrator.cpp:537`) |

### A.3 Устаревшие (legacy) маршруты, о которых в RFC нет ничего

`SsvcOpenConnect::begin()` конструирует `TelegramSettingsService` и `VkSettingsService`, и те
регистрируют у себя **второй** `HttpEndpoint` на путях `/api/...` с ключами, отличными от
`/rest/telegram/config`:

| № | Метод | Путь | Предикат | Ключи | Источник |
|---|---|---|---|---|---|
| 74 | GET | `/api/telegram/config` | `IS_ADMIN` | `botToken`, `chatId` | `TelegramSettingsService.cpp:46-55` |
| 75 | POST | `/api/telegram/config` | `IS_ADMIN` | `botToken`, `chatId` | там же |
| 76 | GET | `/api/vk/config` | `IS_ADMIN` | `access_token`,`api_version`,`group_id`,`peer_id`,`*_enabled` | `VkSettingsService.cpp:106-115` |
| 77 | POST | `/api/vk/config` | `IS_ADMIN` | то же | там же |

**Итого: 77 пар «метод+путь»** (27 фреймворковых, 46 прикладных `/rest/*`, 4 legacy `/api/*`).

### A.4 Маршруты, которых НЕТ в прошивке (объявлены в клиентском API, но вызовов нет)

Обе функции ниже **не вызываются ни из одного компонента** (проверено grep’ом по `interface/src`,
подробнее — § M). Строки таблицы описывают, что произойдёт, если их всё-таки позвать.

| Вызов | Метод | Где объявлен | Что происходит на устройстве |
|---|---|---|---|
| `/rest/sensors/thresholds` | **PUT** | `interface/src/lib/api/ssvcApi.ts:138-146` (функция `updateSensorThresholds`) | ни один обработчик не совпадает → catch-all **404 text/plain «Not Found»** (§ D.1). Пороги меняются только через `GET`/`POST /rest/alarms` |
| `/rest/profiles/import` | **POST** | `interface/src/lib/api/Profiles.ts:77-80` (функция `importProfile`) | путь совпадает с `POST /rest/profiles` (BackwardCompatible-матчер, `HandlerRegistrator.cpp:294`), тело приходит без `name` → **400 `{"error": "Missing or invalid 'name' field"}`** (`ProfileHandler.cpp:74-77`) |
| `/rest/files/copy` как GET | GET | в клиенте не вызывается | попадает в маршрут 67 (дерево файлов) |
| `/rest/sleep` | POST | `interface/src/routes/statusbar.svelte:18`, `SystemStatusTab.svelte:128`, `SystemStatus.svelte:104` | 404 (фича выключена) |

---

## B. Форма запроса и ответа по семействам

### B.1 `/rest/telemetry` — GET (admin), POST (admin, no-op)

GET: `TelemetryService::readStateToJson` десериализует сохранённую строку и вставляет её **в корень**
ответа (`TelemetryService.cpp:18-38`), плюс `lastUpdate`. Внешняя форма:

```
{ "telemetry": { … }, "status": { … }, "lastUpdate": <unsigned long> }
```

`lastUpdate` = `millis()` на момент изменения снимка (`TelemetryService.cpp:157`) — **это не epoch**,
а миллисекунды с загрузки устройства (переполнение ~49.7 сут).

Ключи `telemetry` формируются `RectificationProcess::writeTelemetryTo` (`RectificationProcess.cpp:755-910`),
и почти все они **условные** — это важно для zod-схем, всё необязательно:

| Ключ | Тип | Условие появления | Строка |
|---|---|---|---|
| `type` | string (стадия) | всегда | `:758` |
| `pid` | int | `pid != 0` | `:760-763` |
| `start_time`, `end_time` | string | длина > 0 | `:765-772` |
| `tank_mmhg` | float | `!= 0` | `:773-776` |
| `tp1_target`, `tp2_target` | float | `!= 0` | `:778-785` |
| `countdown`, `release`, `time` | string | непустые | `:787-800` |
| `open` | float | `> 0` | `:802-805` |
| `period` | uint | `> 0` | `:806-809` |
| `valveOpen` | int (0…10000) | `period > 0` | `:811-815` |
| `volumeSpeed` | int | `period > 0` и известна `valve_bw` стадии | `:816-821`, `:1002-1026` |
| `v1`, `v2`, `v3` | uint | `> 0` | `:822-835` |
| `stop` | bool | всегда | `:837` |
| `stops` | uint8 | всегда | `:838` |
| `preempt_cnt` | uint | `type == "hearts"` **или** `> 0` | `:839-842` |
| `common` | object | всегда | `:844-879` |
| `hysteresis` | float | всегда | `:882` |
| `volume` | object | всегда | `:886-893` |
| `event`, `info` | string | `event != EMPTY` | `:897-903` |
| `alc` | float | `!= 0` | `:905-908` |

`common` = `{ mmhg (float, только если truthy — `:845`), tp1 (float), tp2 (float), relay (bool),
signal (bool), cfg_chgd (bool, только если true — `:867`), tts (int, только `> 0` — `:871`),
heatingOn (bool), overclockingOn (bool), relative_pressure (float, только если
`initial_pressure_set` — `:877-879`) }`.

`volume` = `{heads:int, hearts:int}` + либо `tails` (ветка «tails»), либо `late_heads` (иначе) —
`RectificationProcess.cpp:886-893`.

`status` (`getStatus`, `RectificationProcess.cpp:912-931` + `TelemetryService.cpp:131-132`):

```
{ "stage": string, "status": string, "startTime": string, "endTime": string,
  "stages": { "<stage>": "<state>", … }, "uartConnectionError": bool }
```

`stages` собирается только из уже встречавшихся этапов (`:920-929`), ключ `empty` отбрасывается.
`uartConnectionError` приходит из `SsvcConnector::uartCommunicationError` и добавляется **после**
`getStatus`, то есть в `/rest/telemetry`, но не в другие потребители `getStatus`.

POST: `HttpEndpoint` примет объект, `TelemetryState::update` вернёт `UNCHANGED`
(`TelemetryService.h:47-50`), тело невалидного типа → 400 без тела (`HttpEndpoint.h:82-86`),
ответ — текущее состояние.

### B.2 `/rest/sensor` — GET/POST (admin)

`SensorDataState::read` (`SensorDataService.cpp:24-56`):

```
{ "<zone>": { "<ADDRESS>": { "v": <float>, "u": "<unit>", "type": "<type>" } } }
```

- `zone` — `SensorZoneHelper::toString`: `unknown|inlet_water|outlet_water|act|tank|outdoor|
  receiver_tank_1|receiver_tank_2|receiver_tank_3|deleted` (`components/Zone/SensorZone.h:48-64`).
  Зона `deleted` в ответ не попадает: датчики удаляются из `SensorManager`.
- `ADDRESS` — **16 hex-символов в верхнем регистре**, `sprintf("%02X")` по 8 байтам
  (`SensorManager.cpp:165-171`).
- `u`/`type` берутся у объекта датчика; если датчик не найден — `""` и `"undefined"`
  (`SensorDataService.cpp:52-53`).
- Фактические значения: DS18B20 → `u:"°C"`, `type:"thermal"` (`DS18B20Sensor.cpp:87-92`);
  BMP581 → `u:"мм рт. ст."`, `type:"pressure"` (`BMP581Sensor.cpp:248-253`);
  XKC-25 → `u:""`, `type:"level"` (`Xkc25Sensor.cpp:308-313`).

POST — no-op (см. таблицу A.2), ответ — тот же объект.

### B.3 `/rest/zones` — GET/POST (admin)

`SensorConfigState::read`: `{"zones": { "<ADDRESS>": <int zoneId> }}` (`SensorConfigService.cpp:20-26`).
POST принимает `{"zones": {"<ADDR>": <int>}}` **или** строковое имя зоны
(`SensorConfigService.cpp:48-64`). Логика удаления: всё, чего нет во входящей карте, **удаляется**,
кроме раннего бута, когда `SensorManager` пуст (`:76-100`). Ответ — полное состояние после записи
(формат `HttpEndpoint.h:101-107`), не `{success}`.

### B.4 `/rest/alarms` — GET/POST (admin)

`AlarmThresholdsState::read`: `{"thresholds": {"<ADDR>": {"enabled":bool,"min":float,
"dangerous":float,"critical":float}}}` (`AlarmThresholdService.h:44-57`, `AlarmThresholdService.cpp:40-57`).

POST: то же тело; отсутствие `thresholds`/не объект → `UNCHANGED`, но HTTP всё равно 200 с текущим
состоянием (`AlarmThresholdService.cpp:62-72`). Датчики, отсутствующие во входящем объекте,
удаляются, кроме раннего бута (`:150-160`).

**Ловушка дефолтов:** при сериализации дефолты `min=0, dangerous=50, critical=80`
(`AlarmThresholdService.h:54-59`), при разборе — `min=0, dangerous=80, critical=100` (`:62-69`).
Объект `{"enabled": true}` на входе даст в состоянии 0/80/100, а не 0/50/80.

### B.5 `/rest/settings`

**GET** (auth) — `{"ssvcSettings": {...}}` (`SettingsHandler.cpp:22-29`); вложенность обязательна.
`fillSettings` (`SsvcSettings.cpp:58-167`) пишет:

- пары `[speed, period]`: `heads`, `hearts`, `late_heads` (только если `period > -1 || open > -1`,
  `:75-79`), `tails` (аналогично, `:82-88`), `parallel`/`parallel_v1` (только если обе компоненты
  не нули, `:141-151`);
- `valve_bw` — массив из трёх int (`:96-99`);
- `parallel_v3` — массив `[[float,float,int], …]`, только если непустой (`:153-161`);
- `tails_temp` — только если `> 0` (`:90-93`);
- скаляры: `hyst`, `decrement`, `formula` (int 0/1/2), `tank_mmhg`, `heads_timer`,
  `late_heads_timer`, `hearts_timer`, `start_delay`, `hearts_finish_temp`, `formula_start_temp`
  (`:102-111`);
- **bool**: `sound`, `pressure`, `relay_inverted`, `relay_autostart`, `autoresume`, `auto_mode`
  (`:114-119`), `hearts_pause`, `tp_filter`, `signal_tp1_control`, `signal_inverted`,
  `tp1_control_start`, `stab_limit_finish` (`:122-132`);
- float/int: `hearts_temp_shift`, `predec`, `extended_heads`, `tp2_shift`, `tp1_control_temp`,
  `stab_limit_time` (`:121-132`);
- string: `backlight` (`:134`);
- release-поля всегда: `release_timer`, `release_speed`, `heads_final` (`:137-139`).

**PUT** (auth) — тело **плоский объект без обёртки** (`SettingsHandler.cpp:57-81`).

- неизвестный ключ → попадает в `ignored`, запрос не отменяется (`SettingsHandler.cpp:64-72`);
- известный ключ с неверным типом/диапазоном → `errors[key]="Invalid value"` и **400 целиком,
  ничего не применяется** (батч отбрасывается, `:50,74-88`);
- иначе `builder.applySettings()` → SET-пакеты в очередь; отчёт:
  - 200 `{"success":true,"message":"Settings updated successfully","accepted":N, "ignored"?:[…],
    "skipped"?:[…]}` (`:141-148`);
  - 503 `{"success":false,"error":"Settings queue is unavailable",
    "message":"Часть настроек не попала в очередь контроллера","partial":true,"accepted":N,
    "failedChunks":N, "failed":[…], "ignored"?:…, "skipped"?:…}` (`:116-136`);
  - 400 `{"<key>":"Invalid value", …}` (`:83-88`);
  - 400 `{"error":"Body must be a JSON object"}` (`:38-41`).

`skipped` — параметры SET, отброшенные по версии API устройства
(`SsvcSettings.h:403-404`, `SsvcApiCapabilities.h:314-332`).

### B.6 `/rest/commands` — POST (auth)

Тело: `{"commands": "<name>", "parameters": "<строка>"}`; `parameters` необязателен и читается только
если это строка (`CommandHandler.cpp:70-72`).

Ответы — см. § D.3. **200 — это «принято в очередь»**, не «выполнено» (`CommandHandler.cpp:74-77`).

### B.7 `/rest/sensors/zone` — PUT (auth), параметры **только в query**

Обработчик зарегистрирован через `wrapRequest`, JSON-тело не разбирается
(`HandlerRegistrator.cpp:83-88`). `hasParam`/`getParam` по умолчанию смотрят **только query-параметры**
(`WebRequest.cpp:1156-1172` в `.pio/libdeps/.../ESPAsyncWebServer/src` — сравнение `p.isPost() == post`
при `post=false`). Значит запрос обязан быть `PUT /rest/sensors/zone?address=…&zone=…`.

Успех 200: `{"status":"success","message":"Zone updated successfully (Persistence initiated)",
"address":"<addr>","zone":"<name>"}` (`SensorHandler.cpp:64-85`).
См. § D.4 про 400/404 и § 8.4 про недостижимую ветку валидации зоны.

### B.8 `/rest/subsystem` — GET/PUT (auth)

GET: плоская карта `{"<subsystem>": bool}` из `SubsystemManager::getSubsystemsStatus()`
(`SubsystemHandler.cpp:22-36`) — порядок ключей не определён (`std::unordered_map`, `:26`).
Фактический набор имён зависит от конфигурации железа (`SsvcOpenConnect.cpp:203-236`):
`settings`, `thermal`, `i2c_bus`, `atm_sensor` (если `pressureSensorEnabled`), `lcd1602_display`
(если `lcd1602Enabled`), `xkc25_sensor` (если включён хоть один датчик), `telegram_bot`, `vk_bot`.

PUT: `{"<subsystem>": bool}`; ответ 200 `{"success": bool, "errors": [{"subsystem","message"}, …]}`
(`SubsystemHandler.cpp:38-73`). Неизвестное имя → `success:false` + запись в `errors`.
Не-bool значение (`as<bool>()`) трактуется как `false` → подсистема **выключится**.
Флаг — целевое состояние, а неhealth: `enable()` может вернуться раньше фактической готовности.

### B.9 `/rest/oc/info` — GET (auth)

`OpenConnectHandler::getInfo` (`OpenConnectHandler.cpp:100-136`):

```
{ "ssvc": { "version": string, "api": string, "api_code": int, "mode": "tails"|"late_heads" },
  "oc":   { "version": string, "is_support_api": bool, "is_support_release": bool,
            "api_min": string, "api_target": string,
            "api_compatibility": "unknown"|"too_old"|"supported",
            "api_features": [ { "name": string, "min_api": string, "available": bool } ] } }
```

- `mode` = `"tails"`, если версия SSVC начинается с `2.2`, иначе `"late_heads"`
  (`OpenConnectHandler.cpp:111`, `SsvcSettings.cpp:220-228`).
- `is_support_release` = `!supportTails` (`SsvcSettings.cpp:218`), то есть на прошивке «tails» —
  `false`.
- `api_compatibility` при неизвестной версии — `"unknown"`, при `api_code < 107` — `"too_old"`
  (`SsvcApiCapabilities.h:132-138`).
- `api_features` — ровно 8 записей в порядке enum (`SsvcApiCapabilities.h:42-52,73-82`), см. § C.4.
- Пока `api_code == 0`, `available` считается по минимальной версии прошивки: `true` только у
  `base`, `formula`, `tank_mmhg_act` (`SsvcApiCapabilities.h:160-166`).

### B.10 `/rest/oc/hardware-config` — GET/PUT (auth)

Тело ответа = `OpenConnectHardwareConfig::read` + пробы + `capabilities` + `needsReboot`:

```
{ "schemaVersion": int, "pressureSensorEnabled": bool, "bmp581I2cAddress": int,
  "userRelayPcfEnabled": bool, "relayPcf8574Addresses": [int…], "pendingReboot": bool,
  "rtcEnabled": bool, "ds3231I2cAddress": int, "oledEnabled": bool, "lcd1602Enabled": bool,
  "lcd1602I2cAddress": int,
  "xkc25Sensors": [ { "enabled","pinSource","gpio","pcfAddress","pcfBit","activeLow",
                      "zoneIndex","calibrated","levelWithoutLiquid","levelWithLiquid",
                      "calibratedAt","sharedAcrossTanks" } ],
  "xkc25AutoPause": bool, "xkc25AbortRectification": bool, "xkc25TriggerAction": int,
  "xkc25SharedTankIndex": int, "xkc25StartGuardMode": int,
  "bmp581ProbeOk"?: bool, "ds3231ProbeOk"?: bool, "lcd1602ProbeOk"?: bool,
  "relayPcf8574ProbeOk"?: [bool…], "relayHardwareOk"?: bool,
  "capabilities": { "pinoutUseGpio": bool, "relayOptionsAvailable": bool },
  "needsReboot": bool }
```

Источники: `OpenConnectHardwareConfig.cpp:141-183` (ключи состояния), `OpenConnectHandler.cpp:350-403`
(пробы и обёртка — проба присутствует только если соответствующая функция включена), `:317-326`
(capabilities), `:329-345` (пробы реле только при `PINOUT_USE_GPIO=0`).

PUT: частичный объект тех же ключей; ошибка валидации → 400
`{"success":false,"error":"validation_failed","message":"Настройки не прошли проверку: адреса, пины
или привязка зон"}` (`OpenConnectHandler.cpp:421-426`), не объект → 400
`{"success":false,"error":"body_must_be_object","message":"Тело запроса должно быть JSON-объектом"}`
(`:406-413`). Успех → 200 с той же структурой, что GET (`:428-476`). Признак «нужна перезагрузка» —
`pendingReboot`/`needsReboot`.

### B.11 `/rest/oc/hardware-faults` — GET (auth)

```
{ "faults": [ {"code": string, "codeLabel": string, "address": int, "role": string} ],
  "count": int, "i2cBusActive": bool, "i2cScan": [int…] }
```

`OpenConnectHandler.cpp:71-98`; `address` — 7-битный I²C-адрес (0 для `ssvc_uart`). `i2cScan` —
адреса, ответившие на шине (`:91-94`).

### B.12 `/rest/oc/xkc25/*`

GET `status` (`OpenConnectHandler.cpp:596-638`):

```
{ "sensors": [ {"index","enabled","pinSource","gpio","pcfAddress","pcfBit","pinLabel",
                "zoneIndex","zone","zoneLabel","sharedAcrossTanks","activeLow","calibrated",
                "calibratedAt","levelWithoutLiquid","levelWithLiquid","live","rawLevel","filled"} ],
  "triggerAction": int, "sharedTankIndex": int, "startGuardMode": int,
  "blockStart": bool, "blockReasons": [string…], "warnings": [string…] }
```

`index` — **0-based слот конфигурации** (`:507`), `zone` — строка зоны (`receiver_tank_1…3`),
`rawLevel` = `-1`, если живой подсистемы нет (`:524`).

POST `calibrate` (`OpenConnectHandler.cpp:640-702`): тело `{"index": 0..2, "step": "...",
"timeoutMs"?: 0..5000}`.

- `step` ∈ `without_liquid` | `with_liquid` | `verify_return` (`:673-697`);
- для `verify_return` — 202 `{"status":"pending","index":N,"code":"ok",
  "message":"Проверка возврата запущена"}` (`:582-592`), результат — через `verify-status`;
- иначе ответ — тело шага: `{ok, completed, code, rawLevel, message, checklist[], sensor{…}}`
  (`buildXkc25CalibrationBody`, `:551-579`), HTTP-код по `code` (`:529-548`).

GET `verify-status` (`OpenConnectHandler.cpp:704-733`):
`{"status":"none"}` | `{"status":"pending","index":N}` |
`{"status":"done","index":N,"ok":bool,"completed":bool,"code":string,"rawLevel":int,"message":string}`;
при неактивной подсистеме — `{"status":"none","code":"sensor_not_available","message":"XKC-25
subsystem is not active"}`.

### B.13 `/rest/oc/relay/*`

`capabilities` — `{"supported":false}` на GPIO-сборке (`OpenConnectHandler.cpp:139-140`) либо
`{"supported":true,"pcf8574":{"linesPerChip":int,"relayPcf8574Addresses":[int…],"relayChipCount":int,
"relayLineCount":int,"i2cAddress"?:int,"alarmBitDangerous":int,"alarmBitCritical":int,
"alarmGlobalBitDangerous":int,"alarmGlobalBitCritical":int,"firstChipAlarmReservedBits":int}}`
(`RelayRuleEngine.cpp:325-343`).

`metadata` (`OpenConnectHandler.cpp:152-248`):

```
{ "supported": true, "relayChipCount": int, "relayLineCount": int,
  "relays": [ {"index": 1-based int, "label": "Реле N", "editable": bool,
               "reason"?: string, "targetBit": 0-based int} ],
  "sensorNames": [string…],
  "hardwareFaultCodes": [ {"value": int, "key": string, "label": string} ],
  "hardwareRoles": [string…],
  "rectificationStages": [ {"value": string, "label": string} ],
  "ssvcFields": [ {"key": string, "label": string, "valueType": "bool"|"int"|"float"} ] }
```

Ключевая деталь: `index` **1-based** (`displayIndex = 1`, `:172-177`), `targetBit` — **0-based**
глобальный бит (`:187`). `editable:false` только у бита 0 и 1 **первого** чипа
(`SSVC_RELAY_PCF8574_BIT_DANGEROUS=0`, `..._BIT_CRITICAL=1`, `SsvcConnector.h:103-108`;
проверка — `OpenConnectHandler.cpp:179-186`), причина — строка
`"Системное реле AlarmManager (только просмотр)"`.

`state` — `{"supported":true,"userShadow":uint8,"userShadows":[uint8…]}` (`OpenConnectHandler.cpp:250-266`).

`override` — тело `{"bit": int, "enable"?: bool=true, "energized"?: bool=false}`
(`OpenConnectHandler.cpp:277-300`), ответ `{"ok":true}`. На GPIO-сборке — 400
`{"error":"PCF8574 relay path not enabled in this build"}` (`:269-271`).

### B.14 `/rest/profiles*`

- GET `/rest/profiles` → **массив** `[{"id","name","createdAt","isApplied"}, …]`
  (`ProfileService.cpp:604-619`), **не объект**.
- GET `/rest/profiles/active` → `{"id":"…"}` или 404 `{"error": "No active profile found"}`
  (`ProfileHandler.cpp:16-27`).
- GET `/rest/profiles/content?id=` → сырое содержимое файла профиля: произвольные ключи
  (`ProfileService.cpp:584-602`). Набор ключей задают наблюдатели профиля:
  `ssvcSettings` (`SsvcSettings.h:46`), `alarmThresholds` (`AlarmThresholdService.h:123`),
  `telegram` (`TelegramSettingsService.cpp:102-104`), `vk` (`VkSettingsService.cpp:128-130`).
  Пустой `id` → 400 `{"error": "Field 'id' cannot be empty"}`; нет параметра → 400
  `{"error": "Missing 'id' parameter in request"}` (`ProfileHandler.cpp:30-40`).
- POST `/rest/profiles` `{name, content?}` → **201** `{"success":true,"id":"…"}`
  (`ProfileHandler.cpp:96-106`); без `name` → 400 `{"error": "Missing or invalid 'name' field"}`;
  `content` не объект → 400 `{"error": "Invalid 'content' field, must be a JSON object"}`.
  Есть эвристика: тело `{id, content}` без `name` перенаправляется в обновление содержимого
  (`:56-72`).
- POST `/rest/profiles/copy` `{sourceId,newName}` → 201 `{"success":true,"id":…}` либо
  **409** `{"error": "Failed to copy profile. source not found "}` (`:134-178`).
- POST `/rest/profiles/content` `{id, content}` → 200
  `{"success": "Profile content updated"}` / 404 `{"error": "Profile not found or failed to update
  content"}` (`:266-296`).
- POST `/rest/profiles/set-active` `{id}` → 200 `{"success": "Profile set as active and applied"}`
  / 500 `{"error": "Failed to set active and apply profile"}` (`:210-237`).
- POST `/rest/profiles/save` `{id}` → 200 `{"success":true,"message":"Current settings saved to
  profile."}` / 404 `{"success":false,"message":"Profile not found or failed to save current
  settings."}` (`:239-264`).
- PUT `/rest/profiles/meta` `{id,name}` → 200 `{"success": "Profile updated"}` / 404
  `{"error": "Profile not found or failed to update"}` (`:180-208`).
- DELETE `/rest/profiles/delete` `{id}` → 200 `{"success": "Profile deleted"}` / 404
  `{"error": "Profile not found or failed to delete"}` (`:109-132`). Тело в DELETE — да,
  `wrapCallback` парсит JSON у любого метода.

### B.15 `/rest/files*`

- GET `/rest/files` → массив дерева: элемент `{"name":string,"type":"directory","children":[…]}` или
  `{"name":string,"type":"file","size":int}` (`FileHandler.cpp:109-128`).
- GET `/rest/files/<path>` — задумано как `application/octet-stream` с `request->send(*_fs, path, …)`
  (`:26-47`), но фактически перекрыт (см. § 8.1). Пустой путь — 400 `text/plain`
  `"File path cannot be empty"`, отсутствует — 404 `text/plain` `"File not found or is a directory"`.
- POST `/rest/files/copy` `{source,destination}` → **201 без тела** (`:104`); нет полей → 400
  `text/plain "Missing 'source' or 'destination' fields"`; нет источника → 404 `text/plain
  "Source file not found"`; не удалось создать — 500 `text/plain`.
- DELETE `/rest/files/<path>` → **204 без тела** (`:60`), ошибка удаления — 500 `text/plain
  "Failed to delete file"`.

### B.16 `/rest/logs*`

GET `/rest/logs` (`HandlerRegistrator.cpp:347-430`): параметры распознаются **по факту наличия**
(`hasParam`), значение не читается — `?statusOnly=0` работает как `?statusOnly=1`
(`:373-374`). Ответ:

```
{ "status": "idle"|"receiving"|"list"|"completed"|"error",
  "total": int, "received": int,
  "error"?: string,
  "files"?: [string…]   // только при status == "list"
}
```

GET `/rest/logs/{id}` (`:432-532`): при готовом файле — 200 `text/csv`,
`Content-Disposition: attachment; filename="<id>.CSV"` (`:513-517`); иначе 202 с тем же снимком
`{status,total,received,error?}` (`:521-530`).

DELETE `/rest/logs` (`:537-549`): `transfer.cancel()` + `{"status": "<statusName>"}`, всегда 200.

### B.17 Мессенджеры

GET `/rest/telegram/config` (`TelegramBotHandler.cpp:9-33`):
`{"enable":bool,"subsystem_name":"telegram_bot","status":"ok"}` + `{"token","chat_id"}` если
подсистема включена; **если выключена — HTTP 423** с `{"enable":false,
"subsystem_name":"telegram_bot","status":"telegram_bot disable"}`.
PUT (`:35-79`): `{"token"?: string, "chat_id"?: string|number}` → 200 `{"result":true}`; ничего
валидного → 400 `{"result":false}`.

GET `/rest/vk/config` (`VkBotHandler.cpp:126-163`): при выключенной подсистеме — **423**
`{"enable":false,"subsystem_name":"vk_bot","status":"vk_bot disabled"}`; иначе поля
`access_token,api_version,group_id,peer_id,live_enabled,alerts_enabled,summary_enabled,
wall_post_enabled` (`:150-160`). PUT → 200 `{"result":true}` (`:165-192`).
POST `/rest/vk/peers` → 200 `{"peers":[{"peer_id","type"?,"title"?}],"ok":true}` либо 502
`{"peers":[],"ok":false,"error":"<причина>"}` (`:194-238`).

### B.18 `/rest/features`, `/rest/systemStatus`

`/rest/features` (`FeaturesService.cpp:76-152`): плоская карта bool —
`security, mqtt, ntp, upload_firmware, download_firmware, sleep, battery, analytics, coredump,
event_use_json, ethernet, vk_bot` + `firmware_version`, `firmware_name`, `firmware_built_target`
+ пользовательские фичи. **Отдаётся без токена.**

`/rest/systemStatus` (`SystemStatus.cpp:131-167`): `esp_platform, firmware_version, max_alloc_heap,
free_psram?, used_psram?, psram_size?, cpu_freq_mhz, cpu_type, cpu_rev, cpu_cores, free_heap,
used_heap, total_heap, min_free_heap, sketch_size, free_sketch_space, sdk_version, arduino_version,
flash_chip_size, flash_chip_speed, fs_total, fs_used, core_temp, cpu_reset_reason, uptime`.

---

## C. Точные перечисления

### C.1 `COMMAND_MAP` (`SsvcCommandsQueue.cpp:111-154`)

| Имя | Параметры | Что делает |
|---|---|---|
| `at` | — | UART `AT` (`:112`) |
| `next` | — | `NEXT` |
| `pause` | — | `PAUSE` |
| `stop` | — | `STOP` |
| `start` | — | `START` (проходит предохранитель XKC-25, `CommandHandler.cpp:48-66`) |
| `resume` | — | `RESUME` |
| `version` | — | `VERSION` |
| `get_settings` | — | `GET_SETTINGS` |
| `settings` | — | псевдоним `get_settings` (`:120`) |
| `emergency_stop` | — | аварийный STOP вне FIFO (`:121`, `SsvcCommandsQueue.h:101-104`) |
| `status` | произвольная строка | вывод на дисплей SSVC, ≤15 символов, перекодируется в CP1251 (`SsvcCommandsQueue.cpp:955-958`) |
| `set` | строка вида `heads=[1.0,2]` | сырой `SET` (`:957`) |
| `get_log` | пусто → список, число → файл | `GET_LOG`/`GET_LOG <n>`, требует API 1.10 (`:124-153`) |

Причины отказа очереди, читаемые REST-слоем (`CommandHandler.cpp:78-88`):
`feature_unsupported` → 409, `queue_full` → 503, иначе 503 `"Command not accepted"`.

### C.2 Стадии, состояния, события

`RectificationTypes.h:15-50` (enum) и `RectificationProcess.cpp` (строки):

| enum `Stage` | строка в API | источник |
|---|---|---|
| EMPTY | `empty` | `:119-120` |
| WAITING | `waiting` | `:121-122` |
| TP1_WAITING | `tp1_waiting` | `:123-124` |
| DELAYED_START | `delayed_start` | `:125-126` |
| HEADS | `heads` | `:127-128` |
| LATE_HEADS | `late_heads` | `:129-130` |
| HEARTS | `hearts` | `:131-132` |
| TAILS | `tails` | `:133-134` |
| SETTINGS | **`unknown`** (нет `case`!) | `:117-139` — при этом в `metadata.rectificationStages` значение `settings` присутствует (`OpenConnectHandler.cpp:232-235`) |
| PARSE_ERROR | `error` | `:135-136` |
| — | `unknown` (default) | `:137-138` |

| enum `ProcessState` | строка |
|---|---|
| IDLE | `idle` |
| RUNNING | `running` |
| PAUSED | `paused` |
| FINISHED | `finished` |
| SKIPPED | `skipped` |
| **STOPPED** | **`unknown`** — нет `case` (`RectificationProcess.cpp:166-180`) |
| **PROCESS_ERROR** | **`unknown`** — нет `case` |

| enum `Event` | строка (`RectificationProcess.cpp:186-208`) |
|---|---|
| HEADS_FINISHED | `heads_finished` |
| HEARTS_FINISHED | `hearts_finished` |
| TAILS_FINISHED | `tails_finished` |
| DS_ERROR | `ds_error` |
| DS_ERROR_STOP | `ds_error_stop` |
| STABILIZATION_LIMIT | `stabilization_limit` |
| REMOTE_STOP | `remote_stop` |
| MANUALLY_CLOSED | `manually_closed` |
| MANUALLY_OPENED | `manually_opened` |
| прочее | `unknown_event` |

### C.3 `PARAM_HANDLERS` — полный список ключей PUT `/rest/settings`

Определение: `SettingsSetterHandlers.cpp:20-233`; объявление `SettingsSetterHandlers.h:407`.
Ровно **30 ключей**:

`hyst`, `heads`, `hearts`, `late_heads`, `tails`, `decrement`, `formula`, `tank_mmhg`,
`heads_timer`, `late_heads_timer`, `hearts_timer`, `tails_temp`, `start_delay`,
`hearts_finish_temp`, `formula_start_temp`, `valve_bw`, `release_speed`, `release_timer`,
`heads_final`, `parallel`, `parallel_v1`, `parallel_v3`, `s_temp`, `s_hyst`, `s_speed`,
`s_decrement`, `s_timer`, `predec`, `extended_heads`, `tank_mmhg_act`.

Типы/валидация обработчиков:

| Ключ | Ожидаемый JSON | Валидация/диапазон | Строка |
|---|---|---|---|
| `hyst` | float | `SingleFloatHandler` (без диапазона в обработчике) | `:24-27` |
| `heads`, `hearts`, `late_heads`, `tails`, `parallel`, `parallel_v1`, `s_speed` | `[float, int]` | ровно 2 элемента | `:30-50,147-156,188-194`, `SettingsSetterHandlers.h:43-68` |
| `decrement`, `s_decrement` | int | `UnsignedCharHandler` (0…255) | `:53-57,196-201` |
| `formula` | int | 0…2 | `:60-63` |
| `tank_mmhg`, `tails_temp`, `hearts_finish_temp`, `formula_start_temp`, `release_speed`, `heads_final`, `s_temp`, `s_hyst` | float | без диапазона в обработчике | `:66-70,94-97,107-111,114-118,128-131,141-144,174-178,180-186` |
| `heads_timer`, `late_heads_timer`, `start_delay`, `release_timer`, `s_timer` | uint | `UnsignedIntHandler` | `:73-77,80-84,100-104,134-138,203-208` |
| `hearts_timer` | int | `UnsignedCharHandler` (0…255), хотя в API максимум 30 | `:87-91` |
| `valve_bw` | `[int,int,int]` | ровно 3 элемента | `:121-125` |
| `parallel_v3` | `[[float,float,int], …]` | каждый элемент — массив из 3 (float,float,int) | `:160-171`, `SettingsSetterHandlers.h:379-398` |
| `predec` | int | 0…13 и `isValidPredec` → только 0/7/13 | `:211-215` |
| `extended_heads` | int | 0…1 | `:218-221` |
| `tank_mmhg_act` | float | без диапазона | `:224-227` |

**Версии появления ключей** (таблица `SsvcApiCapabilities.h:73-82`): `base`-набор (включая
`formula` 0/1, `tank_mmhg_act`, `formula_start_temp`, `release_*`, `heads_final`) — с **1.7**;
`extended_heads` — с **1.8**; `predec` и `formula=2` — с **1.9**; команда `get_log` — с **1.10**.
Перечень «базовых» SET-параметров продублирован списком
`BASE_SET_PARAMETERS` (`SsvcApiCapabilities.h:179-185`).

**Ключи только для чтения** (есть в GET, но отсутствуют в `PARAM_HANDLERS`, поэтому PUT их молча
игнорирует): `sound`, `pressure`, `relay_inverted`, `relay_autostart`, `autoresume`, `auto_mode`,
`hearts_temp_shift`, `hearts_pause`, `tp2_shift`, `tp_filter`, `signal_tp1_control`,
`signal_inverted`, `tp1_control_temp`, `tp1_control_start`, `stab_limit_time`, `stab_limit_finish`,
`backlight` (сравните `SsvcSettings.cpp:114-139` и `SettingsSetterHandlers.cpp:20-229`).

### C.4 Реестр возможностей `/rest/oc/info` → `oc.api_features`

| `name` | `min_api` | enum | Строка |
|---|---|---|---|
| `base` | `1.7` | `Base` | `SsvcApiCapabilities.h:74` |
| `formula` | `1.7` | `FormulaModes` | `:75` |
| `predec` | `1.9` | `Predec` | `:76` |
| `extended_heads` | `1.8` | `ExtendedHeads` | `:77` |
| `tank_mmhg_act` | `1.7` | `TankMmhgAct` | `:78` |
| `formula_auto92` | `1.9` | `FormulaAuto92Plus` | `:79` |
| `hearts_preempt_cnt` | `1.8` | `HeartsPreemptCnt` | `:80` |
| `get_log` | `1.10` | `GetLog` | `:81` |

Порядок в массиве — порядок `enum class SsvcFeature` (`:42-52`), то есть **не** по возрастанию версии.
Сравнение версий — целым кодом `major*100 + minor` (`:105-118` в `SsvcUartApiSpec.h`).

### C.5 Аппаратные сбои

`HardwareFaultCode` → строки (`OpenConnectHandler.cpp:39-59`):

| enum | `code` | `codeLabel` |
|---|---|---|
| NONE | `none` | «Нет сбоя» |
| I2C_NACK | `i2c_nack` | «I2C: нет подтверждения (NACK)» |
| I2C_BUS_DOWN | `i2c_bus_down` | «I2C: шина недоступна» |
| DEVICE_NOT_PRESENT | `device_not_present` | «Устройство не обнаружено» |
| UART_LINK_LOST | `uart_link_lost` | «Потеря связи с SSVC (UART)» |
| прочее | `unknown` | «Неизвестный код» |

Роли устройств (`role`) в `faults[]`: `i2c_bus` (`BMP581Sensor.cpp:114`, `I2CBusSubsystem.h:98`),
`ssvc_uart` (`SsvcConnector.cpp:289,377`), `bmp581` (`BMP581Sensor.h:23`), `lcd1602`
(`Lcd1602Subsystem.cpp:243,319`), `pcf8574_in` для входного расширителя
(`DigitalInputPin.cpp:12`), `pcf8574_port0…port7` для реле (`RelayPortCoordinator.cpp:13-22`).

Адреса по умолчанию: реле PCF8574 `0x24` (`SsvcConnector.h:97-99`), входной PCF8574 `0x22`
(`:92-94`), BMP581 `0x47` (`OpenConnectHardwareConfig.h:60`), DS3231 `0x68` (`:71`),
LCD1602 `0x27` (`:75`). Допустимый диапазон 7-битных адресов — `0x08…0x77`
(`OpenConnectHardwareConfig.h:107`).

### C.6 Реле: метаданные

- `linesPerChip` = `SSVC_RELAY_PCF8574_LINES_PER_CHIP` = 8 (`SsvcConnector.h:110-112`).
- Максимум чипов — 8 (`OpenConnectHardwareConfig.h:145`), максимум линий — 64.
- Зарезервированные биты: **локальные 0 и 1 первого чипа** (`SsvcConnector.h:103-108`), что
  соответствует глобальным битам 0 и 1 (`RelayPortCoordinator.cpp:119-127`).
- Инверсия: «включено» = бит сброшен в 0 (`RelayPortCoordinator.cpp:139-145`, `buildAlarmByte`)
  — то есть `userShadow`/`userShadows` — **активный низкий уровень**.
- `index` реле — 1-based, `targetBit` — 0-based (§ B.13).

`ssvcFields[].key` (для правил реле) — `RelayRuleEngine.cpp:34-63`:
`formula`(int), `auto_mode`(bool), `sound`(bool), `pressure`(bool), `relay_inverted`(bool),
`relay_autostart`(bool), `autoresume`(bool), `hearts_pause`(bool), `signal_tp1_control`(bool),
`signal_inverted`(bool), `tp1_control_start`(bool), `stab_limit_finish`(bool), `decrement`(int),
`heads_timer`(int), `late_heads_timer`(int), `hearts_timer`(int), `start_delay`(int),
`stab_limit_time`(int), `release_timer`(int), `tank_mmhg`(float), `hyst`(float),
`hearts_finish_temp`(float), `formula_start_temp`(float), `tails_temp`(float),
`release_speed`(float), `hearts_temp_shift`(float), `tp2_shift`(float), `tp1_control_temp`(float).

### C.7 Калибровка XKC-25

Коды (`Xkc25Subsystem.h:20-32`): `ok`, `sensor_not_available`, `sensor_not_configured`,
`unstable_signal`, `no_external_pull`, `not_reacted`, `not_calibrated`, `persist_failed`,
`verify_failed`, `verify_in_progress`.

Код → HTTP (`OpenConnectHandler.cpp:529-548`): `ok`/`verify_failed` → 200; `sensor_not_available`/
`sensor_not_configured` → 404; `persist_failed` → 500; остальные → 409.

Состояния `verify-status`: `none` | `pending` | `done` (`OpenConnectHandler.cpp:708,714,717`).

Итог шага: `{"ok","completed","code","rawLevel","message","checklist","sensor"}`;
`rawLevel = -1` — чтение не выполнялось (`Xkc25Subsystem.h:38`).
Таймаут `verify_return` по умолчанию и максимум — 5000 мс (`OpenConnectHandler.cpp:485-487,678-680`).

### C.8 Журналы

Внутренние состояния (`SsvcLogProtocol.h:67-78`): `IDLE, QUEUED_LIST, WAITING_LIST, LIST_RECEIVED,
QUEUED_FILE, WAITING_START, RECEIVING, COMPLETED, ERROR, DRAINING`.
Наружу (`HandlerRegistrator.cpp:319-335`): `idle` (IDLE), `list` (LIST_RECEIVED), `completed`
(COMPLETED), `error` (ERROR), **`receiving`** — для всех прочих, включая `DRAINING`.

Коды ошибок в поле `error`: причина отмены/таймаута (`SsvcLogProtocol.h:533`: `_error = reason`,
по умолчанию `"aborted"`), плюс коды обработчика `api_version_unsupported` (409, с полями
`feature`, `required_api`, `device_api` — `HandlerRegistrator.cpp:355-365`),
`log_transfer_busy` (409), `queue_full` (503), `log_heap_low` (503),
`invalid_process_id` (400).

Ограничения: один журнал ≤ 64 КБ (`SsvcLogProtocol.h:42`), таймаут тишины 10 с (`:60`), готовый
результат удерживается 5 минут (`HandlerRegistrator.cpp:339`).

### C.9 `/rest/features` и `/rest/subsystem`

Ключи `/rest/features` — см. § B.18 (`FeaturesService.cpp:76-152`). Для данной сборки фактические
значения: `security` = `false` у обычной/`true` у `-secure`, `mqtt:true, ntp:true,
upload_firmware:true, download_firmware:true, sleep:false, battery:false, analytics:true,
coredump:true, event_use_json:false, ethernet:false, vk_bot:true`.

Имена подсистем (`/rest/subsystem`): `settings` (`SettingsSubsystem.h:24`), `thermal`
(`ThermalSubsystem.h:28`), `i2c_bus` (`I2CBusSubsystem.h:33`), `atm_sensor`
(`AtmosphericSubsystem.h:11`), `lcd1602_display` (`Lcd1602Subsystem.h:14`), `xkc25_sensor`
(`Xkc25Subsystem.h:95`), `telegram_bot` (`TelegramBotSubsystem.h:13`), `vk_bot`
(`VkBotSubsystem.h:10`). Условность набора — `SsvcOpenConnect.cpp:203-236`.

### C.10 Датчики: зоны, единицы, типы

Зоны (`components/Zone/SensorZone.h:28-40`), числовые коды: `unknown=0, inlet_water=1,
outlet_water=2, act=3, tank=4, outdoor=5, deleted=6, receiver_tank_1=7, receiver_tank_2=8,
receiver_tank_3=9`.

| Датчик | `getUnit()` | `getType()` | `MeasuredValueType` |
|---|---|---|---|
| DS18B20 | `°C` | `thermal` | TEMPERATURE (`DS18B20Sensor.cpp:76-92`) |
| BMP581 | `мм рт. ст.` | `pressure` | PRESSURE (`BMP581Sensor.cpp:248-253`) |
| XKC-25 | `""` | `level` | LEVEL (`Xkc25Sensor.cpp:79-80,308-313`) |

---

## D. Коды HTTP и тела ошибок

### D.1 Общая механика

- Провал предиката → **401 без тела** (`SecuritySettingsService.cpp:129-141,143-155`). Для
  `IS_ADMIN` это тоже 401, не 403.
- Нет подходящего обработчика (неизвестный путь **или** неподдерживаемый метод) → **404
  `text/plain` «Not Found»** (`WebHandlers.cpp:313`).
- Невалидный/отсутствующий JSON в `wrapCallback` → 400 без тела
  (`AsyncJson.h/.cpp` внутри библиотеки; в прикладных обработчиках обычно есть своя проверка).
- В secure-сборке `Content-Type` ответа задаётся явно в каждом `send`; статусы без тела
  (401/404/204) тела не имеют.

### D.2 `/rest/settings`

| Код | Тело |
|---|---|
| 200 | `{"success":true,"message":"Settings updated successfully","accepted":N[,"ignored":[..]][,"skipped":[..]]}` (`SettingsHandler.cpp:141-148`) |
| 400 | `{"<key>":"Invalid value"}` — по одному полю на каждый невалидный ключ (`:83-88`) |
| 400 | `{"error":"Body must be a JSON object"}` (`:38-41`) |
| 503 | `{"success":false,"error":"Settings queue is unavailable","message":"Часть настроек не попала в очередь контроллера","partial":true,"accepted":N,"failedChunks":N,"failed":[..][,"ignored"][,"skipped"]}` (`:116-136`) |
| 401 | пусто (нет токена/не тот пользователь) |

GET: 200 `{"ssvcSettings":{…}}`, 401 пусто.

### D.3 `/rest/commands`

| Код | Content-Type | Тело | Условие |
|---|---|---|---|
| 200 | text/plain | `Command accepted` | принято, предупреждений нет (`CommandHandler.cpp:109`) |
| 200 | application/json | `{"status":"accepted","command":"start","warnings":["…"]}` | принято, есть предупреждения XKC-25 (`:96-107`) |
| 400 | text/plain | `Invalid JSON format` | тело не объект (`:26-29`) |
| 400 | text/plain | `Missing or invalid 'commands' field` | нет строкового `commands` (`:31-34`) |
| 409 | application/json | `{"status":"rejected","command":"start","blockReasons":["…"]}` | предохранитель XKC-25 в режиме block (`:49-65`) |
| 409 | text/plain | `Command unsupported by device API` | причина отказа `feature_unsupported` (`:82-84`) |
| 501 | text/plain | `Command not implemented` | имени нет в `COMMAND_MAP` (`:40-44`) |
| 503 | text/plain | `Command queue is full` | `queue_full` (`:85`) |
| 503 | text/plain | `Command not accepted` | прочие отказы очереди (`:87`) |
| 500 | text/plain | `Command execution failed` | исключение (`:110-113`) |

### D.4 `/rest/sensors/zone`

| Код | Тело |
|---|---|
| 200 | `{"status":"success","message":"Zone updated successfully (Persistence initiated)","address":"…","zone":"…"}` (`SensorHandler.cpp:66-70,84-85`) |
| 400 | `{"status":"error","message":"Missing required parameters: 'address' and 'zone'"}` (`:34-37`) |
| 400 | `{"status":"error","message":"Invalid 1-Wire address format. Must be 16 hex characters.","address":"…"}` (`:43-46`) |
| 400 | `{"status":"error","message":"Invalid zone value. Valid values: unknown, inlet_water, outlet_water, act","received_zone":"…"}` — **ветка недостижима**, см. § 8.4 (`:49-55`) |
| 404 | `{"status":"error","message":"Failed to update zone. Sensor not found or invalid parameters","address":"…","zone":"…"}` (`:71-81`) |
| 500 | `{"message":"Zone Service not initialized"}` (`:57-60`) |

### D.5 `/rest/oc/*`

| Код | Тело | Где |
|---|---|---|
| 200 | `{"supported":false}` | relay GET на GPIO-сборке (`OpenConnectHandler.cpp:140,154,252`) |
| 400 | `{"error":"PCF8574 relay path not enabled in this build"}` | relay override на GPIO-сборке (`:270`) |
| 400 | `{"error":"Invalid JSON"}` | calibrate/override не объект (`:274,642`) |
| 400 | `{"error":"Missing bit"}` / `{"error":"Invalid bit"}` / `{"error":"bit out of range"}` | override (`:279,284,290`) |
| 400 | `{"error":"Missing index"}` / `{"error":"index out of range"}` / `{"error":"Missing step"}` / `{"error":"Unknown step"}` | calibrate (`:648,653,659,696`) |
| 400 | `{"success":false,"error":"body_must_be_object","message":"…"}` | hardware-config PUT (`:409-411`) |
| 400 | `{"success":false,"error":"validation_failed","message":"…"}` | hardware-config PUT (`:422-424`) |
| 403 | `{"error":"Relay is reserved for AlarmManager"}` | override зарезервированного бита (`:294`) |
| 404 | `{"error":"XKC-25 subsystem is not active","code":"sensor_not_available"}` | calibrate (`:666-667`) |
| 409 | тело шага с `code` ∈ `unstable_signal`/`no_external_pull`/`not_reacted`/`not_calibrated` | calibrate (`:684`, `:529-548`) |
| 500 | тело шага с `code":"persist_failed"` | calibrate (`:542-544`) |
| 202 | `{"status":"pending","index":N,"code":"ok","message":"Проверка возврата запущена"}` | verify_return (`:691-693`) |

### D.6 `/rest/profiles*` — см. § B.14 (400/404/409/500/201).

### D.7 `/rest/files*`

| Код | Тело |
|---|---|
| 200 | JSON-дерево |
| 400 | `text/plain` `File path cannot be empty` (`FileHandler.cpp:31-33,55-58`) |
| 404 | `text/plain` `File not found or is a directory` (`:37-40`), `Source file not found` (`:84-86`) |
| 201 | пусто (copy, `:104`) |
| 204 | пусто (delete, `:60`) |
| 500 | `text/plain` `Could not open root directory` / `Failed to delete file` / `Could not create destination file` (`:12-14,62,90-92`) |

### D.8 `/rest/logs*`

| Код | Тело |
|---|---|
| 200 | снимок `{status,total,received[,error][,files]}` (GET) / `{status}` (DELETE) |
| 200 | `text/csv` + `Content-Disposition: attachment; filename="N.CSV"` (`HandlerRegistrator.cpp:513-517`) |
| 202 | `{status,total,received[,error]}` (`:521-530`) |
| 400 | `{"error":"invalid_process_id"}` (`:441,473-475`) |
| 409 | `{"error":"api_version_unsupported","feature":"get_log","required_api":"1.10","device_api":"X.Y"}` (`:360-363,455-458`) |
| 409 | `{"error":"log_transfer_busy"}` (`:379-381,397-399,479-481`) |
| 503 | `{"error":"queue_full"}` (`:406-408,490-492`) |
| 503 | `{"error":"log_heap_low"}` (`:507-509`) |

### D.9 Мессенджеры

| Код | Тело |
|---|---|
| 200 | см. § B.17 |
| 400 | `{"result":false}` (telegram PUT без валидных полей, `TelegramBotHandler.cpp:66-74`) |
| **423** | `{"enable":false,"subsystem_name":"telegram_bot","status":"telegram_bot disable"}` / `{"…","status":"vk_bot disabled"}` (`TelegramBotHandler.cpp:22-30`, `VkBotHandler.cpp:134-142`) |
| 502 | `{"peers":[],"ok":false,"error":"…"}` (vk/peers, `VkBotHandler.cpp:224-232`) |
| 500 | пусто (нет `VkSettingsService`, `VkBotHandler.cpp:146,178`) |

### D.10 501 и 503 в целом

`501` в прошивке встречается **только** в `POST /rest/commands` (неизвестная команда). `503` —
в `/rest/settings` (очередь), `/rest/commands` (очередь), `/rest/logs*` (очередь/память).
`409` — `/rest/commands` (два разных тела!), `/rest/logs*`, `/rest/profiles/copy`,
`/rest/oc/xkc25/calibrate`.

---

## E. Аутентификация

### E.1 `/rest/signIn`

`POST /rest/signIn` — `{"username":"…","password":"…"}` (`AuthenticationService.cpp:27-43`).
Успех: **200** `{"access_token":"<JWT>"}`. Провал: **401 без тела** (`:42`).
Тело не объект → тоже 401 (проверка `json.is<JsonObject>()`, `:29`). Пустые поля не валидируются
отдельно — просто не совпадут с пользователем.

### E.2 JWT

- Payload ровно `{"username":…,"admin":bool}` (`SecuritySettingsService.cpp:98-102`).
- Подпись HMAC-SHA256 по секрету (`ArduinoJsonJWT.cpp:38-52`), заголовок `JWT_HEADER` (`:62`).
- **`exp`/`iat`/`nbf` не добавляются** (`ArduinoJsonJWT.cpp:54-68`), но при проверке payload
  сравнивается на **полное равенство** с `{"username","admin"}` (`SecuritySettingsService.cpp:104-110`):
  токен с *любыми* дополнительными полями будет отвергнут. MCP-сервер не должен «обогащать» токен.
- Секрет: `FACTORY_JWT_SECRET` → `securitySettings.jwtSecret` (`SecuritySettingsService.cpp:22,62-65`);
  смена секрета инвалидирует все токены.
- Передача: `Authorization: Bearer <jwt>` (`:43-53`) либо `?access_token=<jwt>` (`:54-58`).
- `GET /rest/generateToken?username=<u>` — только admin, 200 `{"token":"<jwt>"}` или 400/401
  (`SecuritySettingsService.cpp:157-178`).

### E.3 Поведение при выключенной security

`FT_SECURITY=0`:

- `/rest/signIn` и `/rest/verifyAuthorization` **не регистрируются** → 404 (`AuthenticationService.cpp:18`);
- `wrapRequest`/`wrapCallback` — тождественные обёртки, предикат не проверяется
  (`SecuritySettingsService.cpp:204-214`);
- `authenticateRequest` возвращает admin-пользователя всегда (`:199-202`), поэтому `IS_ADMIN`-пути
  тоже открыты;
- `/rest/features` отдаёт `security:false` (`FeaturesService.cpp:79-83`) — по этому полю MCP-сервер
  должен решать, идти ли на `/rest/signIn`.

### E.4 Маршрут → admin обязателен (secure-сборка)

| Маршрут | admin |
|---|---|
| `GET /rest/telemetry`, `POST /rest/telemetry` | **да** |
| `GET/POST /rest/sensor` | **да** |
| `GET/POST /rest/zones` | **да** |
| `GET/POST /rest/alarms` | **да** |
| `GET/POST /rest/securitySettings`, `/rest/wifiSettings`, `/rest/apSettings`, `/rest/ntpSettings`, `/rest/mqttSettings` | **да** |
| `GET /rest/scanNetworks`, `/rest/listNetworks`, `POST /rest/time` | **да** |
| `POST /rest/restart`, `POST /rest/factoryReset` | **да** |
| `POST /rest/uploadFirmware`, `GET /rest/downloadUpdate` | **да** |
| `GET /rest/generateToken` | **да** |
| `GET/POST /api/telegram/config`, `/api/vk/config` | **да** |
| всё остальное прикладное (`/rest/settings`, `/rest/commands`, `/rest/sensors/zone`, `/rest/subsystem`, `/rest/telegram/config`, `/rest/vk/*`, `/rest/oc/*`, `/rest/profiles*`, `/rest/files*`, `/rest/logs*`) | нет (любой аутентифицированный) |
| `GET /rest/systemStatus`, `/rest/coreDump`, `/rest/*Status` | нет |
| `GET /rest/features` | нет (публичный, токен не нужен) |
| `POST /rest/signIn` | нет |
| `GET /rest/verifyAuthorization` | проверяет токен сам |

На не-secure сборке admin-требование не действует нигде.

---

## F. Фикстуры

Значения иллюстративные, **имена полей и структура — из кода**. Где поле условное, в фикстуре оно
присутствует; в мок-устройстве его надо уметь и не отдавать.

### F.1 `GET /rest/oc/info` на API 1.7 (`ssvc.version = "2.3.1"`, ветка late_heads)

```json
{
  "ssvc": { "version": "2.3.1", "api": "1.7", "api_code": 107, "mode": "late_heads" },
  "oc": {
    "version": "0.2.9.1",
    "is_support_api": true,
    "is_support_release": true,
    "api_min": "1.7",
    "api_target": "1.10",
    "api_compatibility": "supported",
    "api_features": [
      { "name": "base",               "min_api": "1.7",  "available": true  },
      { "name": "formula",            "min_api": "1.7",  "available": true  },
      { "name": "predec",             "min_api": "1.9",  "available": false },
      { "name": "extended_heads",     "min_api": "1.8",  "available": false },
      { "name": "tank_mmhg_act",      "min_api": "1.7",  "available": true  },
      { "name": "formula_auto92",     "min_api": "1.9",  "available": false },
      { "name": "hearts_preempt_cnt", "min_api": "1.8",  "available": false },
      { "name": "get_log",            "min_api": "1.10", "available": false }
    ]
  }
}
```

### F.2 `GET /rest/oc/info` на API 1.10 (`ssvc.version = "2.5.1"`)

```json
{
  "ssvc": { "version": "2.5.1", "api": "1.10", "api_code": 110, "mode": "late_heads" },
  "oc": {
    "version": "0.2.9.1",
    "is_support_api": true,
    "is_support_release": true,
    "api_min": "1.7",
    "api_target": "1.10",
    "api_compatibility": "supported",
    "api_features": [
      { "name": "base",               "min_api": "1.7",  "available": true },
      { "name": "formula",            "min_api": "1.7",  "available": true },
      { "name": "predec",             "min_api": "1.9",  "available": true },
      { "name": "extended_heads",     "min_api": "1.8",  "available": true },
      { "name": "tank_mmhg_act",      "min_api": "1.7",  "available": true },
      { "name": "formula_auto92",     "min_api": "1.9",  "available": true },
      { "name": "hearts_preempt_cnt", "min_api": "1.8",  "available": true },
      { "name": "get_log",            "min_api": "1.10", "available": true }
    ]
  }
}
```

Вариант «версия ещё не получена»: `"api": "", "api_code": 0, "mode": "late_heads"`,
`"api_compatibility": "unknown"`, `available:true` только у `base`, `formula`, `tank_mmhg_act`.

### F.3 `GET /rest/telemetry`

```json
{
  "telemetry": {
    "type": "heads",
    "pid": 120,
    "start_time": "17.11.2026 14:02:11",
    "countdown": "00:41:03",
    "time": "00:12:57",
    "tank_mmhg": 12.4,
    "tp1_target": 78.6,
    "tp2_target": 77.9,
    "open": 1.6,
    "period": 3,
    "valveOpen": 5333,
    "volumeSpeed": 21,
    "v1": 1,
    "v2": 0,
    "stop": false,
    "stops": 0,
    "common": {
      "mmhg": 12.4,
      "tp1": 78.6,
      "tp2": 77.9,
      "relay": true,
      "signal": false,
      "cfg_chgd": true,
      "tts": 12,
      "heatingOn": true,
      "overclockingOn": false,
      "relative_pressure": 3.1
    },
    "hysteresis": 0.25,
    "volume": { "heads": 480, "hearts": 0, "late_heads": 0 },
    "alc": 0.0
  },
  "status": {
    "stage": "heads",
    "status": "running",
    "startTime": "17.11.2026 14:02:11",
    "endTime": "",
    "stages": { "waiting": "finished", "heads": "running" },
    "uartConnectionError": false
  },
  "lastUpdate": 918452
}
```

Обратите внимание: `early`-поля (`pid`, `tank_mmhg`, `tp1_target`, `v1`…) в реальном ответе могут
отсутствовать; `mmhg` в `common` — **float**, не строка; `volume` содержит либо `tails`, либо
`late_heads`, но не оба.

### F.4 `GET /rest/settings`

```json
{
  "ssvcSettings": {
    "heads": [1.2, 3],
    "hearts": [0.9, 2],
    "late_heads": [0.7, 2],
    "valve_bw": [1200, 900, 1500],
    "hyst": 0.25,
    "decrement": 3,
    "formula": 1,
    "tank_mmhg": 10.0,
    "heads_timer": 3600,
    "late_heads_timer": 1200,
    "hearts_timer": 5,
    "start_delay": 0,
    "hearts_finish_temp": 82.0,
    "formula_start_temp": 88.0,
    "sound": true,
    "pressure": false,
    "relay_inverted": false,
    "relay_autostart": true,
    "autoresume": false,
    "auto_mode": true,
    "hearts_temp_shift": 0.0,
    "hearts_pause": false,
    "predec": 7,
    "extended_heads": 0,
    "tp2_shift": 0.0,
    "tp_filter": false,
    "signal_tp1_control": false,
    "signal_inverted": false,
    "tp1_control_temp": 0.0,
    "tp1_control_start": false,
    "stab_limit_time": 0,
    "stab_limit_finish": false,
    "backlight": "on",
    "release_timer": 0,
    "release_speed": -1.0,
    "heads_final": -1.0,
    "parallel": [0.5, 2],
    "parallel_v1": [0.4, 2],
    "parallel_v3": [[10.0, 20.0, 1], [20.0, 30.0, 1], [30.0, 40.0, 1], [40.0, 50.0, 1]]
  }
}
```

`tails`/`tails_temp`/`release_*`/`parallel*` могут отсутствовать (см. § B.5), `formula` — int, а не bool.

### F.5 `PUT /rest/settings` — успех

```json
{
  "success": true,
  "message": "Settings updated successfully",
  "accepted": 2,
  "skipped": ["predec=7"]
}
```

`ignored` появляется только если в теле были неизвестные ключи, `skipped` — только если устройство
старее и параметр отброшен по версии API.

### F.6 `PUT /rest/settings` — частичное применение (`partial:true`, HTTP 503)

```json
{
  "success": false,
  "error": "Settings queue is unavailable",
  "message": "Часть настроек не попала в очередь контроллера",
  "partial": true,
  "accepted": 1,
  "failedChunks": 1,
  "failed": ["decrement=3"],
  "ignored": ["sound"]
}
```

### F.7 `GET /rest/sensor`

```json
{
  "tank": {
    "28FF64A1B2C3D4E5": { "v": 78.62, "u": "°C", "type": "thermal" }
  },
  "inlet_water": {
    "28FF64A1B2C3D4E6": { "v": 21.4, "u": "°C", "type": "thermal" }
  },
  "receiver_tank_1": {
    "3C01A816D4E5F601": { "v": 1.0, "u": "", "type": "level" }
  },
  "act": {
    "47": { "v": 745.2, "u": "мм рт. ст.", "type": "pressure" }
  }
}
```

Адрес BMP581 — 16 hex-символов из 7-битного адреса в первом байте (`AbstractSensor.h:68-70`,
`SensorManager.cpp:165-171`), поэтому `"47"` в фикстуре — сокращение: реально будет
`"4700000000000000"`. Для моков лучше воспроизводить 16 символов.

### F.8 `GET /rest/alarms`

```json
{
  "thresholds": {
    "28FF64A1B2C3D4E5": { "enabled": true, "min": 0.0, "dangerous": 50.0, "critical": 80.0 },
    "28FF64A1B2C3D4E6": { "enabled": false, "min": 5.0, "dangerous": 80.0, "critical": 100.0 }
  }
}
```

### F.9 `GET /rest/oc/xkc25/status`

```json
{
  "sensors": [
    {
      "index": 0,
      "enabled": true,
      "pinSource": 0,
      "gpio": 4,
      "pcfAddress": 34,
      "pcfBit": 0,
      "pinLabel": "GPIO 4",
      "zoneIndex": 0,
      "zone": "receiver_tank_1",
      "zoneLabel": "Приёмная ёмкость 1",
      "sharedAcrossTanks": false,
      "activeLow": true,
      "calibrated": true,
      "calibratedAt": 1763300000,
      "levelWithoutLiquid": 1,
      "levelWithLiquid": 0,
      "live": true,
      "rawLevel": 1,
      "filled": false
    },
    {
      "index": 1,
      "enabled": false,
      "pinSource": 0,
      "gpio": 5,
      "pcfAddress": 34,
      "pcfBit": 0,
      "pinLabel": "GPIO 5",
      "zoneIndex": 1,
      "zone": "receiver_tank_2",
      "zoneLabel": "Приёмная ёмкость 2",
      "sharedAcrossTanks": false,
      "activeLow": true,
      "calibrated": false,
      "calibratedAt": 0,
      "levelWithoutLiquid": 1,
      "levelWithLiquid": 0,
      "live": false,
      "rawLevel": -1,
      "filled": false
    }
  ],
  "triggerAction": 0,
  "sharedTankIndex": 0,
  "startGuardMode": 1,
  "blockStart": false,
  "blockReasons": [],
  "warnings": ["Датчик уровня #2 (GPIO 5, Приёмная ёмкость 2) не откалиброван — работа датчика не проверена"]
}
```

`pinLabel` для PCF-источника формируется `DigitalInputSource::label()` — «не подтверждено», что
строка выглядит как `PCF8574 0x22 bit 0`; в § N.

### F.10 `GET /rest/oc/relay/metadata` (сборка `PINOUT_USE_GPIO=0`, один чип 0x24)

```json
{
  "supported": true,
  "relayChipCount": 1,
  "relayLineCount": 8,
  "relays": [
    { "index": 1, "label": "Реле 1", "editable": false,
      "reason": "Системное реле AlarmManager (только просмотр)", "targetBit": 0 },
    { "index": 2, "label": "Реле 2", "editable": false,
      "reason": "Системное реле AlarmManager (только просмотр)", "targetBit": 1 },
    { "index": 3, "label": "Реле 3", "editable": true, "targetBit": 2 },
    { "index": 4, "label": "Реле 4", "editable": true, "targetBit": 3 },
    { "index": 5, "label": "Реле 5", "editable": true, "targetBit": 4 },
    { "index": 6, "label": "Реле 6", "editable": true, "targetBit": 5 },
    { "index": 7, "label": "Реле 7", "editable": true, "targetBit": 6 },
    { "index": 8, "label": "Реле 8", "editable": true, "targetBit": 7 }
  ],
  "sensorNames": ["Куб", "Входящая вода"],
  "hardwareFaultCodes": [
    { "value": 0, "key": "none",                "label": "Нет сбоя" },
    { "value": 1, "key": "i2c_nack",            "label": "I2C: нет подтверждения (NACK)" },
    { "value": 2, "key": "i2c_bus_down",        "label": "I2C: шина недоступна" },
    { "value": 3, "key": "device_not_present",  "label": "Устройство не обнаружено" },
    { "value": 4, "key": "uart_link_lost",      "label": "Потеря связи с SSVC (UART)" }
  ],
  "hardwareRoles": ["i2c_bus", "ssvc_uart", "pcf8574_port0", "bmp581"],
  "rectificationStages": [
    { "value": "waiting",       "label": "Дежурный режим" },
    { "value": "tp1_waiting",   "label": "Ожидание нагрева колонны" },
    { "value": "delayed_start", "label": "Отложенный старт" },
    { "value": "heads",         "label": "Головы" },
    { "value": "late_heads",    "label": "Подголовники" },
    { "value": "hearts",        "label": "Тело" },
    { "value": "tails",         "label": "Хвосты" },
    { "value": "settings",      "label": "<UNK>" }
  ],
  "ssvcFields": [
    { "key": "formula",     "label": "Формула скорости",           "valueType": "int" },
    { "key": "auto_mode",   "label": "Автоматический режим",       "valueType": "bool" },
    { "key": "tank_mmhg",   "label": "Давление в кубе (мм рт.ст.)","valueType": "float" }
  ]
}
```

Точный состав `hardwareRoles` зависит от числа чипов и живых сбоев (`OpenConnectHandler.cpp:218-229`),
`ssvcFields` — полный список в § C.6 (28 элементов), здесь сокращён.

### F.11 `GET /rest/logs` (первый запрос из idle и последующий)

Первый запрос из `idle` (команда только поставлена в очередь):

```json
{ "status": "receiving", "total": 0, "received": 0 }
```

После получения списка:

```json
{ "status": "list", "total": 0, "received": 0, "files": ["12.CSV", "13.CSV"] }
```

Скачивание:

```json
{ "status": "receiving", "total": 120, "received": 40 }
```

Ошибка версии API:

```json
{ "error": "api_version_unsupported", "feature": "get_log", "required_api": "1.10", "device_api": "1.7" }
```

Готовая передача — 200 `text/csv`, заголовок `Content-Disposition: attachment; filename="12.CSV"`.

---

## G. Расхождения с RFC

Формат: «в RFC сказано X → в коде Y». Проверялись `docs/develop/mcp-server.md` §2/§2.5/§4 и
`docs/develop/mcp-server-agents.md` §5–§13.

1. **403 против 401.** RFC §2.2 и WP4/WP8 предписывают маппить отказ admin-доступа с `403` в
   `forbidden_admin_required` (`mcp-server.md:114`, `mcp-server-agents.md:596`). В коде **все**
   предикаты, включая `IS_ADMIN`, отвечают **401 без тела**: `SecuritySettingsService.cpp:136,150`.
   Единственный `403` в прикладном коде — зарезервированный бит реле (`OpenConnectHandler.cpp:294`),
   и WP5 сам это признаёт (`mcp-server-agents.md:686-687`). Следствие: 401 в MCP-сервере означает
   «токен не тот **или** не хватает прав», различить нельзя без повторного логина под admin.
2. **`/rest/features` без аутентификации.** RFC §2.4.1 помечает его как `auth`
   (`mcp-server.md:136`). В коде маршрут регистрируется голой лямбдой без `wrapRequest`
   (`FeaturesService.cpp:23-29`) — доступен без токена, и именно так его читает UI до логина
   (`interface/src/routes/+layout.ts:8`). Это единственный способ узнать `security:false` без 401.
3. **`admin` у фреймворковых маршрутов преувеличен.** RFC §2.4.1: `systemStatus`, `coreDump`,
   `wifiStatus/wifiSettings/...`, `apStatus`, `ntpStatus`, `mqttStatus` — «admin»
   (`mcp-server.md:137-142`). В коде `systemStatus`, `coreDump`, все `*Status` — `IS_AUTHENTICATED`
   (`SystemStatus.cpp:126`, `CoreDump.cpp:34`, `WiFiStatus.cpp:27`, `APStatus.cpp:29`,
   `NTPStatus.cpp:26`, `MqttStatus.cpp:29`); admin нужен только для `*Settings`, `scanNetworks`,
   `listNetworks`, `time`, `restart`, `factoryReset`, `uploadFirmware`, `downloadUpdate`,
   `generateToken` (`WiFiScanner.cpp:25-35`, `NTPSettingsService.cpp:49-53` и т. д.). Плюс
   `/rest/restart` — **только POST**, «GET/POST» в RFC неверно (`RestartService.cpp:24`).
4. **`type` датчика в `/rest/sensor`.** RFC WP1 приводит `"type": "DS18B20"`
   (`mcp-server-agents.md:411`, повторено в `:588`). В коде `type` — это класс измеряемой величины:
   `thermal` / `pressure` / `level` (`DS18B20Sensor.cpp:91-92`, `BMP581Sensor.cpp:252-253`,
   `Xkc25Sensor.cpp:312-313`); модель датчика в ответе вообще не передаётся. Единица BMP581 —
   `"мм рт. ст."`, а не `°C` (`BMP581Sensor.cpp:248-249`).
5. **`status` процесса не имеет значений `stopped`/`error`.** RFC и каталог инструментов
   рассчитывают на «процесс остановлен/ошибка» (`mcp-server-agents.md:425`, `mcp-server.md:613`).
   В коде `ProcessState::STOPPED` и `PROCESS_ERROR` **не имеют `case`** в `stateToString`
   (`RectificationProcess.cpp:163-181`) → отдаётся строка `"unknown"`. То же со стадией `SETTINGS`:
   в `stageToString` её нет (`:117-139`), хотя `metadata.rectificationStages` рекламирует
   `settings` (`OpenConnectHandler.cpp:232-235`). Zod-схема обязана включать `unknown`, а MCP не
   должен трактовать `unknown` как «процесс не идёт».
6. **`GET /rest/logs` из `idle` не возвращает `"status":"list"`.** RFC WP7 показывает
   `GET /rest/logs → {status:"list", files:[…]}` как ответ на первый запрос
   (`mcp-server-agents.md:755-756`). В коде запрос из `idle` сначала **резервирует сессию и ставит
   команду** (`HandlerRegistrator.cpp:387-410`), а снимок берётся сразу после, когда статус —
   `QUEUED_LIST`, что отображается как `receiving` (`SsvcLogProtocol.h:484`,
   `HandlerRegistrator.cpp:319-335`). То есть первый ответ — `{"status":"receiving","total":0,
   "received":0}`, и список приходит только следующим опросом. Кроме того, в RFC не перечислены
   `409 api_version_unsupported` (с `required_api`/`device_api`) и `400 invalid_process_id`
   (`HandlerRegistrator.cpp:360-363,441`).
7. **Индекс датчика XKC-25.** WP6: «`index` 0..3» (`mcp-server-agents.md:723`). В коде максимум
   три слота: `kMaxXkc25Sensors = 3` (`OpenConnectHardwareConfig.h:146`) и проверка
   `index >= kMaxXkc25Sensors` → **400 `{"error":"index out of range"}`**
   (`OpenConnectHandler.cpp:652-655`), то есть допустимо 0..2. Таймаут — поле `timeoutMs`,
   а не `timeout_ms`, и он **зажимается сверху 5000 мс**, а не «1000..15000»
   (`OpenConnectHandler.cpp:485-487,678-680`).
8. **`supported:false` не единственная форма отказа реле.** RFC §2.5 п.9 и WP5 утверждают, что
   GPIO-сборка отдаёт `{"supported":false}` вместо ошибки (`mcp-server.md:223`,
   `mcp-server-agents.md:683-684`). Для GET это верно (`OpenConnectHandler.cpp:140,154,252`), но
   `POST /rest/oc/relay/override` на GPIO-сборке отдаёт **400**
   `{"error":"PCF8574 relay path not enabled in this build"}` (`:269-271`). Обработчик должен
   считать признаком GPIO-сборки и этот 400.
9. **Ключи `/rest/systemStatus`.** WP9 приводит `{heap, uptime, firmware}`
   (`mcp-server-agents.md:875`). Реальные ключи: `free_heap`, `total_heap`, `used_heap`,
   `min_free_heap`, `max_alloc_heap`, `uptime`, `firmware_version`, `esp_platform`, `cpu_type`,
   `psram_size`/`free_psram` (при наличии PSRAM) и др. (`SystemStatus.cpp:136-163`) — ключей
   `heap` и `firmware` не существует.
10. **Поля только для чтения в `/rest/settings`.** RFC WP3 перечисляет закрытый набор
    `PARAM_HANDLERS` верно (`mcp-server-agents.md:541-545`), но не предупреждает, что GET отдаёт
    ещё ~17 ключей, которых **нет** в обработчиках: `sound, pressure, relay_inverted,
    relay_autostart, autoresume, auto_mode, hearts_temp_shift, hearts_pause, tp2_shift, tp_filter,
    signal_tp1_control, signal_inverted, tp1_control_temp, tp1_control_start, stab_limit_time,
    stab_limit_finish, backlight` (`SsvcSettings.cpp:114-139` против
    `SettingsSetterHandlers.cpp:20-229`). PUT с ними вернёт `200`, но в `ignored`. MCP-инструмент
    `oc_update_settings` не должен объявлять их записываемыми, иначе «успех» будет ложным.
11. **Состав `/rest/subsystem`.** RFC §2.4.2 фиксирует шесть имён
    (`mcp-server.md:154`). Фактически набор зависит от конфигурации: добавляются
    `lcd1602_display` и `xkc25_sensor`, а `atm_sensor` присутствует только при
    `pressureSensorEnabled` (`SsvcOpenConnect.cpp:203-236`). Схема ответа должна быть
    «словарь bool с известными ключами, остальные допустимы».
12. **`200 text/plain` у `/rest/commands` — не всегда.** RFC §2.5 п.10 и §2.4.2 говорят
    «200 = принято, text/plain» (`mcp-server.md:153,224`). Если предстартовая проверка XKC-25 дала
    предупреждения, ответ — **200 `application/json`**
    `{"status":"accepted","command":"start","warnings":[…]}` (`CommandHandler.cpp:96-107`).
    Также в RFC отсутствуют `501 Command not implemented` (`:40-44`) и `400` на кривое тело
    (`:26-34`), а отказ `feature_unsupported` — это **409 text/plain**, а не JSON (`:82-84`).
13. **`/rest/sensors/thresholds` и `/rest/profiles/import`.** RFC §2.5 п.11 верно говорит, что их
    нет в прошивке (`mcp-server.md:225`), но не описывает, **что реально происходит** при вызове:
    thresholds → 404 catch-all (маршрута нет вообще), import → перехватывается
    BackwardCompatible-матчером `POST /rest/profiles` и отдаёт 400 «Missing or invalid 'name' field»
    (`HandlerRegistrator.cpp:294`, `ProfileHandler.cpp:74-77`). Для мок-устройства это разные
    ответы, и их стоит воспроизвести.
14. **`lastUpdate` — не время.** RFC WP1 называет поле `last_update_ms` и говорит о «возрасте
    снимка» (`mcp-server-agents.md:424,438`). В коде это `millis()` (мс с загрузки)
    (`TelemetryService.cpp:157`), то есть привязать его к настенному времени нельзя; «возраст»
    считается только относительно другого `lastUpdate` с того же устройства.
15. **Семантика `xkc25TriggerAction` расходится с UI.** В прошивке `0 = PAUSE`, `1 = ABORT`,
    `2 = NOTIFY` (`OpenConnectHardwareConfig.h:95-99`), а в типах фронтенда написано обратное:
    «0 — только уведомление, 1 — пауза отбора, 2 — прерывание»
    (`interface/src/lib/types/ssvc.ts:246-251`). RFC WP6 приводит `"triggerAction": 1` без
    расшифровки (`mcp-server-agents.md:705`). На живом устройстве нужно проверить, какая
    интерпретация соответствует фактическому поведению (§ N).
16. **`POST` у read-only сервисов.** RFC нигде не упоминает, что `HttpEndpoint` регистрирует POST
    для **всех** состояний: `POST /rest/telemetry`, `POST /rest/sensor` существуют и отвечают 200
    текущим состоянием (`HttpEndpoint.h:78-114`; `TelemetryState::update` → `UNCHANGED`,
    `TelemetryService.h:47-50`). Мок-устройство должно их принимать, а MCP — не считать, что
    «POST /rest/sensor» меняет данные.
17. **Дефолты порогов тревог.** RFC приводит `{enabled, min:0, dangerous:80, critical:100}`
    (`mcp-server-agents.md:592`) как форму. Код сериализует дефолты `0/50/80`
    (`AlarmThresholdService.h:54-59`), а при разборе подставляет `0/80/100` (`:62-69`), поэтому
    частичный POST может «сдвинуть» пороги, которых клиент не передавал. Это надо явно
    обрабатывать в MCP: всегда отправлять все четыре поля.

Дополнительно (не про RFC, но про соседнюю документацию): `docs/develop/api/sensors.md:20-40`
описывает `PUT /rest/sensors/zone` с JSON-телом `{"sensorId","zone"}` — это неверно, параметры
передаются **только в query** (§ B.7), а поля называются `address` и `zone`.

---

## H. Ловушки

Дополнение к RFC §2.5 (`mcp-server.md:209-229`).

### H.1 Порядок регистрации и матчеры

1. Строковый маршрут в ESPAsyncWebServer — **не exact**, а `BackwardCompatible`:
   `path == value || path.startsWith(value + "/")` (`WebServer.cpp:216-232,336` в
   `.pio/libdeps/.../ESPAsyncWebServer/src`). Матчится **первый** подходящий обработчик
   (`WebServer.cpp:142-151`), поэтому порядок регистрации = приоритет.
2. `GET /rest/files` (маршрут 67) зарегистрирован **раньше** `GET /rest/files/*` (68) и перекрывает
   его: любой `GET /rest/files/<path>` уходит в обработчик дерева. Скачивание файла в этой ревизии
   недостижимо (см. § N — проверить на живом устройстве).
3. `GET /rest/logs` намеренно зарегистрирован через `AsyncURIMatcher::exact`
   (`HandlerRegistrator.cpp:341-347`) — иначе он перекрывал бы `/rest/logs/{id}`. Это же правило
   придётся соблюдать в мок-устройстве.
4. `DELETE /rest/files/*` и `DELETE /rest/logs` регистрируются после GET-маршрутов; для DELETE
   перекрытий нет, потому что `canHandle` проверяет ещё и метод (`WebHandlers.cpp:302-307`).
5. `/rest/profiles/*` регистрируются **до** `/rest/profiles` (`HandlerRegistrator.cpp:240-297`) —
   иначе `POST /rest/profiles` съел бы `set-active`, `copy`, `save`, `content`.
6. Завершающий слэш ведёт себя **по-разному** в зависимости от матчера. У обычного строкового
   маршрута (`/rest/settings`) `BackwardCompatible` пропускает и `/rest/settings`, и
   `/rest/settings/`: второй вариант начинается с `value + "/"` (`WebServer.cpp:216-232`). У
   exact-маршрута `/rest/logs` (`HandlerRegistrator.cpp:347`) `/rest/logs/` **не** совпадёт и уйдёт
   в обработчик `/rest/logs/*` с пустым id → 400 `{"error":"invalid_process_id"}`
   (`HandlerRegistrator.cpp:436-443`). Вывод: слешей не добавлять; для мок-устройства это два
   разных поведения, и `/rest/logs/` нужно воспроизводить как 400, а не как список.
7. Тот же эффект у вложенных путей: `/rest/oc/xkc25/status/` совпадёт с `/rest/oc/xkc25/status`,
   а `/rest/sensor/` — с `/rest/sensor` (оба — `HttpEndpoint`, строковые пути
   `SensorDataService.h:39`, `OpenConnectHandler.cpp:187`).

### H.2 Вложенность против плоскости

- `GET /rest/settings` → **вложенный** `ssvcSettings`; `PUT` → **плоский** объект. Ошибка в любую
  сторону тихая: PUT с обёрткой `{"ssvcSettings":{…}}` уйдёт в `ignored`, вернёт **200** с
  `"accepted":0` и ничего не применит (`SettingsHandler.cpp:64-72,141-148`), а разбор GET без
  разворачивания даст `undefined` во всех полях.
- `GET /rest/zones`, `/rest/alarms` → вложенные `zones`/`thresholds`; POST ждёт ту же обёртку
  (`SensorConfigService.cpp:32`, `AlarmThresholdService.cpp:69`), иначе `CHANGED` не наступит и
  ответ будет 200 без изменений.
- `GET /rest/profiles` → **массив**, а не `{profiles:[…]}`.
- `/rest/oc/info` → вложенные `ssvc` и `oc`; `/rest/telemetry` → `telemetry`, `status` + `lastUpdate`
  в корне.

### H.3 Дефолты и необязательные поля

- `relay/override`: `enable` по умолчанию **true**, `energized` — **false**
  (`OpenConnectHandler.cpp:297-298`); передавать оба всегда.
- `xkc25TriggerAction`/`startGuardMode` дефолты: PAUSE и WARN (`OpenConnectHardwareConfig.h:85,105`).
- Пороги тревог: см. § G.17.
- Почти вся телеметрия условна (§ B.1) — «нет поля» ≠ «ноль».
- `late_heads`/`tails` в `/rest/settings` отсутствуют, если не пришли с устройства
  (дефолты `{-1,-1}`, `SsvcSettings.h:272-277`), а `tails_temp` — если `<= 0`
  (`SsvcSettings.cpp:90-93`).

### H.4 Асинхронность и таймауты

- `POST /rest/commands` → 200 = «в очереди» (`CommandHandler.cpp:74-77`); результат команды узнать
  неоткуда, кроме последующего состояния.
- `PUT /rest/settings` → 200 = SET-пакеты поставлены в очередь; фактическое применение — после
  ответа контроллера, повторный `GET /settings` сразу может вернуть старое. Ретрай после 503
  запрещён — часть SET уже ушла (§ B.5).
- `POST /rest/oc/xkc25/calibrate {"step":"verify_return"}` → 202; результат — только через
  `GET /rest/oc/xkc25/verify-status` (опрос).
- `GET /rest/logs/{id}` → 202 до готовности; готовый результат живёт 5 минут
  (`HandlerRegistrator.cpp:339`), после — снова запрос и новая UART-сессия.
- `PUT /rest/subsystem {"telegram_bot":true}` может занять десятки секунд; отдельного статуса
  готовности нет, а GET отдаёт целевой флаг сразу (`SubsystemManager.cpp:79-102`).
- `GET /rest/telemetry` обновляется внутренним таймером раз в 2 с
  (`TelemetryService.cpp:12`), `GET /rest/sensor` — циклом опроса датчиков, поэтому снимки
  «разъезжаются» по времени.
- `GET /rest/oc/hardware-config` и `PUT` выполняют реальные I²C-пробы под `Guard` шины
  (`OpenConnectHandler.cpp:367-397,441-471`) — на медленной/висящей шине запрос может тянуться.

### H.5 Коды, которых не ждёшь

- **423** у `/rest/telegram/config` и `/rest/vk/config` при выключенной подсистеме
  (`TelegramBotHandler.cpp:27`, `VkBotHandler.cpp:139`).
- **502** у `/rest/vk/peers` при ошибке VK API (`VkBotHandler.cpp:230`).
- **409 text/plain** (не JSON) у `/rest/commands` при `feature_unsupported`
  (`CommandHandler.cpp:82-84`).
- **500 text/plain** у `/rest/commands` при исключении (`:110-113`).
- **409** у `/rest/profiles/copy` при «источник не найден» — не 404 (`ProfileHandler.cpp:176`).
- **401 без тела** у любого admin-маршрута — не путать с «нет такого маршрута» (404 text/plain).
- Нет метода — 404, а не 405 (см. § A).

### H.6 Прочее

- `?statusOnly=0` == `?statusOnly=1` (`HandlerRegistrator.cpp:373`): проверяется наличие параметра.
- `GET /rest/settings` может отдавать `mode`-зависимый набор полей (§ B.5) — сравнивать профили
  «в лоб» нельзя.
- `PUT /rest/settings` **не** поддерживает `sound/pressure/...` (§ C.3) — «настройка есть в GET, но
  не пишется».
- `DELETE` с JSON-телом (`/rest/profiles/delete`) — работает, потому что `wrapCallback` парсит тело
  независимо от метода (`HandlerRegistrator.cpp:306-309`), но многие HTTP-клиенты это ломают:
  проверять, что тело реально ушло.
- `/rest/profiles/content?id=` — id **в query**; PUT-подобное «обновление содержимого» — это
  **POST** `/rest/profiles/content`.
- `POST /rest/zones` удаляет всё, чего нет в теле (§ B.3): «частичное обновление» карты зон
  невозможно, только полная замена.
- `POST /rest/alarms` ведёт себя так же (§ B.4).
- Адреса датчиков — **верхний регистр** (`SensorManager.cpp:165-171`); `stringToAddress`
  принимает оба регистра (`strtol` по hex), но `SensorManager` хранит и сравнивает ключи
  в верхнем, поэтому `28ff...` в query приведёт к 404 «Sensor not found» (§ N: проверить).
- `GET /rest/oc/info` кэшируется UI до переподключения (`interface/src/lib/api/ssvcApi.ts:35-79`) —
  для MCP всегда читать заново после старта процесса/смены контроллера.

---

## I. EventSocket (`/ws/events`)

### I.1 Как устроен канал

| Что | Значение | Источник |
|---|---|---|
| Путь | `/ws/events` | `lib/framework/EventSocket.h:25` |
| Тип | `AsyncWebSocket` поверх того же `AsyncWebServer`, что и REST | `EventSocket.h:43`, `EventSocket.cpp:14-25` |
| Владелец | `ESP32SvelteKit::_socket`, доступ `getSocket()` | `ESP32SvelteKit.h:116-119,220` |
| Старт | `_socket.begin()` в `ESP32SvelteKit::begin()` | `ESP32SvelteKit.cpp:143` |
| Аутентификация | `setFilter(filterRequest(IS_AUTHENTICATED))` | `EventSocket.cpp:19-22`, предикат по умолчанию — `EventSocket.h:41` |
| Передача токена | `?access_token=<jwt>` в URL рукопожатия | `SecuritySettingsService.cpp:54-58` (браузер не умеет задавать заголовки в WS) |

Регистрация и эмиссия — три метода (`EventSocket.h:33-40`):

- `registerEvent(name)` — только объявляет имя, повторная регистрация даёт warning
  (`EventSocket.cpp:26-34`); эмиссия незарегистрированного события игнорируется с warning
  (`:135-139`), подписка на незарегистрированное — тоже (`:108-116`).
- `emitEvent(event, JsonObject&, originId, onlyToSameOrigin)` — `:133-190`.
- `onSubscribe(event, cb)` — вызывается при подписке клиента (`:110-111`), используется для
  «догоняющей» отправки текущего состояния (`EventEndpoint.h:41-45`).

**Формат кадра на проводе.** При `EVENT_USE_JSON == 0` (так в этой сборке, `lib/framework/Features.h:62`)
кадры — **бинарный MessagePack**, и клиент обязан присылать `WS_BINARY`; при `EVENT_USE_JSON == 1` —
текстовый JSON (`EventSocket.cpp:87-101`). Структура одинакова:

```
// клиент → устройство (подписка)
{"event": "subscribe",   "data": "<имя события>"}
{"event": "unsubscribe", "data": "<имя события>"}
{"event": "<любое>",     "data": {...}}      // дергает onEvent-колбэки, :122-126

// устройство → клиент (эмиссия)
{"event": "<имя события>", "data": { ... }}
```

`originId` — строка `client->id()`; `onlyToSameOrigin` используется, чтобы не отправить событие
инициатору изменения (`EventSocket.cpp:141,173-184`). Хранение подписок — `std::map<String,
std::list<int>>` под мьютексом (`:4,164-185`), удаление при закрытии сокета (`:69-79`).

**Стоимость эмиссии.** На каждое событие: `measureMsgPack` + `new char[len+1]` + сериализация
целиком в этот буфер + копия в каждого подписчика, буфер освобождается (`EventSocket.cpp:149-190`).
Если подписчиков на событие нет — ранний выход **после** сериализации (`:164-171`), то есть цена
платится даже без клиентов. Отсюда правило: не эмитить чаще, чем нужно, и не эмитить большие
телеметрические кадры «вхолостую» (у `TELEMETRY` нет подписчиков ⇒ 2 с × сериализация кадра).

### I.2 Какие события уже зарегистрированы в этой сборке

| Имя | Где регистрируется | Состояние |
|---|---|---|
| `features` | `FeaturesService.cpp:33-41` | активно, с догоняющей отправкой по подписке |
| `notification` | `NotificationService.cpp:12-21` | активно; `{type:"error"\|"warning"\|"info"\|"success", message}` |
| `rssi`, `reconnect` | `WiFiSettingsService.cpp:55-56` | активно (раз в 500 мс, `WiFiSettingsService.h:50`) |
| `otastatus` | `UploadFirmwareService.cpp:38`, `DownloadFirmwareService.cpp:199` | активно |
| `analytics` | `AnalyticsService.h:22,32` | регистрируется при `FT_ANALYTICS=1` (реализация header-only, `.cpp` нет) |
| `battery` | `BatteryService.cpp:44` | **выключено** (`FT_BATTERY=0`) |
| `ethernet` | `EthernetSettingsService.cpp:44` | **выключено** (`FT_ETHERNET=0`) |

Ни `telemetry`, ни `stage`, ни `alarm` **не зарегистрированы** — их нет ни в одном `registerEvent`
(полный список выше получен grep’ом по `registerEvent`).

### I.3 Где физически рождаются данные телеметрии

Путь кадра: UART → RX-задача `SsvcConnector` → `lastMessage` → задача `RectificationProcess::update`
→ `TelemetryService` (2 с) → REST/WS.

1. **RX.** Разобранный не-лог кадр кладётся в `lastMessage` под мьютексом
   (`SsvcConnector.cpp:399-406`) и взводит `BIT0` (`:477-484`). Кадры `type:"response"`
   обрабатываются отдельно (`:417-458`) и `BIT0` не взводят.
2. **Разбор.** `RectificationProcess::update` ждёт `BIT0` (`RectificationProcess.cpp:296-303`),
   берёт `getLastMessage()` (`:309`), десериализует и определяет стадию по `type` (`:314-334`),
   ведёт `pid` (`:340-352`). **Здесь же уже есть детектор события**: `currentEvent` объявлен в
   цикле (`:288`), заполняется при разборе (`:476`), обрабатывается
   `endEventHandler(currentEvent)` (`:484-485`, реализация `:726-745`). Это готовый хук для
   события `stage`: смена стадии/`event` уже вычисляется, остаётся эмиссия.
3. **Публикация состояния.** Раз в `TELEMETRY_UPDATE_INTERVAL_MS = 2000` мс
   (`TelemetryService.cpp:12,73-80`) таймер вызывает `updateTelemetryState()` (`:114-177`),
   который пересобирает JSON из `RectificationProcess` и в конце вызывает
   `callUpdateHandlers("InternalTimer")` (`:166`). Частота кадров от контроллера — «не подтверждено»
   (зависит от SSVC); известно только, что публикация в REST/WS квантуется 2 с.

### I.4 Что нужно, чтобы появились `telemetry`, `stage`, `alarm`

**`telemetry` — минимальная правка.** `EventEndpoint<TelemetryState>` требует `StatefulService`,
а `TelemetryService` им является: `EventEndpoint` подписывается на `addUpdateHandler`
(`EventEndpoint.h:31-35`, `StatefulService.h:71-82`) и эмитит состояние при каждом изменении
(`EventEndpoint.h:50-56`). Достаточно `_socket->registerEvent("telemetry")` +
эмиссия в обработчике обновления — сработает ровно на `callUpdateHandlers("InternalTimer")`
(`TelemetryService.cpp:166`), то есть каждые 2 с. Цена: одно дерево `{telemetry,status}` ≈
0.6–1.2 КБ JSON / ≈0.4–0.8 КБ msgpack (оценка по числу полей § B.1 — **не подтверждено замером**),
сериализация в куче на каждые 2 с, плюс копия на клиента.

**`stage` — дешёвый вариант.** Эмитить не весь кадр, а `{stage, status, event, pid}` в точке
`:476-485` (`RectificationProcess::update`) — только при смене значения. Размер кадра ~60–120 Б,
частота — по факту смены стадии (единицы за процесс). Требуется передать `EventSocket*` в
`RectificationProcess` (сейчас он его не знает: `SsvcOpenConnect::begin` получает сокет —
`src/main.cpp:44-49` — но в `RectificationProcess` не прокидывает).

**`alarm` — средняя правка.** Готовые точки: `AlarmMonitor::subscribe(IAlarmSubscriber*)` и
`notifySubscribers(const AlarmEvent&)` (`AlarmMonitor.h:29-33,61`), а также
`raiseHardwareFault`/`clearHardwareFault` (`AlarmMonitor.h:38-39`) — их уже вызывают из
`SsvcConnector.cpp:377-378,414`. Эмитить `{kind:"threshold"|"hardware", address, role, code,
level}` при `notifySubscribers` и при смене `HardwareFaultCode`. Частота — по событиям;
`checkAllSensors()` (`AlarmMonitor.h:33`) вызывается, «не подтверждено» с какой периодичностью.

**Итоговая оценка цены:**

| Событие | Кадр | Частота | RAM на эмиссию | Что менять |
|---|---|---|---|---|
| `telemetry` | 0.4–1.2 КБ | 0.5 Гц (2 с) | heap-буфер на кадр + копии клиентам | `TelemetryService` (эмиссия в update-handler) |
| `stage` | ~100 Б | по смене стадии | пренебрежимо | `RectificationProcess` (прокинуть сокет) |
| `alarm` | ~150 Б | по событию | пренебрежимо | `AlarmMonitor` (эмиссия в `notifySubscribers`) |

Ограничение: `emitEvent` вызывается из задач (RX, таймер, worker) — он потокобезопасен по
подпискам (`clientSubscriptionsMutex`), но `AsyncWebSocket::textAll/binaryAll` из чужой задачи —
«не подтверждено», нужен ли `xSemaphore`/делегирование в loop-задачу; проверить на устройстве
под нагрузкой.

---

## J. `SsvcCommandsQueue`

### J.1 Устройство

| Что | Значение | Источник |
|---|---|---|
| Синглтон | `SsvcCommandsQueue::getQueue()` | `SsvcCommandsQueue.h:64-70` |
| Основная очередь | FreeRTOS, **50** слотов | `SsvcCommandsQueue.h:205` |
| Элемент очереди | **указатель** `SsvcCommand*` (объект в куче) | `:206-214` |
| Аварийная очередь | отдельный `safety_queue`, забирается **до** FIFO | `:192-194`, `SsvcCommandsQueue.cpp:303-320` |
| Ожидание FIFO | 500 мс, срез ожидания ответа 100 мс | `SsvcCommandsQueue.cpp:307-320,491` |
| Попытки | `ATTEMPT_COUNT = 3`, таймаут `3000` мс | `SsvcCommandsQueue.h:96-110` |
| Пауза при блокировке лог-сессией | 100 мс, команда возвращается в **хвост** | `SsvcCommandsQueue.cpp:304,346-356` |
| Отправка | `SsvcConnector::sendCommand("<CMD>\n")` | `:373-432`, `SsvcConnector.cpp:498-521` |
| Ожидание ответа | `xEventGroupWaitBits` по биту типа | `:358-370,485-500` |
| Ретрай | только идемпотентные типы | `:62-70,545-556` |

`isIdempotentRetry` запрещает повтор для `START`, `NEXT`, `RESUME` (`SsvcCommandsQueue.cpp:62-70`),
а при «неопределённом исходе» (нет ответа после отправки) ретрай вообще пропускается (`:545-550`).

### J.2 Идентификатор команды и корреляция ответов

**Идентификатора у команды нет.** `SsvcCommand` — это `{type, parameters, attempt_count,
safetyEpoch, generation}`; ни порядкового номера, ни UUID, ни отметки времени
(`SsvcCommandsQueue.h:283-298` и объявление структуры там же). Узнать «судьбу» конкретной команды
через REST нельзя.

**Корреляция есть только косвенная, по эху `request`.** Контроллер в ответе возвращает поле
`request` с текстом команды (`SsvcConnector.cpp:417-423`), и RX-задача разветвляется по нему:
`GET_SETTINGS` → `BIT10` (`:435-443`), `VERSION` → `BIT11` (`:444-448`), `GET_LOG` → трансфер
(`:423-434`), **любой другой `result == "OK"` → `BIT9`** (`:450-454`), иначе → `BIT1` (`:455-457`).
Воркер ждёт бит, соответствующий типу **текущей** команды (`commandToExpectedBit`,
`SsvcCommandsQueue.h:219-230`: `BIT9` для `STOP/PAUSE/RESUME/NEXT/AT/SET/STATUS/START`, `BIT10` —
`GET_SETTINGS`, `BIT11` — `VERSION`). Так как воркер **один**, а биты перед отправкой очищаются
(`SsvcCommandsQueue.cpp:365-370`), корреляция держится на сериализации, а не на номере.

Ловушки этой схемы:

- `BIT9` взводится **любым** ответом с `result:"OK"` — если SSVC пришлёт «чужой» ответ (например,
  на автоматический `GET_SETTINGS` из `:480`), он засчитается текущей команде;
- кадры `type:"response"` **не** попадают в `lastMessage` (`:399` — только не-лог, но `BIT9`-ветка
  до записи `lastMessage`; фактически `lastMessage` пишется для всех не-лог кадров `:399-406`, а
  `storeResponse` — только для `VERSION`/OK: `:446,452`);
- команды, на которые SSVC может не ответить вовсе (`STOP` во время лог-сессии, `GET_LOG`), дают
  «нет ответа» = таймаут, а не «ошибка» — REST уже вернул 200 «принято».

### J.3 `lastRejectionReason` и где он заполняется

Единая точка постановки в очередь `enqueue()` вызывает `rememberRejection(reason, feature,
requiredApi, deviceApi)` (`SsvcCommandsQueue.cpp:822-831`) и складывает 4 строки под мьютекс
(`SsvcCommandsQueue.h:152-176`). Значения:

| reason | Когда | Строка |
|---|---|---|
| `invalid_parameters` | параметры не прошли валидацию | `SsvcCommandsQueue.cpp:730` |
| `no_supported_params` | после фильтрации по версии не осталось ни одного SET-параметра | `:752` |
| `feature_unsupported` | возможности нет на устройстве (`get_log`) | `:780` |
| `not_ready` | тракт не готов (до `begin()`) | `:788` |
| `queue_full` | `xQueueSend` не смог | `:808` |
| `safety_rejected` | аварийная команда отклонена | `:225` |

Это **глобальное** «последнее» значение, не привязанное к команде: два параллельных POST
перезапишут его друг другом (мьютекс защищает только целостность строк). REST использует его
сразу после неудачной постановки (`CommandHandler.cpp:78-88`).

### J.4 Минимальный дизайн `commandId`

Идея: не менять UART-протокол, а добавить **журнал попыток** в самой очереди.

**Хранение.** Кольцевой буфер фиксированного размера в `SsvcCommandsQueue` (не в куче):

```
struct CommandRecord {           // ~96–128 Б
  uint32_t id;                   // монотонный счётчик, выдаётся в enqueue()
  SsvcCommandType type;          // 1 Б
  uint8_t state;                 // queued|running|done|rejected|timeout
  char params[64];               // усечённые параметры (SET/STATUS/GET_LOG)
  uint32_t queuedAt, startedAt, finishedAt;  // millis()
  int8_t rejection;              // индекс строки причины
  bool answeredOk;               // пришёл ли BIT9/BIT10/BIT11, а не BIT1
};
static CommandRecord _journal[16];   // ~2 КБ .bss, heap 0
```

Почему 16: воркер один, реальная скорость — единицы команд в секунду, а окно «узнать судьбу»
нужно на время одного UI-раунда; 16 записей × 5 минут TTL с запасом покрывают это. Усечение
`params` до 64 Б достаточно для `SET` одного-двух полей и `STATUS` (лимит `SSVC_SET_CMD_MAX_LEN`
300 Б — `SsvcUartApiSpec.h`, но хранить полный текст в журнале не нужно).

**Переходы.** `queued` — в `enqueue()` после успешного `xQueueSend` (`:790-808`); `running` — в
воркере перед отправкой (`:372`); `done` — после успешного `bitCallbacks` (`:500-560`);
`rejected` — при `rememberRejection` (там же, где сейчас пишется причина); `timeout` — когда
`attempt_count` исчерпан (`:556-570`).

**API.** `POST /rest/commands` → добавить в 200-ответ `"commandId": N` (тело уже бывает JSON при
warnings — `CommandHandler.cpp:96-107`; для text/plain-ветки `:109` нужен переход на JSON или
заголовок `X-Command-Id`). `GET /rest/commands/{id}` → `{id, command, state, queuedAt, startedAt,
finishedAt, ok, reason}`; регистрировать **exact-матчером** рядом с `/rest/commands`
(`HandlerRegistrator.cpp:73-78`), иначе `POST /rest/commands` перехватит `GET /rest/commands/5`
(см. § H.1.1). 404 — если записи нет/истекла.

**Что считать результатом — два варианта:**

| | A. Транспортный (рекомендую) | B. Семантический |
|---|---|---|
| Что отдаём | `queued/running/done/timeout/rejected` + `ok` = взведён ли бит успеха | пытаемся вытащить `result`/`request` из последнего ответа |
| Плюсы | всё уже есть в коде: биты, попытки, отказы; ноль изменений в UART; честно отражает «мы отправили и получили/не получили ответ» | ближе к «команда выполнена» |
| Минусы | `done` ≠ «применено»: SSVC может ответить `OK` и не выполнить; для `START` ответа может не быть вовсе | корреляции нет: при двух командах подряд последний ответ нельзя привязать к конкретной; `lastResponse` перезаписывается (`SsvcConnector.cpp:446,452`); часть команд молчит |
| Риски | ложное «done» при чужом `OK` (§ J.2) | требует правки протокола SSVC (номер в команде и в ответе) — вне этой прошивки |

Практический компромисс: вариант A + явное поле `confidence:"transport"` и текст ошибки
`reason:"no_response"` для таймаута; для `START/NEXT/RESUME/STOP` не обещать `done`, а отдавать
`sent` (ответа может не быть — `isIdempotentRetry` уже кодирует это знание,
`SsvcCommandsQueue.cpp:62-70`). Долгосрочно — добавить в SSVC-протокол эхо-номер (`SET #42 ...`),
и тогда вариант B становится корректным.

**Риски памяти/поведения:** журнал статический (2 КБ `.bss`), но `enqueue()` вызывается из REST-задачи,
воркер — из своей; нужен мьютекс (уже есть `_rejectionMutex`, лучше отдельный). TTL и перезапись
кольца обязательны, иначе `GET /rest/commands/{id}` будет отдавать «чужую» команду после
переполнения. Никаких `std::string` в записи — только `char[64]`, иначе фрагментация кучи на каждой
команде.

---

## K. Безопасность и JWT

### K.1 Структура и проверка

- Payload — **ровно два поля**: `{"username": "<строка>", "admin": <bool>}`
  (`SecuritySettingsService.cpp:98-102`).
- Подпись — HMAC-SHA256 (`ArduinoJsonJWT.cpp:38-52`), заголовок фиксированный `JWT_HEADER`
  (`:62`); `exp`, `iat`, `nbf`, `iss`, `aud` **не добавляются** (`:54-68`).
- Транспорт: `Authorization: Bearer <jwt>` (`SecuritySettingsService.cpp:43-53`) либо
  `?access_token=<jwt>` (`:54-58`). Cookie-сессии нет — ни `Set-Cookie`, ни чтения cookie в
  `authenticateRequest` (`:29-58`).
- Refresh-эндпоинта нет: единственные источники токена — `POST /rest/signIn`
  (`AuthenticationService.cpp:27-44`) и `GET /rest/generateToken?username=` под admin
  (`SecuritySettingsService.cpp:157-178`); оба идут через один `generateJWT` (`:112-118`).

### K.2 Что именно ломается при добавлении `exp`/`iat`

`validatePayload` сравнивает разобранный payload с **эталоном, собранным из БД**, и требует
**полного равенства объектов** (`SecuritySettingsService.cpp:104-110`). Любое дополнительное поле
в токене делает объекты неравными ⇒ `authenticateJWT` возвращает «невалиден» ⇒ **401 на всех
маршрутах**, включая `/rest/verifyAuthorization`. То есть:

- добавили `exp` при выдаче → сломались **все** клиенты, включая веб-UI (он хранит токен и
  шлёт его как есть);
- добавили проверку `exp` без изменения выдачи → старые токены (без `exp`) должны считаться
  валидными, иначе после обновления прошивки все разлогинены;
- `generateToken` и `signIn` используют один билдер, поэтому «половинчатая» правка невозможна:
  либо оба пути добавляют `exp`, либо ни один.

### K.3 Совместимый путь

Заменить сравнение объектов на **сравнение по подмножеству**:

```
bool valid = payload["username"].is<const char*>() &&
             payload["admin"].is<bool>() &&
             strcmp(payload["username"], user.username) == 0 &&
             payload["admin"].as<bool>() == user.admin &&
             (!payload["exp"].is<long>() || payload["exp"].as<long>() > time(nullptr)) &&
             (!payload["nbf"].is<long>() || payload["nbf"].as<long>() <= time(nullptr));
```

Плюсы: старые токены без `exp` продолжают работать (условие «если есть — проверить»); новые
получают срок жизни; подпись по-прежнему защищает от подделки. Минусы/риски:

- **нет отзыва**: выданный токен живёт до `exp`, сменить пароль/секрет — единственный способ
  инвалидировать (сейчас так же, но бессрочно);
- **часы**: `exp` требует синхронного времени; `time(nullptr)` до NTP-синхронизации = эпоха 1970 ⇒
  все токены «просрочены» сразу после загрузки. Нужно либо не проверять `exp`, пока время <
  порога (например, `time(nullptr) < 1700000000`), либо выдавать токен с `exp` только после
  синхронизации;
- **тест на живом устройстве обязателен**: токен, выданный до обновления, должен приниматься
  после (обратная совместимость), а токен с `exp` в прошлом — отвергаться.

Правка локализована в одной функции (`validatePayload`) + один билдер (`populateJWTPayload`/
`generateJWT`), но затрагивает вход во всё устройство, поэтому её надо катить вместе с
`FEATURES`/UI-логикой «перелогиниться при 401».

---

## L. Транспорт, TLS, CORS

### L.1 TLS на сервере: чего нет

- Единственный сервер создаётся как `AsyncWebServer server(80)` (`src/main.cpp:16`) и передаётся
  в `ESP32SvelteKit` (`:18`); mDNS объявляет `http` и `ws` на порту 80
  (`ESP32SvelteKit.cpp:133-134`). HTTP-only.
- В используемой версии `ESPAsyncWebServer` **нет класса `AsyncWebServerSecure`**: в `src/` только
  `WebServer.cpp`, `WebHandlers.cpp` и т. д. (`ls .pio/libdeps/.../ESPAsyncWebServer/src`), а
  `beginSecure(cert, key, password)` объявлен под `#if ASYNC_TCP_SSL_ENABLED`
  (`ESPAsyncWebServer.h:1703-1706`) — этот макрос нигде не определён (проверено grep’ом по
  `ESPAsyncWebServer/src` и `AsyncTCP/src`), то есть для ESP32-S3 метод недоступен.
- `ssl_certs/` — это **исходящие** CA-бандлы для OTA (`ssl_certs/cacert.pem` ≈ 68 КБ,
  `DigiCert_Global_Root_CA.pem`), вшитые через `board_build.embed_files`; серверного
  сертификата/ключа в репозитории нет.

Вывод: «опциональный HTTPS» — это не флаг, а **замена транспорта**. Минимум:

1. серверный сертификат X.509 + ключ (RSA-2048 или EC P-256; для ESP32-S3 предпочтительнее EC —
   меньше RAM на рукопожатие), с SAN на `openconnect.local`/IP;
2. `esp_https_server` (esp-tls/esp_http_server) **или** форк `ESPAsyncWebServer` с TLS; при этом
   `ESP32SvelteKit` завязан на `AsyncWebServer*` (`ESP32SvelteKit.h:220,116-119`) и на
   `AsyncWebSocket` для `/ws/events` (`EventSocket.h:43`), а WebSocket-поверх-TLS в этой связке
   отсутствует — понадобится отдельный WS-стек для `wss://`;
3. правки: тип сервера в `ESP32SvelteKit`/`SsvcOpenConnect::begin` (`src/main.cpp:44-49` — там
   `server` и `getSocket()`), порт/mDNS (`ESP32SvelteKit.cpp:133-134`), UI-база URL
   (`interface/src/lib/api/...` — сейчас относительные пути), плюс редирект 80→443 или
   параллельный HTTP для локальной сети.

Оценки памяти (RSA/ECDSA, mbedtls): рукопожатие ≈ 16–30 КБ heap на соединение, буферы записей
≈ 4–16 КБ — **не подтверждено замером**; на S3 с PSRAM обычно приемлемо, но при 2–3 одновременных
TLS-сессиях + JSON-телеметрия риск OOM реален. Размер образа растёт на ~30–60 КБ (mbedtls уже
линкуется для OTA-клиента, поэтому прирост меньше, чем кажется). Проверять `free_heap` до/после.

Альтернатива без TLS на устройстве: оставить HTTP в локальной сети, а шифрование возложить на
MCP-сервер/реверс-прокси (nginx/caddy) — тогда прошивку трогать не нужно вообще. Это же снимает
проблему `wss`.

### L.2 CORS: как включается и что реально делает

`ENABLE_CORS` в `platformio.ini:36` закомментирован ⇒ CORS-заголовков нет. При включении:

- глобально добавляются `Access-Control-Allow-Origin: CORS_ORIGIN` (по умолчанию `"*"`,
  `ESP32SvelteKit.h:55-56`), `Access-Control-Allow-Headers: Accept, Content-Type, Authorization`,
  `Access-Control-Allow-Credentials: true` (`ESP32SvelteKit.cpp:123-128`);
- `OPTIONS`-обработчики регистрируются только у `HttpEndpoint<T>`-сервисов
  (`HttpEndpoint.h:51-62`) и у `SleepService` (`SleepService.cpp:31-40`).

Чего не хватает для рабочего preflight:

1. **`Access-Control-Allow-Methods`** не выставляется нигде — браузер отклонит preflight для
   `PUT/POST/DELETE`;
2. `OPTIONS` нет у прикладных маршрутов (`/rest/settings`, `/rest/commands`, `/rest/oc/*`,
   `/rest/profiles*` — `HandlerRegistrator.cpp:56-309`), они попадут в catch-all 404
   (`WebHandlers.cpp:313`);
3. комбинация `Allow-Origin: "*"` + `Allow-Credentials: true` **запрещена** спецификацией —
   браузер такой ответ отклонит; нужен конкретный origin из конфига;
4. `EventSocket` (`/ws/events`) Origin не проверяет вообще — токен в query решает доступ, но
   CSRF-подобный сценарий «страница открыла WS с украденным токеном» не блокируется.

Для MCP-сервера это значит: **не рассчитывать на CORS** в текущей прошивке; либо включить
`ENABLE_CORS` и добавить `Allow-Methods` + `OPTIONS` для прикладных путей (небольшая правка
`HandlerRegistrator`/`HttpEndpoint`), либо проксировать запросы через сам MCP-сервер (он
server-side, CORS не нужен).

---

## M. Мёртвые вызовы UI

### M.1 `/rest/sensors/thresholds` (`ssvcApi.ts:143`)

- Определение: `updateSensorThresholds(address, warning, critical)` — `PUT
  /rest/sensors/thresholds?address=…&warning=…&critical=…`
  (`interface/src/lib/api/ssvcApi.ts:138-146`).
- **Вызовов нет**: grep по `interface/src` находит только определение; ни один `.svelte` не
  импортирует эту функцию (импорты в `SensorCard.svelte:2` — только `updateAlarmThresholds` и
  `updateSensorZone`).
- Что уже есть: пороги читаются `GET /rest/alarms` (`ssvcApi.ts:604-606`) и пишутся
  `POST /rest/alarms` целым объектом `AlarmThresholdsState` (`ssvcApi.ts:619-621`); редактор —
  `interface/src/lib/components/OCSettings/SensorCard.svelte:95-120` (`saveThresholds` собирает
  `newState.thresholds[address] = {enabled,min,dangerous,critical}` и вызывает
  `updateAlarmThresholds`), поля — `:244,258,272`, кнопка — `:283`.

**Минимальный вариант: удалить функцию** (11 строк) — поведение не меняется, а ложное
впечатление «есть отдельный эндпоинт порогов» исчезает. Если нужен точечный сеттер для MCP —
делать его **на сервере** как тонкую обёртку над `/rest/alarms` (PATCH-семантика), а не в UI:
сейчас `POST /rest/alarms` требует полную карту и **удаляет** отсутствующих
(`AlarmThresholdService.cpp:150-160`), что для точечного изменения опасно (§ H.6).

### M.2 `/rest/profiles/import` (`Profiles.ts:78`)

- Определение: `importProfile(profile)` — `POST /rest/profiles/import`
  (`interface/src/lib/api/Profiles.ts:77-80`).
- **Вызовов нет**: grep находит только определение. Импорт в UI реализован **на клиенте**:
  `ProfileManager.svelte:215-239` читает файл через `FileReader`, парсит JSON и вызывает
  `createProfiles(profileName, profileContent)` (`:228`), то есть `POST /rest/profiles`
  с `{name, content}` (`Profiles.ts:23-27`), который отвечает **201** `{"success":true,"id":…}`
  (`ProfileHandler.cpp:96-106`).
- Кнопка импорта — `ProfileManager.svelte:292-299` (`Fa icon={faFileImport}`), скрытый
  `<input type=file>` — рядом (`:200-214`).

**Минимальный вариант: удалить функцию.** Серверный маршрут не нужен: сценарий «импорт профиля»
уже покрыт `POST /rest/profiles` с полем `content` (форма — § B.14). Если позже понадобится
серверный импорт (например, из MCP), его надо реализовать как `POST /rest/profiles` с
`{name, content}` — **не** добавлять `/rest/profiles/import`, потому что этот путь уже
перехватывается BackwardCompatible-матчером `POST /rest/profiles` (`HandlerRegistrator.cpp:294`)
и вернёт 400 «Missing or invalid 'name' field» (`ProfileHandler.cpp:74-77`), то есть новый
маршрут придётся регистрировать **до** него и это скрытая ловушка порядка (§ H.1.5).

Итог по M: обе функции — мёртвый код без серверной поддержки; удаление не требует изменений в
прошивке. Единственное, что стоит сделать на сервере, — задокументировать `POST /rest/alarms`
как полную замену карты (а не «патч одного датчика»), чтобы MCP-инструмент не собрал
деструктивный запрос.

---

## N. Что проверить на живом устройстве (не подтверждено кодом)

1. **Перекрытие `GET /rest/files/<path>`** деревом файлов (§ H.1.2). Из кода библиотеки следует
   перекрытие, но UI этот маршрут не вызывает — нет ни одного вызова в `interface/src`. Проверить:
   `curl -i -H "Authorization: Bearer $JWT" http://dev/rest/files/config/zones.json` — если вернулся
   JSON-массив дерева, баг подтверждён.
2. **`pinLabel` для PCF-источника XKC-25** (§ F.9). Строка формируется `DigitalInputSource::label()`
   (используется в `Xkc25Subsystem.cpp:26-35,505`), реализацию я не читал построчно — уточнить
   фактический формат (`"PCF8574 0x22 bit 3"`?).
3. **Семантика `xkc25TriggerAction`** (§ G.15): 0 = PAUSE или 0 = NOTIFY. Проверить поведением:
   включить датчик, налить жидкость, посмотреть, встал ли отбор на паузу при значении 0.
4. **`formula` в `/rest/settings`** — int (`SsvcSettings.cpp:104`) или bool? Код отдаёт int, а
   `smoke_rest.py:215` отправляет `bool`. Проверить, что устройство принимает оба варианта
   (обработчик `SingleIntHandler` с bool не пройдёт → 400).
5. **Регистр адреса в `/rest/sensors/zone`**: пройдёт ли `28ff…` (нижний) или только `28FF…`
   (§ H.6). Код парсит оба, но поиск в `SensorManager` — по ключу верхнего регистра
   (`SensorManager.cpp:165-171`), а `setZoneForSensor` вызывает `assignZone` напрямую.
6. **Время включения `telegram_bot`/`vk_bot`** — RFC говорит «>60 с» (`mcp-server.md:226`); в коде
   только синхронный `enableSubsystem` (`SubsystemManager.cpp:79-102`). Замерить фактические
   таймауты на устройстве.
7. **`i2cScan` на пустой/висящей шине**: `scanAddresses(false)` (`OpenConnectHandler.cpp:92`) —
   сколько занимает и что возвращает при `i2cBusActive:false`.
8. **`cache-control`/`ETag`**: в коде не выставляются (`AsyncJsonResponse` без заголовков);
   подтвердить, что промежуточный прокси не кэширует `/rest/telemetry`.
9. **Поведение `/rest/logs` при `refresh=1` во время файловой сессии** — по коду 409
   `log_transfer_busy` (`HandlerRegistrator.cpp:376-382`); проверить, что UI это переживает.
10. **`GET /rest/profiles/content` для профиля, созданного другой версией**, — набор ключей
    (`ssvcSettings`/`alarmThresholds`/`telegram`/`vk`) может быть неполным; для моков безопасно
    считать любой из них опциональным (`ProfileService.cpp:283-291` требует, чтобы значение было
    объектом, иначе ключ пропускается).
