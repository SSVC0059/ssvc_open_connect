declare module 'svelte-modals' {
	export type ModalComponent<T = any> = any;
	export const Modals: any;
	export function onBeforeClose(handler: () => boolean): void;

	export const modals: {
		open: (component: any, props?: Record<string, unknown>) => void;
		close: (id?: unknown) => void;
		closeAll: () => void;
	};
}
