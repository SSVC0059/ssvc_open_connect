# Аудит стилей и план по Issue #97

**Цель:** выбор основной системы стилей (Tailwind vs SCSS) и документирование (часть #70). Соответствует [Issue #97](https://github.com/SSVC0059/ssvc_open_connect/issues/97).

**Статус:** стратегия зафиксирована в [styling.md](./styling.md) (вариант A — Tailwind + DaisyUI). План актуализирован с учётом требований issue, документации в `docs/develop/frontend` и текущего состояния папки `interface`. Часть плана уже выполнена (см. [styles-cleanup-audit.md](./styles-cleanup-audit.md)); ниже — актуальное состояние и оставшиеся шаги.

---

## 1. Текущее состояние (после чистки)

### 1.1 Что подключено и откуда грузится

| Система | Где подключается | Версии / конфиг |
|--------|-------------------|------------------|
| **Tailwind CSS** | `interface/src/app.css` (`@import "tailwindcss"`), `vite.config.ts` (`@tailwindcss/vite`) | tailwindcss ^4.1.11 |
| **DaisyUI** | `app.css`: `@plugin "daisyui"` (themes: corporate — default, business — prefers-dark) | daisyui ^5.0.46 |
| **Flowbite / flowbite-svelte** | Удалены (заменены на DaisyUI tooltip/loading и Tabler) | — |
| **Глобальный SCSS** | `+layout.svelte`: `import '$lib/styles/open-connect-main.scss'` | sass-embedded ^1.93.2 |

### 1.2 Структура SCSS (актуальная)

**Точка входа:** `interface/src/lib/styles/open-connect-main.scss`.

**Подключает в main:**
- **base:** `_variables.scss`, `_mixins.scss`, `_animations.scss`
- **layout:** `_container.scss`, `_settings-grid.scss`
- **themes:** `_dark-theme.scss` (пустой блок после миграции на DaisyUI), `_responsive.scss`

**Подключаются только в компонентах** через `@use` в `<style lang="scss">`:
- `_setting-containers.scss` — Wizard steps 1–7, SsvcSettings, OCSettings, TelegramSettings, ChangeStageModal
- `_valve-table.scss` — Wizard steps 3–7, ChangeStageModal
- `_profile-manager.scss` — ProfileManager
- `_wizard.scss` (views) — StartWizard, ChangeStageModal

**Удалены (чистка по styles-cleanup-audit):** `_buttons.scss`, `_inputs.scss`, `_modals.scss`, `_tabs.scss`. Стили кнопок, полей ввода, модалок и вкладок задаются в `+layout.svelte` (`:global`) и в компонентах.

**Не подключаются ни в main, ни в Svelte** (стили дублированы/перенесены в компоненты; при необходимости можно удалить отдельной задачей):
- `_sensors-view.scss`, `_sensor-card.scss`, `_toggles.scss`

### 1.3 Использование по типам файлов

- **Svelte с Tailwind/DaisyUI в разметке:** menu, login, wifi, system, user, часть страниц (классы `btn`, `btn-ghost`, `menu`, `rounded-box`, `bg-base-200`, `text-base-content` и т.д.).
- **Svelte с scoped SCSS:** **33 файла** с `<style lang="scss">`. Подключают:
  - `base/variables` и/или `base/mixins` — ProfileEditor, Ssvc.svelte, oc/settings/Settings, oc/ssvc/Settings, SensorCard, Wizard steps, SsvcSettings, OCSettings, ValveParameters, +layout и др.
  - `setting-containers` — Wizard steps, SsvcSettings, OCSettings, TelegramSettings, ChangeStageModal
  - `valve-table` — Wizard steps 3–7, ChangeStageModal
  - `profile-manager` — ProfileManager
  - `views/wizard` — StartWizard, ChangeStageModal
- **DaisyUI в коде:** `$lib/DaisyUiHelper.ts` — SystemMetrics.svelte, BatteryMetrics.svelte (цвета темы для Chart.js).
- **Flowbite-svelte:** удалён; замены — DaisyUI tooltip/loading, Tabler (unplugin-icons).

### 1.4 Дизайн-токены и общие переменные

- **В app.css** (`@layer base`): `:root` — `--primary-*`, `--glass-*`, `--blue-*`, `--red-*`, `--gray-*`, тени, радиусы, z-index; алиасы для NumberInput/TimeInput (`--border-color`, `--input-bg`, `--text-color` и т.д. через oklch(var(--b2)), var(--bc))); блок `@media (prefers-color-scheme: dark)` с `--dark-*` и переопределениями для полей ввода.
- **В SCSS:** `_variables.scss` — брейкпоинты (Tailwind-aligned). Миксины в `_mixins.scss` (glassmorphism, flex-center, dark-theme-color, compact-points и т.д.) используют CSS-переменные из app.css.

### 1.5 Пересечения и риски (актуально)

- **Кнопки, модалки, вкладки, поля ввода:** единственный источник стилей — `+layout.svelte` (`:global`) и компоненты (в т.ч. _wizard.scss для модалок wizard); дублирующие SCSS-файлы удалены.
- **Тёмная тема:** длинные селекторы в `_dark-theme.scss` удалены; цвета переведены на семантику DaisyUI в компонентах и в _settings-grid, _setting-containers, _valve-table, _profile-manager, Ssvc.svelte. Файл _dark-theme оставлен с пустым `@media (prefers-color-scheme: dark)` для возможных будущих глобальных правок.

