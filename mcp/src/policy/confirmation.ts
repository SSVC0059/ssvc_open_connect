/**
 * Политика подтверждения и запись аудита (контракт §2.6).
 *
 * `enforceRisk` вызывается доменом в начале обработчика: до любых сетевых вызовов,
 * чтобы «забытый confirm» не доходил до контроллера.
 */

import type { ToolContext } from '../mcp/types.js';
import { OcError } from '../errors.js';
import { RISK_LABELS, isRisk4Allowed, isRiskExempt, riskPolicy, type RiskClass } from './risk.js';

export const MIN_REASON_LENGTH = 10;

export interface RiskArgs {
  confirm?: boolean;
  reason?: string;
}

/**
 * Проверяет аргументы на соответствие классу риска.
 *
 * Матрица:
 *  0    — без требований;
 *  1    — без требований, результат пишется в лог;
 *  2    — `confirm === true`, иначе confirmation_required;
 *  3    — `confirm === true` + `reason` (≥10 символов) + `SSVC_ALLOW_DESTRUCTIVE=1`;
 *  4    — то же, что 3, плюс операция должна быть в списке разрешённых (пуст по умолчанию);
 *  `oc_emergency_stop` — защитное действие, требований нет.
 *
 * Отсутствие `confirm` и отсутствие `reason` различаются: первое — confirmation_required,
 * второе — validation_failed (именно этого ждут тесты пакетов, см. WP8: «удаление без reason
 * → validation_failed»).
 *
 * @throws OcError
 */
export function enforceRisk(ctx: ToolContext, tool: string, risk: RiskClass, args: RiskArgs): void {
  if (isRiskExempt(tool)) {
    ctx.log.debug('Защитное действие: политика риска не применяется', { tool, risk });
    return;
  }

  const policy = riskPolicy(risk);
  const label = RISK_LABELS[risk];

  if (policy.requireConfirm && args.confirm !== true) {
    auditDenial(ctx, tool, risk, args, 'confirmation_required');
    throw new OcError(
      'confirmation_required',
      `Действие «${tool}» (${label}) требует явного подтверждения`,
      {
        hint:
          'Повторите вызов с confirm: true, если действие действительно нужно выполнить. ' +
          'Без подтверждения контроллер не будет ничего менять.',
        details: { tool, risk, required: 'confirm' },
      },
    );
  }

  if (policy.requireReason) {
    const reason = typeof args.reason === 'string' ? args.reason.trim() : '';
    if (reason.length < MIN_REASON_LENGTH) {
      auditDenial(ctx, tool, risk, args, 'validation_failed');
      throw new OcError(
        'validation_failed',
        `Для действия «${tool}» нужна причина: не короче ${MIN_REASON_LENGTH} символов`,
        {
          hint:
            `Передайте reason — короткое объяснение, зачем выполняется ${label} ` +
            `(не короче ${MIN_REASON_LENGTH} символов). Причина попадает в журнал аудита.`,
          details: { tool, risk, required: 'reason', min_length: MIN_REASON_LENGTH },
        },
      );
    }
  }

  if (policy.requireDestructiveFlag && !ctx.cfg.allowDestructive) {
    auditDenial(ctx, tool, risk, args, 'disabled_by_policy');
    throw new OcError(
      'disabled_by_policy',
      `Действие «${tool}» (${label}) выключено настройкой сервера`,
      {
        hint:
          'Разрушительные операции отключены: установите SSVC_ALLOW_DESTRUCTIVE=1 в .env ' +
          'и перезапустите сервис, если такие действия действительно разрешены.',
        details: { tool, risk, flag: 'SSVC_ALLOW_DESTRUCTIVE' },
      },
    );
  }

  if (policy.requireAllowlist && !isRisk4Allowed(tool)) {
    auditDenial(ctx, tool, risk, args, 'disabled_by_policy');
    throw new OcError(
      'disabled_by_policy',
      `Действие «${tool}» (${label}) не входит в список разрешённых операций`,
      {
        hint:
          'Операции класса 4 разрешаются точечно: список пуст по умолчанию. ' +
          'Если операция действительно нужна — это отдельное решение по безопасности.',
        details: { tool, risk, required: 'allowlist' },
      },
    );
  }

  if (risk === 1) {
    // Риск 1 разрешён свободно, но изменение должно быть видно в логе.
    ctx.log.info('Обратимая запись разрешена политикой', { tool, risk });
  }
}

/**
 * Отказ политики тоже пишется в аудит: иначе «отклонено» — единственный класс
 * событий, которого в журнале нет, хотя именно он интересен при разборе попыток
 * выполнить разрушительное действие без прав.
 */
function auditDenial(
  ctx: ToolContext,
  tool: string,
  risk: RiskClass,
  args: RiskArgs,
  code: string,
): void {
  auditAction(ctx, {
    tool,
    risk,
    confirm: args.confirm === true ? true : undefined,
    reason: typeof args.reason === 'string' ? args.reason : undefined,
    result: code,
    details: { denied: true, code, required: code === 'confirmation_required' ? 'confirm' : null },
  });
}

export interface AuditActionEntry {
  tool: string;
  risk: RiskClass;
  confirm?: boolean;
  reason?: string;
  result: string;
  details?: Record<string, unknown>;
}

/**
 * Записывает действие в аудит. Вызывается после выполнения (в том числе при отказе),
 * поэтому не бросает исключений: журнал не должен ломать операцию.
 */
export function auditAction(ctx: ToolContext, entry: AuditActionEntry): void {
  try {
    ctx.audit.write({
      tool: entry.tool,
      risk: entry.risk,
      confirm: entry.confirm,
      reason: entry.reason,
      result: entry.result,
      details: entry.details,
    });
  } catch (error) {
    ctx.log.error('Не удалось записать действие в аудит', { tool: entry.tool, error });
  }
}
