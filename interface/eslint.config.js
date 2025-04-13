import js from '@eslint/js';
import svelteParser from 'svelte-eslint-parser';

export default [
	js.configs.recommended,
	{
		files: [
			'**/*.svelte',
			'*.svelte',
			// Need to specify the file extension for Svelte 5 with rune symbols
			'**/*.svelte.js',
			'*.svelte.js',
			'**/*.svelte.ts',
			'*.svelte.ts'
		],
		languageOptions: {
			parser: svelteParser
		},
		env: {
			browser: true,
			es2017: true,
			node: true
		}
	}
];
