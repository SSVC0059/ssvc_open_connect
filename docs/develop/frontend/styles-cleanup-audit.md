# Аудит для чистки стилей (будущие задачи)

**Цель:** зафиксировать список задач по приведению стилей в порядок **без обязательных изменений в коде на текущем этапе**. Выполнение — по отдельным задачам/PR после утверждения стратегии ([styling.md](./styling.md)).

Выполнять пункты **последовательно**; отмечать чекбоксы по мере выполнения.

---

## 1. Неиспользуемые или неочевидные SCSS-модули

- [x] **`_buttons.scss`** — подтверждено: не подключался нигде; стили дублированы в `+layout.svelte` (`:global(.btn)`, `.btn-primary`, `.edit-button`, `.values-container` и т.д.). Файл удалён.
- [x] **`_modals.scss`** — подтверждено: нигде не подключался. Стили модалок задаются: в компонентах через `:global()` (ThresholdCalculatorModal, EditParallelV3, EditNumbersModal, EditTimeModal); для wizard — из `_wizard.scss` (StartWizard, ChangeStageModal); адаптив — из `_responsive.scss` (open-connect-main). Файл удалён.
- [x] **`_tabs.scss`** — подтверждено: нигде не подключался. Стили вкладок задаются локально в `oc/ssvc/Settings.svelte` и `oc/settings/Settings.svelte` (полная копия в `<style>` каждого). Переопределения для тёмной темы — в `_dark-theme.scss` (open-connect-main). Файл удалён.
- [x] **`_inputs.scss`** — подтверждено: нигде не подключался. Стили полей ввода и лейблов задаются в `+layout.svelte` (`:global(.input-label)`, `.input-field`, `.input-cell`, `.time-input`, `.input-wrapper`, `.modal-actions`, `.info-text`). Класс `.settings-description` — в `_settings-grid.scss` (open-connect-main). Файл удалён.

---

## 2. Дублирование и конфликты имён

- [x] **Класс `.btn`** — решение: файл `_buttons.scss` удалён; единственный источник стилей кнопок — `+layout.svelte` (и DaisyUI где используются его классы).
- [x] **Модальные классы** — после удаления `_modals.scss`: базовые стили в своих модалках задают 4 компонента (`:global()`); wizard-модалки — из `_wizard.scss`; глобальные адаптивные правки — `_responsive.scss`. Дублирование в виде отдельного неиспользуемого файла убрано.

---

## 3. Глобальный SCSS и специфичность

- [ ] **`_dark-theme.scss`** — запланировать постепенный перевод длинных селекторов на **классы/атрибуты темы DaisyUI** (см. аудит П.3 ниже), чтобы уменьшить зависимость от структуры DOM.
- [ ] **Список подключаемых в main файлов** — зафиксировать в комментариях в `open-connect-main.scss` или в документации актуальный набор (base, layout, themes, animations). После чистки неиспользуемых модулей обновить описание в [styles-audit-and-plan.md](./styles-audit-and-plan.md) и в этом файле.

---

## Аудит П.3: _dark-theme.scss (детальный отчёт)

**Файл:** `interface/src/lib/styles/themes/_dark-theme.scss`. Подключается через `open-connect-main.scss`. Переменные тёмной темы (`--dark-surface-color`, `--dark-footer-bg`, `--dark-border-color` и т.д.) заданы в `app.css` в блоке `@media (prefers-color-scheme: dark) :root`.

### 3.1 Группы правил и привязка к коду

