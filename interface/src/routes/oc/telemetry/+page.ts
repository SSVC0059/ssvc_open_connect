import type { PageLoad } from './$types';

export const load = (async ({ fetch }) => {
	return {
		title: 'SSVC Open Connect'
	};
}) satisfies PageLoad;
