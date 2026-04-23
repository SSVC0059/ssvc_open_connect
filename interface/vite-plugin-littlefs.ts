import type { Plugin } from 'vite';

export default function viteLittleFS(): Plugin[] {
	return [
		{
			name: 'vite-plugin-littlefs',
			enforce: 'post',
			apply: 'build',

			async config(config, _configEnv) {
				const output = config.build?.rollupOptions?.output as
					| {
							assetFileNames?: string;
							chunkFileNames?: string;
							entryFileNames?: string;
					  }
					| undefined;
				const assetFileNames = output?.assetFileNames;
				const chunkFileNames = output?.chunkFileNames;
				const entryFileNames = output?.entryFileNames;
				if (!config.build) config.build = {};
				if (!config.build.rollupOptions) config.build.rollupOptions = {};

				// Handle Server-build + Client Assets
				config.build.rollupOptions.output = {
					...(config.build.rollupOptions.output as object),
					assetFileNames: (assetFileNames ?? 'assets/[name].[ext]').replace('.[hash]', '')
				};

				// Handle Client-build
				if ((chunkFileNames ?? '').includes('hash')) {
					config.build.rollupOptions.output = {
						...(config.build.rollupOptions.output as object),
						chunkFileNames: (chunkFileNames ?? 'chunks/[name].js').replace('.[hash]', ''),
						entryFileNames: (entryFileNames ?? 'entries/[name].js').replace('.[hash]', '')
					};
				}
			}
		}
	];
}
