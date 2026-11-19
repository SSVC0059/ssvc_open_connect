/**
 * Схемы ответа `GET /rest/oc/info` (контракт §5, прошивка OpenConnectHandler::getInfo).
 *
 * Ответ:
 * {
 *   "ssvc": {"version":"2.5.1","api":"1.10","api_code":110,"mode":"late_heads"|"tails"},
 *   "oc":   {"version":"0.2.9.1","is_support_api":true,"is_support_release":true,
 *            "api_min":"1.7","api_target":"1.10","api_compatibility":"supported",
 *            "api_features":[{"name":"get_log","min_api":"1.10","available":true}]}
 * }
 *
 * Поля, которых может не быть на старых прошивках, объявлены optional: сервис должен
 * переживать прошивки API 1.7…1.10, а не падать на отсутствии нового поля.
 */

import { z } from 'zod';
import { apiVersionCode } from './common.js';

export const apiFeatureSchema = z
  .object({
    /** Имя возможности: base, formula, predec, get_log, … */
    name: z.string(),
    /** Минимальная версия UART API, где возможность появилась. */
    min_api: z.string(),
    /** Доступна ли возможность на подключённом контроллере. */
    available: z.boolean(),
  })
  .passthrough();

export const ssvcInfoSchema = z
  .object({
    /** Версия прошивки SSVC. */
    version: z.string(),
    /** Версия UART API строкой: «1.7», «1.10» (числом нельзя: 1.10 === 1.1). */
    api: z.string(),
    /** Целочисленный код версии API: major * 100 + minor. */
    api_code: z.number().int().optional(),
    /** Ветка прошивки SSVC: late_heads или tails. */
    mode: z.string().optional(),
  })
  .passthrough();

export const ocFirmwareInfoSchema = z
  .object({
    /** Версия прошивки OpenConnect. */
    version: z.string(),
    /** Поддерживает ли прошивка UART API SSVC. */
    is_support_api: z.boolean().optional(),
    /** Признак наличия «Сброса и снижения» (только late_heads). */
    is_support_release: z.boolean().optional(),
    /** Минимальная версия API, с которой работает прошивка. */
    api_min: z.string().optional(),
    /** Версия API, которую реализует прошивка. */
    api_target: z.string().optional(),
    /** Совместимость версии API: unknown | too_old | supported. */
    api_compatibility: z.enum(['unknown', 'too_old', 'supported']).optional(),
    /** Реестр возможностей устройства. */
    api_features: z.array(apiFeatureSchema).optional(),
  })
  .passthrough();

export const ocInfoSchema = z
  .object({
    ssvc: ssvcInfoSchema,
    oc: ocFirmwareInfoSchema,
  })
  .passthrough();

export type ApiFeature = z.infer<typeof apiFeatureSchema>;
export type SsvcInfo = z.infer<typeof ssvcInfoSchema>;
export type OcFirmwareInfo = z.infer<typeof ocFirmwareInfoSchema>;
export type OcInfo = z.infer<typeof ocInfoSchema>;

/** Код версии API с фолбэком на разбор строки `ssvc.api`. */
export function infoApiCode(info: OcInfo): number {
  return info.ssvc.api_code ?? apiVersionCode(info.ssvc.api);
}

/** Совместимость: поле прошивки, иначе вывод из `is_support_api`. */
export function infoCompatibility(info: OcInfo): 'unknown' | 'too_old' | 'supported' {
  if (info.oc.api_compatibility !== undefined) return info.oc.api_compatibility;
  if (info.oc.is_support_api === false) return 'too_old';
  return 'unknown';
}

/** Имена недоступных возможностей — для `oc_get_capabilities`. */
export function unavailableFeatures(info: OcInfo): string[] {
  return (info.oc.api_features ?? []).filter((feature) => !feature.available).map((f) => f.name);
}
