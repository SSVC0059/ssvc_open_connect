/**
 * Классы риска (контракт §2.6, §3.11).
 *
 * 0 — чтение; 1 — обратимая запись; 2 — влияет на процесс; 3 — разрушительное;
 * 4 — разрушительное, требующее отдельного разрешения.
 */

import type { ToolAnnotations } from '@modelcontextprotocol/sdk/types.js';

export type RiskClass = 0 | 1 | 2 | 3 | 4;

export interface RiskPolicy {
  /** Обязателен `confirm: true`. */
  requireConfirm: boolean;
  /** Обязателен `reason` длиной ≥ 10 символов. */
  requireReason: boolean;
  /** Требуется `SSVC_ALLOW_DESTRUCTIVE=1`. */
  requireDestructiveFlag: boolean;
  /** Требуется явное разрешение операции (список класса 4). */
  requireAllowlist: boolean;
}

const POLICIES: Readonly<Record<RiskClass, RiskPolicy>> = {
  0: { requireConfirm: false, requireReason: false, requireDestructiveFlag: false, requireAllowlist: false },
  1: { requireConfirm: false, requireReason: false, requireDestructiveFlag: false, requireAllowlist: false },
  2: { requireConfirm: true, requireReason: false, requireDestructiveFlag: false, requireAllowlist: false },
  3: { requireConfirm: true, requireReason: true, requireDestructiveFlag: true, requireAllowlist: false },
  4: { requireConfirm: true, requireReason: true, requireDestructiveFlag: true, requireAllowlist: true },
};

export function riskPolicy(risk: RiskClass): RiskPolicy {
  return POLICIES[risk];
}

/** Человеческое название класса — для логов и описаний. */
export const RISK_LABELS: Readonly<Record<RiskClass, string>> = {
  0: 'чтение',
  1: 'обратимая запись',
  2: 'влияет на процесс',
  3: 'разрушительное действие',
  4: 'разрушительное действие с отдельным разрешением',
};

/** Инструменты, для которых риск-политика не применяется (защитное действие). */
export const RISK_EXEMPT_TOOLS: readonly string[] = ['oc_emergency_stop'];

/**
 * Разрешённые операции класса 4. Список пуст по умолчанию (контракт §2.6):
 * такие операции появляются только после отдельного решения по безопасности.
 */
export const RISK4_ALLOWED_OPERATIONS: readonly string[] = [];

export function isRiskExempt(tool: string): boolean {
  return RISK_EXEMPT_TOOLS.includes(tool);
}

export function isRisk4Allowed(tool: string, allowlist: readonly string[] = RISK4_ALLOWED_OPERATIONS): boolean {
  return allowlist.includes(tool);
}

/**
 * Аннотации MCP выводятся из класса риска, чтобы описание и поведение не расходились.
 * `destructiveHint` включаем с класса 2: действие меняет состояние процесса.
 */
export function annotationsForRisk(
  risk: RiskClass,
  overrides: Partial<ToolAnnotations> = {},
): ToolAnnotations {
  return {
    readOnlyHint: risk === 0,
    destructiveHint: risk >= 2,
    idempotentHint: risk === 0,
    openWorldHint: false,
    ...overrides,
  };
}
