/**
 * Промпты-сценарии WP10 (mcp-server-agents.md §14; mcp-server.md §3.7).
 *
 * Промпт — это не действие, а готовая последовательность шагов для агента: какие
 * инструменты вызвать, в каком порядке, где остановиться и где требуется явное
 * подтверждение оператора. Сам промпт ничего не вызывает и не меняет: тексты
 * описывают порядок, а решения принимает модель вместе с оператором.
 *
 * Инструменты упоминаются только по реальным именам из `src/tools/*.tools.ts`;
 * это проверяет `test/prompts.test.ts` по спискам `*Tools()`.
 *
 * Регистрация — на сессию: `http/app.ts` создаёт `McpServer` для каждой сессии,
 * поэтому `registerPrompts(server)` вызывается сразу после `createMcpServer`.
 */

import type { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import { z } from 'zod';

/** Описание одного промпта: метаданные, схема аргументов и сборка текста. */
export interface PromptSpec {
  readonly name: string;
  readonly title: string;
  readonly description: string;
  /** Схема аргументов; у промпта без аргументов поле не задано. */
  readonly argsSchema?: z.ZodRawShape;
  /** Готовый текст с подставленными аргументами. */
  readonly build: (args: Record<string, unknown>) => string;
}

/** Строковый аргумент промпта: пустая строка и пробелы считаются «не передан». */
function stringArg(args: Record<string, unknown>, key: string): string | undefined {
  const value = args[key];
  if (typeof value !== 'string') return undefined;
  const trimmed = value.trim();
  return trimmed === '' ? undefined : trimmed;
}

/** Числовой аргумент промпта (zod уже привёл его к number). */
function numberArg(args: Record<string, unknown>, key: string): number | undefined {
  const value = args[key];
  return typeof value === 'number' && Number.isFinite(value) ? value : undefined;
}

const startRectification: PromptSpec = {
  name: 'start_rectification',
  title: 'Запустить ректификацию',
  description:
    'Проверяет версию API, статус процесса и предохранитель XKC-25, затем после явного ' +
    'подтверждения предлагает oc_start и ожидание стадии heads. Действия не выполняет.',
  argsSchema: {
    profile_id: z
      .string()
      .min(1)
      .optional()
      .describe(
        'Идентификатор профиля для проверки перед запуском. Профиль не применяется: ' +
          'применение — отдельный сценарий apply_profile с подтверждением.',
      ),
    confirm_token: z
      .string()
      .min(1)
      .optional()
      .describe(
        'Токен явного подтверждения оператора. Без него сценарий останавливается перед ' +
          'oc_start и ждёт подтверждения в чате.',
      ),
  },
  build: (args) => {
    const profileId = stringArg(args, 'profile_id');
    const confirmToken = stringArg(args, 'confirm_token');

    const profileStep =
      profileId === undefined
        ? 'Профиль не задан: работай с действующими настройками контроллера. Не вызывай oc_profile_apply — применение профиля в этот сценарий не входит.'
        : `Вызови oc_profile_get с id: "${profileId}" и убедись, что профиль существует и относится к ректификации. Не применяй профиль здесь: применение — отдельный сценарий apply_profile с подтверждением.`;

    const startStep =
      confirmToken === undefined
        ? 'Подтверждения нет (confirm_token не передан). Не вызывай oc_start: остановись на этом шаге и жди явного подтверждения оператора в чате.'
        : `Оператор передал подтверждение (confirm_token: "${confirmToken}") — считай это явным согласием. Вызови oc_start с аргументом confirm: true.`;

    return [
      'Ты — помощник оператора ректификации на контроллере OpenConnect (SSVC). Подготовь запуск процесса и доведи его до стадии heads. Этот сценарий сам ничего не запускает: действия на контроллере выполняются только после явного подтверждения оператора.',
      '',
      'Порядок:',
      '1. Вызови oc_get_info. Проверь compatibility и is_support_release. Если compatibility = too_old — остановись и объясни, чего не хватает; дальше не иди.',
      '2. Вызови oc_get_process_status. Если status = running или стадия уже идёт — не запускай второй процесс: покажи stage, status и started_at и остановись, спросив оператора о дальнейших шагах.',
      '3. Вызови oc_get_xkc25_status. Если block_start = true — не вызывай oc_start. Перечисли block_reasons и warnings дословно, объясни каждый код и предложи сценарий troubleshoot_start_blocked. Это условие остановки.',
      `4. ${profileStep}`,
      '5. Покажи оператору сводку: стадия и статус, готовность датчиков уровня, предупреждения, выбранный профиль. Запроси явное подтверждение запуска.',
      `6. ${startStep}`,
      '7. После accepted:true от oc_start вызови oc_wait_for_change с expect: { stage: "heads" } и timeout_s: 180. Помни: accepted — это «принято в очередь», а не «процесс пошёл»; физического подтверждения нет.',
      '8. Если matched:false, timeout:true или uart_error:true — остановись и доложи наблюдённые stage, status и uart_error. Не повторяй oc_start без нового подтверждения оператора.',
      '9. В итоге сообщи: что принято контроллером, текущая стадия, предупреждения и что делать дальше.',
      '',
      'Условия остановки: слишком старая версия API, процесс уже идёт, block_start = true, нет подтверждения оператора, потеря UART или таймаут ожидания стадии.',
    ].join('\n');
  },
};

const troubleshootStartBlocked: PromptSpec = {
  name: 'troubleshoot_start_blocked',
  title: 'Разобрать блокировку старта',
  description:
    'Разбирает причины block_start по датчикам уровня XKC-25 и аппаратным сбоям, ' +
    'предлагает действия и проверку. Команды управления процессом не отправляет.',
  build: () =>
    [
      'Ты — диагност контроллера OpenConnect (SSVC). Старт ректификации заблокирован предохранителем XKC-25. Разбери причины и предложи действия; команды управления процессом не отправляй (oc_start, oc_pause, oc_resume, oc_next, oc_stop не вызывай).',
      '',
      '1. Вызови oc_get_xkc25_status. Для каждого датчика в sensors[] зафиксируй index, zone, enabled, pinSource, gpio, pinLabel, calibrated, live, rawLevel и filled. Отдельно выпиши trigger_action, start_guard_mode, block_start, block_reasons и warnings.',
      '2. Вызови oc_get_hardware_faults. Разбери faults[] (code, codeLabel, address, role), count, i2c_bus_active и i2c_scan.',
      '3. Сопоставь block_reasons с конкретными датчиками и аппаратными сбоями: укажи, какой index/зона и какой fault связаны с каждой причиной. Если связь неочевидна, так и напиши — не додумывай.',
      '4. Что делать по типовым кодам:',
      '   - датчик не откалиброван (calibrated:false) — предложи oc_xkc25_calibrate по шагам without_liquid → with_liquid → verify_return, между шагами проверяй oc_get_xkc25_status, а результат возврата — через oc_xkc25_verify_status;',
      '   - unstable_signal — сигнал нестабилен: попроси успокоить жидкость, проверить контакт и помехи, затем повторить шаг;',
      '   - not_reacted — датчик не отреагировал на изменение уровня: попроси проверить погружение/извлечение датчика, питание и линию GPIO, затем повторить шаг;',
      '   - i2c_nack, шина недоступна, устройство не найдено — сверь адрес с i2c_scan, проверь питание и шлейф I²C;',
      '   - потеря UART — проверь кабель между ESP32 и контроллером SSVC.',
      '5. Не выдумывай коды: цитируй только значения, которые вернул контроллер. Неизвестный код назови неизвестным и перечисли, что нужно проверить.',
      '6. Проверка результата: после действий оператора снова вызови oc_get_xkc25_status — block_start должен стать false, а block_reasons — опустеть. Только тогда предложи вернуться к сценарию start_rectification.',
      '7. Если причина не устраняется по кодам, перечисли, что проверить физически (питание, общий провод, контакты GPIO, уровень жидкости), и остановись.',
      '',
      'Формат ответа: причина блокировки → связанный датчик или сбой → действие → как проверить, что блокировка снята.',
    ].join('\n'),
};

const calibrateXkc25: PromptSpec = {
  name: 'calibrate_xkc25',
  title: 'Откалибровать датчик XKC-25',
  description:
    'Ведёт мастер из трёх шагов калибровки датчика уровня с проверкой статуса между шагами ' +
    'и разбором unstable_signal/not_reacted. Сам шаги не выполняет.',
  argsSchema: {
    index: z
      .number()
      .int()
      .min(0)
      .max(3)
      .describe('Номер слота датчика, целое 0..3; возьмите из oc_get_xkc25_status.'),
  },
  build: (args) => {
    const index = numberArg(args, 'index');
    const indexText = index === undefined ? '(не задан)' : String(index);
    return [
      `Ты — помощник калибровки датчика уровня XKC-25, слот index = ${indexText}. Калибровка меняет сырые уровни датчика, поэтому каждый шаг выполняется только после того, как оператор подтвердил физическую подготовку. Этот сценарий шаги не выполняет: веди оператора и вызывай инструменты по одному.`,
      '',
      '0. Вызови oc_get_xkc25_status и найди датчик с index = ' +
        indexText +
        '. Если датчика нет, enabled:false или pinSource указывает не на тот вход — остановись и объясни, что настроить.',
      '1. Шаг without_liquid: попроси оператора извлечь датчик из жидкости и подтвердить это. Затем вызови oc_xkc25_calibrate с index: ' +
        indexText +
        ' и step: "without_liquid". Проверь ok, completed, code, message и checklist.',
      '2. Проверка между шагами: снова вызови oc_get_xkc25_status. Если датчик не отвечает (live:false) или код шага неуспешный — не переходи к следующему шагу, а разбери причину.',
      '3. Шаг with_liquid: попроси оператора погрузить датчик в жидкость до рабочего уровня и подтвердить это. Затем вызови oc_xkc25_calibrate с index: ' +
        indexText +
        ' и step: "with_liquid"; проверь те же поля.',
      '4. Шаг verify_return: попроси оператора вернуть датчик в исходное положение и подтвердить. Затем вызови oc_xkc25_calibrate с index: ' +
        indexText +
        ' и step: "verify_return" (timeout_ms 5000). Если verify.status = "pending", вызови oc_xkc25_verify_status и дождись статуса done; не запускай verify_return повторно, пока проверка идёт.',
      '5. Если код unstable_signal: сигнал нестабилен — попроси успокоить жидкость, проверить контакт и помехи, затем повтори шаг. Если код not_reacted: датчик не увидел изменения уровня — попроси проверить погружение/извлечение, питание и линию GPIO, затем повтори шаг.',
      '6. Если unstable_signal или not_reacted повторяются после исправления — остановись, вызови oc_get_hardware_faults для проверки I²C/UART и предложи сценарий troubleshoot_start_blocked.',
      '7. Итог: какие шаги выполнены, что показал oc_get_xkc25_status (calibrated, live, rawLevel), какие коды вернул контроллер и что осталось сделать.',
      '',
      'Условия остановки: датчик не найден или выключен, шаг вернул неуспешный код, проверка возврата не завершилась, повторяющиеся unstable_signal/not_reacted.',
    ].join('\n');
  },
};

const analyzeLastRun: PromptSpec = {
  name: 'analyze_last_run',
  title: 'Разобрать последний прогон',
  description:
    'Запрашивает список журналов, скачивает нужный и делает краткую сводку по фракциям ' +
    'без вставки всего CSV. Только чтение.',
  argsSchema: {
    process_id: z
      .number()
      .int()
      .min(1)
      .optional()
      .describe('Номер процесса; если не задан, берётся последний журнал из oc_logs_list.'),
  },
  build: (args) => {
    const processId = numberArg(args, 'process_id');
    const processStep =
      processId === undefined
        ? 'Номер процесса не задан: возьми максимальный номер из files (файл «12.CSV» — процесс 12).'
        : `Номер процесса задан: ${processId}.`;
    return [
      'Ты — аналитик журналов ректификации OpenConnect. Задача — краткая сводка по последнему или указанному прогону. Ничего на контроллере не меняй: используй только чтение.',
      '',
      '1. Вызови oc_logs_list. Если status = receiving — повтори вызов, пока список не будет готов. Если files пуст — сообщи, что журналов нет, и остановись.',
      `2. Определи процесс: ${processStep} Если указанного процесса нет в files, остановись и покажи доступные номера.`,
      '3. Вызови oc_logs_download с process_id и сохрани файл. Возьми из ответа saved_path, bytes, lines и preview (первые 20 строк). Полный CSV в ответ не вставляй.',
      '4. Сделай краткую сводку по фракциям: длительность каждой стадии, объёмы, ключевые температуры и давление, моменты перехода между фракциями. Отметь пропуски данных, скачки и обрывы.',
      '5. Если preview недостаточно, прочитай сохранённый файл по saved_path вне MCP и посчитай агрегаты; в ответ включи только агрегаты и 3–5 характерных строк, а не весь CSV.',
      '6. В конце укажи путь к файлу, число строк, ограничения анализа (например, «журнал обрывается на стадии tails») и что стоит проверить в следующий раз.',
      '',
      'Условия остановки: список журналов пуст, указанного процесса нет, скачивание вернуло busy или ошибку — доложи причину и не повторяй вызов без необходимости.',
    ].join('\n');
  },
};

const applyProfile: PromptSpec = {
  name: 'apply_profile',
  title: 'Применить профиль',
  description:
    'Показывает содержимое профиля и разницу с действующими настройками, затем после явного ' +
    'подтверждения применяет его. Сам ничего не применяет.',
  argsSchema: {
    profile_id: z
      .string()
      .min(1)
      .describe('Идентификатор профиля; возьмите его из oc_profiles_list.'),
  },
  build: (args) => {
    const profileId = stringArg(args, 'profile_id') ?? '(не задан)';
    return [
      `Ты — помощник применения профиля ректификации OpenConnect. Профиль: "${profileId}". Применение меняет настройки контроллера и влияет на процесс, поэтому выполняется только после явного подтверждения оператора. Этот сценарий сам ничего не применяет.`,
      '',
      '1. Вызови oc_profiles_list и убедись, что профиль с id "' +
        profileId +
        '" существует. Если его нет — остановись и покажи доступные id и названия.',
      '2. Вызови oc_profile_get с id: "' +
        profileId +
        '" — получи полное содержимое профиля (ssvcSettings, фракции, пороги, правила реле).',
      '3. Вызови oc_get_settings — получи действующие настройки SSVC.',
      '4. Покажи diff: только ключи, которые изменятся, в виде «ключ: было → станет». Сгруппируй по смыслу (фракции, таймеры, гистерезис, декремент, формула). Если diff пуст — сообщи, что профиль совпадает с текущими настройками.',
      '5. Оцени влияние: какие фракции и таймеры затронуты, нужен ли перезапуск, что будет с текущим процессом. Если процесс running — предупреди оператора явно.',
      '6. Запроси явное подтверждение применения. Без подтверждения остановись: не вызывай oc_profile_apply.',
      '7. После подтверждения вызови oc_profile_apply с id: "' +
        profileId +
        '" и confirm: true. Помни: успех означает, что профиль стал активным и применён, а не что процесс пошёл по-новому.',
      '8. Проверь результат: вызови oc_profiles_list (active_id/isApplied) и при необходимости oc_get_settings. Доложи applied_id, warning и фактический diff после применения.',
      '',
      'Условия остановки: профиль не найден, diff не показан оператору, нет явного подтверждения, профиль применяется во время running без согласия оператора.',
    ].join('\n');
  },
};

/** Пять промптов-сценариев в порядке таблицы §14. */
export const PROMPT_SPECS: readonly PromptSpec[] = [
  startRectification,
  troubleshootStartBlocked,
  calibrateXkc25,
  analyzeLastRun,
  applyProfile,
];

/** Превращает текст сценария в ответ `prompts/get`. */
function toPromptResult(spec: PromptSpec, text: string) {
  return {
    description: spec.description,
    messages: [
      {
        role: 'user' as const,
        content: { type: 'text' as const, text },
      },
    ],
  };
}

/**
 * Регистрирует пять промптов в `McpServer` сессии.
 * Вызывается один раз на сервер: повторная регистрация имени — ошибка SDK.
 */
export function registerPrompts(server: McpServer): void {
  for (const spec of PROMPT_SPECS) {
    if (spec.argsSchema === undefined) {
      // Промпт без аргументов: `arguments` в `prompts/list` не отдаётся.
      server.registerPrompt(
        spec.name,
        { title: spec.title, description: spec.description },
        () => toPromptResult(spec, spec.build({})),
      );
      continue;
    }
    server.registerPrompt(
      spec.name,
      { title: spec.title, description: spec.description, argsSchema: spec.argsSchema },
      (args) => toPromptResult(spec, spec.build(args as Record<string, unknown>)),
    );
  }
}