### 1.6 Документация

- **[frontend.md](./frontend.md)** — стратегия (Tailwind + DaisyUI), описание SCSS-классов и миксинов, источники стилей после чистки (+layout, компоненты).
- **[styling.md](./styling.md)** — стратегия для нового кода, исключения (SCSS), общие переменные/миксины, Flowbite (удалён), ссылки на аудит.
- **[styles-cleanup-audit.md](./styles-cleanup-audit.md)** — выполненные задачи чистки и инструкции проверки.
- **README** — при необходимости можно добавить ссылку на docs/develop/frontend/styling.md.

---

## 2. Сводная таблица

| Аспект | Tailwind / DaisyUI | SCSS (глобальный + компонентный) |
|--------|--------------------|-----------------------------------|
| Точка входа | app.css, Vite plugin | open-connect-main.scss + @use в 33 компонентах |
| Дизайн-токены | app.css (:root + dark), семантика DaisyUI | breakpoints в _variables; миксины используют CSS vars из app.css |
| Кнопки / меню / карточки в UI | menu, login, страницы (DaisyUI) | Стили кнопок — +layout.svelte (:global); _buttons удалён |
| Формы, настройки, таблицы, wizard | — | setting-containers, valve-table, wizard, profile-manager (@use в компонентах) |
| Модалки | — | Компоненты (:global) + _wizard.scss; _modals удалён |
| Вкладки | — | Локально в oc/settings/Settings, oc/ssvc/Settings; _tabs удалён |
| Тёмная тема | DaisyUI business (prefers-dark), семантика в компонентах | _dark-theme — пустой блок; переменные в app.css |
| Документация | frontend.md, styling.md | frontend.md (SCSS + стратегия), styles-cleanup-audit.md |

---

## 3. План действий (актуализированный)

### Фаза 1: Решение и документ стратегии (по Issue #97) — выполнено

- [x] **Принять решение:** основа — вариант **A (Tailwind + DaisyUI)** с постепенной миграцией существующего SCSS.
- [x] **Зафиксировать в репозитории:** создан [styling.md](./styling.md) с правилами для нового кода, исключениями (SCSS) и подключением общих миксинов/переменных.
- [x] Ссылка на стратегию добавлена в [frontend.md](./frontend.md).

### Фаза 2: Чистка и уточнения — частично выполнено

Выполнено (подробно в [styles-cleanup-audit.md](./styles-cleanup-audit.md)):

- [x] Удалены неиспользуемые SCSS: _buttons, _modals, _tabs, _inputs. Стили кнопок, модалок, вкладок и полей — в +layout.svelte и компонентах.
- [x] Тёмная тема переведена на семантику DaisyUI; длинные селекторы убраны из _dark-theme.scss (файл оставлен с пустым блоком).
- [x] Flowbite-svelte удалён; везде DaisyUI loading и Tabler.
- [x] Документация обновлена: frontend.md, styling.md, styles-cleanup-audit.md.

Оставшиеся задачи (по желанию / отдельным PR):

- [x] **Зафиксировать в open-connect-main.scss** актуальный список подключаемых файлов в комментариях (base, layout, themes, animations; без удалённых _buttons, _modals, _tabs, _inputs). Синхронизировано с п. 1.2 этого документа.
- [x] **Неподключаемые модули _sensors-view, _sensor-card, _toggles:** явно описано в комментариях в open-connect-main.scss и в styles-cleanup-audit.md (подраздел 1.1) — намеренно не подключаются; при необходимости файлы можно удалить отдельной задачей после проверки внешнего вида.
- [x] **README / docs index:** добавлена ссылка на стратегию стилей: в корневом README.md (раздел «Документация для разработчиков») и в docs/index.md (блок «Документация для разработчиков»).

---

## 4. Вопросы к команде / мейнтейнеру (для справки)

Решения уже приняты и зафиксированы в [styling.md](./styling.md) и [styles-cleanup-audit.md](./styles-cleanup-audit.md):

- **Основа:** вариант A (Tailwind + DaisyUI); новый код — на них, существующий SCSS — постепенная миграция.
- **Flowbite-svelte:** удалён; используются DaisyUI и Tabler.
- **Дизайн-токены:** переменные в app.css, брейкпоинты в _variables.scss, миксины в _mixins.scss.

При дальнейшей миграции экранов на Tailwind/DaisyUI можно ориентироваться на приоритеты из styles-cleanup-audit (П.3): общие правила → wizard → OC Settings → телеметрия.

---

## 5. Критерии приёмки (из Issue #97)

- [x] Стратегия стилей зафиксирована в репозитории (документ [styling.md](./styling.md) + ссылка в [frontend.md](./frontend.md)).
- [x] Разработчикам понятно, как писать стили в новом коде (основа Tailwind/DaisyUI, исключения для SCSS, общие миксины/переменные — см. styling.md).

**Итог:** Issue #97 можно считать выполненным по критериям приёмки. Детальный аудит и выполненные шаги чистки — в [styles-cleanup-audit.md](./styles-cleanup-audit.md). Оставшиеся опциональные задачи (комментарии в open-connect-main, судьба _sensors-view/_sensor-card/_toggles, ссылка в README) — в п. 3 выше.
