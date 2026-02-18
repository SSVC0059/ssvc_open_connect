# Аудит стилей и план по Issue #97

**Цель:** выбор основной системы стилей (Tailwind vs SCSS) и документирование (часть #70).

Пока **изменений в код не вносим** — только аудит и план.

---

## 1. Текущее состояние

### 1.1 Что подключено и откуда грузится

| Система | Где подключается | Версии / конфиг |
|--------|-------------------|------------------|
| **Tailwind CSS** | `interface/src/app.css` (`@import "tailwindcss"`), `vite.config.ts` (`@tailwindcss/vite`) | tailwindcss ^4.1.11 |
| **DaisyUI** | `app.css`: `@plugin "daisyui"` (themes: corporate — default, business — prefers-dark) | daisyui ^5.0.46 |
| **Flowbite / flowbite-svelte** | Только импорты в компонентах | flowbite ^3.1.2, flowbite-svelte ^0.48.4 |
| **Глобальный SCSS** | `+layout.svelte`: `import '$lib/styles/open-connect-main.scss'` | sass-embedded ^1.93.2 |

### 1.2 Структура SCSS

**Точка входа:** `interface/src/lib/styles/open-connect-main.scss`.

Подключает:
- **base:** `_variables.scss`, `_mixins.scss`, `_animations.scss`
- **layout:** `_container.scss`, `_settings-grid.scss`
- **themes:** `_dark-theme.scss`, `_responsive.scss`

**Не подключает** (по комментариям в open-connect-main — «перенесены в компоненты»):
- `_buttons.scss`, `_inputs.scss`, `_modals.scss`, `_tabs.scss`
- `_sensors-view.scss`, `_sensor-card.scss`, `_valve-table.scss`
- `_setting-containers.scss`, `_profile-manager.scss`, `_toggles.scss`
- `_wizard.scss` (views)

Эти файлы подключаются **точечно** в Svelte-компонентах через `@use` в `<style lang="scss">`.

### 1.3 Использование по типам файлов

- **Svelte с Tailwind/DaisyUI классами в разметке:** маршруты и часть компонентов — например `menu.svelte` (классы вроде `btn`, `btn-ghost`, `menu`, `rounded-box`, `bg-base-200`, `text-base-content`), логин, wifi, system, user и др.
- **Svelte с scoped SCSS:** 31 файл с `<style lang="scss">`, часть из них подключает общие модули:
  - `@use "$lib/styles/base/variables"` и/или `base/mixins` — много компонентов (ProfileEditor, Ssvc.svelte, Settings, SensorCard, Wizard steps, OCSettings, SsvcSettings и др.)
  - `@use .../setting-containers` — Wizard steps, SsvcSettings, OCSettings, ChangeStageModal
  - `@use .../valve-table` — Wizard steps 3–7, ChangeStageModal
  - `@use .../profile-manager` — ProfileManager
  - `@use .../views/wizard` — StartWizard, ChangeStageModal
- **DaisyUI в коде:** `$lib/DaisyUiHelper.ts` — используется в `SystemMetrics.svelte` и `BatteryMetrics.svelte` (получение цветов темы для Chart.js).
- **Flowbite-svelte:** только 3 компонента — `ValveParameters.svelte`, `SensorCard.svelte`, `KpnPopover.svelte` (Popover, Spinner, иконки).

### 1.4 Дизайн-токены и общие переменные

- **В app.css** (внутри `@layer base`): большой блок `:root` с переменными — `--primary-*`, `--glass-*`, `--blue-*`, `--red-*`, `--gray-*`, тени, радиусы, z-index, а также блок `@media (prefers-color-scheme: dark)` с `--dark-*`.
- **В SCSS:** `_variables.scss` — только брейкпоинты (Tailwind-aligned). Остальные токены не дублируются, SCSS опирается на CSS-переменные из app.css и миксины из `_mixins.scss` (glassmorphism, flex-center, dark-theme-color, parameter-container, compact-points).

### 1.5 Пересечения и риски

- **Класс `.btn`:**  
  - В SCSS: `_buttons.scss` (кастомные .btn-primary, .btn-warning, .btn-success, .btn-back).  
  - В приложении: `_buttons.scss` **нигде не подключается** (нет в open-connect-main и нет импорта в Svelte). Фактически кнопки в разметке используют **DaisyUI** (например `btn btn-ghost` в menu.svelte). То есть кастомные кнопки из SCSS сейчас не задействованы в бандле.
- **Модальные окна:**  
  Стили из `_modals.scss` (`.modal-overlay`, `.modal-dialog`, `.modal-content` и т.д.) в open-connect-main не входят. Используются ли они — зависит от того, подключают ли какие-то компоненты `_modals.scss` напрямую; по текущему поиску прямого `@use` modals в Svelte нет. Модальные стили могут идти из `_wizard.scss` (там есть .modal-header, .modal-title и т.д.) в StartWizard и ChangeStageModal.
- **Вкладки:**  
  Классы из `_tabs.scss` (.tabs-container, .tab, .tab-active) упоминаются в `_dark-theme.scss`. Подключение _tabs в Svelte не найдено — значит, глобально вкладки могут стилизоваться только если какой-то родительский SCSS подтягивает tabs или классы заданы через открытый main. Требуется уточнение.
- **Специфичность в тёмной теме:**  
  В `_dark-theme.scss` используются длинные селекторы с `body .container .tabs-container ...` и `body .telemetry-container ...` для переопределения scoped-стилей компонентов — хрупкая связь между глобальным SCSS и разметкой.

### 1.6 Документация

- **docs/develop/frontend/frontend.md** — описывает только SCSS: миксины, классы настроек, вкладок, модалок, wizard, датчики, кнопки. Tailwind и DaisyUI не упомянуты.
- **README** — про стили ничего нет.
- **CONTRIBUTING** и **.cursor/rules** — отсутствуют.

---

## 2. Сводная таблица

| Аспект | Tailwind / DaisyUI | SCSS (глобальный + компонентный) |
|--------|--------------------|-----------------------------------|
| Точка входа | app.css, Vite plugin | open-connect-main.scss + @use в 31 компоненте |
| Дизайн-токены | Частично в app.css (:root) | Переменные в app.css + breakpoints в _variables, миксины используют CSS vars |
| Кнопки / меню / карточки в UI | menu, login, часть страниц (DaisyUI) | Кастомные .btn в _buttons не подключены |
| Формы, настройки, таблицы, wizard | — | setting-containers, valve-table, wizard, profile-manager (подключаются в компонентах) |
| Модалки | — | _modals не в main; wizard использует свои .modal-* |
| Тёмная тема | DaisyUI business (prefers-dark) | _dark-theme.scss + переменные в app.css |
| Документация | Нет | frontend.md (только SCSS) |

---

## 3. План действий (без внесения изменений в код на текущем этапе)

### Фаза 1: Решение и документ стратегии (по Issue #97)

1. **Принять решение:** основа — вариант A (Tailwind + DaisyUI) или B (SCSS).
   - Учитывать: уже много SCSS (layout, темы, миксины, 31 компонент с scoped SCSS), при этом навигация/часть UI — на DaisyUI; есть дублирование/мёртвый код (.btn в SCSS).
2. **Зафиксировать в репозитории:**
   - Создать документ (например `docs/develop/frontend/styling.md` или `.cursor/rules/styles.mdc` / раздел в CONTRIBUTING):
     - Какую систему использовать для **новых** компонентов и страниц.
     - Когда допустимы **исключения** (сложные миксины, анимации, стекломорфизм).
     - Как подключать **общие** миксины/переменные (оставить ли переменные в app.css, как использовать SCSS модули при варианте A или наоборот при B).
   - Добавить ссылку из README или docs (например из docs/develop/frontend/frontend.md или docs/index) на этот документ.

### Фаза 2: Уточнения и техдолг (после выбора стратегии)

3. **Проверить фактическое использование SCSS-модулей:**
   - Выяснить, откуда берутся стили для модалок и вкладок (какие файлы реально попадают в бандл, нет ли дублей с DaisyUI).
   - Решить судьбу неиспользуемых файлов (например _buttons.scss, возможно _modals.scss / _tabs.scss) — удалить или явно подключить.
4. **Привести документацию в соответствие со стратегией:**
   - Обновить `frontend.md`: описать Tailwind/DaisyUI, если основа — A; либо явно указать «основа — SCSS», и когда допустимы утилиты Tailwind.
   - Упомянуть Flowbite-svelte (ограниченное использование) и DaisyUiHelper.
5. **По возможности уменьшить дублирование и специфичность:**
   - Единый источник токенов (только app.css или только SCSS variables) в зависимости от выбора A/B.
   - Избегать длинных переопределений в _dark-theme (body .container ...), если будет выбран путь миграции на утилиты/темы Tailwind/DaisyUI.

---

## 4. Вопросы к команде / мейнтейнеру

1. **Вариант A (Tailwind + DaisyUI) как основа:** готовы ли вы постепенно переводить существующие SCSS-компоненты (настройки, wizard, модалки, таблицы) на утилиты Tailwind и компоненты DaisyUI, или хотите только «новый код — на Tailwind», а старый SCSS оставить как есть?
2. **Вариант B (SCSS как основа):** планируется ли отказ от DaisyUI для основного UI (меню, кнопки, карточки) и переход на кастомные классы из _buttons, _modals и т.д.? Если да — нужно ли тогда оставить DaisyUI только для тем (цвета) и утилит?
3. **Flowbite-svelte:** оставляем для Popover/Spinner или хотим заменить на DaisyUI/кастомные компоненты для единообразия?
4. **Дизайн-токены:** устраивает ли текущее разделение (переменные в app.css, брейкпоинты в SCSS), или нужен один явный слой (например только CSS vars в app.css с возможностью импорта в SCSS)?
5. **Приоритет:** важнее быстрее зафиксировать стратегию в документе (и описать исключения), или сначала провести точечную чистку (подключить/удалить неиспользуемые SCSS) и потом писать правила?

---

## 5. Критерии приёмки (из Issue #97) — как закрыть

- [x] Стратегия стилей зафиксирована в репозитории (документ + при необходимости ссылка из README/docs).
- [x] Разработчикам понятно, как писать стили в новом коде (основа + исключения + общие миксины/переменные).

**Итог:** принят вариант **A (Tailwind + DaisyUI)** с постепенной миграцией; стратегия описана в [styling.md](./styling.md). Аудит для будущей чистки (без изменений в коде) — в [styles-cleanup-audit.md](./styles-cleanup-audit.md).
