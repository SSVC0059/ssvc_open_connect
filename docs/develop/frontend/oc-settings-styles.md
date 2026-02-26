## Структура и стили страницы `/oc/settings`

### 1. Структура маршрута

- **Глобальный лейаут** (`interface/src/routes/+layout.svelte`):
  - Каркас приложения (DaisyUI `drawer`, `Statusbar`, `Menu`).
  - Подключения: `app.css`, `normalize.css`, `open-connect-main.scss`.
  - Глобальные стили:
    - Кнопки: `:global(.btn)`, `.btn-primary`, `.btn-warning`, `.btn-success`, `.btn-back`, `.btn-icon`.
    - Поля ввода: `:global(.input-label)`, `.input-field`, `.input-cell`, `.time-input`, `.input-wrapper`.
    - Модалки / вспомогательные элементы: `:global(.modal-actions)`, `.info-text`.

- **Страница `/oc/settings`**:
  - `interface/src/routes/oc/settings/+page.svelte`:
    - Контейнер страницы: `mx-0 my-1 flex flex-col space-y-4 sm:mx-8 sm:my-8` (Tailwind).
    - Внутри рендерит `<Settings />`.
  - `interface/src/routes/oc/settings/Settings.svelte`:
    - Логика вкладок: `availableTabs`, `filteredTabs`, `activeTab`, фильтрация по `SubsystemsState`.
    - Разметка:
      - Корневой блок вкладок: `.tabs-container` внутри `.container`.
      - Вкладки реализованы через **DaisyUI radio tabs-lift + tab content** (`tabs tabs-lift`):
        - Для каждой вкладки: `input type="radio" name="oc_settings_tabs" role="tab" class="tab ..."`.
        - Контент рядом: `div role="tabpanel" class="tab-content ..."` с соответствующим компонентом.
        - Активный таб получает фон `background-color: var(--color-primary)` (см. локальный SCSS).
      - Контент вкладок рендерит:
        - `ProfileManager` (вкладка «Профили»).
        - `SensorsSettings` (датчики температуры/давления).
        - `TelegramSettings` (настройки Telegram).
    - Локальный SCSS:
      - `.tabs-container` — колонка, плюс усиленная рамка/тень для вложенного `.settings-container`.
      - Глобальные оверрайды DaisyUI:
        - `:global(.tabs .tab)` — размер и жирность шрифта (как `h3`).
        - `:global(.tabs .tab:checked)` — фон активного таба (`--color-primary`).
        - `:global(.tabs.tabs-lift)` и `:global(.tabs.tabs-lift .tab)` — нижняя граница под всеми табами.
      - Вспомогательные классы: `.error-text`.

### 2. Вкладки и соответствующие компоненты

- **Вкладка «Профили»**:
  - Компонент: `ProfileManager` (`interface/src/lib/components/profiles/ProfileManager.svelte`).
  - Стили: `@use '$lib/styles/components/profile-manager';` → `_profile-manager.scss`.
  - Основные блоки:
    - Лейаут: `.profile-manager-layout` (+ модификатор `.editing`).
    - Левая колонка (список профилей): `.profile-list-panel`, `.profile-list-header`, `.profile-actions`, `.applied-profile-summary`, `.profile-list`, `.profile-list-item`, `.profile-name-button`, `.profile-name`, `.profile-item-actions` (DaisyUI `dropdown` c кнопкой `...`).
    - Правая колонка (просмотр/редактор): `.profile-settings-panel`, `.profile-editor-grid`, `.profile-settings-placeholder`, `.profile-loading-state`, `.profile-error-state`.
    - Кнопки-иконки: `.icon-button`.

