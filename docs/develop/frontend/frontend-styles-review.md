# Ревью стилей фронтенда SSVC Open Connect

Дата: 2026-11-10
Ветка: `review/frontend-styles`
Область: `interface/src` — глобальные стили, SCSS-слои, локальные стили Svelte-компонентов, сборка.

## 0. Статус выполнения

Выполнено в этой ветке (проверка `npm run check` — 0 ошибок, 0 предупреждений):

- **P0-1 — миграция DaisyUI 4 → 5.** 110 замен в 15 файлах: `--b1/--b2/--b3/--bc/--p` → `--color-base-*` / `--color-primary`, альфа-варианты → `color-mix(in oklab, …)`. В коде не осталось `oklch(var(--b…)` и `oklch(var(--p…)`.
- **P0-2 — управляемая тема.** `data-theme="corporate|business"` на `<html>`, инлайн-скрипт в `app.html` до отрисовки (без FOUC), переключатель `ThemeToggle.svelte` в статусбаре, сохранение в `localStorage`. `--prefersdark` отключён, чтобы тема не расходилась с `data-theme`.
- **P0-3 — контраст.** В `[data-theme="business"]` заданы светлые значения `--primary-*`, `--glass-*`, тень и границы; текст/границы переводятся на `--color-base-content` и `color-mix`.
- **P0-4 — `color-scheme`.** `light` по умолчанию и `dark` для `[data-theme="business"]`; фон — `var(--color-base-100)`.
- **P0-6 — доступность.** `lang="ru"`, `theme-color` для обеих схем, `viewport-fit=cover`, глобальный `:focus-visible`, `cursor: not-allowed` для disabled, `scroll-behavior: smooth` только при `prefers-reduced-motion: no-preference`.
- **P0-5 — мобильные стили без `!important`.** Убраны все 39 `!important` из `_settings-stack-card.scss` (22), `_system-tabs-mobile.scss` (7) и `_profile-manager.scss` (10). Селекторы начинаются с класса-обёртки, поэтому специфичнее базовых правил; в `_profile-manager.scss` тёмная тема переведена с `@media (prefers-color-scheme: dark)` на `:global([data-theme='business'])`, чтобы следовать переключателю, а не только системе. Там же исправлен порядок `border` / `border-left` (shorthand сбрасывал 4px-полосу) и добавлен `color: var(--color-primary-content)` на активном элементе.
- **P1 (часть).** `--transition` вместо `all`; `--z-modal-backdrop` отделён от `--z-sticky`; убран дубль `--gray-500`; `border-color` базового слоя — через `color-mix`; из `normalize.css` удалены дубли Tailwind preflight; стилизованы скроллбары.

Не входит в этот шаг (следующие PR): ленивый `chart.js`, замена FontAwesome на `unplugin-icons`, токены отступов/радиусов и дизайн-полировка.

## 1. Карта стилей

| Слой | Где | Комментарий |
|---|---|---|
| Tailwind 4 + DaisyUI 5 | `app.css` | `@plugin "daisyui"` с темами `corporate` (по умолчанию) и `business` (`--prefersdark`) |
| Глобальные переменные | `app.css` `:root` + `@media (prefers-color-scheme: dark)` | Два независимых набора: DaisyUI-темы и собственные `--primary-*`, `--glass-*`, `--dark-*` |
| Нормализация | `normalize.css` | Дублирует Tailwind preflight, задаёт `color-scheme` и фон |
| SCSS-слои | `lib/styles/**` | `base`, `layout`, `components`, `themes`, `views` |
| Локальные стили | `<style>` в Svelte-компонентах | ~15 файлов с собственными `@media (prefers-color-scheme: dark)` |
| Сборка | `vite.config.ts` | Tailwind-плагин, unplugin-icons, адаптер static |

Главная архитектурная проблема: **три источника истины для цвета** — темы DaisyUI, переменные в `app.css` и локальные переопределения в компонентах. Они расходятся, отсюда большинство визуальных дефектов.

## 2. Критические проблемы (P0)

### P0-1. Переменные DaisyUI 4 в проекте на DaisyUI 5

