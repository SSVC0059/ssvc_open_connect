# Шаблоны Sub-Issue для Issue #70 (Рефакторинг стилей)

Использование: скопировать нужный блок в буфер обмена и вставить как описание нового issue. В начале описания можно добавить: `Part of #70` или `Parent: #70`. Рекомендуемые labels: `frontend`, при необходимости `help wanted`.

---

## Этап 1.1 — Добавить недостающие CSS-переменные

**Title:** `[Styles] Добавить недостающие CSS-переменные (glass-*, red-100, *-rgb)`

**Labels:** `frontend`

```markdown
Part of #70 (Глобальный рефакторинг работы со стилями).

## Задача

В коде используются CSS-переменные, которые не определены в `app.css`. Необходимо добавить их или алиасы к существующим.

## Действия

1. **Алиасы для glass-переменных** в `app.css` (`:root`):
   - `--glass-primary-800` → `var(--primary-800)` (или определить, если нет)
   - `--glass-primary-200` → `var(--primary-200)`
   - `--glass-secondary-600`, `--glass-secondary-300`, `--glass-secondary-700`, `--glass-secondary-500` → соответствующие `var(--primary-*)` или алиасы

2. **Добавить переменную** `--red-100` (например `#fee2e2` или по дизайн-системе).

3. **Добавить RGB-переменные** для использования в `rgba()`:
   - `--primary-100-rgb: 240, 244, 248`
   - `--primary-500-rgb: 98, 125, 152`
   - `--black-rgb: 0, 0, 0`
   - `--blue-500-rgb: 59, 130, 246`
   (значения взять из текущих hex в `app.css`.)

## Файлы

- `Interface/src/app.css` (или основной файл с `:root`)

## Критерии приёмки

- [ ] Все переменные `--glass-primary-*` и `--glass-secondary-*` определены или являются алиасами.
- [ ] `--red-100` определена.
- [ ] RGB-переменные добавлены для primary-100, primary-500, black, blue-500.
- [ ] Стили в ThermalSensors, Ssvc.svelte, ProfileEditor и миксинах работают без ошибок.
```

---

## Этап 1.2 — Исправить rgba() с CSS-переменными

**Title:** `[Styles] Исправить rgba() с hex-переменными в SCSS`

**Labels:** `frontend`

```markdown
Part of #70 (Глобальный рефакторинг работы со стилями).

## Задача

В нескольких SCSS-файлах используется `rgba(var(--primary-*), alpha)` и аналоги. Переменные содержат hex (`#f0f4f8` и т.д.), а `rgba()` ожидает компоненты RGB. Нужно заменить на `rgba(var(--*-rgb), alpha)` или на `color-mix()`.

## Действия

1. **_tabs.scss** (строки ~121, 242, 247):
   - `rgba(var(--primary-100), 0.3)` → `rgba(var(--primary-100-rgb), 0.3)` или `color-mix(in srgb, var(--primary-100) 30%, transparent)`
   - `rgba(var(--primary-500), 0.1)` → аналогично с `--primary-500-rgb` или color-mix

2. **_modals.scss**: `rgba(var(--black), 0.5)` → `rgba(var(--black-rgb), 0.5)` (предполагается, что `--black-rgb` добавлена в этапе 1.1).

3. **_mixins.scss**: `rgba(var(--black), 0.25)` → `rgba(var(--black-rgb), 0.25)`.

4. **_inputs.scss**: `rgba(var(--blue-500), 0.2)` → `rgba(var(--blue-500-rgb), 0.2)`.

## Файлы

- `Interface/src/lib/styles/components/_tabs.scss`
- `Interface/src/lib/styles/components/_modals.scss`
- `Interface/src/lib/styles/base/_mixins.scss`
- `Interface/src/lib/styles/components/_inputs.scss`

## Критерии приёмки

- [ ] Нет вызовов `rgba(var(--*-hex), alpha)` с hex-переменными.
- [ ] Визуально стили (подсветка табов, оверлеи, инпуты) выглядят корректно в светлой и тёмной теме.
```

---

## Этап 1.3 — Удалить дубликат normalize.css

**Title:** `[Styles] Удалить дубликат normalize.css`

**Labels:** `frontend`

```markdown
Part of #70 (Глобальный рефакторинг работы со стилями).

## Задача

Существуют два файла нормализации:
- `Interface/src/normalize.css` — импортируется в layout
- `Interface/src/lib/styles/normalize.css` — не используется

Необходимо оставить один источник и удалить дубликат.