- **Вкладка «Датчики»**:
  - Компонент списка: `SensorsSettings` (`interface/src/lib/components/OCSettings/SensorsSettings.svelte`).
  - Локальный SCSS:
    - Контейнер страницы: `.sensors-main-container`.
    - Панель зоны: `.zone-panel`, `.zone-title`.
    - Сетка карточек: `.sensors-grid`.
    - Состояния: `.loading-container`, `.loading-text`, `.error-container`, `.error-text`.
  - Карточка датчика: `SensorCard` (`interface/src/lib/components/OCSettings/SensorCard.svelte`):
    - Карточка: `.sensor-card`.
    - Шапка: `.sensor-header`, `.sensor-address`, `.sensor-temperature`, `.sensor-value.font-mono`.
    - Компактный бар значений: `.temperature-bar-compact-container`, `.temperature-bar-compact`, `.temperature-fill-compact`, `.temperature-threshold-marker`, `.warning-marker`, `.critical-marker`, `.temperature-labels-compact`.
    - Управление: `.sensor-controls`, `.sensor-actions`, `.zone-controls-compact`, `.threshold-editor`, `.control-actions`.
    - Используются общие классы:
      - Кнопки: `btn`, `btn-primary`, `btn-warning`, `btn-back`.
      - Инпуты: `input-field` (и производные).

- **Вкладка «Telegram»**:
  - Компонент: `TelegramSettings` (`interface/src/lib/components/OCSettings/TelegramSettings.svelte`).
  - Стили:
    - `@use '$lib/styles/components/setting-containers';` → `_setting-containers.scss`.
    - Глобальные layout-стили: `_settings-grid.scss` (через `open-connect-main.scss`).
  - Основные блоки:
    - Корневой контейнер: `.settings-container` (карточка настроек).
    - Строка состояния подсистемы: `.settings-item`, `.input-label-container`, `.input-label`, `.settings-description`, `Toggle`.
    - Секция настроек: `.settings-section`.
    - Группа полей: `.settings-group` (карточка с glassmorphism).
    - Поля ввода: `.settings-item` (в т.ч. модификатор `.settings-item--stacked` для вертикального стека `label → input-wrapper → description`), `.input-wrapper` (ширина 50% на десктопе), `.input-field`, `.edit-button`, `.settings-description`.
    - Нижняя панель действий: `.modal-actions` + `btn btn-primary`.

### 3. Глобальные и общие SCSS, влияющие на `/oc/settings`

- `open-connect-main.scss`:
  - Точка входа SCSS, подключает:
    - `base`: `_variables.scss`, `_mixins.scss`, `_animations.scss`.
    - `layout`: `_container.scss`, `_settings-grid.scss`.
    - `components`: `_setting-containers.scss`, `_valve-table.scss`, `_profile-manager.scss` и др.
- `layout/_settings-grid.scss`:
  - Карточки/панели настроек: `.settings-container`, `.settings-panel`, `.settings-section`.
  - Сетка: `.settings-grid` (1 или 2 колонки в зависимости от брейкпоинта).
  - Заголовки/описания: `.settings-title` (общий паттерн, **на вкладках `/oc/settings` сейчас используются обычные `h2` без класса**), `.settings-description`.
- `components/_setting-containers.scss`:
  - Строки настроек: `.settings-group` (карточка с glassmorphism, padding, border), `.settings-toggle`, `.settings-item`, модификатор `.settings-item--stacked`, `.settings-label`, `.settings-value`, `.toggle-container`.
  - Спец-кейс для карточек клапанов: `.valve-card`.
  - Read-only: `.read-only` и глобальный `body .read-only, body .read-only *`.
- `components/_profile-manager.scss`:
  - Паттерн двухколоночного редактора профилей и плиток в списке, плюс контейнер `applied-profile-summary` и стили для состояний `.active` и `.applied`.

### 4. Таблица элементов интерфейса и стилей

#### 4.1 Вкладки и обёртка

| Элемент / блок                      | Компонент / файл                           | Классы / стили                                           | Источник |
|-------------------------------------|--------------------------------------------|----------------------------------------------------------|---------|
| Внешний контейнер страницы          | `+page.svelte`                             | `mx-0 my-1 flex flex-col space-y-4 sm:mx-8 sm:my-8`      | Tailwind |
| Корневой контейнер вкладок          | `Settings.svelte`                          | `container`, `.tabs-container`                           | Локальный SCSS |
| DaisyUI-вкладки                     | `Settings.svelte`                          | `tabs tabs-lift`, `tab`, `tab-content`, `tab:checked`    | DaisyUI + локальный SCSS |
| Общий контур контента вкладок       | `Settings.svelte` + `_settings-grid.scss`  | `.tabs-container .settings-container`                    | Локальный + layout |