| Группа | Селекторы | Назначение | Где используется DOM |
|--------|-----------|------------|----------------------|
| **Панели настроек** | `.settings-container`, `.settings-panel`, `.settings-grid` | Рамка и тень в тёмной теме | Глобально: классы из `_settings-grid.scss` (main), используются на страницах SSVC/OC настроек, в wizard и т.д. |
| **Wizard: подписи** | `body .wizard .wizard-content .settings-item .input-label`, `body .wizard .wizard-content .input-label` | Цвет подписей в мастере | **StartWizard.svelte** (класс `.wizard`), шаги с `.wizard-content`; **WizardStep1–7**, **ChangeStageModal** — подключают setting-containers, используют `.input-label`. Переопределяют scoped-стили компонентов. |
| **Wizard: таблицы** | `body .wizard .wizard-content .valve-table thead th` | Цвет заголовков таблиц в визарде | **WizardStep4, 5, 6, 7**, **ChangeStageModal** — рендерят `.valve-table` (стили из `_valve-table.scss`). |
| **OC Settings: профили** | `body .container .tabs-container .profile-list-header h3`, `.profile-list-item .profile-name`, `.profile-list-item.active` | Заголовок списка и имена профилей, активный элемент | **oc/settings/Settings.svelte** (корневой `.container`, `.tabs-container`); **ProfileManager.svelte** (`.profile-list-header`, `.profile-list-item`, `.profile-name`). Класс `.container` из `_container.scss` (main). |
| **OC Settings: подписи** | `body .container .tabs-container .input-label` | Подписи полей во вкладках (в т.ч. Telegram) | **oc/settings/Settings.svelte** (та же структура); контент вкладки — **TelegramSettings.svelte**, **SensorsSettings** и др. с `.input-label`. |
| **Телеметрия** | `body .telemetry-container .status-bar .status-item .label`, `body .telemetry-container .glassmorphism .controls-container .sensor-readings .readings-list .reading-item .reading-label/.reading-value`, `body .telemetry-container ... .panel-title` (несколько путей) | Цвет подписей, значений и заголовков панелей на странице телеметрии | **oc/telemetry/Ssvc.svelte** — единственный источник: корень `.telemetry-container`, вложенные `.status-bar`, `.glassmorphism`, `.controls-container`, `.sensor-readings`, `.readings-list`, `.reading-item`, `.panel`, `.parameters-readings`, `.panel-title`, `.main-content`, `.responsive-grid`, `.sidebar-left/.right`, `.center-panel`. Длинные пути нужны, чтобы перебить scoped-стили компонента (высокая специфичность). |
| **Общие** | `.settings-title`, `.valve-card .settings-title` | Заголовки секций | Используются в layout настроек и в шагах wizard (valve-card, valve-table-title). |
| **Поля ввода** | `.input-label`, `.input-field`, `.input-cell` | Фон, рамка, цвет текста в тёмной теме | Глобально: классы задаются в `+layout.svelte`, используются во многих формах. |
| **Таблицы и карточки** | `.valve-table-title`, `.valve-table` (th/td, thead th, .input-readonly), `.valve-info` | Таблицы клапанов, readonly-ячейки, блок информации | **WizardStep4–7**, **ChangeStageModal** — подключают `_valve-table.scss`; классы `.valve-card` из `_setting-containers.scss`. |

### 3.2 Зависимости от структуры DOM

- **Длинные селекторы с `body`** используются, чтобы переопределить **scoped-стили Svelte**: у сгенерированных классов специфичность выше, чем у простых глобальных правил. Комментарии в файле явно указывают: «body повышает специфичность выше, чем у scoped».
- **Риски:** при изменении разметки (переименование класса, перестановка вложенности, перенос в другой компонент) правила тёмной темы перестают срабатывать. Наиболее хрупкие — телеметрия (много вложенных классов в одном компоненте) и цепочка `.container .tabs-container` (зависит от того, что страницы Settings оборачивают контент в `.container` и `.tabs-container`).

### 3.3 Рекомендации для постепенной миграции

1. **Приоритет по компонентам:** сначала страницы с короткими путями и одним источником (например, общие `.input-label` / `.input-field` уже можно дублировать в компонентах через Tailwind `dark:` или семантику DaisyUI), затем wizard, затем OC Settings (профили + вкладки), в конце — телеметрия (самая длинная цепочка).
2. **Подход:** в переводимых компонентах заменить зависимость от глобального `_dark-theme.scss` на **классы и атрибуты темы DaisyUI** (`data-theme`, семантические классы цветов). Утилиты Tailwind с `dark:` не используем для темы — только DaisyUI. После перевода удалить соответствующие блоки из `_dark-theme.scss`, чтобы не дублировать стили.
3. **Переменные:** `--dark-*` из `app.css` можно по мере миграции заменять на переменные темы DaisyUI или оставить для обратной совместимости в оставшихся SCSS-блоках.
4. **Проверка:** после каждого изменения включать `prefers-color-scheme: dark` и проверять страницы: Wizard (все шаги), OC → Настройки (профили, Telegram), OC → Телеметрия.

