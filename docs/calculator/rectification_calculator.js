var __defProp = Object.defineProperty;
var __defNormalProp = (obj, key, value) => key in obj ? __defProp(obj, key, { enumerable: true, configurable: true, writable: true, value }) : obj[key] = value;
var __publicField = (obj, key, value) => __defNormalProp(obj, typeof key !== "symbol" ? key + "" : key, value);
function deepClone(obj) {
  return JSON.parse(JSON.stringify(obj));
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
    if (b <= 0) return 0;
    if (b >= 97.17) return 97.17;
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
    var _a, _b, _c, _d, _e;
    const lateHeadsEnabled = profile.late_heads.enabled;
    const tailsEnabled = profile.tails.enabled;
    const netPowerWatts = profile.powerKw * 1e3;
    const stabSec = profile.stabilizationMin * 60;
    let currentStrengthVol = profile.strengthVol;
    let currentVolumeL = profile.volumeL;
    let totalAS_ml = currentVolumeL * 1e3 * (currentStrengthVol / 100);
    const initialTotalAS = totalAS_ml;
    const bw_late = ((_b = (_a = profile.ssvcSettings) == null ? void 0 : _a.valve_bw) == null ? void 0 : _b[1]) || 0;
    const bw_valve3 = ((_d = (_c = profile.ssvcSettings) == null ? void 0 : _c.valve_bw) == null ? void 0 : _d[2]) || 0;
    const physicsHeads = this.getVaporPhysics(currentStrengthVol, netPowerWatts);
    const totalHeadsVolMl = initialTotalAS * profile.heads.percent / 100;
    const bw_heads = ((_e = profile.ssvcSettings.valve_bw) == null ? void 0 : _e[0]) || 0;
    const rTimer = Number(
      profile.ssvcSettings.release_timer < 0 ? 0 : profile.ssvcSettings.release_timer
    );
    const rOpen = Number(
      profile.ssvcSettings.release_speed < 0 ? 0 : profile.ssvcSettings.release_speed
    );
    const hFinal = profile.ssvcSettings.heads_final > 0 ? profile.ssvcSettings.heads_final : 0;
    const headsCyclePeriod = profile.ssvcSettings.heads[0] || 10;
    const alcoholMassG = initialTotalAS * this.RHO_ETHANOL;
    const totalVaporMassPerSec = netPowerWatts / 1e3 / physicsHeads.lMix;
    const alcoholVaporMassPerSecG = totalVaporMassPerSec * physicsHeads.strengthMass * 1e3;
    const oneCycleSec_heads = alcoholVaporMassPerSecG > 0 ? alcoholMassG / alcoholVaporMassPerSecG : 0;
    const releaseFlowMlh = rOpen / headsCyclePeriod * bw_heads;
    const releaseMl = releaseFlowMlh * rTimer / 3600;
    let headsStartFlowMlh;
    if (profile.heads.targetFlowMlh && profile.heads.targetFlowMlh > 0) {
      headsStartFlowMlh = Math.min(profile.heads.targetFlowMlh, bw_heads);
    } else {
      const alcoholMassG2 = initialTotalAS * this.RHO_ETHANOL;
      const totalVaporMassPerSec2 = netPowerWatts / 1e3 / physicsHeads.lMix;
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
    const hyst = profile.ssvcSettings.hyst || 0.1;
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
    let tailsFlow = profile.tails.targetFlowMlh || bw_valve3;
    const tailsTimerSec = tailsFlow > 0 ? tailsVol / tailsFlow * 3600 : 0;
    const tailsOpenTime = tailsFlow / bw_valve3 * profile.ssvcSettings.tails[1];
    const tailsRefluxRatio = this.calculateRefluxRatio(physicsTails.volVaporPerHour, tailsFlow);
    const totalProcessSec = stabSec + headsTimerSec + lateHeadsTimerSec + heartsTimerSec + tailsTimerSec;
    const phlegmaticHeads = this.calculatePhlegmatic(netPowerWatts, headsStartFlowMlh);
    const phlegmaticLateHeads = this.calculatePhlegmatic(netPowerWatts, lateHeadsFlow);
    const phlegmaticHearts = this.calculatePhlegmatic(netPowerWatts, heartsInitialSpeed);
    const phlegmaticTails = this.calculatePhlegmatic(netPowerWatts, tailsFlow);
    return {
      ...profile,
      hearts: {
        ...profile.hearts,
        percent: profile.ssvcSettings.hearts_finish_temp > 0 ? calculatedHeartsPercent : profile.hearts.percent
      },
      ssvcSettings: {
        ...profile.ssvcSettings,
        heads: [Number(headsOpenTime.toFixed(1)), headsCyclePeriod],
        late_heads: [Number(lateHeadsOpenTime.toFixed(1)), profile.ssvcSettings.late_heads[1]],
        hearts: [Number(heartsOpenTime.toFixed(1)), profile.ssvcSettings.hearts[1]],
        tails: [Number(tailsOpenTime.toFixed(1)), profile.ssvcSettings.tails[1]]
      },
      analytics: {
        totalAS: Math.round(initialTotalAS),
        boilingTemp: Number(this.calculateBoilingTemp(profile.strengthVol).toFixed(2)),
        residueMl: Math.round(currentVolumeL * 1e3),
        residualFortress: Number(currentStrengthVol.toFixed(1)),
        oneCycleTime: Math.round(oneCycleSec_heads),
        // Добавлено для отображения переиспарения
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
          heads: profile.heads.enabled ? Math.round(headsTimerSec) : 0,
          late_heads: Math.round(lateHeadsTimerSec),
          hearts: Math.round(heartsTimerSec),
          tails: profile.tails.enabled ? Math.round(tailsTimerSec) : 0,
          total_process: Math.round(totalProcessSec)
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
document.addEventListener("DOMContentLoaded", () => {
  function formatTime(seconds) {
    if (isNaN(seconds) || seconds < 0) return "00:00:00";
    const h = Math.floor(seconds / 3600).toString().padStart(2, "0");
    const m = Math.floor(seconds % 3600 / 60).toString().padStart(2, "0");
    const s = Math.floor(seconds % 60).toString().padStart(2, "0");
    return `${h}:${m}:${s}`;
  }
  const model = new DistillationCycleModel();
  const initialParams = {
    powerKw: 2.5,
    volumeL: 18,
    strengthVol: 40,
    stabilizationMin: 20,
    heads: { percent: 3, valveMlh: 7e3, pwmPeriod: 120, enabled: true },
    subHeads: { percent: 7, valveMlh: 7e3, pwmPeriod: 60, enabled: true },
    body: { valveMlh: 12e3, pwmPeriod: 10, initialSpeedMlh: 1800, decrementPercent: 10 },
    tails: { percent: 5, valveMlh: 15e3, pwmPeriod: 360, enabled: true }
  };
  let params = deepClone(initialParams);
  let customSsvcSettings = {};
  const inputElements = Array.from(document.querySelectorAll(".settings-section input[data-path], .fraction-settings input[data-path], .summary-controls input[data-path]"));
  const ssvcInputElements = Array.from(document.querySelectorAll(".ssvc-grid input[data-path]"));
  const ssvcResetButtons = Array.from(document.querySelectorAll(".ssvc-grid button[data-path]"));
  function getAnalyticsElement(id) {
    const el = document.getElementById(`an-${id}`);
    if (!el) return { value: null, initial: null, container: null };
    return {
      value: el.querySelector(".analytics-value"),
      initial: el.querySelector(".initial-value"),
      container: el
    };
  }
  function getSsvcElement(path) {
    return document.querySelector(`.ssvc-grid [data-path="${path}"]`);
  }
  function updateUI() {
    const calculatedResult = model.calculateProcess(params);
    const calculatedSsvcSettings = calculatedResult.ssvcSettings;
    if (Object.keys(customSsvcSettings).length === 0) {
      customSsvcSettings = deepClone(calculatedSsvcSettings);
    }
    const finalResult = model.calculateProcess({ ...params, ssvcSettings: customSsvcSettings });
    updateAnalytics(finalResult.analytics, calculatedResult.analytics);
    updateSsvcInputs(customSsvcSettings, calculatedSsvcSettings);
    ["heads", "subHeads", "tails"].forEach((key) => {
      const details = document.getElementById(`details-${key}`);
      const summary = document.getElementById(`summary-${key}`);
      summary.classList.toggle("active", params[key].enabled);
      if (params[key].enabled) {
        details.open = true;
      } else {
        details.open = false;
      }
    });
  }
  function updateAnalytics(analytics, calculatedAnalytics) {
    getAnalyticsElement("totalAS").value.textContent = `${analytics.totalAS.toFixed(0)} мл`;
    getAnalyticsElement("boilingTemp").value.textContent = `${analytics.boilingTemp.toFixed(2)} °C`;
    getAnalyticsElement("oneCycleTime").value.textContent = analytics.oneCycleTime;
    getAnalyticsElement("total_process_timer").value.textContent = formatTime(analytics.timers.total_process_timer);
    getAnalyticsElement("residueMl").value.textContent = `${(analytics.residueMl / 1e3).toFixed(2)} л`;
    ["headsMl", "subHeadsMl", "bodyMl", "tailsMl"].forEach((key) => {
      getAnalyticsElement(`fractions-${key}`).value.textContent = `${analytics.fractions[key].toFixed(0)} мл`;
    });
    ["heads", "subHeads", "body", "tails"].forEach((key) => {
      const el = getAnalyticsElement(`flows-${key}`);
      el.value.textContent = `${analytics.flows[key]} мл/ч`;
      const initialVal = `${calculatedAnalytics.flows[key]} мл/ч`;
      const isModified = el.value.textContent !== initialVal;
      el.container.classList.toggle("modified", isModified);
      if (el.initial) {
        el.initial.textContent = isModified ? `(${initialVal})` : "";
      }
    });
    ["heads_timer", "subHeads_timer", "body_timer", "tails_timer"].forEach((key) => {
      getAnalyticsElement(`timers-${key}`).value.textContent = formatTime(analytics.timers[key]);
    });
  }
  function updateSsvcInputs(current, initial) {
    ["heads", "subHeads", "body", "tails"].forEach((key) => {
      const periodInput = getSsvcElement(`${key}.period`);
      const openTimeInput = getSsvcElement(`${key}.openTime`);
      const periodInitialEl = getSsvcElement(`${key}.period-initial`);
      const openTimeInitialEl = getSsvcElement(`${key}.openTime-initial`);
      const resetBtn = getSsvcElement(`${key}-reset`);
      const itemContainer = document.getElementById(`ssvc-${key}`);
      periodInput.value = current[key].period;
      openTimeInput.value = current[key].openTime;
      const isPeriodModified = String(current[key].period) !== String(initial[key].period);
      const isOpenTimeModified = Number(current[key].openTime).toFixed(1) !== Number(initial[key].openTime).toFixed(1);
      periodInput.classList.toggle("modified", isPeriodModified);
      periodInitialEl.textContent = isPeriodModified ? `было: ${initial[key].period}` : "";
      openTimeInput.classList.toggle("modified", isOpenTimeModified);
      openTimeInitialEl.textContent = isOpenTimeModified ? `было: ${Number(initial[key].openTime).toFixed(1)}` : "";
      const isModified = isPeriodModified || isOpenTimeModified;
      itemContainer.classList.toggle("modified", isModified);
      resetBtn.classList.toggle("visible", isModified);
    });
  }
  function readParamsFromUI() {
    const newParams = deepClone(params);
    inputElements.forEach((el) => {
      const path = el.dataset.path.split(".");
      let current = newParams;
      for (let i = 0; i < path.length - 1; i++) {
        current = current[path[i]];
      }
      if (el.type === "checkbox") {
        current[path[path.length - 1]] = el.checked;
      } else {
        current[path[path.length - 1]] = parseFloat(el.value) || 0;
      }
    });
    return newParams;
  }
  function readSsvcFromUI() {
    const newSsvc = deepClone(customSsvcSettings);
    ["heads", "subHeads", "body", "tails"].forEach((key) => {
      newSsvc[key].period = parseFloat(getSsvcElement(`${key}.period`).value) || 0;
      newSsvc[key].openTime = parseFloat(getSsvcElement(`${key}.openTime`).value) || 0;
    });
    return newSsvc;
  }
  function setParamsToUI(p) {
    inputElements.forEach((el) => {
      const path = el.dataset.path.split(".");
      let val = p;
      path.forEach((k) => val = val[k]);
      if (el.type === "checkbox") {
        el.checked = val;
      } else {
        el.value = val;
      }
    });
  }
  inputElements.forEach((el) => {
    el.addEventListener("input", () => {
      params = readParamsFromUI();
      customSsvcSettings = {};
      updateUI();
    });
  });
  ssvcInputElements.forEach((el) => {
    el.addEventListener("input", () => {
      customSsvcSettings = readSsvcFromUI();
      updateUI();
    });
  });
  ssvcResetButtons.forEach((btn) => {
    btn.addEventListener("click", () => {
      const key = btn.dataset.path.replace("-reset", "");
      const calculatedSsvc = model.calculateProcess(params).ssvcSettings;
      customSsvcSettings[key] = deepClone(calculatedSsvc[key]);
      updateUI();
    });
  });
  document.getElementById("reset-all-btn").addEventListener("click", () => {
    params = deepClone(initialParams);
    customSsvcSettings = {};
    setParamsToUI(params);
    updateUI();
  });
  setParamsToUI(params);
  updateUI();
});