#### 4.2 Карточки настроек и сетка

| Элемент / блок                  | Компонент / файл                  | Классы / стили                          | Источник |
|---------------------------------|-----------------------------------|-----------------------------------------|---------|
| Карточка настроек              | `TelegramSettings` и др.          | `.settings-container`                   | `_settings-grid.scss` |
| Панель внутри сетки            | Общие настройки                   | `.settings-panel`                       | `_settings-grid.scss` |
| Секция настроек                | `TelegramSettings`                | `.settings-section`                     | `_settings-grid.scss` |
| Заголовок секции               | Разные настройки                  | `h2` (без `settings-title`)             | Локальный SCSS / браузер по умолчанию |
| Описание настройки             | Разные настройки                  | `.settings-description`                 | `_settings-grid.scss` |
| Двуколоночная сетка панелей    | Общий layout                      | `.settings-grid`                        | `_settings-grid.scss` |

#### 4.3 Строки настроек, формы, состояния

| Элемент / блок               | Компонент / файл            | Классы / стили                                             | Источник |
|------------------------------|-----------------------------|------------------------------------------------------------|---------|
| Группа настроек             | `TelegramSettings`          | `.settings-group`                                          | `_setting-containers.scss` |
| Строка настройки            | `TelegramSettings`          | `.settings-item`, модификатор `.settings-item--stacked`    | `_setting-containers.scss` |
| Подписи/значения            | `TelegramSettings`          | `.settings-label`, `.settings-value`                       | `_setting-containers.scss` |
| Описание под строкой        | `TelegramSettings`          | `.settings-description`                                    | `_settings-grid.scss` |
| Тумблер подсистемы          | `TelegramSettings`          | `.toggle-container` + `Toggle`                             | `_setting-containers.scss` + DaisyUI |
| Подпись к полю ввода        | Разные компоненты           | `.input-label`                                             | `+layout.svelte` (global) |
| Поля ввода                  | Разные компоненты           | `.input-field`, `.input-cell`, `.time-input`              | `+layout.svelte` (global) |
| Обёртка поля                | Разные компоненты           | `.input-wrapper`                                           | `+layout.svelte` (global) |
| Кнопка-иконка у поля        | `TelegramSettings`          | `.edit-button`                                             | `+layout.svelte` (global) |
| Кнопки действий внизу       | `TelegramSettings`, модалки | `.modal-actions` + `btn btn-primary/btn-warning/btn-back` | `+layout.svelte` (global) |
| Read-only состояние          | Любые настройки             | `.read-only`, `body .read-only, body .read-only *`        | `_setting-containers.scss` |

#### 4.4 Вкладка «Профили»

| Элемент / блок                 | Компонент / файл      | Классы / стили                            | Источник |
|--------------------------------|-----------------------|-------------------------------------------|---------|
| Общий лейаут вкладки          | `ProfileManager`      | `.profile-manager-layout`, `.editing`     | `_profile-manager.scss` |
| Левая панель со списком       | `ProfileManager`      | `.profile-list-panel`, `.profile-list-header`, `.profile-actions`, `.applied-profile-summary`, `.profile-list` | `_profile-manager.scss` |
| Элемент списка профилей       | `ProfileManager`      | `.profile-list-item`, `.profile-name-button`, `.profile-name`, `.profile-item-actions`, состояния `.active` / `.applied` | `_profile-manager.scss` |
| Правая панель (контент)       | `ProfileManager`      | `.profile-settings-panel`, `.profile-editor-grid`, `.profile-settings-placeholder` | `_profile-manager.scss` |
| Состояния загрузки/ошибки     | `ProfileManager`      | `.profile-loading-state`, `.profile-error-state`          | `_profile-manager.scss` |
| Кнопки-иконки                 | `ProfileManager`      | `.icon-button`                                            | `_profile-manager.scss` |

#### 4.5 Вкладка «Датчики»

