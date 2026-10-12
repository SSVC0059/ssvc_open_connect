# Перенос возможностей UART API 1.9 в ветку `feature/api-v1.9`

## Зачем

Ветка `feature/api-v1.10-update` (11 коммитов над `main`) содержит одновременно поддержку
API 1.8, 1.9 и 1.10, слой обратной совместимости и набор исправлений. Возможности API 1.10
(команда `GET_LOG`, скачивание журналов ректификации) ещё не готовы — ни в этой прошивке, ни
в контроллере SSVC. Поэтому из неё выделяется ветка `feature/api-v1.9`, в которую попадает всё,
что относится к API до 1.9 включительно, плюс обратная совместимость и исправления.

## Что относится к API 1.9

По спецификации
[SmartModule-ru/ssvc0059_v2_uart_api_docs](https://github.com/SmartModule-ru/ssvc0059_v2_uart_api_docs)
(документ озаглавлен «SSVC0059_V2 UART API Версия 1.9»):

| Возможность | Мин. версия | Где |
|---|---|---|
| `predec=[0\|7\|13]` | 1.9 | SET / GET_SETTINGS |
| `formula=[0\|1\|2]`, значение `2` = «Авто 92+» | 1.9 (0/1 — с 1.7) | SET / GET_SETTINGS |
| `extended_heads=[0\|1]` («Сброс и снижение») | 1.8 | SET / GET_SETTINGS |
| `tank_mmhg_act` | 1.7 | SET |
| `preempt_cnt`, `cfg_chgd`, `tts` | 1.8 | телеметрия |

Команды `GET_LOG` в спецификации 1.9 нет — это единственное, что относится к 1.10 и в ветку
не переносится.

## Исходная цепочка коммитов

```
main 3b23d66
  → 9dfb53f  Исправление компиляции VkSettingsService
  → 1dbb2b5  Исправление компиляции
  → 8014ff6  support API 1.8        (backend)
  → e575ece  Поддержка API 1.8      (frontend)
  → e9e02fd  Тесты: formula boolean → number
  → 516a746  Поддержка API 1.9
  → 4b47a00  Поддержка API 1.10     ← смешанный: 1.10 + полезный код 1.9
  → 83cb61e  Исправление тестов
  → 452d138  Исправление ошибок deploy-preview
  → 52f9a1a  Реализация функционала обратной совместимости API  ← смешанный
  → d32b6cd  Исправление замечаний MR                          ← смешанный
```

## Целевое состояние ветки

- `SSVC_API_VERSION_TARGET = "1.9"`, `SSVC_API_VERSION_MIN = "1.7"` (`SsvcApiCapabilities.h` и
  `factory_settings.ini`).
- Таблица возможностей: `base`/`formula`/`tank_mmhg_act` = 1.7, `extended_heads`/`hearts_preempt_cnt` = 1.8,
  `predec`/`formula_auto92` = 1.9. Запись `get_log` и `FEATURE_GET_LOG` удалены.
- Полностью отсутствуют: `SsvcLogProtocol.h`, `SSVC_CMD_GET_LOG`, обработка `GET_LOG` в очереди и
  коннекторе, `/rest/logs` (`registerLogHandlers`), UI журналов (`/oc/logs`, пункт меню, log-типы,
  log-API, log-тесты), `docs/develop/api/logs.md`.
- `platformio.ini` остаётся на `-std=gnu++14`: C++17 требовался только `std::string_view` из log-кода.

## План переносов по коммитам

| Коммит | Действие | Берём | Исключаем |
|---|---|---|---|
| `9dfb53f`, `1dbb2b5` | cherry-pick | правки компиляции | — |
| `8014ff6` (API 1.8 backend) | cherry-pick | `SingleIntHandler`, `predec`, `extended_heads`, `tank_mmhg_act`, `formula` 0/1/2, геттеры/сеттеры, фикс строки `release_timer`, const-фикс VK | — |
| `e575ece` (API 1.8 frontend) | cherry-pick | `formula: number`, `<select>` вместо checkbox, типы `predec`/`extended_heads` | — |
| `e9e02fd` | cherry-pick | тест formula-number | — |
| `516a746` (API 1.9) | cherry-pick | `isValidFormula` → 0/1/2, опция «Авто 92+» в селектах, баннеры/FAQ v1.9 | — |
| `4b47a00` (API 1.10) | cherry-pick `-n` + ручная правка | `isValidPredec`/`isValidExtendedHeads`, `formula` bool→int в `SsvcSettings`, `RelayRuleEngine` INT + `matchSsvc`, телеметрия `cfg_chgd`/`tts`/`preempt_cnt` (`RectificationProcess`), TS-типы телеметрии, TEST_CASE валидаторов 1.9 | `SsvcLogProtocol.h`, `test/native/test_ssvc_log_protocol/`, `api-v1.10-update.patch`, `docs/develop/api-v1.10-support-plan.md`, `SSVC_CMD_GET_LOG`, GET_LOG в очереди/коннекторе, `registerLogHandlers`, панель журналов в `Ssvc.svelte`, log-функции `ssvcApi.ts`, log-типы `ssvc.ts`, nav-пункт плана в `mkdocs.yml`; `docs/ssvc_uart_api_docs.md` → спецификация 1.9 |
| `83cb61e` (fix тестов) | cherry-pick `-n` + ручная правка | `createMockSsvcSettings.ts`, `generalSettingsExpectedRequests.ts`, `GeneralSettings.test.ts` | правка `test_ssvc_log_protocol` |
| `452d138` (deploy-preview) | cherry-pick | `package.json`/`package-lock.json` (`picomatch`, `sass`) | — |
| `52f9a1a` (обр. совместимость) | cherry-pick `-n` + ручная правка | `SsvcApiCapabilities.h` (без `GetLog`), `ApiVersion`/`parseApiVersion`, TARGET/MIN (**1.9**), строковая версия + `getSsvcApiVersionCode`/`hasFeature`/`isSupportRelease`, bool-гейт `pushCommandInQueue` + `filterSetParameters` + отказы, `OpenConnectHandler` api-реестр, `SettingsHandler` `skipped`, string-версия в `SsvcOpenConnect`/Telegram/VK, frontend `apiVersionCode`, `ApiVersionGuard`, `SsvcApiWarning`, `versioning.ts`, `ssvcHelper`, `resetInfoCache`/`infoCacheVersion`, гейты `GeneralSettings` (1.9), `Settings.svelte`, `statusbar.svelte`, `.gitignore`, удаление `pio_native_verbose.txt` и доки v1.6 | `SsvcLogProtocol.h` и его правки, GET_LOG-гейт и `getLog` в очереди, log-transfer-поведение `SsvcConnector`, `registerLogHandlers` + 409-гейт, `get_log` в `FEATURE_INFO`/`FEATURE_GET_LOG`, `docs/develop/api/logs.md`, nav logs, FAQ-абзац 1.10, роут `/oc/logs`, пункт меню, `cancelLogDownload`, `ssvcApi.logs.test.ts` |
| `d32b6cd` (fix MR) | cherry-pick `-n` + ручная правка | границы `formula` в handler, `predec`/`extended_heads` в `fillSettings`/`updateStateFromJson`/`applySettingsToController`, типизация условий реле, правки FAQ/спеки | busy-guard `HandlerRegistrator`, `_pending` `SsvcLogProtocol` |
| финальный | отдельный коммит | TARGET `1.9`, fallback `targetApiVersion()` → `{1,9}`, починка битого nav `mkdocs.yml` | — |

## Backend — что удаляем (API 1.10)

- `lib/ssvcOpenConnect/core/SsvcLogProtocol/` — целиком.
- `SsvcCommandsQueue.h/.cpp`: `SsvcCommandType::GET_LOG`, ветка в `commandProcessorTask`, `getLog()`,
  запись в dispatch-map.
- `SsvcConnector.cpp`: `#include` лог-протокола, ветки `request == "GET_LOG"` / `type == "file"`,
  поведение по `logTransferActive`. Фикс `if` → `else if` для `VERSION` — сохраняется.
- `HandlerRegistrator.cpp/.h`: `registerLogHandlers()` и его вызов.
- `SsvcUartApiSpec.h`: `SSVC_CMD_GET_LOG`, баннер `(v1.10)` → `(v1.9)`.
- `SsvcApiCapabilities.h`: `SsvcFeature::GetLog`, `FEATURE_GET_LOG`, строка `get_log` в `FEATURE_INFO`.
- `factory_settings.ini`: TARGET → `1.9`.

## Frontend — что удаляем (API 1.10)

- `interface/src/routes/oc/logs/` (`+page.svelte`, `+page.ts`, `RectificationLogs.svelte`).
- `ssvcApi.ts`: `fetchLogStatus`, `startLogDownload`, `downloadLog`, `requestLogFile`, `cancelLogDownload`
  (инфраструктура `resetInfoCache`/`infoCacheVersion` сохраняется).
- `types/ssvc.ts`: `LogFile`, `LogListResponse`, `LogTransferStart`, `LogChunk`, `LogTransferError`,
  `SsvcLogStatus`.
- `menu.svelte`: пункт «Журналы ректификации».
- `tests/unit/api/ssvcApi.logs.test.ts`.

## Что переносим во фронтенде (1.8/1.9)

- `preempt_cnt`, `cfg_chgd`, `tts` в телеметрии (`routes/oc/telemetry/Ssvc.svelte`).
- Гейты по `api_features`: `formula_auto92` (значение 2, «Требуется API 1.9»), `formula`
  (`formula_start_temp`).
- Секция `release_speed`/`release_timer`/`heads_final` по `is_support_release`.
- `formula` как число 0/1/2 в `GeneralSettings`, `WizardStep1`, `ProfileEditor`, `DistillationCycleModel`.
- Инфраструктура версий: `apiVersionCode`, `ApiVersionGuard`, `SsvcApiWarning`, `versioning.ts`,
  `ssvcHelper.ts`.

## Проверка

1. `git grep -n "GET_LOG\|get_log\|SsvcLogProtocol\|rest/logs\|getLog\|RectificationLogs"` — пусто.
2. Backend: сборка PlatformIO; нативные тесты `test/native/test_uart_api_spec` и embedded
   `test/embedded/test_uart_api` (ожидается `"api": "1.9"`); `test_ssvc_log_protocol` отсутствует;
   `platformio.ini` — `gnu++14`.
3. Frontend: `npm run build` и `npm test` в `interface/`.
4. `git log --oneline main..feature/api-v1.9`, `git diff feature/api-v1.10-update..feature/api-v1.9` —
   ровно удаление 1.10 и понижение целевой версии.

## Примечания

- В коммите `8014ff6` поле `formula` в `SsvcSettings.h` объявлено как `bool`; на `int` его меняет
  только `4b47a00`. Без этого переноса значение `formula=2` («Авто 92+») схлопнется в `1` —
  правку типа нужно перенести обязательно.
- Спецификация 1.9 в апстриме содержит пример `VERSION` с `"api": "1.7"` и несколько расхождений
  (единицы `hearts_timer`, `formula=2` в блоке ошибочных команд). В `docs/ssvc_uart_api_docs.md`
  кладётся текст апстрима; поправлено только поле `"api"` в примере — на `1.9`.
- `mkdocs.yml` до этой ветки ссылался на удалённую доку `develop/ssvc-api-v1.6/api_docs.md` —
  ссылка чинится на `ssvc_uart_api_docs.md`.
