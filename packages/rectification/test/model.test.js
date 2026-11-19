/**
 * Единые тесты математики ректификации.
 *
 * Это единственный источник истины для расчёта: интерфейс, MCP и документация
 * используют тот же `src/index.js`, поэтому проверять три копии не нужно.
 */

import assert from 'node:assert/strict';
import test from 'node:test';
import { DistillationCycleModel, createDefaultProfile } from '../src/index.js';

const model = new DistillationCycleModel();

function close(actual, expected, epsilon = 1e-6, message = '') {
  assert.ok(
    Math.abs(actual - expected) <= epsilon,
    `${message} ожидалось ${expected}, получено ${actual}`,
  );
}

test('физика: температура кипения воды и азеотропа', () => {
  close(model.calculateBoilingTemp(0), 99.97, 0.01, 'вода');
  close(model.calculateBoilingTemp(97.17), 78.15, 0.01, 'азеотроп');
  close(model.calculateBoilingTemp(-10), 99.97, 0.01, 'отрицательная крепость');
});

test('физика: бисекция возвращает крепость по температуре', () => {
  const strength = model.findStrengthByTemp(85);
  close(model.calculateBoilingTemp(strength), 85, 0.05, 'обратный пересчёт');
});

test('флегмовое число: нулевой отбор — 999, положительный — конечно', () => {
  assert.equal(model.calculateRefluxRatio(10, 0), 999);
  assert.ok(Number.isFinite(model.calculateRefluxRatio(10, 2)));
});

test('АС: 20 л × 40 % = 8000 мл, 18 л × 40 % = 7200 мл', () => {
  const twenty = createDefaultProfile();
  twenty.volumeL = 20;
  twenty.strengthVol = 40;
  assert.equal(model.calculateProcess(twenty).analytics.totalAS, 8000);

  const eighteen = createDefaultProfile();
  eighteen.volumeL = 18;
  eighteen.strengthVol = 40;
  assert.equal(model.calculateProcess(eighteen).analytics.totalAS, 7200);
});

test('тело по температуре: hearts_finish_temp даёт объём и пересчитывает процент', () => {
  const profile = createDefaultProfile();
  profile.volumeL = 10;
  profile.strengthVol = 40;
  profile.ssvcSettings.hearts_finish_temp = 90;

  const result = model.calculateProcess(profile);
  assert.ok(result.analytics.fractions.heartsMl > 0);
  assert.notEqual(result.hearts.percent, 75);
});

test('декремент: финальная и средняя скорости ниже начальной', () => {
  const profile = createDefaultProfile();
  profile.ssvcSettings.formula = 1;
  profile.ssvcSettings.decrement = 10;
  profile.ssvcSettings.hearts_finish_temp = 95;
  profile.hearts.targetFlowMlh = 2000;

  const result = model.calculateProcess(profile);
  assert.ok(result.analytics.flows.hearts_final < result.analytics.flows.hearts);
  assert.ok(result.analytics.flows.hearts_avg < result.analytics.flows.hearts);
  assert.ok(result.analytics.flows.hearts_avg > result.analytics.flows.hearts_final);
});

test('подголовники: заданная скорость пересчитывается в объём и таймер', () => {
  const profile = createDefaultProfile();
  profile.late_heads.enabled = true;
  profile.late_heads.percent = 10;
  profile.late_heads.targetFlowMlh = 200;

  const result = model.calculateProcess(profile);
  assert.ok(result.analytics.fractions.lateHeadsMl > 0);
  assert.ok(result.analytics.timers.late_heads > 0);
  assert.ok(result.ssvcSettings.late_heads[0] > 0);
});

test('подголовники: авторасчёт при нулевой скорости', () => {
  const profile = createDefaultProfile();
  profile.late_heads.enabled = true;
  profile.late_heads.percent = 10;
  profile.late_heads.targetFlowMlh = 0;

  const result = model.calculateProcess(profile);
  assert.ok(result.analytics.fractions.lateHeadsMl > 0);
  assert.ok(result.analytics.timers.late_heads > 0);
  assert.ok(result.analytics.flows.late_heads > 0);
  assert.ok(result.ssvcSettings.late_heads[0] > 0);
});

test('хвосты: включаются отдельно, подголовники не рассчитываются', () => {
  const profile = createDefaultProfile();
  profile.late_heads.enabled = false;
  profile.tails.enabled = true;
  profile.tails.percent = 5;
  profile.tails.targetFlowMlh = 1500;

  const result = model.calculateProcess(profile);
  assert.ok(result.analytics.fractions.tailsMl > 0);
  assert.ok(result.analytics.timers.tails > 0);
  assert.equal(result.analytics.flows.tails, 1500);
  assert.equal(result.analytics.fractions.lateHeadsMl, 0);
  assert.equal(result.analytics.timers.late_heads, 0);
});

test('createDefaultProfile: полный ssvcSettings и valve_bw', () => {
  const profile = createDefaultProfile();
  assert.equal(profile.ssvcSettings.valve_bw_tails, 15000);
  assert.deepEqual(profile.ssvcSettings.valve_bw, [7000, 12000, 7000]);
  assert.equal(profile.ssvcSettings.release_timer, 0);
  assert.equal(profile.ssvcSettings.release_speed, 0);
  assert.equal(profile.ssvcSettings.heads_final, 0);
  assert.deepEqual(profile.ssvcSettings.parallel_v3, []);
});

