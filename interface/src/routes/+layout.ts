import type { LayoutLoad } from './$types';

// This can be false if you're using a fallback (i.e. SPA mode)
export const prerender = false;
export const ssr = false;

export const load = (async ({ fetch }) => {
	const result = await fetch('/rest/features');
	const item = await result.json();
	return {
		features: item,
		title: 'SSVC Open Connect',
		github: 'SSVC0059/ssvc_open_connect',
		copyright: '2024 @thedemoncat',
		appName: 'SSVC Open Connect'
	};
}) satisfies LayoutLoad;
