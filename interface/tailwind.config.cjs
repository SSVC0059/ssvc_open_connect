/** @type {import('tailwindcss').Config} */

module.exports = {
	content: ['./src/**/*.{html,js,svelte,ts}', './node_modules/flowbite-svelte/**/*.{html,js,svelte,ts}'],
	theme: {
		animation: {
			info: 'blink-info 2s infinite',
			warning: 'blink-warning 1.5s infinite',
			critical: 'blink-critical 1s infinite',
		},
		keyframes: {
			'blink-info': {
				'0%, 100%': { opacity: '1' },
				'50%': { opacity: '0.5' },
			},
			'blink-warning': {
				'0%, 100%': { backgroundColor: 'rgba(255, 255, 150, 1)' },
				'50%': { backgroundColor: 'rgba(255, 255, 0, 1)' },
			},
			'blink-critical': {
				'0%, 100%': { backgroundColor: 'rgba(255, 50, 50, 1)', transform: 'scale(1)' },
				'50%': { backgroundColor: 'rgba(255, 0, 0, 0.9)', transform: 'scale(1.1)' },
			},
		},
		extend: {
			fontFamily: {
				sans: ['Inter', 'ui-sans-serif', 'system-ui'],
				display: ['Poppins', 'sans-serif'],
			},
		colors: {
				primary: '#1D4ED8',
				secondary: '#64748B',
			},
		},
	},
	plugins: [require('daisyui')],
	daisyui: {
		themes: ['corporate', 'business'],
		darkTheme: 'business'
	}
};
