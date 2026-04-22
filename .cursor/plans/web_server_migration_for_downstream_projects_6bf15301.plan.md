---
name: Web server migration for downstream projects
overview: План перехода на новый веб-сервер (ESPAsyncWebServer) для проектов, использующих ESP32-sveltekit как базу. Учитывает завершённую миграцию в базовом репозитории и задаёт чек-лист для потребителей и их прикладного кода.
todos: []
isProject: false
---

# План перехода на новый веб-сервер для проектов на базе ESP32-sveltekit

## Контекст

В базовом репозитории [ESP32-sveltekit](D:\microcontrollers\esp32\OpenConnect\PhysicHTTP-to-AsynWebserver\ESP32-sveltekit) миграция с **PsychicHttp** на **ESPAsyncWebServer** уже выполнена (Phases 1–8 по [MIGRATION_PLAN.md](MIGRATION_PLAN.md)). Проекты, использующие ESP32-sveltekit как базу (форк, submodule или копия `lib/framework` + свой `src/`), должны привести в соответствие свой код и конфигурацию сборки.

**Что меняется для потребителя:**

- HTTP/WebSocket: типы и API от PsychicHttp заменены на ESPAsyncWebServer (в базе уже сделано).
- MQTT: **PsychicMqttClient** остаётся без изменений (в scope миграции только веб-сервер).

---

## 1. Варианты использования ESP32-sveltekit и стратегия перехода


| Вариант                                   | Действия при переходе                                                                                                            |
| ----------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------- |
| **Форк/клон репо**                        | Обновить ветку до версии с ESPAsyncWebServer; обновить только свой прикладной код в `src/` и при необходимости `platformio.ini`. |
| **Git submodule** (`lib/ESP32-sveltekit`) | Обновить submodule на коммит с миграцией; в своём проекте обновить `platformio.ini` и код в `src/`.                              |
| **Копия только framework**                | Скопировать обновлённые `lib/framework/`* и привести зависимости/приложение по чек-листу ниже.                                   |


Общий порядок: сначала перейти на **версию ESP32-sveltekit с ESPAsyncWebServer**, затем выполнить шаги раздела 2–5 в своём проекте.

---

## 2. Конфигурация сборки (platformio.ini)

- **Удалить** явную зависимость от PsychicHttp (если была в `lib_deps` или `lib_extra_dirs`). В базовом [platformio.ini](platformio.ini) используется только:
  - `ESP32Async/ESPAsyncWebServer#v3.10.0` (или `@ ^3.10.0`)
  - `elims/PsychicMqttClient@^0.2.4` (если нужен MQTT)
- **Добавить/оставить** `lib_compat_mode = strict` (как в базовом проекте), чтобы изолировать зависимости и избежать конфликтов с PsychicMqttClient.
- Не добавлять PsychicHttp в корневой проект — MQTT-клиент тянет свои зависимости сам.

---

## 3. Точка входа и веб-сервер (src/main.cpp)

Привести к тому же паттерну, что и в [src/main.cpp](src/main.cpp):

- Подключать: `#include <ESPAsyncWebServer.h>` (вместо PsychicHttp).
- Объявлять сервер: `AsyncWebServer server(80);` (вместо `PsychicHttpServer`).
- Передавать в конструктор фреймворка указатель на этот сервер: `ESP32SvelteKit esp32sveltekit(&server);`.
- Все сервисы приложения, принимающие веб-сервер, должны получать `AsyncWebServer`* (см. следующий пункт).

Изменения только в `main.cpp` и заголовках/реализациях своих сервисов; ядро фреймворка уже ожидает `AsyncWebServer`*.

---

## 4. Прикладной слой (собственные сервисы в src/)

Типичная структура как в [LightStateService](src/LightStateService.h) / [LightMqttSettingsService](src/LightMqttSettingsService.h):

- **Конструкторы сервисов:** параметр типа `PsychicHttpServer`* заменить на `AsyncWebServer`*.
- **Заголовки:** `#include <PsychicHttp.h>` заменить на `#include <ESPAsyncWebServer.h>` (или убрать, если достаточно включений из шаблонов фреймворка).
- **Члены классов:** указатели на HTTP-сервер/запросы — только `AsyncWebServer`*, `AsyncWebServerRequest`* и т.д. Тип **PsychicMqttClient*** оставить для MQTT (получение через `esp32sveltekit.getMqttClient()` или аналог).