В DaisyUI 5 нет переменных `--b1`, `--b2`, `--b3`, `--bc`, `--p`: вместо них `--color-base-100`, `--color-base-200`, `--color-base-300`, `--color-base-content`, `--color-primary`, и они уже содержат полный `oklch(...)`. Выражения вида `oklch(var(--b1, fallback))` поэтому **всегда используют fallback** и не следуют теме.

Затронуто:
- `interface/src/app.css:97-102`
- `interface/src/normalize.css:107,130,135`
- `interface/src/lib/styles/components/_settings-page-mobile.scss:13,44,60`
- `interface/src/lib/styles/components/_settings-stack-card.scss:31`
- `interface/src/lib/styles/components/_profile-manager.scss` (много мест)
- `interface/src/lib/components/profiles/ProfileEditor.svelte:628+`
- `interface/src/lib/components/profiles/ProfileViewer.svelte:262+`
- `interface/src/lib/components/profiles/ProfileOpenConnectRelayTab.svelte:837+`

Последствия: фон страницы, границы, поверхности карточек и текст не совпадают с активной темой; в `business` возможны светлые «пятна» и низкий контраст.

Исправление:
```css
/* было */
color: oklch(var(--bc));
border: 1px solid oklch(var(--bc) / 0.2);
background: oklch(var(--b2));

/* стало */
color: var(--color-base-content);
border: 1px solid color-mix(in oklab, var(--color-base-content) 20%, transparent);
background: var(--color-base-200);
```
Добавить в CI проверку `grep -R "oklch(var(--b" interface/src` — она не должна находить совпадений.

### P0-2. Нет управляемой темы — только системная

Тема выбирается исключительно через `prefers-color-scheme`: `--prefersdark` в `app.css` плюс 11+ локальных медиазапросов. Пользователь не может переключить тему, а если добавить `data-theme`, локальные блоки `@media` останутся в прежнем состоянии и разойдутся с темой DaisyUI.

Исправление:
1. `data-theme="corporate|business"` на `<html>`.
2. Инлайн-скрипт в `app.html` до отрисовки (устраняет FOUC).
3. Переключатель в статусбаре/настройках, сохранение в `localStorage`.
4. Замена `@media (prefers-color-scheme: dark)` на `[data-theme="business"]` (или на семантические переменные, чтобы медиазапросы вообще не понадобились).

### P0-3. Нечитаемый текст и границы в тёмной теме

`--glass-primary-800` (`#243b53`) и `--primary-200` (`#d9e2ec`) заданы один раз и в тёмной теме не переопределяются, но используются как цвет текста и границ:
- `lib/styles/base/_mixins.scss` — `parameter-container`, `.reading-label`, `.reading-value`, `.point-label`, `.point-value`;
- часть компонентов переопределяет цвет вручную через `dark-theme-color`, часть — нет.

Результат: тёмно-синий текст на тёмно-серой карточке и светлые линии там, где их не ждут.

Исправление: перевести текст на `--color-base-content`, границы — на `color-mix(...)`, либо задать полный набор `--primary-*`/`--glass-*` для тёмной темы.

### P0-4. `color-scheme` зашит в светлый

`normalize.css:112` задаёт `color-scheme: light`, а `:147-155` переключает на `dark` только по системной схеме. При ручном переключении темы нативные `select`, `input[type=date]`, чекбоксы и скроллбары останутся светлыми.

Исправление: `color-scheme: light dark` на `:root` и привязка к `[data-theme="business"]`.

### P0-5. Мобильная вёрстка держится на `!important`

| Файл | `!important` |
|---|---|
| `_settings-stack-card.scss` | 26 |
| `_profile-manager.scss` | 10 |
| `_system-tabs-mobile.scss` | 4 |

Это признак борьбы со специфичностью: любое изменение DaisyUI-классов или порядка подключения стилей ломает экран. Отдельно: `:global(...)` в `_settings-page-mobile.scss:73,80,84,89,95` работает только потому, что файл импортируется в `<style lang="scss">` Svelte-компонента. При выносе в глобальный CSS селекторы перестанут совпадать.

