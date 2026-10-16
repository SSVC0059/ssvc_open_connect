/**
 * Тесты живой подписи датчика уровня (Xkc25Section).
 *
 * Прошивка считает поле `filled` в цикле опроса датчиков (до 10 с), а `rawLevel`
 * читается в момент запроса. Пока вердикт брался из `filled`, подпись противоречила
 * соседней надписи: «Сигнал на пине: высокий (1) | Жидкости напротив датчика нет».
 * Здесь проверяется, что вердикт считается из живого чтения пина и его полярности.
 */
import { describe, it, expect, vi } from 'vitest';
import { render, screen } from '@testing-library/svelte';
import Xkc25Section from '$lib/components/OCSettings/Xkc25Section.svelte';
import type {
	Xkc25SensorConfig,
	Xkc25SensorStatus,
	Xkc25StatusResponse
} from '$lib/types/ssvc';

// Клиентский рантайм SvelteKit в jsdom не поднимается — API мокается, как в соседних тестах.
vi.mock('$lib/api/ssvcApi', () => ({ postXkc25Calibrate: vi.fn() }));

function sensor(overrides: Partial<Xkc25SensorConfig> = {}): Xkc25SensorConfig {
	return { enabled: true, gpio: 4, activeLow: false, zoneIndex: 0, calibrated: true, ...overrides };
}

function liveStatus(overrides: Partial<Xkc25SensorStatus> = {}): Xkc25SensorStatus {
	return {
		index: 0,
		enabled: true,
		gpio: 4,
		zoneIndex: 0,
		zone: 'receiver_tank_1',
		zoneLabel: 'Приёмная ёмкость 1',
		activeLow: false,
		calibrated: true,
		calibratedAt: 1792057811,
		levelWithoutLiquid: 0,
		levelWithLiquid: 1,
		live: true,
		rawLevel: 0,
		filled: false,
		...overrides
	};
}

const renderSection = (status: Xkc25SensorStatus, sensors = [sensor()]) =>
	render(Xkc25Section, {
		props: {
			sensors,
			triggerAction: 1,
			sharedTankIndex: 0,
			startGuardMode: 1,
			busy: false,
			status: {
				sensors: [status],
				triggerAction: 1,
				sharedTankIndex: 0,
				startGuardMode: 1,
				blockStart: false,
				blockReasons: [],
				warnings: []
			} as Xkc25StatusResponse
		}
	});

describe('Xkc25Section: живой вердикт датчика уровня', () => {
	it('высокий уровень пина и устаревшее filled=false → «жидкость есть»', () => {
		renderSection(liveStatus({ rawLevel: 1, filled: false }));

		expect(screen.getByText(/Сигнал на пине/).textContent).toContain('высокий (1)');
		expect(screen.getByText('Жидкость напротив датчика есть')).toBeInTheDocument();
	});

	it('низкий уровень пина и устаревшее filled=true → «жидкости нет»', () => {
		renderSection(liveStatus({ rawLevel: 0, filled: true }));

		expect(screen.getByText(/Сигнал на пине/).textContent).toContain('низкий (0)');
		expect(screen.getByText('Жидкости напротив датчика нет')).toBeInTheDocument();
	});

	it('инвертированная полярность: жидкость есть при низком уровне', () => {
		renderSection(liveStatus({ rawLevel: 0, filled: false, activeLow: true }), [
			sensor({ activeLow: true })
		]);

		expect(screen.getByText('Жидкость напротив датчика есть')).toBeInTheDocument();
	});

	it('без живого чтения пина (rawLevel = -1) используется поле filled прошивки', () => {
		renderSection(liveStatus({ rawLevel: -1, filled: true }));

		expect(screen.getByText('Жидкость напротив датчика есть')).toBeInTheDocument();
	});
});
