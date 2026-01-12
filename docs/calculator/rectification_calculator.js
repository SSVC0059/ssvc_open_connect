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
  // private formatTime(seconds: number): string {
  // 	const h = Math.floor(seconds / 3600);
  // 	const m = Math.floor((seconds % 3600) / 60);
  // 	const s = Math.round(seconds % 60);
  // 	return `${h}ч ${m}м ${s}с`;
  // }
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
  calculateProcess(profile) {
    var _a, _b, _c, _d, _e, _f;
    const lateHeadsEnabled = profile.late_heads.enabled;
    const tailsEnabled = profile.tails.enabled;
    const strengthVol = profile.strengthVol;
    const totalAS_ml = profile.volumeL * 1e3 * (strengthVol / 100);
    const netPowerWatts = profile.powerKw * 1e3;
    const physics = this.getVaporPhysics(strengthVol, netPowerWatts);
    const alcoholMassG = totalAS_ml * this.RHO_ETHANOL;
    const totalVaporMassPerSec = netPowerWatts / 1e3 / physics.lMix;
    const alcoholVaporMassPerSecG = totalVaporMassPerSec * physics.strengthMass * 1e3;
    const oneCycleSec = alcoholVaporMassPerSecG > 0 ? alcoholMassG / alcoholVaporMassPerSecG : 0;
    const stabSec = profile.stabilizationMin * 60;
    const bw_heads = (_b = (_a = profile.ssvcSettings) == null ? void 0 : _a.valve_bw) == null ? void 0 : _b[0];
    const bw_late = (_d = (_c = profile.ssvcSettings) == null ? void 0 : _c.valve_bw) == null ? void 0 : _d[1];
    const bw_valve3 = (_f = (_e = profile.ssvcSettings) == null ? void 0 : _e.valve_bw) == null ? void 0 : _f[2];
    const headsVol = totalAS_ml * profile.heads.percent / 100;
    let headsFlow;
    let headsTimerSec;
    if (profile.heads.targetFlowMlh && profile.heads.targetFlowMlh > 0) {
      headsFlow = Math.min(profile.heads.targetFlowMlh, bw_heads);
      headsTimerSec = headsFlow > 0 ? headsVol / headsFlow * 3600 : 0;
    } else {
      const cycles = profile.heads.targetCycles;
      const targetTotalSec = oneCycleSec * cycles;
      const requestedTimerSec = Math.max(0, targetTotalSec - stabSec);
      let requestedFlow = requestedTimerSec > 0 ? headsVol * 3600 / requestedTimerSec : 0;
      headsFlow = Math.min(requestedFlow, bw_heads);
      headsTimerSec = headsFlow > 0 ? headsVol / headsFlow * 3600 : 0;
    }
    const headsOpenTime = headsFlow / bw_heads * profile.ssvcSettings.heads[1];
    const headRefluxRatio = this.calculateRefluxRatio(physics.volVaporPerHour, headsFlow);
    let lateHeadsTimerSec;
    let lateHeadsOpenTime = 0;
    let lateHeadsFlow;
    let lateHeadsVol;
    let lateHeadsRefluxRatio;
    if (profile.late_heads.enabled) {
      lateHeadsVol = lateHeadsEnabled ? totalAS_ml * (profile.late_heads.percent / 100) : 0;
      if (profile.late_heads.targetFlowMlh && profile.late_heads.targetFlowMlh > 0) {
        lateHeadsFlow = profile.late_heads.targetFlowMlh;
      } else {
        lateHeadsFlow = bw_late * 0.1;
      }
      lateHeadsTimerSec = lateHeadsFlow > 0 ? lateHeadsVol / lateHeadsFlow * 3600 : 0;
      lateHeadsOpenTime = lateHeadsFlow / bw_late * profile.ssvcSettings.late_heads[1];
      lateHeadsRefluxRatio = this.calculateRefluxRatio(physics.volVaporPerHour, lateHeadsFlow);
    } else {
      lateHeadsVol = 0;
      lateHeadsFlow = 0;
      lateHeadsTimerSec = 0;
      lateHeadsRefluxRatio = 0;
    }
    const heartsProductMl = totalAS_ml * profile.hearts.percent / 100;
    let heartsInitialSpeed = profile.hearts.targetFlowMlh || physics.volVaporPerHour * 1e3;
    const heartsTimerSec = heartsInitialSpeed > 0 ? heartsProductMl / heartsInitialSpeed * 3600 : 0;
    const heartsOpenTime = bw_valve3 > 0 ? heartsInitialSpeed / bw_valve3 * profile.ssvcSettings.hearts[1] : 0;
    const heartsRefluxRatio = this.calculateRefluxRatio(
      physics.volVaporPerHour,
      heartsInitialSpeed
    );
    let tailsTimerSec = 0;
    let tailsOpenTime = 0;
    let tailsFlow = 0;
    let tailsRefluxRatio = 0;
    const tailsVol = tailsEnabled ? totalAS_ml * (profile.tails.percent / 100) : 0;
    tailsFlow = profile.tails.targetFlowMlh || bw_valve3;
    tailsTimerSec = tailsVol / tailsFlow * 3600;
    tailsOpenTime = tailsFlow / bw_valve3 * profile.ssvcSettings.tails[1];
    tailsRefluxRatio = this.calculateRefluxRatio(physics.volVaporPerHour, tailsFlow);
    const totalProcessSec = stabSec + headsTimerSec + lateHeadsTimerSec + heartsTimerSec + tailsTimerSec;
    const updatedSsvcSettings = {
      ...profile.ssvcSettings,
      heads: [Number(headsOpenTime.toFixed(1)), profile.ssvcSettings.heads[1]],
      // Используем индекс 1 для периода
      hearts: [Number(heartsOpenTime.toFixed(1)), profile.ssvcSettings.hearts[1]]
    };
    updatedSsvcSettings.tails = [Number(tailsOpenTime.toFixed(1)), profile.ssvcSettings.tails[1]];
    updatedSsvcSettings.late_heads = [
      Number(lateHeadsOpenTime.toFixed(1)),
      profile.ssvcSettings.late_heads[1]
    ];
    return {
      ...profile,
      ssvcSettings: updatedSsvcSettings,
      analytics: {
        totalAS: Math.round(totalAS_ml),
        boilingTemp: Number(this.calculateBoilingTemp(strengthVol).toFixed(2)),
        oneCycleTime: oneCycleSec,
        residueMl: Math.round(totalAS_ml - headsVol - lateHeadsVol - heartsProductMl - tailsVol),
        flows: {
          heads: Math.round(headsFlow),
          late_heads: Math.round(lateHeadsFlow),
          hearts: Math.round(heartsInitialSpeed),
          tails: Math.round(tailsFlow)
        },
        fractions: {
          headsMl: Math.round(headsVol),
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
