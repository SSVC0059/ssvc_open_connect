import { test, expect } from '@playwright/test';

/** Баннер согласия Material (если включён в mkdocs.yml). */
async function dismissConsentIfAny(page: import('@playwright/test').Page) {
	const accept = page
		.locator('[data-md-component="consent"] button, .md-consent__form button')
		.filter({ hasText: /Accept|Принять|accept/i })
		.first();
	try {
		await accept.waitFor({ state: 'visible', timeout: 4000 });
		await accept.click();
	} catch {
		/* нет баннера — ок */
	}
}

/** Дождаться инициализации калькулятора: до неё поля ещё не связаны с моделью. */
async function waitForCalculator(page: import('@playwright/test').Page) {
	await expect(page.locator('#app')).toBeVisible();
	await expect(page.locator('#app')).toHaveAttribute('data-calc-ready', '1', { timeout: 15_000 });
}

test.describe('Калькулятор: предекремент и формула', () => {
	// Статический сервер документации (python http.server) не выдерживает
	// шесть параллельных браузеров: часть запросов получает ERR_CONNECTION_REFUSED.
	// Для этого файла запускаем тесты последовательно.
	test.describe.configure({ mode: 'serial' });

	test('предупреждает, что предекремент не сработает при малом гистерезисе', async ({ page }) => {
		await page.goto('/calculator/');
		await dismissConsentIfAny(page);
		await waitForCalculator(page);

		await page.locator('[data-path="ssvcSettings.hyst"]').fill('0.06');
		await page.locator('[data-path="ssvcSettings.predec"]').selectOption('13');

		const warning = page.locator('#protection-warning');
		await expect(warning).toBeVisible();
		await expect(warning).toContainText('не сработает');
		await expect(warning).toContainText('0,18');
	});

	test('предупреждает, что «Авто 92+» не сработает без предекремента', async ({ page }) => {
		await page.goto('/calculator/');
		await dismissConsentIfAny(page);
		await waitForCalculator(page);

		await page.locator('[data-path="ssvcSettings.formula"]').selectOption('2');

		const warning = page.locator('#protection-warning');
		await expect(warning).toBeVisible();
		await expect(warning).toContainText('Авто 92+');
	});

	test('показывает параметры формулы и снижение на первом предекременте', async ({ page }) => {
		await page.goto('/calculator/');
		await dismissConsentIfAny(page);
		await waitForCalculator(page);

		await page.locator('[data-path="ssvcSettings.hearts_finish_temp"]').fill('97');
		await page.locator('[data-path="ssvcSettings.formula"]').selectOption('2');
		await page.locator('[data-path="ssvcSettings.predec"]').selectOption('13');
		await page.locator('[data-path="ssvcSettings.hyst"]').fill('0.25');
		await page.locator('[data-path="planning.firstPredecTd2"]').fill('96');

		await expect(page.locator('[data-result="analytics.formula.start"]')).toContainText('92');
		await expect(page.locator('[data-result="analytics.formula.end"]')).toContainText('99');
		// 92→99 с первым предекрементом на 96 °C: (99−96)/(99−92) = 3/7 → снижение 57 %.
		await expect(page.locator('[data-result="analytics.formula.initialDropPct"]')).toHaveText('57 %');
		await expect(page.locator('[data-result="analytics.flows.hearts_avg"]')).not.toHaveText('');
		await expect(page.locator('#protection-warning')).toBeHidden();
	});
});
