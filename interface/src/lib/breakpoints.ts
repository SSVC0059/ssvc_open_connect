/**
 * Единый источник брейкпоинтов для JS-медиазапросов.
 *
 * Значения синхронизированы с Sass-переменными из `$lib/styles/base/_variables.scss`:
 * $breakpoint-md = 768px, $breakpoint-lg = 1024px. Медиазапрос максимальной ширины
 * берёт границу на 1px меньше, чтобы не пересекаться с min-width правилами Tailwind.
 *
 * Использовать вместо строковых литералов `(max-width: 767px)` в компонентах:
 * раньше одно и то же число было продублировано в четырёх файлах.
 */

export const MOBILE_MAX_WIDTH = 767;
export const COMPACT_MAX_WIDTH = 1023;

/** Мобильный режим: настройки в одну колонку, мобильные табы. */
export const MOBILE_MQ = `(max-width: ${MOBILE_MAX_WIDTH}px)`;

/** Узкий десктоп: свёрнутое меню и компактная шапка. */
export const COMPACT_MQ = `(max-width: ${COMPACT_MAX_WIDTH}px)`;
