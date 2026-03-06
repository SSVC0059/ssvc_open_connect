import type { PageLoad } from './$types';

export const load = (async () => {
	return { title: 'Обновление встроенного ПО' };
}) satisfies PageLoad;
