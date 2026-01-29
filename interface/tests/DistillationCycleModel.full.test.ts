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
