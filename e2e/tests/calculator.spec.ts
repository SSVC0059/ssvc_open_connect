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

test.describe('Калькулятор в собранной документации', () => {
	test('прямой заход: поля и общий АС заполняются', async ({ page }) => {
		await page.goto('/calculator/');
		await dismissConsentIfAny(page);

		const root = page.locator('#app');
		await expect(root).toBeVisible();

		await expect(root.locator('[data-path="volumeL"]')).toHaveValue('18');
		await expect(root.locator('[data-path="strengthVol"]')).toHaveValue('40');

		const totalAS = page.locator('[data-result="analytics.totalAS"]');
		await expect(totalAS).not.toHaveText('', { timeout: 15_000 });
		await expect(totalAS).toContainText('7200');
	});

	test('изменение объёма пересчитывает общий АС', async ({ page }) => {
		await page.goto('/calculator/');
		await dismissConsentIfAny(page);

		await page.locator('#app [data-path="volumeL"]').fill('25');
		await expect(page.locator('[data-result="analytics.totalAS"]')).toContainText('10000', {
			timeout: 10_000
		});
	});

	test('переход по ссылке с главной: калькулятор и аналитика', async ({ page }) => {
		await page.goto('/');
		await dismissConsentIfAny(page);

		await page.locator('a[href*="calculator"]').first().click();
		await expect(page).toHaveURL(/calculator/i, { timeout: 15_000 });

		await expect(page.locator('[data-result="analytics.totalAS"]')).not.toHaveText('', {
			timeout: 15_000
		});
		await expect(page.locator('[data-result="analytics.totalAS"]')).toContainText('7200');
	});
});