### 3.4 Итог аудита

- Файл один, но тесно привязан к разметке не менее чем **трёх зон**: wizard, OC Settings (container + tabs + ProfileManager/вкладки), телеметрия (Ssvc.svelte).
- Постепенный перевод на DaisyUI по зонам снизит хрупкость и позволит со временем сократить или убрать длинные селекторы в `_dark-theme.scss`.

### 3.5 План перехода на классы/атрибуты DaisyUI (последовательно)

Выполнять по порядку; после каждого пункта проверять тёмную тему (`prefers-color-scheme: dark`) на затронутых экранах.

**Подготовка**

- [ ] Проверить в `app.css` темы DaisyUI: **corporate** (по умолчанию), **business** (`prefers-color-scheme: dark`). Убедиться, что переключение темы работает до изменений.

**Этап 1 — Общие правила (глобальные стили)**

- [x] **Панели настроек:** в `_settings-grid.scss` добавлены border и box-shadow через `oklch(var(--b3))`, `oklch(var(--bc) / 0.12)`. Блок с этими классами удалён из `_dark-theme.scss`.
- [x] **Поля ввода и лейблы:** в `+layout.svelte` стили переведены на семантику DaisyUI (`--bc`, `--b1`, `--b3`, `--p`). Блоки `.input-label`, `.input-field`, `.input-cell` удалены из `_dark-theme.scss`.
- [x] **Заголовки секций:** в `_settings-grid.scss` для `.settings-title` и `.valve-card .settings-title` задан `color: oklch(var(--bc))`. Правило удалено из `_dark-theme.scss`. Описание `.settings-description` переведено на `oklch(var(--bc) / 0.8)`.

**Этап 2 — Wizard (подписи и таблицы)**

- [x] **Подписи в визарде:** в `_setting-containers.scss` переведены `.settings-toggle`, `.settings-label`, `.settings-value`, `.read-only .input-label` и др. на `oklch(var(--bc))`. Селекторы `body .wizard .wizard-content .settings-item .input-label`, `body .wizard .wizard-content .input-label` удалены из `_dark-theme.scss`.
- [x] **Таблицы в визарде:** в `_valve-table.scss` заголовки и ячейки переведены на семантику DaisyUI (`--bc`, `--b2`, `--b3`, `--p`). Селектор `body .wizard .wizard-content .valve-table thead th` удалён из `_dark-theme.scss`.

**Этап 3 — Таблицы и карточки клапанов (общие)**

- [x] **Valve-table и valve-info:** стили в `_valve-table.scss` переведены на DaisyUI (`.valve-table-title`, `.valve-table` th/td/thead, `.input-cell`, `.input-readonly`, `.valve-info`). Блоки `.valve-table-title`, `.valve-table`, `.valve-info` удалены из `_dark-theme.scss`.

**Этап 4 — OC Settings (профили и вкладки)**

- [x] **Профили:** в `_profile-manager.scss` переведены `.profile-list-header`, `.profile-list-item`, `.profile-name`, `.active`, `.profile-settings-panel`, `.profile-loading-state`, `.profile-error-state`, `.icon-button` на DaisyUI (`--bc`, `--b2`, `--b3`, `--p`, `--in`, `--er`). Селекторы `body .container .tabs-container .profile-list-header h3`, `body .container .tabs-container .profile-list-item .profile-name`, `body .container .tabs-container .profile-list-item.active` удалены из `_dark-theme.scss`.
- [x] **Подписи во вкладках OC Settings:** подписи `.input-label` во вкладках наследуют глобальные стили из `+layout.svelte` (Этап 1). Селектор `body .container .tabs-container .input-label` удалён из `_dark-theme.scss`.

**Этап 5 — Телеметрия (последний приоритет)**

