var __defProp = Object.defineProperty;
var __defNormalProp = (obj, key, value) => key in obj ? __defProp(obj, key, { enumerable: true, configurable: true, writable: true, value }) : obj[key] = value;
var __publicField = (obj, key, value) => __defNormalProp(obj, typeof key !== "symbol" ? key + "" : key, value);
function stripNumericKeysAtRoot(obj) {
  const out = { ...obj };
  for (const k of Object.keys(out)) {
    if (/^\d+$/.test(k)) {
      delete out[k];
    }
  }
  return out;
}
function defaultSsvcSettingsForProfile() {
  return {
    valve_bw: [7e3, 12e3, 7e3],
    heads: [0, 120],
    late_heads: [0, 60],
    hearts: [0, 10],
    tails: [0, 2],
    hyst: 0.06,
    decrement: 0,
    sound: 0,
    pressure: 0,
    relay_inverted: 0,
    relay_autostart: 0,
    autoresume: 0,
    auto_mode: 0,
    heads_timer: 0,
    late_heads_timer: 0,
    hearts_timer: 0,
    tails_temp: 0,
    start_delay: 0,
    hearts_finish_temp: 0,
    parallel_v3: [],
    parallel_v1: [0, 0],
    parallel: [0, 0],
    hearts_temp_shift: false,
    hearts_pause: false,
    formula: false,
    formula_start_temp: 0,
    tank_mmhg: 0,
    tp2_shift: 0,
    tp_filter: false,
    signal_tp1_control: 0,
    signal_inverted: 0,
    tp1_control_temp: 0,
    tp1_control_start: 0,
    stab_limit_time: 0,
    stab_limit_finish: 0,
    backlight: "off",
    release_speed: 0,
    release_timer: 0,
    heads_final: 0,
    valve_bw_tails: 15e3
  };
}
function createDefaultProfile() {
  return {
    powerKw: 2.5,
    volumeL: 18,
    strengthVol: 40,
    stabilizationMin: 20,
    heads: {
      percent: 3,
      period: 120,
      enabled: true,
      targetCycles: 2,
      targetFlowMlh: 0
    },
    late_heads: {
      percent: 7,
      period: 60,
      enabled: true,
      targetFlowMlh: 150
    },
    hearts: {
      period: 10,
      percent: 75,
      enabled: true,
      targetFlowMlh: 2500
    },
    tails: {
      percent: 5,
      period: 360,
      enabled: false,
      targetFlowMlh: 2500
    },
    ssvcSettings: defaultSsvcSettingsForProfile(),
    openConnectRelayRules: {
      schemaVersion: 1,
      rules: []
    }
  };
}
class DistillationCycleModel {
  constructor() {
    // private readonly L_ETHANOL = 855;
    // private readonly L_WATER = 2256;
    __publicField(this, "RHO_ETHANOL", 0.7893);
  }
  calculateVaporStrength(b) {
    if (b <= 0) return 0;
    if (b >= 97.17) return 97.17;
    const part1 = 1.04749494522173 * b;
    const part2 = 0.018725730342732 * Math.pow(b, 2);
    const part3 = 11082005414225e-17 * Math.pow(b, 3);
    const expPart1 = 43.670453012901 * (1 - Math.exp(-0.246196276366746 * Math.pow(b, 0.966659341971092)));
    const expPart2 = Math.exp(0.0638669100921283 * Math.pow(b, 0.437695537197651));
    return part1 - part2 + part3 + expPart1 * expPart2;
  }
  calculateBoilingTemp(b) {
    if (b <= 0) return 99.97;
    if (b >= 97.17) return 78.15;
    const term1 = 99.974;
    const term2 = 0.93136 * b;
    const term3 = 0.02395 * Math.pow(b, 2);
    const term4 = 365956e-9 * Math.pow(b, 3);
    const term5 = 293273e-11 * Math.pow(b, 4);
    const term6 = 961e-11 * Math.pow(b, 5);
    return term1 - term2 + term3 - term4 + term5 - term6;
  }
  // Заменяем константы на методы
  getLWater(temp) {
    return 2501 - 2.384 * temp;
  }
  getLEthanol(temp) {
    return 940 - 1.12 * temp;
  }
  /**
   * Перевод объемной доли (0.0-1.0) в массовую.
   * Используется для расчета теплоты испарения смеси.
   */
  volToMassFraction(volFraction) {
    if (volFraction <= 0) return 0;
    const rhoEth = 0.7893;
    const rhoWater = 1;
    const mEth = volFraction * rhoEth;
    const mWater = (1 - volFraction) * rhoWater;
    return mEth / (mEth + mWater);
  }
  /**
   * Расчет Флегмового Числа (ФЧ)
   */
  calculateRefluxRatio(volVaporPerHour, drawRateMlH) {
    if (drawRateMlH <= 0) return 999;
    const drawRateLH = drawRateMlH / 1e3;
    const refluxRatio = volVaporPerHour / drawRateLH - 1;
    return Math.max(0, refluxRatio);
  }
  /**
   * Расчет флегмового числа (ФЧ)
   * @param powerWatts Мощность в Ваттах (например, 2500)
   * @param drawRateMlh Скорость отбора в мл/ч (например, 2500)
   */
  calculatePhlegmatic(powerWatts, drawRateMlh) {
    if (drawRateMlh <= 0) return 99.9;
    const volAlc = powerWatts * 3.6 / 857 / 0.789 * 1e3;
    const volReturn = volAlc - drawRateMlh;
    const f = volReturn / drawRateMlh;
    return Math.max(0, f);
  }
  /**
   * Вычисляет физические свойства пара для заданной спиртуозности в кубе и мощности.
   * Централизованное место для физических расчетов.
   */
  getVaporPhysics(b, powerWatts) {
    const currentTemp = this.calculateBoilingTemp(b);
    const vVapVol = this.calculateVaporStrength(b) / 100;
    const vVapMass = this.volToMassFraction(vVapVol);
    const lMix = vVapMass * this.getLEthanol(currentTemp) + (1 - vVapMass) * this.getLWater(currentTemp);
    const massVaporPerHour = powerWatts * 3600 / (lMix * 1e3);
    const volVaporPerHour = massVaporPerHour * vVapMass / 0.7893 + massVaporPerHour * (1 - vVapMass) / 0.96;
    return { strengthMass: vVapMass, lMix, volVaporPerHour };
  }
  /**
   * Находит спиртуозность в кубе (vol %) для заданной температуры кипения.
   * Использует итерационный поиск (метод бисекции).
   */
  findStrengthByTemp(targetTemp) {
    if (targetTemp >= 100) return 0;
    if (targetTemp <= 78.15) return 97.17;
    let min = 0;
    let max = 97.17;
    let iterations = 0;
    while (iterations < 20) {
      let mid = (min + max) / 2;
      let temp = this.calculateBoilingTemp(mid);
      if (Math.abs(temp - targetTemp) < 0.01) return mid;
      if (temp > targetTemp) {
        min = mid;
      } else {
        max = mid;
      }
      iterations++;
    }
    return min;
  }
  calculateProcess(profile) {
    var _a, _b, _c, _d, _e, _f, _g, _h, _i, _j;
    const lateHeadsEnabled = profile.late_heads.enabled;
    const tailsEnabled = profile.tails.enabled;
    const netPowerWatts = profile.powerKw * 1e3;
    const stabSec = profile.stabilizationMin * 60;
    let currentStrengthVol = profile.strengthVol;
    let currentVolumeL = profile.volumeL;
    let totalAS_ml = currentVolumeL * 1e3 * (currentStrengthVol / 100);
    const initialTotalAS = totalAS_ml;
    const bw_heads = ((_b = (_a = profile.ssvcSettings) == null ? void 0 : _a.valve_bw) == null ? void 0 : _b[0]) || 0;
    const bw_late = profile.virtual_bw_late || ((_d = (_c = profile.ssvcSettings) == null ? void 0 : _c.valve_bw) == null ? void 0 : _d[2]) || 0;
    const bw_tails = profile.virtual_bw_tails || ((_f = (_e = profile.ssvcSettings) == null ? void 0 : _e.valve_bw) == null ? void 0 : _f[2]) || ((_h = (_g = profile.ssvcSettings) == null ? void 0 : _g.valve_bw) == null ? void 0 : _h[1]) || 0;
    const bw_valve3 = ((_j = (_i = profile.ssvcSettings) == null ? void 0 : _i.valve_bw) == null ? void 0 : _j[1]) || 0;
    const physicsHeads = this.getVaporPhysics(currentStrengthVol, netPowerWatts);
    const totalHeadsVolMl = initialTotalAS * profile.heads.percent / 100;
    const rawReleaseTimer = profile.ssvcSettings.release_timer;
    const rTimer = Number(
      rawReleaseTimer != null && rawReleaseTimer >= 0 ? rawReleaseTimer : 0
    );
    const rawReleaseSpeed = profile.ssvcSettings.release_speed;
    const rOpen = Number(rawReleaseSpeed != null && rawReleaseSpeed >= 0 ? rawReleaseSpeed : 0);
    const rawHeadsFinal = profile.ssvcSettings.heads_final;
    const hFinal = rawHeadsFinal != null && rawHeadsFinal > 0 ? rawHeadsFinal : 0;
    const headsCyclePeriod = profile.ssvcSettings.heads[1] || 10;
    const alcoholMassG = initialTotalAS * this.RHO_ETHANOL;
    const totalVaporMassPerSec = netPowerWatts > 0 ? netPowerWatts / 1e3 / physicsHeads.lMix : 0;
    const alcoholVaporMassPerSecG = totalVaporMassPerSec * physicsHeads.strengthMass * 1e3;
    const oneCycleSec_heads = alcoholVaporMassPerSecG > 0 ? alcoholMassG / alcoholVaporMassPerSecG : 0;
    const releaseFlowMlh = rOpen / headsCyclePeriod * bw_heads;
    const releaseMl = releaseFlowMlh * rTimer / 3600;
    let headsStartFlowMlh;
    if (profile.heads.targetFlowMlh && profile.heads.targetFlowMlh > 0) {
      headsStartFlowMlh = Math.min(profile.heads.targetFlowMlh, bw_heads);
    } else {
      const alcoholMassG2 = initialTotalAS * this.RHO_ETHANOL;
      const totalVaporMassPerSec2 = netPowerWatts > 0 ? netPowerWatts / 1e3 / physicsHeads.lMix : 0;
      const alcoholVaporMassPerSecG2 = totalVaporMassPerSec2 * physicsHeads.strengthMass * 1e3;
      const oneCycleSec_heads2 = alcoholVaporMassPerSecG2 > 0 ? alcoholMassG2 / alcoholVaporMassPerSecG2 : 0;
      const cycles = profile.heads.targetCycles || 2;
      const targetTotalTimeSec = Math.max(1, oneCycleSec_heads2 * cycles);
      const remainingToCollect = Math.max(0, totalHeadsVolMl - releaseMl);
      headsStartFlowMlh = remainingToCollect * 3600 / Math.max(1, targetTotalTimeSec - rTimer);
    }
    const headsFinalFlowMlh = hFinal > 0 ? hFinal / headsCyclePeriod * bw_heads : headsStartFlowMlh;
    let mainHeadsTimerSec = 0;
    const remainingVolMl = Math.max(0, totalHeadsVolMl - releaseMl);
    if (remainingVolMl > 0) {
      if (hFinal > 0 && headsFinalFlowMlh < headsStartFlowMlh) {
        const avgFlowMlh = (headsStartFlowMlh + headsFinalFlowMlh) / 2;
        mainHeadsTimerSec = remainingVolMl / avgFlowMlh * 3600;
      } else {
        mainHeadsTimerSec = remainingVolMl / headsStartFlowMlh * 3600;
      }
    }
    const headsTimerSec = rTimer + mainHeadsTimerSec + stabSec;
    const headsOpenTime = headsStartFlowMlh / bw_heads * headsCyclePeriod;
    const headRefluxRatio = this.calculateRefluxRatio(
      physicsHeads.volVaporPerHour,
      headsStartFlowMlh
    );
    totalAS_ml -= totalHeadsVolMl * 0.96;
    currentVolumeL -= totalHeadsVolMl / 1e3;
    currentStrengthVol = Math.max(0.1, totalAS_ml / (currentVolumeL * 1e3) * 100);
    const physicsLate = this.getVaporPhysics(currentStrengthVol, netPowerWatts);
    let lateHeadsTimerSec = 0, lateHeadsOpenTime = 0, lateHeadsFlow = 0, lateHeadsVol = 0, lateHeadsRefluxRatio = 0;
    if (lateHeadsEnabled) {
      lateHeadsVol = initialTotalAS * (profile.late_heads.percent / 100);
      lateHeadsFlow = profile.late_heads.targetFlowMlh || bw_late * 0.1;
      lateHeadsTimerSec = lateHeadsFlow > 0 ? lateHeadsVol / lateHeadsFlow * 3600 : 0;
      lateHeadsOpenTime = lateHeadsFlow / bw_late * profile.ssvcSettings.late_heads[1];
      lateHeadsRefluxRatio = this.calculateRefluxRatio(physicsLate.volVaporPerHour, lateHeadsFlow);
      totalAS_ml -= lateHeadsVol * 0.96;
      currentVolumeL -= lateHeadsVol / 1e3;
      currentStrengthVol = Math.max(0.1, totalAS_ml / (currentVolumeL * 1e3) * 100);
    }
    const physicsHearts = this.getVaporPhysics(currentStrengthVol, netPowerWatts);
    let heartsProductMl = 0;
    let calculatedHeartsPercent = 0;
    if (profile.ssvcSettings.hearts_finish_temp && profile.ssvcSettings.hearts_finish_temp > 0) {
      const targetCubeStrength = this.findStrengthByTemp(profile.ssvcSettings.hearts_finish_temp);
      const remainingAS = currentVolumeL * 1e3 * (targetCubeStrength / 100);
      const availableAS = Math.max(0, totalAS_ml - remainingAS);
      heartsProductMl = availableAS / 0.96;
      calculatedHeartsPercent = Number(
        (heartsProductMl * 0.96 / initialTotalAS * 100).toFixed(1)
      );
    } else {
      heartsProductMl = initialTotalAS * (profile.hearts.percent / 100) / 0.96;
    }
    const startTemp = this.calculateBoilingTemp(currentStrengthVol);
    const endTemp = profile.ssvcSettings.hearts_finish_temp || 90;
    const isFormulaEnabled = profile.ssvcSettings.formula;
    const hyst = profile.ssvcSettings.hyst || 0.25;
    const decFactor = (profile.ssvcSettings.decrement || 0) / 100;
    let heartsInitialSpeed = profile.hearts.targetFlowMlh || physicsHearts.volVaporPerHour * 1e3;
    let heartsAverageSpeed = heartsInitialSpeed;
    let heartsFinalSpeed = heartsInitialSpeed;
    if (isFormulaEnabled && decFactor > 0 && endTemp > startTemp) {
      const n = Math.max(0.1, (endTemp - startTemp) / (hyst * 2));
      heartsFinalSpeed = heartsInitialSpeed * Math.pow(1 - decFactor, n);
      const lnFactor = Math.log(1 - decFactor);
      heartsAverageSpeed = heartsInitialSpeed * ((Math.pow(1 - decFactor, n) - 1) / (n * lnFactor));
    }
    const heartsTimerSec = heartsAverageSpeed > 0 ? heartsProductMl / heartsAverageSpeed * 3600 : 0;
    const heartsOpenTime = heartsInitialSpeed / bw_valve3 * profile.ssvcSettings.hearts[1];
    const heartsRefluxRatio = this.calculateRefluxRatio(
      physicsHearts.volVaporPerHour,
      heartsInitialSpeed
    );
    totalAS_ml -= heartsProductMl * 0.96;
    currentVolumeL -= heartsProductMl / 1e3;
    currentStrengthVol = Math.max(0.1, totalAS_ml / (currentVolumeL * 1e3) * 100);
    const physicsTails = this.getVaporPhysics(currentStrengthVol, netPowerWatts);
    const tailsVol = tailsEnabled ? initialTotalAS * (profile.tails.percent / 100) : 0;
    let tailsFlow = profile.tails.targetFlowMlh || bw_tails;
    const tailsTimerSec = tailsFlow > 0 ? tailsVol / tailsFlow * 3600 : 0;
    const tailsOpenTime = tailsFlow / bw_tails * profile.ssvcSettings.tails[1];
    const tailsRefluxRatio = this.calculateRefluxRatio(physicsTails.volVaporPerHour, tailsFlow);
    if (tailsEnabled) {
      totalAS_ml -= tailsVol * 0.96;
      currentVolumeL -= tailsVol / 1e3;
    }
    const totalProcessSec = stabSec + headsTimerSec + lateHeadsTimerSec + heartsTimerSec + tailsTimerSec;
    const phlegmaticHeads = this.calculatePhlegmatic(netPowerWatts, headsStartFlowMlh);
    const phlegmaticLateHeads = this.calculatePhlegmatic(netPowerWatts, lateHeadsFlow);
    const phlegmaticHearts = this.calculatePhlegmatic(netPowerWatts, heartsInitialSpeed);
    const phlegmaticTails = this.calculatePhlegmatic(netPowerWatts, tailsFlow);
    const profileRoot = stripNumericKeysAtRoot({
      ...profile
    });
    return {
      ...profileRoot,
      hearts: {
        ...profileRoot.hearts,
        percent: profileRoot.ssvcSettings.hearts_finish_temp > 0 ? calculatedHeartsPercent : profileRoot.hearts.percent
      },
      ssvcSettings: {
        ...profileRoot.ssvcSettings,
        heads: [Number(headsOpenTime.toFixed(1)), headsCyclePeriod],
        late_heads: [Number(lateHeadsOpenTime.toFixed(1)), profileRoot.ssvcSettings.late_heads[1]],
        hearts: [Number(heartsOpenTime.toFixed(1)), profileRoot.ssvcSettings.hearts[1]],
        tails: [Number(tailsOpenTime.toFixed(1)), profileRoot.ssvcSettings.tails[1]],
        heads_timer: Math.round(headsTimerSec),
        late_heads_timer: Math.round(lateHeadsTimerSec)
      },
      analytics: {
        totalAS: Math.round(initialTotalAS),
        boilingTemp: Number(this.calculateBoilingTemp(profileRoot.strengthVol).toFixed(2)),
        residueMl: Math.round(currentVolumeL * 1e3),
        residualFortress: Number(currentStrengthVol.toFixed(1)),
        oneCycleTime: Math.round(oneCycleSec_heads),
        flows: {
          heads: Math.round(headsStartFlowMlh),
          heads_release: Math.round(releaseFlowMlh),
          heads_final: Math.round(headsFinalFlowMlh),
          late_heads: Math.round(lateHeadsFlow),
          hearts: Math.round(heartsInitialSpeed),
          hearts_avg: Math.round(heartsAverageSpeed),
          hearts_final: Math.round(heartsFinalSpeed),
          tails: Math.round(tailsFlow)
        },
        fractions: {
          releaseMl: Number(releaseMl.toFixed(2)),
          headsMl: Math.round(totalHeadsVolMl),
          lateHeadsMl: Math.round(lateHeadsVol),
          heartsMl: Math.round(heartsProductMl),
          tailsMl: Math.round(tailsVol)
        },
        timers: {
          heads: profileRoot.heads.enabled ? Math.round(headsTimerSec) : 0,
          late_heads: Math.round(lateHeadsTimerSec),
          hearts: Math.round(heartsTimerSec),
          tails: profileRoot.tails.enabled ? Math.round(tailsTimerSec) : 0,
          total_process: netPowerWatts > 0 ? Math.round(totalProcessSec) : 0
        },
        refluxRatio: {
          heads: Math.round(headRefluxRatio * 100) / 100,
          late_heads: Math.round(lateHeadsRefluxRatio * 100) / 100,
          hearts: Math.round(heartsRefluxRatio * 100) / 100,
          tails: Math.round(tailsRefluxRatio * 100) / 100
        },
        phlegmatic: {
          heads: Number(phlegmaticHeads.toFixed(2)),
          late_heads: Number(phlegmaticLateHeads.toFixed(2)),
          hearts: Number(phlegmaticHearts.toFixed(2)),
          tails: Number(phlegmaticTails.toFixed(2))
        }
      }
    };
  }
}
function deepClone(obj) {
  if (obj === null || typeof obj !== "object") {
    return obj;
  }
  if (obj instanceof Date) {
    return new Date(obj.getTime());
  }
  if (Array.isArray(obj)) {
    const arrCopy = [];
    for (const item of obj) {
      arrCopy.push(deepClone(item));
    }
    return arrCopy;
  }
  const objCopy = {};
  for (const key in obj) {
    if (Object.prototype.hasOwnProperty.call(obj, key)) {
      objCopy[key] = deepClone(obj[key]);
    }
  }
  return objCopy;
}
const CALC_ROOT_SELECTOR = ".editor-layout#app";
const model = new DistillationCycleModel();
let activeDefaultParams = null;
let runUpdate = () => {
};
function getCalcRoot() {
  return document.querySelector(CALC_ROOT_SELECTOR);
}
function getValueByPath(obj, path) {
  return path.split(/[.\[\]]+/).filter(Boolean).reduce((acc, key) => acc == null ? void 0 : acc[key], obj);
}
function setValueByPath(obj, path, value) {
  const keys = path.split(/[.\[\]]+/).filter(Boolean);
  const lastKey = keys.pop();
  const lastObj = keys.reduce((acc, key) => {
    if (acc[key] == null || typeof acc[key] !== "object") acc[key] = {};
    return acc[key];
  }, obj);
  lastObj[lastKey] = value;
}
const formatTime = (s) => {
  if (!s || s < 0 || isNaN(s)) return "00:00:00";
  return new Date(s * 1e3).toISOString().substring(11, 19);
};
function buildFreshDefaults() {
  const p = deepClone(createDefaultProfile());
  p.virtual_bw_late = 7500;
  p.virtual_bw_tails = 7e3;
  return p;
}
let documentSubscribed = false;
function initCalculator() {
  const root = getCalcRoot();
  if (!root) return;
  activeDefaultParams = buildFreshDefaults();
  const formatResultText = (path, value) => {
    if (path.includes("timer") || path.includes("Time") || path.includes("process")) {
      return formatTime(value);
    }
    if (path.includes(".0") || path.includes("Ratio") || path.includes("Temp") || path.includes("phlegmatic")) {
      return value.toFixed(2) + (path.includes("Temp") ? " °C" : "");
    }
    const unit = path.includes("Ml") || path.includes("AS") ? " мл" : path.includes("flows") ? " мл/ч" : path.includes("Fortress") ? " %" : "";
    return Math.round(value) + unit;
  };
  function updateUI() {
    const r = getCalcRoot();
    if (!r || !activeDefaultParams) return;
    const currentParams = deepClone(activeDefaultParams);
    r.querySelectorAll("[data-path]").forEach((el) => {
      let val = el.type === "checkbox" ? el.checked : parseFloat(el.value);
      if (el.type !== "checkbox" && isNaN(val)) {
        val = getValueByPath(activeDefaultParams, el.dataset.path);
      }
      setValueByPath(currentParams, el.dataset.path, val);
    });
    try {
      const profile = model.calculateProcess(currentParams);
      r.querySelectorAll("[data-result]").forEach((el) => {
        const path = el.dataset.result;
        const value = getValueByPath(profile, path);
        if (value !== null && value !== void 0 && !isNaN(value)) {
          el.textContent = formatResultText(path, value);
        } else {
          el.textContent = "";
        }
      });
      const totalPercent = (currentParams.heads.enabled ? currentParams.heads.percent : 0) + (currentParams.late_heads.enabled ? currentParams.late_heads.percent : 0) + (currentParams.tails.enabled ? currentParams.tails.percent : 0) + currentParams.hearts.percent;
      const warningDiv = document.getElementById("fraction-warning");
      if (warningDiv) {
        warningDiv.style.display = totalPercent > 100 ? "block" : "none";
        warningDiv.textContent = `Сумма фракций: ${totalPercent.toFixed(1)}% > 100%`;
      }
    } catch (e) {
      console.error("Ошибка расчета:", e);
    }
  }
  runUpdate = updateUI;
  root.querySelectorAll("[data-path]").forEach((el) => {
    const val = getValueByPath(activeDefaultParams, el.dataset.path);
    if (val !== void 0 && val !== null && !(typeof val === "number" && Number.isNaN(val))) {
      if (el.type === "checkbox") el.checked = !!val;
      else el.value = val;
    }
  });
  updateUI();
}
let delegatedInputBound = false;
function bindDelegatedInputOnce() {
  if (delegatedInputBound) return;
  delegatedInputBound = true;
  document.addEventListener("input", (e) => {
    var _a;
    const t = e.target;
    if (!((_a = t == null ? void 0 : t.dataset) == null ? void 0 : _a.path)) return;
    const root = getCalcRoot();
    if (!(root == null ? void 0 : root.contains(t))) return;
    if (e.isTrusted) {
      if (t.dataset.path === "heads.targetFlowMlh") {
        const c = root.querySelector('[data-path="heads.targetCycles"]');
        if (c) c.value = "";
      }
      if (t.dataset.path === "heads.targetCycles") {
        const f = root.querySelector('[data-path="heads.targetFlowMlh"]');
        if (f) f.value = "";
      }
      if (t.dataset.path === "hearts.percent") {
        const h = root.querySelector('[data-path="ssvcSettings.hearts_finish_temp"]');
        if (h) h.value = "";
      }
      if (t.dataset.path === "ssvcSettings.hearts_finish_temp") {
        const hp = root.querySelector('[data-path="hearts.percent"]');
        if (hp) hp.value = "";
      }
    }
    runUpdate();
  });
}
function scheduleBoot() {
  bindDelegatedInputOnce();
  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", () => initCalculator());
  } else {
    initCalculator();
  }
  if (!documentSubscribed && typeof document$ !== "undefined" && document$.subscribe) {
    documentSubscribed = true;
    document$.subscribe(() => {
      initCalculator();
    });
  }
}
scheduleBoot();
