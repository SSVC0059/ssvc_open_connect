import { sveltekit } from '@sveltejs/kit/vite';
import type { UserConfig } from 'vite';
import Icons from 'unplugin-icons/vite';
import viteLittleFS from './vite-plugin-littlefs';
import tailwindcss from '@tailwindcss/vite';


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
			// Proxying REST: http://localhost:5173/rest/bar -> http://192.168.1.83/rest/bar
			'/rest': {
				target: 'http://192.168.1.116/',
				changeOrigin: true
			},
			// Proxying websockets ws://localhost:5173/ws -> ws://192.168.1.83/ws
			'/ws': {
				target: 'ws://192.168.1.116/',
				changeOrigin: true,
				ws: true
			}
		}
	}
};

export default config;