- [x] **Ssvc.svelte:** переведены цвета на семантику DaisyUI: `.status-item .label`, `.panel-title`, `.section-title`, `.reading-label`, `.reading-value` — `color: oklch(var(--bc))`. Удалены `@include dark-theme-color` для этих элементов. Из `_dark-theme.scss` удалены все селекторы `body .telemetry-container ...`.

**Финализация**

- [x] Удалить из `_dark-theme.scss` оставшиеся неиспользуемые блоки; при необходимости оставить только общие переменные или пустой `@media (prefers-color-scheme: dark)`. — Выполнено: селекторов не осталось, оставлен пустой `@media (prefers-color-scheme: dark)` и комментарий.
- [x] Итоговая проверка: Wizard (все шаги), OC → Настройки (профили, Telegram, датчики), OC → Телеметрия в тёмной теме.

---

## 4. Документация

- [x] Обновить [frontend.md](./frontend.md): убрать или пометить устаревшими классы из удалённых SCSS (например кнопки из _buttons.scss, если файл удалён). — В frontend.md добавлен абзац об удалённых модулях и актуальных источниках стилей (+layout.svelte, компоненты).
- [x] В [styling.md](./styling.md) при необходимости добавить краткий пункт «Очищенные/удалённые модули» со ссылкой на этот аудит. — Добавлен раздел 5 с ссылкой на аудит; раздел «Документация и ссылки» перенумерован в 6.

---

## 5. Flowbite-svelte

**Итог:** зависимости `flowbite`, `flowbite-svelte`, `flowbite-svelte-icons` **удалены** из `interface/package.json`. Все использования заменены на DaisyUI (tooltip, loading) и Tabler-иконки.

- [x] **Зафиксировать список компонентов, использующих Flowbite:** ValveParameters (Popover), SensorCard (Spinner), KpnPopover (Popover + иконка) — см. подзадачи ниже.
- [x] **Вынести задачи на замену отдельно** — выполнено по всем пунктам.

**Задачи на замену (все выполнены):**

- [x] **ValveParameters:** заменить Flowbite `Popover` на DaisyUI **tooltip** (выполнено).
- [x] **SensorCard:** заменить Flowbite `Spinner` на DaisyUI loading (выполнено).
- [x] **KpnPopover:** заменить Flowbite `Popover` и иконку на DaisyUI **tooltip** + Tabler `help-circle` (выполнено).

### 5.1 Единый спиннер: DaisyUI loading везде

**Цель:** во всех компонентах использовать один и тот же индикатор загрузки — **DaisyUI loading** (классы `loading loading-spinner` и при необходимости обёртка), чтобы убрать зависимость от Flowbite и от локальных `Spinner.svelte` / `LoadingSpinner.svelte`.

**Текущее состояние (после замены):** везде используется **DaisyUI loading** (`loading loading-spinner`). Локальные `Spinner.svelte` и `LoadingSpinner.svelte` в перечисленных компонентах заменены; при необходимости эти файлы можно удалить или пометить устаревшими.

**Подзадачи (замена на DaisyUI loading по одному компоненту / PR по желанию):**

- [x] **SensorCard.svelte** — заменить Flowbite `Spinner` на DaisyUI loading (2 вхождения) (выполнено).
- [x] **TelegramSettings.svelte** — заменить `LoadingSpinner` на DaisyUI loading (выполнено).
- [x] **StartWizard.svelte** — заменить `LoadingSpinner` на DaisyUI loading (выполнено).
- [x] **SensorsSettings.svelte** — заменить `LoadingSpinner` на DaisyUI loading (выполнено).
- [x] **ProfileManager.svelte** — заменить `Spinner` на DaisyUI loading (выполнено).
- [x] **ProfileSelector.svelte** — заменить `Spinner` на DaisyUI loading (выполнено).
- [x] **Wifi.svelte** (routes/wifi/sta) — заменить `Spinner` на DaisyUI loading (выполнено).
- [x] **user/+page.svelte** — заменить `Spinner` на DaisyUI loading (выполнено).
- [x] **GithubFirmwareManager.svelte** — заменить `Spinner` на DaisyUI loading (выполнено).
- [x] **Accesspoint.svelte** (routes/wifi/ap) — заменить `Spinner` на DaisyUI loading (2 вхождения) (выполнено).
- [x] **SystemStatus.svelte** — заменить `Spinner` на DaisyUI loading (выполнено).
- [x] **NTP.svelte** (routes/connections/ntp) — заменить `Spinner` на DaisyUI loading (выполнено).
- [x] **MQTTConfig.svelte** — заменить `Spinner` на DaisyUI loading (выполнено).
- [x] **MQTT.svelte** — заменить `Spinner` на DaisyUI loading (выполнено).

