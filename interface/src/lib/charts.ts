import type { Chart as ChartInstance, ChartConfiguration } from 'chart.js';

type ChartModule = typeof import('chart.js');

let modulePromise: Promise<ChartModule> | null = null;

/**
 * Ленивая загрузка chart.js.
 *
 * Библиотека весит сотни килобайт и нужна только на экранах с графиками,
 * поэтому вынесена из начального бандла: `import type` стирается при сборке,
 * а сам модуль подтягивается при первом обращении.
 *
 * Регистрация контроллеров выполняется один раз на всю страницу — повторные
 * вызовы возвращают тот же промис.
 */
export function loadChartJs(): Promise<ChartModule> {
	if (!modulePromise) {
		modulePromise = import('chart.js').then((module) => {
			module.Chart.register(...module.registerables);
			return module;
		});
	}

	return modulePromise;
}

/** Адаптер Luxon нужен только временным осям (история батареи). */
export async function loadLuxonAdapter(): Promise<void> {
	const [{ Chart }, adapter] = await Promise.all([loadChartJs(), import('chartjs-adapter-luxon')]);
	Chart.register(adapter);
}

export type { ChartInstance, ChartConfiguration };
