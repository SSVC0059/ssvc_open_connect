/**
 * Типы единой модели расчёта ректификации OpenConnect.
 *
 * Реализация — `src/index.js` (чистый ESM, без сборки). Этот файл описывает
 * только публичный контракт, чтобы модель одинаково типизировалась в интерфейсе
 * (SvelteKit) и в MCP-сервере (Node + TypeScript).
 */

/** Параметры фракции в профиле: доля от АС, скорость, включённость. */
export interface FractionSettings {
  /** Доля фракции, % от абсолютного спирта. */
  percent: number;
  /** Включён ли отбор фракции. */
  enabled: boolean;
  /** Желаемая скорость отбора, мл/ч (0 — авторасчёт для голов/подголовников). */
  targetFlowMlh: number;
  /** Целевое число циклов переиспарения (только головы). */
  targetCycles?: number;
  /** Пользовательский декремент фракции, % (не используется моделью напрямую). */
  decrementPercent?: number;
}

/** Кортеж SSVC: [время открытия клапана, период], секунды. */
export type SsvcTuple = [number, number];

/** Настройки SSVC, которые читает и пересчитывает модель. */
export interface RectificationSsvcSettings {
  heads: SsvcTuple;
  late_heads: SsvcTuple;
  hearts: SsvcTuple;
  tails: SsvcTuple;
  /** Пропускная способность клапанов, мл/ч при 100% открытии. */
  valve_bw: [number, number, number];
  /** Гистерезис, °C. */
  hyst: number;
  /** Декремент, %. */
  decrement: number;
  /** 0 — формула выключена, >0 — включена. */
  formula: number;
  formula_start_temp: number;
  hearts_finish_temp: number;
  heads_timer: number;
  late_heads_timer: number;
  /** «Сброс и снижение» голов. */
  release_timer: number;
  release_speed: number;
  heads_final: number;
  /** Виртуальные пропускные способности для калькулятора. */
  virtual_bw_late?: number;
  virtual_bw_tails?: number;
  [key: string]: unknown;
}

/** Минимальный профиль, достаточный модели (совместим с Profile интерфейса). */
export interface RectificationProfile {
  powerKw: number;
  volumeL: number;
  strengthVol: number;
  stabilizationMin: number;
  heads: FractionSettings;
  late_heads: FractionSettings;
  hearts: FractionSettings;
  tails: FractionSettings;
  ssvcSettings: RectificationSsvcSettings;
  /** Виртуальные пропускные способности для калькулятора. */
  virtual_bw_late?: number;
  virtual_bw_tails?: number;
  [key: string]: unknown;
}

/** Результаты расчёта, которые добавляются к профилю. */
export interface RectificationAnalytics {
  totalAS: number;
  boilingTemp: number;
  residueMl: number;
  residualFortress: number;
  oneCycleTime: number;
  flows: {
    heads: number;
    heads_release: number;
    heads_final: number;
    late_heads: number;
    hearts: number;
    hearts_avg: number;
    hearts_final: number;
    tails: number;
  };
  fractions: {
    releaseMl: number;
    headsMl: number;
    lateHeadsMl: number;
    heartsMl: number;
    tailsMl: number;
  };
  timers: {
    heads: number;
    late_heads: number;
    hearts: number;
    tails: number;
    total_process: number;
  };
  refluxRatio: {
    heads: number;
    late_heads: number;
    hearts: number;
    tails: number;
  };
  phlegmatic: {
    heads: number;
    late_heads: number;
    hearts: number;
    tails: number;
  };
}

/** Профиль с результатами расчёта. */
export type CalculatedProfile<T extends RectificationProfile> = T & {
  analytics: RectificationAnalytics;
};

/**
 * Единая модель ректификации.
 *
 * Все методы — чистые: ни сети, ни устройства, ни побочных эффектов.
 * `calculateProcess` не мутирует входной профиль.
 */
export class DistillationCycleModel {
  /** Температура кипения водно-спиртовой смеси, °C. */
  calculateBoilingTemp(strengthPct: number): number;
  /** Спиртуозность в кубе по температуре кипения (бисекция). */
  findStrengthByTemp(targetTemp: number): number;
  /** Флегмовое число по объёму пара и скорости отбора. */
  calculateRefluxRatio(volVaporPerHour: number, drawRateMlH: number): number;
  /** Полный расчёт процесса: фракции, скорости, таймеры, КПН, остаток. */
  calculateProcess<T extends RectificationProfile>(profile: T): CalculatedProfile<T>;
}

/** Профиль по умолчанию для калькулятора и редактора (без id/name/createdAt). */
export function createDefaultProfile(): any;