## Действия

1. Убедиться, что в `+layout.svelte` импортируется только `src/normalize.css` (или только файл из `lib/styles`, если решено оставить его).
2. Удалить дубликат: если основной — `src/normalize.css`, удалить `src/lib/styles/normalize.css` (или наоборот, с обновлением импорта в layout).
3. Проверить, что ни один файл не импортирует удалённый normalize.

## Файлы

- `Interface/src/normalize.css`
- `Interface/src/lib/styles/normalize.css` (к удалению)
- `Interface/src/routes/+layout.svelte`

## Критерии приёмки

- [ ] В проекте один файл normalize.
- [ ] Импорт в layout указывает на выбранный файл.
- [ ] Сборка и отображение страниц без регрессий.
```

---

## Этап 1.4 — Унифицировать импорты в SCSS

**Title:** `[Styles] Унифицировать импорты @use в SCSS на $lib/...

**Labels:** `frontend`

```markdown
Part of #70 (Глобальный рефакторинг работы со стилями).

## Задача

В одних файлах используется `@use "../base/mixins"`, в других — `@use "$lib/styles/base/mixins"`. Нужно везде использовать единый формат `$lib/...` для предсказуемости и совместимости с SvelteKit.

## Действия

1. Найти все вхождения относительных импортов в `lib/styles/` (например `@use "../base/mixins"`, `@use "../../base/variables"`).
2. Заменить на абсолютные: `@use "$lib/styles/base/mixins" as m`, `@use "$lib/styles/base/variables"` и т.д.
3. Проверить сборку и что стили применяются.

## Файлы

- Все SCSS-файлы в `Interface/src/lib/styles/` (в т.ч. base, layout, components, views, themes)

## Критерии приёмки

- [ ] Во всех SCSS-файлах импорты идут через `$lib/styles/...`.
- [ ] Сборка проходит без ошибок.
```

---

## Этап 2.1 — Единый источник переменных (переменные и темы в app.css)

**Title:** `[Styles] Перенести переменные и тему в app.css (единый источник)

**Labels:** `frontend`

```markdown
Part of #70 (Глобальный рефакторинг работы со стилями).

## Задача

Переменные и тёмная тема заданы в двух местах: в `app.css` и в `_dark-theme.scss`. Нужно собрать все переменные в одном месте (например в `app.css`: `:root` и `@media (prefers-color-scheme: dark)`).

## Действия

1. Выписать все переменные из `_dark-theme.scss` (в т.ч. `--dark-bg-overlay`, `--dark-surface-color` и т.д.).
2. Перенести их в `app.css` в блок `@media (prefers-color-scheme: dark) { :root { ... } }` (или в отдельный `_tokens.css`, если решено вынести токены).
3. Убедиться, что дубликатов в `:root` и в dark-блоке нет и что приоритет тёмной темы корректен.

## Файлы

- `Interface/src/app.css`
- `Interface/src/lib/styles/themes/_dark-theme.scss`

## Критерии приёмки

- [ ] Все переменные темы и цвета определены в одном месте (app.css или согласованный tokens-файл).
- [ ] Тёмная тема переключается корректно.
```

---

## Этап 2.2 — Упростить _dark-theme.scss

**Title:** `[Styles] Упростить _dark-theme.scss после переноса переменных`

**Labels:** `frontend`

```markdown
Part of #70 (Глобальный рефакторинг работы со стилями).

## Задача

После переноса переменных в app.css (этап 2.1) файл `_dark-theme.scss` должен содержать только переопределения стилей, опирающиеся на эти переменные. Убрать дублирование с блоком тёмной темы в app.css и лишние объявления переменных.

## Действия

1. Удалить из `_dark-theme.scss` объявления переменных, уже перенесённых в app.css.
2. Оставить только селекторы и правила, которые используют переменные из :root (например переопределение фона, границ).
3. При необходимости оставить один способ включения тёмной темы (только media query в app.css или только миксин в SCSS), чтобы логика не дублировалась.

## Файлы

- `Interface/src/lib/styles/themes/_dark-theme.scss`
- `Interface/src/app.css`

## Критерии приёмки

- [ ] В _dark-theme.scss нет объявлений переменных, дублирующих app.css.
- [ ] Внешний вид светлой/тёмной темы без изменений.
```

---

## Этап 2.3 — Разобрать остаточные стили в open-connect-main.scss

**Title:** `[Styles] Перенести .settings-description и .settings-title из open-connect-main.scss`

**Labels:** `frontend`

```markdown
Part of #70 (Глобальный рефакторинг работы со стилями).

