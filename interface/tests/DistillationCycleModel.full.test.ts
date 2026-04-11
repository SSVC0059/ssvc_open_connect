import { describe, it, expect } from 'vitest';
import { DistillationCycleModel, createDefaultProfile } from '$src/lib/actions/DistillationCycleModel';

describe('DistillationCycleModel Physical Calculations', () => {
	const model = new DistillationCycleModel();

	it('должен корректно рассчитывать температуру кипения для воды (0%)', () => {
		const model = new DistillationCycleModel();
		// Теперь должно вернуться ~99.97, а не 0
		const temp = (model as any).calculateBoilingTemp(0);
		expect(temp).toBeCloseTo(99.97, 1);
	});

	it('должен корректно находить спиртуозность по температуре (метод бисекции)', () => {
		const targetTemp = 85;
		const strength = (model as any).findStrengthByTemp(targetTemp);
		const backTemp = (model as any).calculateBoilingTemp(strength);

		// Погрешность итерационного метода в коде выставлена 0.01
		expect(backTemp).toBeCloseTo(targetTemp, 1);
	});

	it('расчет ФЧ должен возвращать 999 при нулевом отборе', () => {
		const rr = model.calculateRefluxRatio(10, 0);
		expect(rr).toBe(999);
	});
});

describe('DistillationCycleModel Process Logic', () => {
	const model = new DistillationCycleModel();

	it('должен рассчитать объем АС (Абсолютного Спирта) правильно', () => {
		const profile = createDefaultProfile();
		profile.volumeL = 20;
		profile.strengthVol = 40; // 8 литров АС

		const result = model.calculateProcess(profile);

		expect(result.analytics.totalAS).toBe(8000);
	});

	it('должен рассчитывать окончание тела по температуре (hearts_finish_temp)', () => {
		const model = new DistillationCycleModel();
		const profile = createDefaultProfile();
		profile.volumeL = 10;
		profile.strengthVol = 40; // 4000мл АС
		// Устанавливаем температуру, при которой в кубе еще точно есть спирт
		profile.ssvcSettings.hearts_finish_temp = 90;

		const result = model.calculateProcess(profile);

		// Проверка, что расчет вообще произведен
		expect(result.analytics.fractions.heartsMl).toBeGreaterThan(0);
		// Проверка, что итоговый процент в аналитике обновился
		expect(result.hearts.percent).not.toBe(75);
	});

	it('должен учитывать декремент (снижение скорости) при включенной формуле', () => {
		const profile = createDefaultProfile();
		profile.ssvcSettings.formula = true;
		profile.ssvcSettings.decrement = 10; // 10%
		profile.ssvcSettings.hearts_finish_temp = 95;
		profile.hearts.targetFlowMlh = 2000;

		const result = model.calculateProcess(profile);

		// Финальная скорость должна быть меньше начальной из-за декремента
		expect(result.analytics.flows.hearts_final).toBeLessThan(result.analytics.flows.hearts);
		// Средняя скорость должна быть между начальной и финальной
		expect(result.analytics.flows.hearts_avg).toBeLessThan(result.analytics.flows.hearts);
		if (result.analytics.flows.hearts_final !== undefined) {
			expect(result.analytics.flows.hearts_avg).toBeGreaterThan(result.analytics.flows.hearts_final);
		}
	});

	it('должен корректно рассчитывать объем тела по температуре и применять декремент скорости', () => {
		const model = new DistillationCycleModel();
		const profile = createDefaultProfile();
		profile.volumeL = 10;
		profile.strengthVol = 40; // 4000мл АС
		profile.ssvcSettings.hearts_finish_temp = 90; // Температура, при которой отбор тела завершается
		profile.ssvcSettings.formula = true; // Включаем формулу декремента
		profile.ssvcSettings.decrement = 10; // Декремент 10%
		profile.ssvcSettings.hyst = 0.1; // Гистерезис
		profile.hearts.targetFlowMlh = 1000; // Начальная скорость отбора тела

		const result = model.calculateProcess(profile);

		// 1. Проверяем, что объем тела рассчитан (т.е. hearts_finish_temp сработал)
		expect(result.analytics.fractions.heartsMl).toBeGreaterThan(0);

		// 2. Проверяем, что декремент применился: финальная скорость должна быть меньше начальной
		expect(result.analytics.flows.hearts).toBe(profile.hearts.targetFlowMlh); // Начальная скорость должна быть установленной
		expect(result.analytics.flows.hearts_final).toBeLessThan(result.analytics.flows.hearts);
		expect(result.analytics.flows.hearts_avg).toBeLessThan(result.analytics.flows.hearts);
		if (result.analytics.flows.hearts_final) {
			expect(result.analytics.flows.hearts_avg).toBeGreaterThan(result.analytics.flows.hearts_final);
		}
	});

	it('должен корректно рассчитывать фракцию "Подголовники" с заданной скоростью', () => {
		const model = new DistillationCycleModel();
		const profile = createDefaultProfile();
	
		// Убедимся, что подголовники включены
		profile.late_heads.enabled = true;
		profile.late_heads.percent = 10;
		profile.late_heads.targetFlowMlh = 200;
	
		const result = model.calculateProcess(profile);
	
		expect(result.analytics.fractions.lateHeadsMl).toBeGreaterThan(0);
		expect(result.analytics.timers.late_heads).toBeGreaterThan(0);
		expect(result.ssvcSettings.late_heads[0]).toBeGreaterThan(0);
	});

	it('должен корректно рассчитывать фракцию "Подголовники" с авто-скоростью', () => {
		const model = new DistillationCycleModel();
		const profile = createDefaultProfile();
	
		profile.late_heads.enabled = true;
		profile.late_heads.percent = 10;
		profile.late_heads.targetFlowMlh = 0; // Устанавливаем 0 для авто-расчета
	
		const result = model.calculateProcess(profile);
	
		expect(result.analytics.fractions.lateHeadsMl).toBeGreaterThan(0);
		expect(result.analytics.timers.late_heads).toBeGreaterThan(0);
		expect(result.analytics.flows.late_heads).toBeGreaterThan(0); // Скорость должна быть рассчитана
		expect(result.ssvcSettings.late_heads[0]).toBeGreaterThan(0); // Время открытия должно быть рассчитано
	});

	it('должен корректно рассчитывать фракцию "Хвосты" (игнорируя подголовники)', () => {
		const model = new DistillationCycleModel();
		const profile = createDefaultProfile();
		
		// Явно выключаем подголовники и включаем хвосты
		profile.late_heads.enabled = false;
		profile.tails.enabled = true;
		profile.tails.percent = 5;
		profile.tails.targetFlowMlh = 1500;
		
		const result = model.calculateProcess(profile);
		
		expect(result.analytics.fractions.tailsMl).toBeGreaterThan(0);
		expect(result.analytics.timers.tails).toBeGreaterThan(0);
		expect(result.analytics.flows.tails).toBe(1500);
		// Убедимся, что подголовники не рассчитывались
		expect(result.analytics.fractions.lateHeadsMl).toBe(0);
		expect(result.analytics.timers.late_heads).toBe(0);
	});
});

