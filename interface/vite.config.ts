import { sveltekit } from '@sveltejs/kit/vite';
import { defineConfig, loadEnv } from 'vite';
import type { UserConfig } from 'vite';
import Icons from 'unplugin-icons/vite';
import viteLittleFS from './vite-plugin-littlefs';
import tailwindcss from '@tailwindcss/vite';

function withTrailingSlash(url: string): string {
	return url.endsWith('/') ? url : `${url}/`;
}

export default defineConfig(({ mode }) => {
	const env = loadEnv(mode, process.cwd(), '');
	let httpTarget = (env.DEV_OC_PROXY_HTTP || '').trim() || 'http://127.0.0.1';
	httpTarget = withTrailingSlash(httpTarget);
	let wsTarget = (env.DEV_OC_PROXY_WS || '').trim();
	if (!wsTarget) {
		wsTarget = httpTarget.replace(/^http/, 'ws');
	} else {
		wsTarget = withTrailingSlash(wsTarget);
	}

	const config: UserConfig = {
		test: {
			environment: 'jsdom',
			include: ['tests/**/*.test.ts', 'src/**/*.test.ts'],
			setupFiles: ['tests/setup.ts'],
			deps: {
				inline: [/@sveltejs\/kit/]
			},
			coverage: {
				provider: 'v8',
				exclude: [
					'**/node_modules/**',
					'**/*.test.ts',
					'**/*.spec.ts',
					'**/tests/lib/*ExpectedRequests.ts'
				]
			}
		},
		// Чтобы в тестах (jsdom) подгружалась браузерная сборка Svelte, а не серверная (mount недоступен на сервере)
		resolve: {
			conditions: ['browser', 'import', 'module', 'svelte', 'default']
		},
		plugins: [
			sveltekit(),
			Icons({
				compiler: 'svelte'
			}),
			tailwindcss(),
			// Shorten file names for LittleFS 32 char limit
			viteLittleFS()
		],
		server: {
			proxy: {
				// DEV_OC_PROXY_HTTP / DEV_OC_PROXY_WS в interface/.env (см. .env.example)
				'/rest': {
					target: httpTarget,
					changeOrigin: true
				},
				'/ws': {
					target: wsTarget,
					changeOrigin: true,
					ws: true
				}
			}
		}
	};

	return config;
});
