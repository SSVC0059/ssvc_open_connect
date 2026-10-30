# План: перенос идей PR #121 (кэширование фронтенда)

Источник: MR [theelims/ESP32-sveltekit#121 «Improve frontend caching»](https://github.com/theelims/ESP32-sveltekit/pull/121) — открыт, не влит, ревью нет. Ниже — не реализация, а объём работ, развилки и критерии приёмки для нашего форка.

## Цель

Убрать устаревание UI после обновления прошивки или образа LittleFS: длительный кэш оставить только для контентно-хешированных ассетов, `index.html` и прочие нехешированные файлы отдавать с ревалидацией, отсутствующий ассет — отдавать как 404, а не подменять на `index.html`.

## Проблема в нашем коде

1. `lib/framework/ESP32SvelteKit.cpp:87` и `:97` — `Cache-Control: public, immutable, max-age=31536000` ставится на **все** ресурсы ветки `EMBED_WWW`, включая `/index.html` и SPA-fallback.
2. `interface/vite.config.ts:57` + `interface/vite-plugin-littlefs.ts` — из имён ассетов вырезается контентный хеш (`.replace('.[hash]', '')`), поэтому имена стабильны между сборками.
3. Результат: браузер законно держит старый `index.html` и старый бандл год → после OTA/`uploadfs` UI не обновляется без hard refresh (upstream issue #68).
4. Дополнительно в ветке LittleFS (`:107-113`): `serveStatic("/_app/", ...)` зарегистрирован на широкий префикс без правил кэша, а `onNotFound` отдаёт `/www/index.html` на любой GET — включая запрос к несуществующему ассету.
5. `interface/svelte.config.js:20-22` (`bundleStrategy: 'single'`) — весь UI собирается в один JS-файл, поэтому одна устаревшая запись = устаревший весь интерфейс.

## Что берём из PR #121, а что нет

**Берём:**
- разделение правил кэша по хешированным и нехешированным путям (`/_app/immutable/*` → immutable, всё остальное → `no-cache`);
- 404 вместо подмены `index.html` для отсутствующего `/_app/`-ассета;
- возврат хешей в имена ассетов + проверку длины имён под LittleFS на этапе сборки.

**Не берём:**
- правки C++ под PsychicHttp (`PsychicRequest`/`PsychicResponse`/`PsychicFileResponse`, `request->reply(404)`, `return response.send()`) — у нас ESPAsyncWebServer;
- мотивацию «ETag не используем»: у нас `AsyncWebServerRequest::_getEtag()` учитывает файл и mtime, а `AsyncStaticWebHandler` умеет 304 по `If-None-Match`;
- попутный фикс `return` в `onNotFound` (наши колбэки возвращают `void`);
- сгенерированный в апстриме `WWWData.h` — у нас он генерируется сборкой и лежит в `.gitignore:18`.

## Развилки

**Вариант A — только политика кэша (плагин с хешами оставляем).**
Мало правок: UI перестаёт устаревать (index.html с `no-cache` и ревалидацией), но бандл с постоянным именем нельзя держать как immutable → на каждой загрузке условный запрос (в FS-режиме 304 по ETag/mtime).

**Вариант B — A + возврат хешей Vite.**
`/_app/immutable/*` держится immutable год, на каждой загрузке ревалидируется только `index.html`. Требует подтверждения лимита длины имён LittleFS.

**Решение:** вариант A делаем сразу как обязательный минимум; вариант B — только после Фазы 0 (gate по лимиту имён).

## Фаза 0. Замеры (обязательна до Фазы 3)

Нужно зафиксировать: (1) какие env собираются с `-DEMBED_WWW`, а какие с образом LittleFS; (2) реальный лимит длины компонента имени в LittleFS нашей сборки.

Ограничение: `platformio.ini` скрыт от инструментов агента (`.gitignore:26`), значения подтверждаются вручную или командой.

- Определить режимы: `Select-String -Path platformio.ini -Pattern 'EMBED_WWW','uploadfs','lib_deps'` — записать, где включён PROGMEM-режим и какая версия ESPAsyncWebServer подключена.
- Быстрая подсказка по лимиту (не доказательство): `Select-String -Path "$env:USERPROFILE\.platformio\packages\framework-arduinoespressif32\**\Kconfig" -Pattern 'LITTLEFS_OBJ_NAME_LEN'`.
- Обязательный эмпирический замер: имя длиннее `LFS_NAME_MAX` в образе есть, но на устройстве не открывается (`stat`/`readdir` дают ошибку → 404). Взять env без `-DEMBED_WWW`, положить пробный файл в ФС, залить и запросить:

```
python -c "import pathlib; pathlib.Path('data/www/_app/immutable').mkdir(parents=True, exist_ok=True); pathlib.Path('data/www/_app/immutable/probe-' + 'a'*40 + '.txt').write_bytes(b'probe')"
pio run -e <env> --target uploadfs
curl -I "http://<ip>/_app/immutable/probe-aaa....txt"
```

Начинать с 63 символов и спускаться; зафиксировать максимальную длину N, при которой ответ 200.

**Критерий:** записаны список env по режимам и подтверждённый лимит `L` (ожидается 63); измерена максимальная длина имени ассетов после включения хешей (см. команду в Фазе 3).

## Фаза 1. Проверка длины имён в сборке (независимая польза)

Файл: `scripts/pio/build_interface.py`.

- Константа `LITTLEFS_NAME_MAX = <L из Фазы 0>` с комментарием, откуда значение.
- Функция `check_littlefs_name_lengths(www_path: Path) -> None`: обход `www_path.rglob("*")`, для каждого пути берутся компоненты `path.relative_to(www_path).parts`; если любой компонент длиннее лимита — печатается `[ERROR]` со списком путей и вызывается `sys.exit(1)`.
- Вызов в `add_app_to_filesystem()` **после** копирования и gzip (имена на диске уже финальные, с суффиксом `.gz`) и до `pio run --target uploadfs`.
- В `build_progmem()` проверку не добавлять: PROGMEM-режим лимитом имён не ограничен.

**Критерий:** при временно заниженном `LITTLEFS_NAME_MAX = 20` сборка `pio run -e <env> --target buildfs` падает с понятным сообщением и ненулевым кодом; при штатном значении сборка проходит без изменений.

## Фаза 2. Политика кэша в `lib/framework/ESP32SvelteKit.cpp` (вариант A)

- Ввести константы уровня файла: `CACHE_IMMUTABLE = "public, immutable, max-age=31536000"` и `CACHE_NO_CACHE = "no-cache"` (рядом с `SVK_TAG`).
- Ветка `EMBED_WWW`: в лямбде регистрации вычислить `const bool isHashed = uri.startsWith("/_app/immutable/");` и ставить заголовок по условию; в `onNotFound` — сначала 404 для не-GET и для путей `/_app/`, затем `index.html` с `CACHE_NO_CACHE`.
- Ветка LittleFS: сузить статику до трёх регистраций и добавить 404 для отсутствующих ассетов.

**Критерий (проверка через `curl -I`):** `/index.html` → `Cache-Control: no-cache`; `/_app/immutable/<hash>.js` → `immutable, max-age=31536000`; несуществующий `/_app/immutable/nope.js` → 404 (не HTML); SPA-маршрут (`/oc/telemetry`) → 200 с `text/html` и `no-cache`. Проверить сборку обоих режимов (`EMBED_WWW` и LittleFS) — ветки под разными `#ifdef`.

## Фаза 3. Возврат хешей (вариант B) — только после Фазы 0

- `interface/vite.config.ts`: убрать импорт `./vite-plugin-littlefs` (строка 4) и вызов `viteLittleFS()` из `plugins` вместе с комментарием про 32 символа (строка 57).
- Удалить `interface/vite-plugin-littlefs.ts`.
- Пересобрать интерфейс и замерить максимальную длину компонента имени:

```
python -c "import pathlib; b=pathlib.Path('interface/build'); print(max((max(len(p) for p in f.relative_to(b).parts), str(f)) for f in b.rglob('*')))"
```

**Критерий:** имена в `interface/build/_app/immutable/` содержат хеш; максимальная длина имени меньше подтверждённого `L`; guard из Фазы 1 проходит; `WWWData.h` перегенерирован; оба режима собираются и UI открывается.

## Фаза 4. Документация и CHANGELOG

- `docs/develop/architecture-manifest.md:194` — переписать фразу «Имена ассетов ограничены длиной, поэтому сборка чистит хеши в именах»: новое правило — длительный кэш только для `/_app/immutable/*`, остальное ревалидируется; длина имён контролируется на сборке.
- `docs/develop/index.md` — сверить раздел про сборку фронтенда и раздачу статики.
- `CHANGELOG.md` — пункт с предупреждением: после обновления открытые вкладки со старым UI получат 404 на старые имена ассетов (одноразово, лечится перезагрузкой страницы).

**Критерий:** документация не противоречит новому поведению; предупреждение для пользователей зафиксировано (при варианте A пункт о 404 не нужен).

## Фаза 5. Сквозная проверка

- Сценарий «обновление»: залить сборку A, открыть UI, залить сборку B с заметным изменением в интерфейсе, нажать обычный F5 → виден новый UI, без Ctrl+F5 и без очистки кэша (это и есть целевой эффект PR).
- Прогнать оба режима раздачи: env с `EMBED_WWW` и env с `uploadfs` (список фиксируется в Фазе 0).
- Регресс: `pio test -e native`, `npm test` в `interface`, e2e-набор не ломается; API/REST/WS не затронуты.

**Критерий:** UI обновляется по F5 в обоих режимах; новых падений тестов нет.

## Риски

| Риск | Митигация |
|---|---|
| Лимит имён LittleFS окажется 32, а не 63 | gate Фазы 0: без подтверждения остаёмся на варианте A (хеши не возвращаем) |
| `serveStatic` в нашей версии ESPAsyncWebServer не принимает аргумент cache-control | запасной путь: хендлер всегда возвращает `AsyncStaticWebHandler&` → `setCacheControl(CACHE_NO_CACHE)` |
| Одноразово ломаются открытые вкладки со старым UI (404 на старые имена) | предупреждение в CHANGELOG и release notes; свежая загрузка страницы работает |
| `no-cache` на `index.html` в EMBED_WWW = повторная передача ~2-4 КБ gzip на каждую загрузку | приемлемо; в FS-режиме `AsyncStaticWebHandler` отвечает 304 по ETag/mtime |
| Расхождение с апстримом при следующих мержах framework-кода | правки сосредоточены в одном .cpp + двух файлах сборки; фиксируем в CHANGELOG со ссылкой на PR #121 |

## Открытые вопросы

1. `platformio.ini` недоступен инструментам агента (`.gitignore:26`), поэтому значения `-DEMBED_WWW`, версия ESPAsyncWebServer и релизный env подтверждаются вручную в Фазе 0. Если файл нужен агенту — добавить `!platformio.ini` в `.workaignore`.
2. Нужно ли переносить план в `.workai/plans/` (с маркером `<!-- workai:plan:update -->`) — пока он лежит в корне по запросу.

## Приложение: эскизы правок

`lib/framework/ESP32SvelteKit.cpp`, ветка `EMBED_WWW` (Фаза 2):

```cpp
#ifdef EMBED_WWW
    WWWData::registerRoutes(
        [&](const String &uri, const String &contentType, const uint8_t *content, size_t len)
        {
            const bool isHashed = uri.startsWith("/_app/immutable/");
            _server->on(uri.c_str(), HTTP_GET, [contentType, content, len, isHashed](AsyncWebServerRequest *request)
                        {
                AsyncWebServerResponse *response = request->beginResponse(200, contentType, content, len);
                response->addHeader("Content-Encoding", "gzip");
                response->addHeader("Cache-Control", isHashed ? CACHE_IMMUTABLE : CACHE_NO_CACHE);
                request->send(response); });

            if (uri.equals("/index.html"))
            {
                _server->onNotFound([content, len](AsyncWebServerRequest *request)
                                    {
                    if (request->method() != HTTP_GET || request->url().startsWith("/_app/")) {
                        request->send(404);
                        return;
                    }
                    AsyncWebServerResponse *response = request->beginResponse(200, "text/html", content, len);
                    response->addHeader("Content-Encoding", "gzip");
                    response->addHeader("Cache-Control", CACHE_NO_CACHE);
                    request->send(response); });
            }
        });
```

`lib/framework/ESP32SvelteKit.cpp`, ветка LittleFS (Фаза 2):

```cpp
#else
    // Хешированные ассеты: имя меняется при каждой сборке → длительный кэш безопасен
    _server->serveStatic("/_app/immutable/", ESPFS, "/www/_app/immutable/", CACHE_IMMUTABLE);
    // index.html, env.js, version.json и прочая нехешированная статика → ревалидация
    _server->serveStatic("/_app/", ESPFS, "/www/_app/", CACHE_NO_CACHE);
    _server->serveStatic("/favicon.png", ESPFS, "/www/favicon.png", CACHE_NO_CACHE);
    _server->onNotFound([](AsyncWebServerRequest *request)
                        {
        if (request->method() != HTTP_GET || request->url().startsWith("/_app/")) {
            request->send(404);
            return;
        }
        AsyncWebServerResponse *response = request->beginResponse(ESPFS, "/www/index.html", "text/html");
        response->addHeader("Cache-Control", CACHE_NO_CACHE);
        request->send(response); });
#endif
```

`scripts/pio/build_interface.py` (Фаза 1):

```python
# Максимальная длина компонента имени в LittleFS нашей сборки (значение из Фазы 0).
# Имя длиннее лимита попадает в образ, но на устройстве не открывается — падаем на сборке.
LITTLEFS_NAME_MAX = 63


def check_littlefs_name_lengths(www_path: Path):
    offenders = []
    for path in www_path.rglob("*"):
        parts = path.relative_to(www_path).parts
        if any(len(part) > LITTLEFS_NAME_MAX for part in parts):
            offenders.append("/".join(parts))
    if offenders:
        print(f"[ERROR] Имена длиннее {LITTLEFS_NAME_MAX} символов не откроются на устройстве:")
        for path in offenders:
            print(f"  {path}")
        sys.exit(1)
```

Вызов — в `add_app_to_filesystem()` после цикла gzip и до `pio run --target uploadfs`:

```python
    for current_path, _, files in os.walk(www_path):
        for file in files:
            gzip_file(os.path.join(current_path, file))
    check_littlefs_name_lengths(www_path)
```