## Задача

В конце `open-connect-main.scss` есть классы `.settings-description` и `.settings-title` с комментарием «Специфичные и не разобранные». Нужно перенести их в соответствующие компоненты или в `_settings-grid.scss`.

## Действия

1. Найти использование классов `.settings-description` и `.settings-title` в разметке (компоненты).
2. Перенести стили в компонентные `<style>` или в `lib/styles/layout/_settings-grid.scss`, если классы общие для страницы настроек.
3. Удалить блок из `open-connect-main.scss`.

## Файлы

- `Interface/src/lib/styles/open-connect-main.scss`
- Компоненты, использующие эти классы, или `_settings-grid.scss`

## Критерии приёмки

- [ ] Стили настроек выглядят так же.
- [ ] В open-connect-main.scss нет «остаточных» блоков с этими классами.
```

---

## Этап 3.1 — Заменить жёстко заданные цвета на переменные

**Title:** `[Styles] Заменить хардкод цветов на CSS-переменные`

**Labels:** `frontend`

```markdown
Part of #70 (Глобальный рефакторинг работы со стилями).

## Задача

В ряде файлов цвета заданы напрямую (hex). Нужно заменить на переменные дизайн-системы.

## Действия

| Файл | Текущее значение | Замена |
|------|------------------|--------|
| ValveParameters.svelte | color: #10b981 | var(--green-500) |
| ApiVersionGuard.svelte | #fde68a, #fef3c7, #92400e | var(--yellow-300), var(--yellow-100), var(--yellow-800) и т.д. |
| NumberInput.svelte | color: #888 | var(--gray-500) |
| _profile-manager.scss | background: #fff | var(--white) |
| RectImg.svelte | #ff6a6e, #00d4ef, #ff0000 и др. | вынести в переменные или использовать существующие (--red-500 и т.д.) |

Добавить недостающие переменные в app.css, если их ещё нет.

## Файлы

- `Interface/src/lib/.../ValveParameters.svelte`
- `Interface/src/lib/.../ApiVersionGuard.svelte`
- `Interface/src/lib/.../NumberInput.svelte`
- `Interface/src/lib/styles/components/_profile-manager.scss`
- `Interface/src/lib/.../RectImg.svelte`
- `Interface/src/app.css` (при необходимости)

## Критерии приёмки

- [ ] В перечисленных файлах нет хардкода hex/rgb для цветов (кроме обоснованных исключений).
- [ ] Визуально цвета соответствуют прежнему виду или дизайн-системе.
```

---

## Этап 3.2 — Унифицировать имена переменных (color-* vs *)

**Title:** `[Styles] Унифицировать имена CSS-переменных (ThermalSensors, app.css)`

**Labels:** `frontend`

```markdown
Part of #70 (Глобальный рефакторинг работы со стилями).

## Задача

В ThermalSensors.svelte используются `--color-blue-400`, `--color-orange-500`, `--color-red-600`, в app.css — `--blue-300`, `--blue-500`, `--red-600`. Привести к одной схеме именования и убедиться, что переменные определены.

## Действия

1. Выбрать единую схему: либо везде `--color-*-*`, либо везде `--*-*` (как в app.css).
2. В ThermalSensors.svelte заменить ссылки на переменные на выбранную схему.
3. При необходимости добавить в app.css алиасы или недостающие переменные.

## Файлы

- `Interface/src/lib/.../ThermalSensors.svelte`
- `Interface/src/app.css`

## Критерии приёмки

- [ ] Одна схема именования цветовых переменных в проекте.
- [ ] ThermalSensors отображает цвета корректно.
```

---

## Этап 3.3 — Унифицировать breakpoints

**Title:** `[Styles] Ввести константы breakpoints и унифицировать медиа-запросы`

**Labels:** `frontend`

```markdown
Part of #70 (Глобальный рефакторинг работы со стилями).

## Задача

Используются разные значения breakpoints (480, 640, 768, 769, 1024 px) без общей шкалы. Ввести SCSS-переменные и выровнять значения (например по Tailwind: 640, 768, 1024, 1280). Исправить 769px → 768px в _tabs.scss.

## Действия

1. В `base/_variables.scss` (или отдельный файл) добавить:
   - `$breakpoint-sm: 640px`
   - `$breakpoint-md: 768px`
   - `$breakpoint-lg: 1024px`
   - `$breakpoint-xl: 1280px`