| Элемент / блок                 | Компонент / файл      | Классы / стили                                               | Источник |
|--------------------------------|-----------------------|--------------------------------------------------------------|---------|
| Контейнер вкладки             | `SensorsSettings`     | `.sensors-main-container`                                   | Локальный SCSS |
| Панель зоны                   | `SensorsSettings`     | `.zone-panel`, `.zone-title`                                | Локальный SCSS + mixins |
| Сетка карточек                | `SensorsSettings`     | `.sensors-grid`                                             | Локальный SCSS |
| Состояния загрузки/ошибки     | `SensorsSettings`     | `.loading-container`, `.loading-text`, `.error-container`, `.error-text` | Локальный SCSS + DaisyUI |
| Карточка датчика              | `SensorCard`          | `.sensor-card`, `.sensor-header`, `.sensor-address`, `.sensor-temperature`, `.sensor-value.font-mono` | Локальный SCSS |
| Бар значений и порогов        | `SensorCard`          | `.temperature-bar-compact-container`, `.temperature-bar-compact`, `.temperature-fill-compact`, `.temperature-threshold-marker`, `.warning-marker`, `.critical-marker`, `.temperature-labels-compact` | Локальный SCSS |
| Управление порогами/зонами    | `SensorCard`          | `.sensor-controls`, `.sensor-actions`, `.zone-controls-compact`, `.threshold-editor`, `.control-actions` | Локальный SCSS |
| Кнопки действий               | `SensorCard`          | `btn btn-primary`, `btn btn-warning`, `btn btn-back`        | `+layout.svelte` (global) |

> Модуль `_valve-table.scss` описывает табличный паттерн (`.valve-table`, `.valve-table-title` и т.п.) и сейчас **не используется напрямую** в `/oc/settings`, но применён в wizard и может быть переиспользован при появлении табличных блоков на вкладках.

### 5. Паттерны организации элементов

- **Карточки и панели**:
  - Общая карточка настроек: `.settings-container` / `.settings-panel` / `.settings-section` / `h2` / `.settings-description`.
  - Плитки в списке профилей: `.profile-list-item` с бордером, тенью и состояниями `.active` / `.applied` / `.disabled`; активный профиль подсвечивается `--color-primary`, имя текущего применённого профиля дополнительно выводится в `.applied-profile-summary`.
  - Карточки датчиков: `.sensor-card` с header-блоком и визуальным баром значений.

- **Сетки и лейауты**:
  - Сетка настроек: `.settings-grid` (1–2 колонки).
  - Сетка датчиков: `.sensors-grid` (`auto-fit`/`minmax`).
  - Двухколоночный редактор профилей: `.profile-manager-layout` с переключением колонок через модификатор `.editing`.

- **Формы и строки настроек**:
  - Строки настроек: `.settings-item` + глобальные `.input-label`, `.input-field`, `.input-wrapper`.
  - Read-only паттерн: `.read-only` на контейнере + глобальный селектор `body .read-only, body .read-only *`.
  - Нижние панели действий: `.modal-actions` + `btn`-классы (`.btn-primary`, `.btn-warning`, `.btn-back`, `.btn-success`).

### 6. Маршрут `/network` (MQTT / NTP)

Маршрут `/network` повторяет паттерны, описанные выше для `/oc/settings`:

- **Страница `interface/src/routes/network/+page.svelte`**:
  - Использует тот же паттерн вкладок DaisyUI (`tabs tabs-lift` + `tab` + `tab-content`), что и `oc/settings/Settings.svelte`.
  - Обёртка вкладок: `.tabs-container` с такими же отступами и нижней рамкой под табами.
- **Компонент `NtpSettings.svelte`**:
  - Статус и настройки NTP оформлены через `settings-container` → `settings-grid` → `settings-panel` → `settings-section`.
  - Блок настроек использует `settings-group` / `settings-item` / `settings-item--stacked`, глобальные `input-label`, `input-wrapper`, `settings-description` и паттерн `read-only` при выключенной подсистеме.
- **Компонент `MqttSettings.svelte`**:
  - Статус MQTT и настройки клиента оформлены идентично NTP, на тех же контейнерах (`settings-container` / `settings-grid` / `settings-panel` / `settings-section` / `settings-group` / `settings-item` / `settings-item--stacked`).
  - Для переключения подсистемы используется `Toggle` в строке `.settings-item` с подписью (`input-label` + `settings-description`) и правым выравниванием тумблера по аналогии с NTP и Telegram.