План: снизить специфичность (один класс-обёртка вместо каскада `.settings-grid .settings-panel .settings-section`), заменить `!important` на CSS-переменные, зафиксировать мобильные стили в одном слое.

### P0-6. Доступность и метаданные

- `app.html`: `lang="en"` при русскоязычном интерфейсе; нет `theme-color`; нет `viewport-fit=cover`.
- Нет глобального `:focus-visible` — клавиатурная навигация не видна (единственный focus-ring в `_mixins.scss`).
- `button { cursor: pointer }` применяется и к `disabled`.
- `scroll-behavior: smooth` глобально — рывки при смене маршрута и конфликт с `prefers-reduced-motion` (частично закрыт).

## 3. Высокий приоритет (P1)

1. `--transition: all 0.3s ease` (`app.css`) — анимация всех свойств. Заменить на `background-color .2s, border-color .2s, color .2s, box-shadow .2s`.
2. Дубли и мёртвые переменные: `--gray-500` объявлена дважды (hex и `oklch`), `--red-700` совпадает с `--red-600-dark`, `--border-radius` почти не используется.
3. `--z-modal-backdrop: 40` и `--z-sticky: 40` — одинаковый слой, риск перекрытия модалки и залипшей шапки.
4. `normalize.css` дублирует preflight Tailwind: `box-sizing`, `margin`, `img { display:block }`, `font: inherit`. Оставить только реально нужное (фон, `height`, `scrollbar-gutter`).
5. `body { min-height: 100%; min-height: 100svh; min-height: 100dvh }` — `100%` без высоты у `html` не работает; оставить `100dvh` с fallback `100svh`.
6. Статический `import { Chart } from 'chart.js'` в трёх файлах (`routes/system/SystemMetricsTab.svelte`, `routes/system/metrics/BatteryMetrics.svelte`, `routes/system/metrics/SystemMetrics.svelte`) — библиотека попадает в основной бандл. **Частично исправлено:** добавлен `$lib/charts.ts` с ленивой загрузкой, `BatteryMetrics.svelte` переведён на него; в двух оставшихся файлах по несколько графиков и `$effect` — нужен отдельный проход.
7. `svelte-fa` + `@fortawesome/free-solid-svg-icons` при уже подключённом `unplugin-icons` — два набора иконок и лишний вес.
8. `open-connect-main.scss` импортируется в корневом `+layout.svelte` — весь SCSS грузится на каждом маршруте.

## 4. Оптимизация загрузки и времени работы

1. **chart.js — лениво.** `const { Chart, registerables } = await import('chart.js')` внутри `onMount`; регистрация один раз на модуль. Ожидаемый эффект — минус сотни КБ из initial bundle.
2. **Иконки.** Перевести оставшиеся FA-иконки на `~icons/mdi/...` (unplugin-icons уже настроен) и удалить `svelte-fa`, `@fortawesome/*`.
3. **Аудит бандла.** `npm run build` + `vite-bundle-visualizer`; цель — зафиксировать бюджет initial JS (например, ≤ 250 КБ gzip) и проверять его в CI.
4. **Разделение CSS.** Критичные стили — инлайн/в layout, остальное (`views/_wizard.scss`, `components/_profile-manager.scss`) — на соответствующих маршрутах.
5. **Длинные списки.** `content-visibility: auto` + `contain-intrinsic-size` для секций настроек и карточек датчиков; при > 100 строк — виртуализация таблиц.
6. **Анимации.** Анимировать `transform`/`opacity`, а не `all`; убрать `will-change` из статических правил.
7. **Раздача.** Проверить, что LittleFS отдаёт хэшированные ассеты с `Cache-Control: immutable`, а HTML — с `no-cache`.

## 5. Мобильная версия: что проверить и исправить