2. Заменить в SCSS и компонентах жёстко заданные значения на переменные.
3. В _tabs.scss заменить 769px на $breakpoint-md (768px).

## Файлы

- `Interface/src/lib/styles/base/_variables.scss` (или новый файл)
- Все файлы, где используются медиа-запросы с указанными значениями (mixins, _wizard, RectImg, _responsive, _sensor-card, _sensors-view, _tabs, _valve-table, _settings-grid, Ssvc, ProfileEditor, _profile-manager, ValveParameters)

## Критерии приёмки

- [ ] Единая шкала breakpoints в переменных.
- [ ] Нет «магических» чисел 480, 640, 768, 769, 1024 в медиа-запросах (используются переменные).
```

---

## Этап 3.4 — Шкала z-index

**Title:** `[Styles] Ввести шкалу z-index в :root и заменить magic numbers`

**Labels:** `frontend`

```markdown
Part of #70 (Глобальный рефакторинг работы со стилями).

## Задача

Z-index заданы разбросанно (10, 40, 50, 1000). Ввести в app.css шкалу и заменить значения в компонентах и SCSS.

## Действия

1. В `app.css` в `:root` добавить, например:
   - `--z-base: 0`
   - `--z-dropdown: 10`
   - `--z-sticky: 40`
   - `--z-modal-backdrop: 40`
   - `--z-modal: 50`
   - `--z-popover: 1000`
2. В _sensor-card, _modals, _wizard, _tabs, ProfileEditor заменить числа на `var(--z-*)`.

## Файлы

- `Interface/src/app.css`
- `Interface/src/lib/styles/components/_sensor-card.scss`
- `Interface/src/lib/styles/components/_modals.scss`
- `Interface/src/lib/styles/views/_wizard.scss` (или аналог)
- `Interface/src/lib/styles/components/_tabs.scss`
- Компонент ProfileEditor

## Критерии приёмки

- [ ] Z-index задаются через переменные из app.css.
- [ ] Наложение слоёв (модалки, дропдауны, попапы) работает как раньше.
```

---

## Этап 4.1 — Видимые индикаторы фокуса (доступность)

**Title:** `[Styles] Добавить видимые индикаторы фокуса для доступности`

**Labels:** `frontend`, `accessibility`

```markdown
Part of #70 (Глобальный рефакторинг работы со стилями).

## Задача

В _inputs.scss, _time-editor.scss, _valve-table.scss и mixins при :focus используется `outline: none` без замены. Для клавиатурной навигации фокус должен быть заметен.

## Действия

1. Найти все `outline: none` (или outline: 0) в контексте :focus в указанных файлах.
2. Добавить видимый индикатор: `box-shadow` (например ring), или `outline` с `outline-offset`, не удаляя фокус полностью.
3. Убедиться, что контраст индикатора достаточен для доступности.

## Файлы

- `Interface/src/lib/styles/components/_inputs.scss`
- `Interface/src/lib/styles/components/_time-editor.scss`
- `Interface/src/lib/styles/components/_valve-table.scss`
- `Interface/src/lib/styles/base/_mixins.scss`

## Критерии приёмки

- [ ] При навигации с Tab фокус виден на интерактивных элементах.
- [ ] Нет снятия outline без альтернативы.
```

---

## Этап 4.2 — Устранить !important

**Title:** `[Styles] Устранить !important, пересмотреть специфичность`

**Labels:** `frontend`

```markdown
Part of #70 (Глобальный рефакторинг работы со стилями).

## Задача

В _tabs.scss, _setting-containers.scss, _buttons.scss и Control.svelte используется `!important` для переопределения специфичности. Пересмотреть селекторы и по возможности убрать !important.

## Действия

1. _tabs.scss: display для .desktop-only, .mobile-only — усилить селектор или использовать классы с большей специфичностью вместо !important.
2. _setting-containers.scss: border-color, cursor: not-allowed — то же.
3. _buttons.scss: transform: none для :disabled — то же.
4. Control.svelte: color: var(--gray-500) — то же.
5. Если после рефакторинга специфичность конфликтует (например с Tailwind), зафиксировать решение в комментарии.

## Файлы

- `Interface/src/lib/styles/components/_tabs.scss`
- `Interface/src/lib/styles/components/_setting-containers.scss`
- `Interface/src/lib/styles/components/_buttons.scss`
- `Interface/src/lib/.../Control.svelte`

## Критерии приёмки

