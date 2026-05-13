import { defineConfig, devices } from '@playwright/test';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
/** Корень репозитория (родитель каталога e2e). */
export const repoRoot = path.resolve(__dirname, '..');

const py = process.platform === 'win32' ? 'python' : 'python3';

export default defineConfig({
	testDir: path.join(__dirname, 'tests'),
	fullyParallel: true,
	forbidOnly: !!process.env.CI,
	retries: process.env.CI ? 1 : 0,
	reporter: process.env.CI ? 'github' : 'list',
	use: {
		/** Порт отличен от дефолтного mkdocs serve (8000), чтобы не конфликтовать с локальным превью. */
		baseURL: 'http://127.0.0.1:8765',
		trace: 'on-first-retry'
	},
	projects: [{ name: 'chromium', use: { ...devices['Desktop Chrome'] } }],
	webServer: {
		/** Сборка + статика: так надёжнее, чем `mkdocs serve` в pipe на Windows (cp1251 + Unicode в Material). */
		command: `mkdocs build && ${py} -m http.server 8765 --bind 127.0.0.1 --directory site`,
		cwd: repoRoot,
		url: 'http://127.0.0.1:8765',
		reuseExistingServer: !process.env.CI,
		timeout: 300_000,
		stdout: 'pipe',
		stderr: 'pipe',
		env: {
			...process.env,
			PYTHONUTF8: '1',
			PYTHONIOENCODING: 'utf-8'
		}
	}
});