test('release_timer/release_speed/heads_final: null, undefined и -1 равны нулю', () => {
  const base = createDefaultProfile();
  base.volumeL = 10;
  base.strengthVol = 40;
  const expected = model.calculateProcess(base);

  const pick = (analytics) => ({
    heads_release: analytics.flows.heads_release,
    releaseMl: analytics.fractions.releaseMl,
    heads_final_flow: analytics.flows.heads_final,
    heads_flow: analytics.flows.heads,
    heads_timer: analytics.timers.heads,
  });

  const a = createDefaultProfile();
  a.volumeL = 10;
  a.strengthVol = 40;
  delete a.ssvcSettings.release_speed;
  a.ssvcSettings.release_timer = null;
  a.ssvcSettings.heads_final = -1;

  const b = createDefaultProfile();
  b.volumeL = 10;
  b.strengthVol = 40;
  b.ssvcSettings.release_timer = undefined;
  b.ssvcSettings.release_speed = undefined;
  b.ssvcSettings.heads_final = -1;

  assert.deepEqual(pick(model.calculateProcess(a).analytics), pick(expected.analytics));
  assert.deepEqual(pick(model.calculateProcess(b).analytics), pick(expected.analytics));
});

test('сброс голов: положительные release_timer/release_speed увеличивают отбор', () => {
  const base = createDefaultProfile();
  base.volumeL = 10;
  base.strengthVol = 40;
  const without = model.calculateProcess(base);

  const withRelease = createDefaultProfile();
  withRelease.volumeL = 10;
  withRelease.strengthVol = 40;
  withRelease.ssvcSettings.release_timer = 3600;
  withRelease.ssvcSettings.release_speed = 60;
  const withR = model.calculateProcess(withRelease);

  assert.equal(without.analytics.flows.heads_release, 0);
  assert.equal(without.analytics.fractions.releaseMl, 0);
  assert.equal(withR.analytics.flows.heads_release, 3500);
  close(withR.analytics.fractions.releaseMl, 3500, 0.5);
});

test('heads_final задаёт финальную скорость голов', () => {
  const profile = createDefaultProfile();
  profile.volumeL = 10;
  profile.strengthVol = 40;
  profile.heads.targetFlowMlh = 2000;
  profile.ssvcSettings.heads_final = 120;

  const result = model.calculateProcess(profile);
  assert.equal(result.analytics.flows.heads, 2000);
  assert.equal(result.analytics.flows.heads_final, 7000);
});

test('время открытия клапана голов зависит от мощности', () => {
  const low = createDefaultProfile();
  low.powerKw = 2;
  const high = createDefaultProfile();
  high.powerKw = 4;

  const rLow = model.calculateProcess(low);
  const rHigh = model.calculateProcess(high);

  assert.ok(rLow.ssvcSettings.heads[0] > 0);
  assert.ok(rHigh.ssvcSettings.heads[0] > 0);
  assert.notEqual(rHigh.ssvcSettings.heads[0], rLow.ssvcSettings.heads[0]);
  assert.equal(rHigh.ssvcSettings.heads[1], rLow.ssvcSettings.heads[1]);
});

test('границы: нулевая мощность, 97.17 % и малый объём', () => {
  const zeroPower = createDefaultProfile();
  zeroPower.powerKw = 0;
  zeroPower.volumeL = 10;
  zeroPower.strengthVol = 40;
  const zero = model.calculateProcess(zeroPower);
  assert.equal(zero.analytics.totalAS, 4000);
  assert.ok(zero.analytics.timers.total_process >= 0);

  const azeotrope = createDefaultProfile();
  azeotrope.strengthVol = 97.17;
  close(model.calculateProcess(azeotrope).analytics.boilingTemp, 78.15, 0.01);

  const small = createDefaultProfile();
  small.volumeL = 0.5;
  small.strengthVol = 40;
  const tiny = model.calculateProcess(small);
  assert.equal(tiny.analytics.totalAS, 200);
  assert.ok(tiny.analytics.fractions.heartsMl >= 0);
});

test('чистота: calculateProcess не мутирует вход и детерминирован', () => {
  const profile = createDefaultProfile();
  profile.volumeL = 18;
  profile.strengthVol = 40;
  const before = structuredClone(profile);

  const first = model.calculateProcess(profile);
  const second = model.calculateProcess(profile);

  assert.deepEqual(profile, before, 'входной профиль изменён');
  assert.deepEqual(first.analytics, second.analytics, 'повторный расчёт отличается');
});

test('таймеры: total_process не учитывает стабилизацию дважды', () => {
  const profile = createDefaultProfile();
  profile.volumeL = 18;
  profile.strengthVol = 40;
  profile.stabilizationMin = 20;
  profile.tails.enabled = false;

  const result = model.calculateProcess(profile);
  const { heads, late_heads: lateHeads, hearts, tails, total_process: total } = result.analytics.timers;

  // heads уже включает стабилизацию: 1200 с не должны попасть в сумму второй раз.
  assert.equal(total, heads + lateHeads + hearts + tails, 'total_process ≠ сумма этапов');
  assert.ok(
    Math.abs(total - (heads + lateHeads + hearts + tails + 1200)) > 1,
    'стабилизация посчитана повторно',
  );
});