- [ ] Количество !important в перечисленных файлах сведено к минимуму или нулю.
- [ ] Визуальное и поведенческое состояние кнопок, табов, контейнеров и Control без регрессий.
```

---

## Этап 4.3 — Заменить inline-стили на классы

**Title:** `[Styles] Заменить статические inline-стили на классы`

**Labels:** `frontend`

```markdown
Part of #70 (Глобальный рефакторинг работы со стилями).

## Задача

Часть статических inline-стилей можно вынести в классы. Динамические (width, left, --value) оставить.

## Действия

| Файл | Пример | Действие |
|------|--------|----------|
| Settings.svelte | style="color: red" для ошибки | класс .error-text или text-red-500 (Tailwind) |
| ThresholdCalculatorModal.svelte | style="text-align: center; margin-bottom: 1rem" | вынести в класс в <style> или utility |
| ProfileEditor.svelte | style="border-top: none; padding-top: 0" | класс-модификатор |

## Файлы

- `Interface/src/.../Settings.svelte`
- `Interface/src/.../ThresholdCalculatorModal.svelte`
- `Interface/src/.../ProfileEditor.svelte`

## Критерии приёмки

- [ ] Указанные статические inline-стили заменены на классы.
- [ ] Динамические inline-стили сохранены.
```

---

## Этап 5.1 — Миграция: loading-spinner, time-editor

**Title:** `[Styles] Миграция стилей loading-spinner и time-editor в компоненты`

**Labels:** `frontend`

```markdown
Part of #70 (Глобальный рефакторинг работы со стилями).

## Задача

Перенести стили из `lib/styles/components/_loading-spinner.scss` и _time-editor в соответствующие Svelte-компоненты в `<style lang="scss">`.

## Действия

1. Найти компоненты LoadingSpinner (или аналог) и TimeEditor.
2. Скопировать правила из SCSS-файлов в <style> компонентов. Подключить миксины через `@use "$lib/styles/base/mixins" as m` при необходимости.
3. Удалить импорт этих модулей из open-connect-main.scss (или оставить до полной миграции этапа 5.5).
4. Проверить отображение.

## Файлы

- `Interface/src/lib/styles/components/_loading-spinner.scss` (или аналог)
- `Interface/src/lib/styles/components/_time-editor.scss`
- Соответствующие .svelte-компоненты
- `Interface/src/lib/styles/open-connect-main.scss`

## Критерии приёмки

- [ ] Стили спиннера и time-editor живут в компонентах.
- [ ] Внешний вид без изменений.
```

---

## Этап 5.2 — Миграция: sensor-card, inputs, buttons, toggles

**Title:** `[Styles] Миграция стилей sensor-card, inputs, buttons, toggles в компоненты`

**Labels:** `frontend`

```markdown
Part of #70 (Глобальный рефакторинг работы со стилями).

## Задача

Перенести стили из _sensor-card.scss, _inputs.scss, _buttons.scss, _toggles.scss в соответствующие Svelte-компоненты. Подключать миксины через `@use "$lib/styles/base/mixins"`.

## Действия

1. Для каждого файла определить компонент-владелец (SensorCard, поля ввода, кнопки, переключатели).
2. Перенести правила в <style lang="scss"> компонентов. Учесть вложенность и общие классы (например .btn может использоваться в нескольких компонентах — вынести в миксин или shared-компонент).
3. Удалить или не импортировать эти модули в open-connect-main.scss (согласовать с этапом 5.5).

## Файлы

- `Interface/src/lib/styles/components/_sensor-card.scss`
- `Interface/src/lib/styles/components/_inputs.scss`
- `Interface/src/lib/styles/components/_buttons.scss`
- `Interface/src/lib/styles/components/_toggles.scss`
- Соответствующие .svelte-компоненты

## Критерии приёмки

- [ ] Стили привязаны к компонентам.
- [ ] Кнопки, инпуты, карточки датчиков и тогглы выглядят и работают как раньше.
```

---

## Этап 5.3 — Миграция: tabs, modals, sensors-view

**Title:** `[Styles] Миграция стилей tabs, modals, sensors-view в компоненты`

**Labels:** `frontend`

```markdown
Part of #70 (Глобальный рефакторинг работы со стилями).

## Задача

Перенести стили из _tabs.scss, _modals.scss, _sensors-view.scss в соответствующие Svelte-компоненты. Учесть сложную вложенность и общие миксины.

## Действия

1. Определить компоненты для табов, модалок и представления сенсоров.
2. Перенести правила в <style lang="scss">; при необходимости вынести общие части в base/mixins.
3. Обновить open-connect-main.scss (удалить импорты этих компонентов после проверки).

