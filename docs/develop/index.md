# Для разработчиков

Краткая карта раздела: от сборки прошивки и тестов до REST API, фронтенда и протокола обмена с SSVC по UART.

## Сборка и инфраструктура

- [Сборка и CI](build.md) — PlatformIO, Docker, релизы и документация прошивок.
- [Тестирование](testing.md) — нативные и встроенные тесты, смоук по HTTP.
- [Превью документации из PR](docs-previews.md) — список открытых PR со ссылками на опубликованные превью на GitHub Pages.
- [Мониторинг I2C](i2c_monitoring.md) — шина и устройства на плате.

## API и интеграция

- [REST API](api/index.md) — эндпоинты OpenConnect.
- [UART API SSVC (справка)](ssvc-api-v1.6/api_docs.md) — команды, поля JSON, версии API.
- [Протокол UART — примеры (JSON/TXT)](../ssvc-serial-protocol/develop/Readme.md) — текстовые примеры и ссылки на файлы рядом с прошивочным протоколом (дополнение к справке).

## Фронтенд

- [Структура и SCSS](frontend/frontend.md) — слои стилей и классы.
- [Стратегия стилей (Tailwind + DaisyUI)](frontend/styling.md) — как писать новый UI.
