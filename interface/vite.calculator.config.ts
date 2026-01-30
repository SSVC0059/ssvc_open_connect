import { defineConfig } from 'vite';
import { resolve } from 'path';

// https://vitejs.dev/config/
export default defineConfig({
	// Отключаем плагины, которые не нужны для этой сборки (например, svelte)
	plugins: [],

	// Определяем пути для корректной работы импортов ($lib, ~icons)
	resolve: {
		alias: {
			$lib: resolve(__dirname, './src/lib'),
			'~icons': resolve(__dirname, './src/lib/icons') // Пример, если иконки понадобятся
		}
	},

	build: {
		// Собираем один файл, без хэшей в названии
		outDir: resolve(__dirname, '../docs/calculator'),
		emptyOutDir: false, // Не очищать папку docs
		rollupOptions: {
			input: {
				// Наша точка входа
				calculator: resolve(__dirname, 'src/calculator.js')
			},
			output: {
				// Имя конечного файла
				entryFileNames: 'rectification_calculator.js',
				// Отключаем создание чанков и других ассетов
				chunkFileNames: `[name].js`,
				assetFileNames: `[name].[ext]`
			}
		},
		// Отключаем минификацию для более читаемого кода (можно включить)
		minify: false
	}
});
