# Стратегия стилей (Tailwind + DaisyUI)

**Контекст:** [Issue #97](https://github.com/SSVC0059/ssvc_open_connect/issues/97), [Issue #70](https://github.com/SSVC0059/ssvc_open_connect/issues/70) — обсуждения рефакторинга стилей (актуальность стека смотрите по версиям в `interface/package.json`).

---

## 1. Основной выбор

**Основа интерфейса — Tailwind CSS v4 + DaisyUI v5.**

- Сборка стилей: **`@tailwindcss/vite`** в `interface/vite.config.ts`, без отдельного `tailwind.config.js` для базового конвейера. В `interface/src/app.css`: `@import "tailwindcss";` и `@plugin "daisyui" { themes: corporate --default, business --prefersdark; }`.
- Новый код (страницы, компоненты, виджеты) пишем с использованием **утилит Tailwind** и **компонентов DaisyUI**.
- Существующий SCSS и кастомные классы остаются до **постепенной миграции**; при доработке старых экранов предпочтительно переводить их на Tailwind/DaisyUI по мере возможности.

---

## 2. Для нового кода

### 2.1 Что использовать

- **Разметка и утилиты:** классы Tailwind (`flex`, `gap-4`, `text-base-content`, `rounded-box` и т.д.).
- **Компоненты UI:** классы DaisyUI (`btn`, `btn-primary`, `card`, `modal`, `menu`, `tabs`, `alert` и т.д.). Темы задаются в `app.css` через `@plugin "daisyui"`: **corporate** (светлая по умолчанию), **business** с модификатором **`--prefersdark`** (тёмная при системной тёмной схеме). Дополнительно в том же файле в `@layer base` заданы кастомные **CSS‑переменные** и блок `@media (prefers-color-scheme: dark)` для старых поверхностей (glass, wizard и т.д.) — их постепенно выравнивают с токенами DaisyUI.
- **Цвета и темы:** предпочтительно семантика DaisyUI (`base-content`, `primary`, `secondary` и т.д.). Для графиков/Chart.js можно использовать `$lib/DaisyUiHelper.ts` (получение цветов темы из DOM).
- **Тёмная тема:** в первую очередь **атрибут/классы темы DaisyUI** и `business --prefersdark`. Переход с глобальных переопределений в `interface/src/lib/styles/themes/_dark-theme.scss` на единый слой DaisyUI — постепенно, по мере переноса экранов.

### 2.2 Когда допустимы исключения (SCSS / scoped-стили)

Допускается использовать **SCSS** (глобальные модули или `<style lang="scss">` в компоненте) в случаях:

- **Сложные миксины**, которые нецелесообразно дублировать утилитами (например компактные сетки, повторяющиеся блоки с вложенностью).
- **Специфичные анимации** или эффекты (keyframes, сложные transition), если их проще держать в SCSS.
- **Легаси или участки, запланированные к миграции позже** — до переноса на Tailwind/DaisyUI можно оставлять текущие SCSS-классы.

При добавлении нового SCSS-кода желательно ограничиваться компонентом (`<style lang="scss">` с `@use` общих модулей при необходимости) и не плодить новые глобальные классы без согласования.

---

## 3. Общие переменные и миксины

### 3.1 Текущее разделение (пока устраивает)

- **CSS-переменные** (цвета, тени, радиусы, z-index, glass-эффекты) задаются в **`interface/src/app.css`** в `:root` и в `@media (prefers-color-scheme: dark)`. Их можно использовать и в Tailwind-разметке (например через `var(--primary-500)` в произвольных значениях), и в SCSS.
- **Брейкпоинты** и прочие SCSS-переменные — в **`$lib/styles/base/_variables.scss`** (выровнены под Tailwind). Подключаются через `@use` в тех компонентах, где нужны миксины или медиа-запросы на эти переменные.
- **Миксины** (glassmorphism, flex-center, dark-theme-color, compact-points и т.д.) — в **`$lib/styles/base/_mixins.scss`**. Используются только в SCSS (глобальном или в компонентах с `@use`).

При миграции участков на Tailwind можно заменять использование миксинов эквивалентными утилитами или темами DaisyUI; переменные из `app.css` при необходимости оставить как общий слой токенов.

### 3.2 Как подключать в компонентах

- **Только Tailwind/DaisyUI:** достаточно глобального `app.css` (уже в layout); дополнительные импорты не нужны.
- **SCSS (исключения):** в компоненте в `<style lang="scss">` использовать `@use "$lib/styles/base/variables" as v;` и/или `@use "$lib/styles/base/mixins" as m;` при необходимости. Для существующих блоков (настройки, таблицы, wizard) по-прежнему допускается `@use` соответствующих модулей из `$lib/styles/...` (см. [frontend.md](./frontend.md)).

---

## 4. Flowbite-svelte

**Итог:** зависимости Flowbite **удалены**. Popover заменён на DaisyUI tooltip (ValveParameters, KpnPopover), Spinner — на DaisyUI loading во всех компонентах, иконки — на Tabler (unplugin-icons). Новый функционал реализовывать на DaisyUI/Tailwind/Tabler, без Flowbite.

---

## 5. Очищенные/удалённые модули

Удалены неиспользуемые SCSS-модули (`_buttons.scss`, `_modals.scss`, `_tabs.scss`, `_inputs.scss`); стили кнопок, полей ввода, модалок и вкладок задаются в `+layout.svelte` и в компонентах (см. [frontend.md](./frontend.md)).

---

## 6. Документация и ссылки

- **Описание существующих SCSS-классов и миксинов:** [frontend.md](./frontend.md).

Ссылка на эту стратегию добавлена в [frontend.md](./frontend.md) и при необходимости может быть добавлена в README или в общий индекс документации.