describe('DistillationCycleModel: дефолтный профиль и опциональные поля голов (SSVC)', () => {
	const model = new DistillationCycleModel();

	it('createDefaultProfile задаёт полный ssvcSettings, включая valve_bw_tails', () => {
		const p = createDefaultProfile();
		expect(p.ssvcSettings.valve_bw_tails).toBe(15000);
		expect(p.ssvcSettings.valve_bw).toEqual([7000, 12000, 7000]);
		expect(p.ssvcSettings.release_timer).toBe(0);
		expect(p.ssvcSettings.release_speed).toBe(0);
		expect(p.ssvcSettings.heads_final).toBe(0);
		expect(Array.isArray(p.ssvcSettings.parallel_v3)).toBe(true);
		expect(p.ssvcSettings.parallel_v3).toEqual([]);
	});

	it('release_timer/release_speed/heads_final: null, undefined и -1 эквивалентны «выключено» (как нули)', () => {
		const base = createDefaultProfile();
		base.volumeL = 10;
		base.strengthVol = 40;
		const expected = model.calculateProcess(base);

		const pick = (r: (typeof expected)['analytics']) => ({
			heads_release: r.flows.heads_release,
			releaseMl: r.fractions.releaseMl,
			heads_final_flow: r.flows.heads_final,
			heads_flow: r.flows.heads,
			heads_timer: r.timers.heads
		});

		const a = createDefaultProfile();
		a.volumeL = 10;
		a.strengthVol = 40;
		delete (a.ssvcSettings as { release_speed?: number }).release_speed;
		(a.ssvcSettings as { release_timer: number | null }).release_timer = null;
		(a.ssvcSettings as { heads_final: number }).heads_final = -1;

		const b = createDefaultProfile();
		b.volumeL = 10;
		b.strengthVol = 40;
		(b.ssvcSettings as { release_timer?: number }).release_timer = undefined as unknown as number;
		(b.ssvcSettings as { release_speed?: number }).release_speed = undefined as unknown as number;
		(b.ssvcSettings as { heads_final: number }).heads_final = -1;

		expect(pick(model.calculateProcess(a).analytics)).toEqual(pick(expected.analytics));
		expect(pick(model.calculateProcess(b).analytics)).toEqual(pick(expected.analytics));
	});

	it('положительные release_timer и release_speed увеличивают отбор «сброса» (heads_release, releaseMl)', () => {
		const base = createDefaultProfile();
		base.volumeL = 10;
		base.strengthVol = 40;
		const without = model.calculateProcess(base);

		const withRelease = createDefaultProfile();
		withRelease.volumeL = 10;
		withRelease.strengthVol = 40;
		withRelease.ssvcSettings.release_timer = 3600;
		withRelease.ssvcSettings.release_speed = 60;
		// releaseFlowMlh = (60 / heads[1]) * valve_bw[0] = (60/120)*7000 = 3500; releaseMl = 3500 * 3600/3600
		const withR = model.calculateProcess(withRelease);

		expect(without.analytics.flows.heads_release).toBe(0);
		expect(without.analytics.fractions.releaseMl).toBe(0);
		expect(withR.analytics.flows.heads_release).toBe(3500);
		expect(withR.analytics.fractions.releaseMl).toBeCloseTo(3500, 0);
	});

	it('heads_final > 0 задаёт финальную скорость отбора голов (heads_final в analytics.flows)', () => {
		const profile = createDefaultProfile();
		profile.volumeL = 10;
		profile.strengthVol = 40;
		profile.heads.targetFlowMlh = 2000;
		profile.ssvcSettings.heads_final = 120;
		// headsCyclePeriod default heads[1] = 120, bw_heads = 7000 -> (120/120)*7000 = 7000
		const result = model.calculateProcess(profile);
		expect(result.analytics.flows.heads).toBe(2000);
		expect(result.analytics.flows.heads_final).toBe(7000);
	});
});