- Рассинхрон breakpoints: `_responsive.scss` — `max-width: 640px`, `_settings-page-mobile.scss`/`_settings-stack-card.scss` — `767px`, часть компонентов — `768px`. Свести к Tailwind-шкале (`sm 640`, `md 768`).
- Дублирование логики: `_settings-page-mobile.scss` и `_settings-stack-card.scss` решают одну задачу (одна карточка на узком экране) разными способами.
- iOS Safari: `100dvh` + фиксированные header/footer — проверить при скрытой адресной строке; `padding-bottom: env(safe-area-inset-bottom)`.
- `_valve-table.scss` — горизонтальный скролл на 360 px, закреплённая первая колонка.
- Тач: обернуть hover-эффекты в `@media (hover: hover)`, минимальная цель 44×44 px для иконок статусбара.
- Модалки: `max-height: calc(100dvh - 2rem)` и внутренний скролл, иначе кнопки уезжают за экран.

Чек-лист ручной проверки: 360×640, 390×844, 768×1024, landscape; системная тёмная тема; увеличение шрифта 150%.

## 6. Дизайн: устранение визуальных проблем

1. **Границы в базовом слое.** `app.css` задаёт `border-color: var(--color-gray-200)` для всех элементов (совместимость с Tailwind 3) — в тёмной теме это светлая рамка у всего, что не переопределено. Заменить на `color-mix(in oklab, var(--color-base-content) 12%, transparent)`.
2. **Тени.** `--glass-shadow` — жёсткая синяя тень, в тёмной теме не меняется; задать отдельное значение для `business`.
3. **Радио-табы.** `border-bottom-color: oklch(0.86 0 0)` — фиксированный светлый цвет, в тёмной теме яркая полоса. Перевести на `--color-base-300`.
4. **Единая шкала.** Сейчас одновременно `0.5rem`, `0.625rem`, `0.75rem`, `calc(var(--border-radius) / 2)`. Ввести токены `--radius-sm/md/lg` и `--space-1..5`.
5. **Типографика.** `.settings-section-title` — `1.25rem` в миксине и `1.125rem` в мобильном SCSS; свести к одной шкале.
6. **Иконки.** Три набора (FA, MDI, Tabler) с разной толщиной штриха — выбрать один.
7. **Скроллбары.** Стилизовать через `scrollbar-color`/`scrollbar-width`, иначе в тёмной теме светлая полоса.
8. **Пустые состояния и загрузка.** Проверить скелетоны для карточек датчиков и таблиц — сейчас пустой экран до прихода данных.

## 7. План работ

### Этап 0 — подготовка (0.5 дня)
- [ ] Зафиксировать baseline: скриншоты светлой/тёмной темы, 360/768/1280.
- [ ] Прогнать `npm run check` и `npm run build`, сохранить размеры чанков.

### Этап 1 — критично (2–3 дня)
- [ ] P0-1: миграция `--b*`/`--bc`/`--p` на переменные DaisyUI 5 (механически + выборочная проверка).
- [ ] P0-2: `data-theme`, инлайн-скрипт, переключатель, удаление дублирующих медиазапросов.
- [ ] P0-4: `color-scheme`, фон, нативные контролы.
- [ ] P0-3: контраст текста и границ в `business` (проверка ≥ 4.5:1).
- [ ] P0-6: `lang="ru"`, `theme-color`, `focus-visible`, `cursor` для disabled.

### Этап 2 — мобильные и поддерживаемость (2 дня)
- [x] P0-5: убрать `!important` из `_settings-stack-card.scss`, `_system-tabs-mobile.scss`, `_profile-manager.scss` (39 штук).
- [ ] Свести breakpoints к `sm/md/lg`: SCSS уже считает `$max-md: v.$breakpoint-md - 1px`, остались JS-медиазапросы `(max-width: 767px)` в `oc/settings/Settings.svelte`, `oc/ssvc/Settings.svelte`, `system/+page.svelte` и `(max-width: 1023px)` в `+layout.svelte`, `menu.svelte`, `SsvcApiWarning.svelte` — вынести в общий модуль.
- [ ] Проверить таблицы, модалки, safe-area, hover на тач-устройствах.