## Файлы

- `Interface/src/lib/styles/components/_tabs.scss`
- `Interface/src/lib/styles/components/_modals.scss`
- `Interface/src/lib/styles/components/_sensors-view.scss`
- Соответствующие .svelte-компоненты

## Критерии приёмки

- [ ] Стили табов, модалок и sensors-view в компонентах.
- [ ] Нет регрессий вёрстки и поведения.
```

---

## Этап 5.4 — Миграция: valve-table, setting-containers, profile-manager, wizard

**Title:** `[Styles] Миграция стилей valve-table, setting-containers, profile-manager, wizard в компоненты/views`

**Labels:** `frontend`

```markdown
Part of #70 (Глобальный рефакторинг работы со стилями).

## Задача

Перенести стили из _valve-table.scss, _setting-containers.scss, _profile-manager.scss и views/wizard в соответствующие компоненты. Wizard можно оставить в views/ или разбить по шагам.

## Действия

1. Сопоставить SCSS-модули с компонентами (ValveTable, контейнеры настроек, ProfileManager, шаги wizard).
2. Перенести стили в компонентные <style> или в общие layout/views-файлы, где это уместно.
3. Удалить импорты из open-connect-main.scss после проверки.

## Файлы

- `Interface/src/lib/styles/components/_valve-table.scss`
- `Interface/src/lib/styles/components/_setting-containers.scss`
- `Interface/src/lib/styles/components/_profile-manager.scss`
- `Interface/src/lib/styles/views/_wizard.scss` (или разбиение по шагам)
- Соответствующие .svelte-компоненты

## Критерии приёмки

- [ ] Глобальные классы из этих файлов перенесены в компоненты или layout/views.
- [ ] UI настроек, таблицы клапанов, профиля и визарда без регрессий.
```

---

## Этап 5.5 — Обновить open-connect-main.scss после миграции

**Title:** `[Styles] Обновить open-connect-main.scss — только base, layout, themes`

**Labels:** `frontend`

```markdown
Part of #70 (Глобальный рефакторинг работы со стилями).

## Задача

После переноса стилей из lib/styles/components/ в компоненты (этапы 5.1–5.4) главный SCSS-файл должен импортировать только базовые слои и темы.

## Действия

1. Удалить из open-connect-main.scss все импорты из `components/*`.
2. Оставить только:
   - `@use "$lib/styles/base/variables"`
   - `@use "$lib/styles/base/mixins" as m`
   - `@use "$lib/styles/layout/container"`
   - `@use "$lib/styles/layout/settings-grid"`
   - `@use "$lib/styles/themes/dark-theme"`
   - `@use "$lib/styles/themes/responsive"`
   - `@use "$lib/styles/base/animations"`
3. Проверить сборку и полный UI.

## Файлы

- `Interface/src/lib/styles/open-connect-main.scss`

## Критерии приёмки

- [ ] В файле нет импортов из components/.
- [ ] Сборка успешна, все страницы отображаются корректно.
```

---

## Этап 6.1 — Выбор и документирование системы стилей

**Title:** `[Styles] Выбор основной системы стилей (Tailwind vs SCSS) и документирование`

**Labels:** `frontend`, `documentation`

```markdown
Part of #70 (Глобальный рефакторинг работы со стилями).

## Задача

Сейчас смешаны Tailwind + DaisyUI, глобальный SCSS и scoped-стили компонентов. Нужно зафиксировать стратегию: вариант A — Tailwind + DaisyUI как основа, вариант B — SCSS как основа. Решение описать в RULE, CONTRIBUTING или отдельном документе.

## Действия

1. Принять решение: A (Tailwind/DaisyUI) или B (SCSS).
2. Кратко описать в документе:
   - какую систему использовать для новых компонентов;
   - когда допустимы исключения (сложные миксины, анимации);
   - как подключать общие миксины/переменные.
3. При необходимости добавить ссылку из README или docs на этот документ.

## Файлы

- Новый или существующий документ (например `.cursor/rules/styles.mdc`, `docs/develop/styling.md`, CONTRIBUTING.md)
- При необходимости README или docs/index

## Критерии приёмки

- [ ] Стратегия стилей зафиксирована в репозитории.
- [ ] Разработчикам понятно, как писать стили в новом коде.
```

---

*Шаблоны соответствуют плану в `docs/issue_70_styles_refactoring_work_plan.md`.*
