# Для разработчиков

Краткая карта раздела: от сборки прошивки и тестов до REST API, фронтенда и протокола обмена с SSVC по UART.

## Сборка и инфраструктура

- [Сборка и CI](build.md) — PlatformIO, Docker, релизы и документация прошивок.
- [Тестирование](testing.md) — нативные и встроенные тесты, смоук по HTTP.
- [Мониторинг I2C](i2c_monitoring.md) — шина и устройства на плате.

## API и интеграция

- [REST API](api/index.md) — эндпоинты OpenConnect.
- [UART API SSVC 1.6 (справка)](ssvc-api-v1.6/api_docs.md) — форматы и совместимость с контроллером SSVC.
- [Протокол UART SSVC](../ssvc-serial-protocol/develop/Readme.md) — примеры запросов, телеметрии и ответов (JSON).

## Фронтенд

- [Структура и SCSS](frontend/frontend.md) — слои стилей и классы.
- [Стратегия стилей (Tailwind + DaisyUI)](frontend/styling.md) — как писать новый UI.
- [Стили настроек OpenConnect](frontend/oc-settings-styles.md) — заметки по разделу настроек.