Если в приложении есть **кастомные HTTP/WS обработчики** (не только через HttpEndpoint/EventEndpoint/WebSocketServer), применить маппинг API из раздела 5.

---

## 5. Маппинг API для кастомных эндпоинтов (по MIGRATION_PLAN.md)

Для любого своего кода, работающего с запросами/ответами/WebSocket, использовать:


| Было (PsychicHttp)                    | Стало (ESPAsyncWebServer)                                                           |
| ------------------------------------- | ----------------------------------------------------------------------------------- |
| `PsychicHttpServer`                   | `AsyncWebServer(80)`                                                                |
| `PsychicRequest`*                     | `AsyncWebServerRequest`*                                                            |
| `PsychicJsonResponse`                 | `AsyncJsonResponse*` (выделять через `new`, перед отправкой вызывать `setLength()`) |
| `request->reply(code)`                | `request->send(code)`                                                               |
| Отправка JSON: `response.send()`      | `response->setLength(); request->send(response);`                                   |
| Обработчик возвращает `esp_err_t`     | Обработчик возвращает `void`                                                        |
| WebSocket: `client->sendMessage(msg)` | `client->text(msg)`                                                                 |
| WebSocket: `client->socket()`         | `client->id()`                                                                      |
| Broadcast: `_socket.sendAll(msg)`     | `_socket.textAll(msg)`                                                              |


Загрузка файлов (OTA и т.п.): не использовать `PsychicUploadHandler` — в фреймворке используется встроенный upload callback в `server->on(..., uploadHandler)` (см. [UploadFirmwareService](lib/framework/UploadFirmwareService.cpp)).

---

## 6. Тестирование после перехода

Проверить по пунктам, актуальным для проекта (из Phase 9 и Risk Register в [MIGRATION_PLAN.md](MIGRATION_PLAN.md)):

- **HTTP:** GET/POST к REST-эндпоинтам (в т.ч. с авторизацией).
- **WebSocket:** подключение к EventSocket и кастомным WS (если есть), доставка событий.
- **OTA:** загрузка прошивки через веб-интерфейс до конца.
- **Безопасность:** работа `wrapRequest` / `filterRequest` (логин, 401 при отсутствии/неверном токене).
- **Память:** при необходимости сравнить потребление heap до/после (особенно при многих подключениях).

Если сборка выполняется на Windows и возникает ошибка линковки/архивирования (ranlib, .sconsign), это среда/инструменты, а не следствие миграции — см. Phase 9 в MIGRATION_PLAN.md.

---

## 7. Риски для проектов-потребителей (из MIGRATION_PLAN.md)

- **PsychicMqttClient** — оставляем; он может тянуть свою зависимость. Изоляция через `lib_compat_mode = strict` уже учтена в базовом `platformio.ini`.
- **Различия поведения WebSocket** — тщательно тестировать сценарии с EventSocket и своими WS.
- **Утечки/управление памятью** — `AsyncJsonResponse` создаётся через `new`; освобождение выполняет фреймворк после отправки; в своём коде не удалять ответ вручную до `send(response)`.

---

## 8. Итоговый чек-лист для проекта

1. Обновить ESP32-sveltekit до версии с ESPAsyncWebServer (форк/submodule/копия lib).
2. В `platformio.ini`: убрать PsychicHttp; оставить ESPAsyncWebServer и при необходимости PsychicMqttClient; `lib_compat_mode = strict`.
3. В `src/main.cpp`: `AsyncWebServer server(80);`, `#include <ESPAsyncWebServer.h>`, передача `&server` в ESP32SvelteKit и сервисы.
4. В своих сервисах в `src/`: заменить типы и инклюды на AsyncWebServer/AsyncWebServerRequest; MQTT оставить на PsychicMqttClient.
5. В кастомных обработчиках применить маппинг из раздела 5.
6. Собрать проект, прогнать тесты по разделу 6.

После этого проект будет единообразно использовать новый веб-сервер (ESPAsyncWebServer) в рамках базового ESP32-sveltekit и собственного кода.