**Финализация (выполнено):** зависимости `flowbite`, `flowbite-svelte`, `flowbite-svelte-icons` удалены из `package.json`. При необходимости удалить или пометить устаревшими `Spinner.svelte` и `LoadingSpinner.svelte` (они больше не импортируются в перечисленных компонентах).

---

## 6. Критерии выполнения чистки (итоговая проверка)

- [x] **Нет SCSS-файлов, которые нигде не подключаются и не упоминаются в документации как «намеренно отключённые».**
  - В `open-connect-main.scss` подключаются: base (variables, mixins, animations), layout (container, settings-grid), themes (dark-theme, responsive). Компоненты точечно подключают: setting-containers, valve-table, profile-manager, wizard, base/variables, base/mixins.
  - Файлы **`_sensors-view.scss`**, **`_sensor-card.scss`**, **`_toggles.scss`** нигде не импортируются (в open-connect-main помечены как «перенесены в компоненты»). Здесь фиксируем: они **намеренно не подключаются** (стили заданы в компонентах). При необходимости их можно удалить отдельной задачей после проверки внешнего вида.
- [x] **Нет двух разных источников стилей для одних и тех же имён классов (кнопки, модалки, вкладки) без явного решения.** Кнопки — единственный источник `+layout.svelte` (и DaisyUI); модалки — компоненты + _wizard.scss + _responsive.scss; вкладки — локально в oc/settings и oc/ssvc Settings. Решения зафиксированы в п. 1–2 аудита.
- [x] **Документация (frontend.md, styling.md, аудит) соответствует фактическому набору файлов и стратегии.** frontend.md описывает удалённые модули и актуальные источники; styling.md — стратегию Tailwind/DaisyUI и итог по Flowbite; аудит отражает выполнение п. 1–5 и список подключаемых модулей.

---

Все изменения по этому аудиту выполняются **отдельно от** фиксации стратегии и только после согласования.

---

## Инструкция: проверка корректности изменений (П.1, _buttons.scss)

После удаления `interface/src/lib/styles/components/_buttons.scss` проверьте:

1. **Сборка**
   - В каталоге `interface` выполните `npm run build`. Сборка должна завершиться без ошибок (файл нигде не импортировался).
   - При необходимости `npm run dev` — приложение должно запуститься без ошибок.

2. **Внешний вид кнопок и связанных блоков**
   - Убедитесь, что стили по-прежнему задаются из `+layout.svelte` (секция `:global(.btn)`, `.btn-primary`, `.edit-button`, `.values-container` и т.д.). Проверьте в браузере:
     - **Кнопки:** страницы с `btn btn-primary`, `btn btn-success`, `btn btn-back`, `btn btn-warning` (например Wizard, настройки SSVC/OC, телеметрия, модалки «Сохранить»/«Отмена», логин, wifi, system).
     - **Блоки значений:** компоненты с `values-container`, `value-item`, `value-text`, `value-unit` (например EditNumbersModal, EditTimeModal).
     - **Кнопки-иконки:** элементы с классом `edit-button` (ParallelValve, TelegramSettings, EditNumbersModal, EditTimeModal).
   - Светлая и тёмная тема (если включена): кнопки и текст в value-unit/value-text должны оставаться читаемыми.

3. **Поиск по коду**
   - Убедитесь, что в репозитории нет оставшихся импортов: поиск по `buttons.scss`, `_buttons`, `styles/.*buttons` в `interface/src` не должен находить ссылок на удалённый файл.

---

## Инструкция: проверка корректности изменений (П.1, _modals.scss)

