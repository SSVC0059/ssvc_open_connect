import type { PageLoad } from './$types';

export const load = (async ({ fetch }) => {
	return {
		title: 'Настройки Open Connect'
	};
}) satisfies PageLoad;
