# Отчёт анализа кода интерфейса

**Дата:** 7 марта 2025  
**Каталог:** `interface/`  
**Статус:** актуализировано

---

## 1. Неиспользуемый код

### 1.1 Неиспользуемые компоненты

| Файл | Описание |
|------|----------|
| ~~`src/lib/components/LoadingSpinner.svelte`~~ | Удалён ✅ |
| `src/lib/components/profiles/rectification_type.ts` | Интерфейсы пока не импортируются. Оставлен как задел на будущее. |

### 1.2 Неиспользуемые импорты

| Файл | Импорт | Примечание |
|------|--------|------------|
| ~~`src/routes/wifi/ap/Accesspoint.svelte`~~ | ~~`Collapsible`~~ | Удалён ✅ |

### 1.3 Неиспользуемые функции и экспорты

| Файл | Функция | Примечание |
|------|---------|------------|
| `src/lib/utils/ssvcHelper.ts` | `isApiVersion()` | Нигде не импортируется и не вызывается. |
| `src/lib/utils/ssvcHelper.ts` | `formatSecondsToHHMMSS()` | Нигде не импортируется и не вызывается. |
| `src/lib/utils/deepMerge.ts` | `deepMerge()` | Используется только внутри файла при рекурсии. Снаружи импортируются только `normalizeProfile` и `deepClone`. |

### 1.4 Отдельная точка входа

| Файл | Примечание |
|------|------------|
| `src/calculator.js` | Отдельная точка входа калькулятора. Подключается через `vite.calculator.config.ts`, не входит в основное SvelteKit-приложение. |

---

## 2. Скрытый / отключённый код

### 2.1 Скрытые пункты меню

| Файл | Находка |
|------|---------|
| ~~`src/routes/menu.svelte`~~ | ~~Пункт **«Соединения»**~~ — удалён ✅ |
| `src/routes/menu.svelte` | Кнопка **Discord** (`discord.active = false`) никогда не отображается. |

### 2.2 Ошибки в URL

| Файл | Статус |
|------|--------|
| `src/routes/menu.svelte` | ~~Пробелы в `/oc/telemetry` и `/oc/settings`~~ — исправлено ✅ |

### 2.3 Закомментированный код

| Файл | Содержимое |
|------|------------|
| `src/lib/components/Telemetry/RectImg.svelte` | Закомментированный CSS |
| `src/lib/stores/socket.ts` | `//console.log('disconnect'...)`, `//console.log('connect')` |
| `src/lib/DaisyUiHelper.ts` | `// console.debug(...)` |

---

## 3. Потенциальные баги и проблемный код

### 3.1 Дублирующее присваивание

| Файл | Статус |
|------|--------|
| `src/lib/utils/deepMerge.ts` | ~~Дубликат `output[key] = source[key]`~~ — исправлено ✅ |

---

## 4. Дублирующийся код

### 4.1 Часовые пояса

| Файлы | Статус |
|-------|--------|
| `src/routes/network/timezones.ts` | Единственный файл. Дубликат удалён вместе с каталогом connections ✅ |

### 4.2 Вспомогательные функции

| Функция | Статус |
|---------|--------|
| `convertSeconds` | Вынесена в `$lib/utils/formatters.ts` ✅ Используется в: NtpSettings, SystemStatusTab, SystemStatus. |
| `preventDefault` | Дублируется в 4 файлах: Accesspoint, ApSettings, EditUser, EditNetwork. Рекомендация: вынести в `$lib/utils/dom.ts`. |

---

## 5. Комментарии для доработки

| Файл | Комментарий |
|------|-------------|
| `src/routes/oc/telemetry/Ssvc.svelte` | `// Make ApiVersionGuard more subtle` |
| `src/lib/components/profiles/ProfileEditor.svelte` | `// Опционально для иконки` |
| `src/lib/components/Telemetry/ThermalSensors.svelte` | `/* В ThermalSensors.svelte */`, `/* Стилизация скроллбара */` |

---

## 6. Рекомендуемые действия

### Выполнено ✅
1. ~~Дублирующее присваивание в `deepMerge.ts`~~
2. ~~Пробелы в URL в `menu.svelte`~~
3. ~~Пункт «Соединения» в меню~~ — удалён
4. ~~`LoadingSpinner.svelte`~~ — удалён
5. ~~Импорт `Collapsible` в `Accesspoint.svelte`~~
6. ~~Каталог `connections/`~~ — удалён
7. ~~Дубликат `timezones.ts`~~ — удалён с connections
8. ~~`convertSeconds`~~ — вынесена в `$lib/utils/formatters.ts`

### Осталось
- Удалить или использовать: `isApiVersion`, `formatSecondsToHHMMSS`, `deepMerge`
- Удалить или раскомментировать закомментированный код (alerts, console.log)
- Вынести `preventDefault` в `$lib/utils/dom.ts`
- Решить судьбу кнопки Discord — включить или удалить

---

## 7. Сводная таблица

| Категория | Актуально |
|-----------|-----------|
| Неиспользуемый код (экспорты) | 3 (`isApiVersion`, `formatSecondsToHHMMSS`, `deepMerge`) |
| Скрытый / отключённый | 1 (Discord) |
| Закомментированный код | 3 файла |
| Дублирование | 1 (`preventDefault` в 4 местах) |
| Комментарии для доработки | 4 |