describe('DistillationCycleModel Edge Cases', () => {
	const model = new DistillationCycleModel();

	it('должен корректно обрабатывать нулевую мощность', () => {
		const profile = createDefaultProfile();
		profile.powerKw = 0;
		profile.volumeL = 10;
		profile.strengthVol = 40;

		const result = model.calculateProcess(profile);
		// Проверяем, что расчет не падает и возвращает корректные структуры
		expect(result.analytics.totalAS).toBe(4000);
		expect(result.analytics.timers.total_process).toBeGreaterThanOrEqual(0);
	});

	it('должен корректно обрабатывать 100% спиртуозность', () => {
		const temp = (model as any).calculateBoilingTemp(97.17);
		expect(temp).toBeCloseTo(78.15, 1);

		const profile = createDefaultProfile();
		profile.strengthVol = 97.17;
		const result = model.calculateProcess(profile);
		expect(result.analytics.boilingTemp).toBeCloseTo(78.15, 1);
	});

	it('должен корректно обрабатывать очень маленький объем', () => {
		const profile = createDefaultProfile();
		profile.volumeL = 0.5;
		profile.strengthVol = 40;

		const result = model.calculateProcess(profile);
		expect(result.analytics.totalAS).toBe(200); // 500мл * 40%
		expect(result.analytics.fractions.heartsMl).toBeGreaterThanOrEqual(0);
	});

	it('должен возвращать 0 при отрицательной спиртуозности', () => {
		const temp = (model as any).calculateBoilingTemp(-10);
		expect(temp).toBe(99.97); // Граничное значение для b <= 0
	});
});