### Этап 3 — производительность (1–2 дня)
- [ ] Ленивый `chart.js`.
- [ ] Замена FA на `unplugin-icons`, удаление зависимостей.
- [ ] Бюджет бандла в CI, разделение CSS по маршрутам.
- [ ] `content-visibility` для длинных секций.

### Этап 4 — дизайн-полировка (1–2 дня)
- [ ] Токены отступов/радиусов, единая типографика.
- [ ] Тени, скроллбары, радио-табы, иконки.
- [ ] Скелетоны и пустые состояния.

## 8. Критерии приёмки

- `grep -R "oklch(var(--b" interface/src` — 0 совпадений.
- Переключение темы без вспышки (FOUC) и без светлых артефактов.
- Контраст текста ≥ 4.5:1 в обеих темах (Lighthouse/axe).
- 360 px — нет горизонтального скролла ни на одной странице.
- Lighthouse Accessibility ≥ 95, Performance ≥ 90 на локальной сборке.
- Initial JS ≤ согласованного бюджета; chart.js — в отдельном чанке.
- Нет `!important` в мобильных SCSS (кроме документированных исключений).

## 9. Риски

| Риск | Митигация |
|---|---|
| Массовая замена переменных затронет все экраны | Этап 1 отдельным PR, скриншот-регрессия по ключевым страницам |
| `data-theme` конфликтует с `--prefersdark` | Явно отключить `--prefersdark` после внедрения переключателя |
| Удаление `!important` меняет вид на мобильных | Пошагово, по одному файлу, с проверкой на 360 px |
| Ленивый chart.js ломает SSR/тесты | Динамический импорт внутри `onMount`, мок в тестах |

## 10. Быстрые победы (можно сделать сразу)

1. `app.html`: `lang="ru"`, `meta name="theme-color"` для обеих схем.
2. Заменить `--transition: all` на список конкретных свойств.
3. Убрать дубль `--gray-500` и `--red-700`.
4. `border-color` базового слоя — через `color-mix`.
5. Ленивый импорт `chart.js` в трёх файлах. — выполнено.

## 11. Аудит бандла (фактические цифры)

Сборка `npm run build` (adapter-static) прошла за 22,65 с. Результат в `interface/build`:

| Файл | Размер |
|---|---|
| `bundle.js` | 953,2 КБ |
| `env.js` | ~0 КБ |

**Вывод: разделения на чанки нет.** Все динамические импорты — включая `import('chart.js')` — попадают в единственный `bundle.js`.

Причина — не Vite и не настройки `manualChunks` (их нет в `vite.config.ts`), а плагин `vite-plugin-littlefs`: он готовит имена файлов под LittleFS с лимитом 32 символа на имя. Плагин склеивает вывод в один файл, поэтому code splitting на диске не сохраняется.

Что это значит для сделанной работы:

- ленивая загрузка `chart.js` **не уменьшает первую загрузку** — браузер всё равно скачивает `bundle.js` целиком;
- но она сохраняет второй эффект: графики создаются после первого рендера, а не блокируют его. На ESP32 с медленным HTTP это заметно;
- цель из раздела 8 «chart.js — в отдельном чанке» при текущей схеме сборки **недостижима** и требует отдельного решения.

Варианты (по возрастанию сложности):

1. Принять как есть: измерять бюджет по размеру `bundle.js`, а не по initial JS.
2. Настроить `vite-plugin-littlefs` на короткие хэши имён (`base36`) вместо склейки — тогда чанки станут отдельными файлами, но нужно проверить лимит LittleFS и число файлов.
3. Вынести графики на отдельный маршрут (`/system/metrics`) — тогда страница метрик грузится по требованию целиком.

Рекомендация: вариант 2 как отдельная задача, с проверкой на реальном LittleFS. Пока его нет, ориентир бюджета — `bundle.js` ≤ 1 МБ.

Отдельно: пакеты `@fortawesome/free-brands-svg-icons`, `@fortawesome/free-solid-svg-icons` и `svelte-fa` больше не импортируются в исходниках. Из `package.json` их нужно удалить вместе с обновлением lockfile (`npm install`) — это меняет дерево зависимостей, поэтому отдельным коммитом.
