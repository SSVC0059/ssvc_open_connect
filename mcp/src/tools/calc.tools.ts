/**
 * Инструмент расчёта плана ректификации (риск 0, без обращения к контроллеру).
 *
 * Математика — в общем пакете `@ssvc/rectification`, том же, что используют
 * интерфейс и сборка калькулятора для документации. Здесь только адаптер:
 * плоские аргументы MCP → профиль модели → компактный план.
 *
 * Инструмент ничего не применяет и не читает с устройства: это чистый расчёт.
 */

import { DistillationCycleModel, createDefaultProfile } from '@ssvc/rectification';
import type { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import { z } from 'zod';
import { getSettings, FLOW_METHOD } from '../domain/settings.js';
import { registerTools } from '../mcp/registry.js';
import { defineTool, type ToolContext, type ToolDef, type ToolResult } from '../mcp/types.js';
import { annotationsForRisk } from '../policy/risk.js';

const RISK = 0 as const;

const percent = z.number().min(0).max(100);
const flow = z.number().min(0).max(20000);
const valveBw = z.number().min(0).max(20000);

/** Собирает профиль модели из плоских аргументов, не мутируя дефолт. */
function buildProfile(args: Record<string, any>): any {
  const profile = createDefaultProfile();

  profile.powerKw = args.power_kw ?? profile.powerKw;
  profile.volumeL = args.volume_l;
  profile.strengthVol = args.strength_pct;
  profile.stabilizationMin = args.stabilization_min ?? profile.stabilizationMin;

  profile.heads.enabled = args.heads_enabled ?? profile.heads.enabled;
  profile.heads.percent = args.heads_percent ?? profile.heads.percent;
  profile.heads.targetFlowMlh = args.heads_target_flow_mlh ?? profile.heads.targetFlowMlh;

  profile.late_heads.enabled = args.late_heads_enabled ?? profile.late_heads.enabled;
  profile.late_heads.percent = args.late_heads_percent ?? profile.late_heads.percent;
  profile.late_heads.targetFlowMlh =
    args.late_heads_target_flow_mlh ?? profile.late_heads.targetFlowMlh;

  profile.hearts.enabled = args.hearts_enabled ?? profile.hearts.enabled;
  profile.hearts.percent = args.hearts_percent ?? profile.hearts.percent;
  profile.hearts.targetFlowMlh = args.hearts_target_flow_mlh ?? profile.hearts.targetFlowMlh;

  profile.tails.enabled = args.tails_enabled ?? profile.tails.enabled;
  profile.tails.percent = args.tails_percent ?? profile.tails.percent;
  profile.tails.targetFlowMlh = args.tails_target_flow_mlh ?? profile.tails.targetFlowMlh;

  // Расширенные настройки — до явных аргументов, чтобы конкретные поля их перекрывали.
  if (args.ssvc_settings !== undefined) {
    for (const [key, value] of Object.entries(args.ssvc_settings)) {
      if (value !== undefined) profile.ssvcSettings[key] = value;
    }
  }

  if (args.valve_bw_mlh !== undefined) {
    profile.ssvcSettings.valve_bw = [...args.valve_bw_mlh];
  }
  profile.virtual_bw_late = args.virtual_bw_late ?? profile.virtual_bw_late;
  profile.virtual_bw_tails = args.virtual_bw_tails ?? profile.virtual_bw_tails;

  profile.ssvcSettings.hearts_finish_temp =
    args.hearts_finish_temp ?? profile.ssvcSettings.hearts_finish_temp;
  profile.ssvcSettings.hyst = args.hyst ?? profile.ssvcSettings.hyst;
  profile.ssvcSettings.decrement = args.decrement ?? profile.ssvcSettings.decrement;
  profile.ssvcSettings.formula = args.formula ?? profile.ssvcSettings.formula;
  profile.ssvcSettings.release_timer = args.release_timer ?? profile.ssvcSettings.release_timer;
  profile.ssvcSettings.release_speed = args.release_speed ?? profile.ssvcSettings.release_speed;
  profile.ssvcSettings.heads_final = args.heads_final ?? profile.ssvcSettings.heads_final;

  return profile;
}

function warningsFor(args: Record<string, any>, profile: Record<string, any>): string[] {
  const warnings: string[] = [];
  const enabledSum =
    (profile.heads.enabled ? profile.heads.percent : 0) +
    (profile.late_heads.enabled ? profile.late_heads.percent : 0) +
    (profile.hearts.enabled ? profile.hearts.percent : 0) +
    (profile.tails.enabled ? profile.tails.percent : 0);

  if (enabledSum > 100) {
    warnings.push(
      `Сумма включённых фракций ${enabledSum.toFixed(1)} % от АС больше 100 % — план противоречив.`,
    );
  }
  if (profile.powerKw <= 0) {
    warnings.push('Мощность 0 кВт: таймеры процесса не рассчитываются.');
  }
  const valveBwSettings = profile.ssvcSettings.valve_bw;
  if (Array.isArray(valveBwSettings) && valveBwSettings.some((value) => Number(value) <= 0)) {
    warnings.push(
      'Одна из пропускных способностей клапанов равна 0: скорость и время этой фракции будут нулевыми.',
    );
  }
  if (profile.ssvcSettings.hearts_finish_temp > 0 && args.hearts_percent !== undefined) {
    warnings.push(
      'hearts_finish_temp > 0: объём тела считается по температуре, hearts_percent игнорируется.',
    );
  }
  if (profile.tails.enabled) {
    warnings.push(
      'Хвосты учитываются как отдельная стадия; в прошивках 2.3.x отдельной стадии хвостов нет — проверьте режим контроллера.',
    );
  }
  if (!profile.heads.enabled) {
    warnings.push(
      'heads_enabled=false: модель не выключает стадию голов — объём голов и таймер считаются как ' +
        'при включённой; в прошивке отключённые головы означают, что спирт уходит в тело.',
    );
  }
  if (!profile.hearts.enabled) {
    warnings.push(
      'hearts_enabled=false: модель не выключает тело — объём и таймер тела считаются как при ' +
        'включённой стадии.',
    );
  }
  if (profile.ssvcSettings.formula === 2) {
    warnings.push(
      'formula=2 (авто 92+) в модели считается как включённая формула: авто-старт по ' +
        'formula_start_temp не моделируется — сверьте расчёт с контроллером.',
    );
  }
  return warnings;
}

/** Ключи, по которым сравнивается рассчитанный план с действующими настройками. */
const DIFF_KEYS = ['heads', 'late_heads', 'hearts', 'tails', 'heads_timer', 'late_heads_timer'] as const;

/** Рецепт фракции: входные доли, а не посчитанные пары настроек. */
function recipeOf(stage: Record<string, any>): Record<string, unknown> {
  return {
    percent: stage.percent,
    period_s: stage.period,
    enabled: stage.enabled,
    target_flow_mlh: stage.targetFlowMlh,
  };
}

/** Дифф «план ↔ контроллер»: только ключи, которые план действительно задаёт. */
function compareWithDevice(
  planned: Record<string, unknown>,
  current: Record<string, unknown>,
): { changed: string[]; unchanged: string[] } {
  const changed: string[] = [];
  const unchanged: string[] = [];
  for (const key of DIFF_KEYS) {
    const same = JSON.stringify(planned[key]) === JSON.stringify(current[key]);
    (same ? unchanged : changed).push(key);
  }
  return { changed, unchanged };
}

export const ocCalculatePlan = defineTool({
  name: 'oc_calculate_plan',
  title: 'Рассчитать план ректификации',
  description:
    'Чистый расчёт по входным данным: АС, фракции, скорости, таймеры, КПН и остаток. ' +
    'Возвращает готовые к записи пары настроек (settings), рецепт фракций (recipe) и ' +
    'допущения. По умолчанию устройство не читает; compare:true добавляет diff с действующими ' +
    'настройками контроллера.',
  inputSchema: {
    volume_l: z.number().positive().max(1000).describe('Объём сырца в кубе, л'),
    strength_pct: z.number().min(0).max(97.17).describe('Крепость сырца, % об.'),
    power_kw: z.number().min(0).max(50).optional().describe('Мощность нагрева, кВт'),
    stabilization_min: z.number().min(0).max(600).optional().describe('Стабилизация, мин'),
    heads_percent: percent.optional().describe('Головы, % от АС'),
    late_heads_percent: percent.optional().describe('Подголовники, % от АС'),
    hearts_percent: percent.optional().describe('Тело, % от АС (если не задана температура)'),
    tails_percent: percent.optional().describe('Хвосты, % от АС'),
    heads_enabled: z.boolean().optional(),
    late_heads_enabled: z.boolean().optional(),
    hearts_enabled: z.boolean().optional(),
    tails_enabled: z.boolean().optional(),
    heads_target_flow_mlh: flow.optional().describe('Скорость голов, мл/ч; 0 — авторасчёт'),
    late_heads_target_flow_mlh: flow.optional().describe('Скорость подголовников, мл/ч'),
    hearts_target_flow_mlh: flow.optional().describe('Скорость тела, мл/ч'),
    tails_target_flow_mlh: flow.optional().describe('Скорость хвостов, мл/ч'),
    valve_bw_mlh: z
      .array(valveBw)
      .length(3)
      .optional()
      .describe('Пропускная способность клапанов [головы, тело, хвосты], мл/ч'),
    virtual_bw_late: valveBw.optional().describe('Пропускная способность подголовников, мл/ч'),
    virtual_bw_tails: valveBw.optional().describe('Пропускная способность хвостов, мл/ч'),
    hearts_finish_temp: z
      .number()
      .min(0)
      .max(110)
      .optional()
      .describe('Температура ТД2 окончания тела, °C; 0 — считать по проценту'),
    hyst: z.number().min(0.06).max(5).optional().describe('Гистерезис, °C'),
    decrement: z.number().min(0).max(100).optional().describe('Декремент, %'),
    formula: z
      .number()
      .int()
      .min(0)
      .max(2)
      .optional()
      .describe(
        'Формула: 0 — выключена, 1 — включена, 2 — авто 92+ (прошивка API ≥1.9). ' +
          'В модели 2 считается как включённая формула: авто-старт по formula_start_temp не моделируется.',
      ),
    release_timer: z.number().min(0).max(1200).optional().describe('Сброс голов, с'),
    release_speed: z.number().min(0).max(99.9).optional().describe('Скорость сброса голов, с'),
    heads_final: z.number().min(0).max(99.9).optional().describe('Снижение голов, с'),
    ssvc_settings: z
      .record(z.unknown())
      .optional()
      .describe('Расширенные настройки SSVC поверх рассчитанных; массивы заменяются целиком'),
    compare: z
      .boolean()
      .optional()
      .describe(
        'Сравнить рассчитанные пары с действующими настройками контроллера (GET /rest/settings) ' +
          'и вернуть diff; по умолчанию устройство не читается',
      ),
  },
  annotations: annotationsForRisk(RISK),
  handler: async (args: Record<string, any>, ctx: ToolContext) => {
    const profile = buildProfile(args);
    const result = new DistillationCycleModel().calculateProcess(profile);
    const a = result.analytics;
    const powerFromInput = args.power_kw !== undefined;
    const stabilizationSec = Math.max(0, Math.round((profile.stabilizationMin ?? 0) * 60));

    // Единственное представление настроек для записи: пары уже пересчитаны моделью,
    // поэтому отдельного `plan.ssvc` и зеркала в профиле больше нет (дефект D3).
    //
    // Тонкость модели: `ssvcSettings.heads_timer` включает стабилизацию
    // (`headsTimerSec = release + heads + stabSec`), а на контроллере стабилизация —
    // это отдельный `start_delay`. Без вычитания этап голов получил бы лишние 20 минут.
    const settings = {
      heads: result.ssvcSettings.heads,
      late_heads: result.ssvcSettings.late_heads,
      hearts: result.ssvcSettings.hearts,
      tails: result.ssvcSettings.tails,
      heads_timer: Math.max(0, Math.round(result.ssvcSettings.heads_timer) - stabilizationSec),
      late_heads_timer: result.ssvcSettings.late_heads_timer,
      start_delay: stabilizationSec,
    };

    // Устройство читается только по явной просьбе: по умолчанию расчёт остаётся чистым.
    let diff: { changed: string[]; unchanged: string[] } | null = null;
    let diffError: string | null = null;
    if (args.compare === true) {
      try {
        const current = await getSettings(ctx.client);
        diff = compareWithDevice(settings, current.settings);
      } catch (error) {
        diffError = error instanceof Error ? error.message : String(error);
      }
    }

    return {
      ok: true,
      plan: {
        total_as_ml: a.totalAS,
        boiling_temp_c: a.boilingTemp,
        residue_ml: a.residueMl,
        residual_strength_pct: a.residualFortress,
        fractions: {
          release_ml: a.fractions.releaseMl,
          heads_ml: a.fractions.headsMl,
          late_heads_ml: a.fractions.lateHeadsMl,
          hearts_ml: a.fractions.heartsMl,
          tails_ml: a.fractions.tailsMl,
        },
        flows_mlh: {
          heads: a.flows.heads,
          heads_release: a.flows.heads_release,
          heads_final: a.flows.heads_final,
          late_heads: a.flows.late_heads,
          hearts: a.flows.hearts,
          hearts_avg: a.flows.hearts_avg,
          hearts_final: a.flows.hearts_final,
          tails: a.flows.tails,
        },
        timers_sec: {
          heads: a.timers.heads,
          late_heads: a.timers.late_heads,
          hearts: a.timers.hearts,
          tails: a.timers.tails,
          total_process: a.timers.total_process,
        },
        reflux_ratio: a.refluxRatio,
        kpn: a.phlegmatic,
      },
      settings,
      recipe: {
        heads: recipeOf(result.heads),
        late_heads: recipeOf(result.late_heads),
        hearts: recipeOf(result.hearts),
        tails: recipeOf(result.tails),
      },
      assumptions: {
        power_kw: profile.powerKw,
        power_kw_source: powerFromInput ? 'input' : 'default',
        warning: powerFromInput
          ? null
          : `Мощность не передана — взят default ${profile.powerKw} кВт: скорости тела и ` +
            'таймеры могут не совпасть с реальной колонной. Передайте power_kw.',
        notes: [
          'Расчёт идеализирован: не учитывает теплопотери, качество насадки и реальную мощность.',
          'Объём тела при hearts_finish_temp считается по температуре куба и крепости отбора 96 %.',
          'Результат — порядок величин и согласованность параметров, а не гарантия процесса.',
          `Скорости: ${FLOW_METHOD} (valve_bw в мл/ч, пара — секунды; множителя 60 нет).`,
        ],
      },
      warnings: warningsFor(args, profile),
      ...(diff !== null ? { diff } : {}),
      ...(diffError !== null ? { diff_error: diffError } : {}),
    } as ToolResult;
  },
});

/** Инструменты домена расчёта. */
export function calcTools(): ToolDef[] {
  return [ocCalculatePlan];
}

/** Регистрирует инструмент в MCP-сервере сессии. */
export function registerCalcTools(server: McpServer, ctx: ToolContext): void {
  registerTools(server, ctx, calcTools());
}