После удаления `interface/src/lib/styles/components/_modals.scss` проверьте:

1. **Сборка**
   - В каталоге `interface` выполните `npm run build`. Сборка должна завершиться без ошибок.
   - При необходимости `npm run dev` — приложение должно запуститься без ошибок.

2. **Внешний вид модальных окон**
   - **ThresholdCalculatorModal** (OC → Настройки → датчики → «Авторасчёт порогов»): overlay, диалог, заголовок, тело, кнопки действий.
   - **EditParallelV3** (SSVC → Настройки → параллельный клапан): открытие модалки редактирования.
   - **EditNumbersModal** (SSVC → Настройки): модалка ввода чисел.
   - **EditTimeModal** (SSVC → Настройки → GeneralSettings): модалка «Изменение времени».
   - **StartWizard** и **ChangeStageModal**: заголовки модалок (стили из `_wizard.scss`).
   - На узком экране (или при max-width: 640px): проверьте, что `.modal-content` и `.modal-actions` ведут себя корректно (стили из `_responsive.scss` в open-connect-main).

3. **Поиск по коду**
   - Поиск по `modals.scss`, `_modals`, `styles/.*modals` в `interface/src` не должен находить ссылок на удалённый файл.

---

## Инструкция: проверка корректности изменений (П.1, _tabs.scss)

После удаления `interface/src/lib/styles/components/_tabs.scss` проверьте:

1. **Сборка**
   - В каталоге `interface` выполните `npm run build`. Сборка должна завершиться без ошибок.
   - При необходимости `npm run dev` — приложение должно запуститься без ошибок.

2. **Внешний вид вкладок**
   - **OC → SSVC → Настройки** (`/oc/ssvc/settings`): блок с вкладками (десктоп — горизонтальные вкладки, активная с подчёркиванием; на узком экране — выпадающее мобильное меню с классом `menu-open`, кнопка переключения, список `mobile-tab` / `mobile-tab-active`).
   - **OC → Настройки** (`/oc/settings`): вкладки «Профили», «Telegram» и др. — та же структура (.tabs-container, .tabs-nav, .tab, .tab-active, мобильное меню).
   - Тёмная тема: цвета заголовков и пунктов внутри `.tabs-container` (профили, подписи) должны соответствовать правилам из `_dark-theme.scss` (селекторы `body .container .tabs-container ...`).

3. **Поиск по коду**
   - Поиск по `tabs.scss`, `_tabs`, `styles/.*tabs` в `interface/src` не должен находить ссылок на удалённый файл.

---

## Инструкция: проверка корректности изменений (П.1, _inputs.scss)

После удаления `interface/src/lib/styles/components/_inputs.scss` проверьте:

1. **Сборка**
   - В каталоге `interface` выполните `npm run build`. Сборка должна завершиться без ошибок.
   - При необходимости `npm run dev` — приложение должно запуститься без ошибок.

2. **Внешний вид полей ввода и лейблов**
   - Стили по-прежнему задаются из `+layout.svelte` (секция «Поля ввода и лейблы (shared)»). Проверьте в браузере:
     - **Профили:** OC → Настройки → Профили → редактирование профиля (input-label, input-field).
     - **Wizard:** шаги 1–7 (input-label, поля ввода).
     - **SSVC → Настройки:** GeneralSettings, SpeedSettings, ValveBandwidth, ParallelValve (input-label, input-field, input-wrapper).
     - **OC → Настройки → Telegram** (input-label, input-wrapper, input-field, settings-description).
     - **Модалки:** ThresholdCalculatorModal, EditNumbersModal, SensorCard (input-label, input-wrapper, input-field).
     - **Компоненты:** NumberInput, TimeInput, ProfileSelector, SsvcInputGroup (input-field, input-wrapper).
   - Фокус на полях: обводка и тень (синяя) при :focus.
   - Тёмная тема: читаемость текста и фона полей (правила из layout + _dark-theme).

3. **Поиск по коду**
   - Поиск по `inputs.scss`, `_inputs`, `styles/.*inputs` в `interface/src` не должен находить ссылок на удалённый файл.
