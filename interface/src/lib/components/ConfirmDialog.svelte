<script lang="ts">
	import { modals } from 'svelte-modals';
	import { focusTrap } from '$lib/actions/focusTrap';
	import { fly } from 'svelte/transition';
	import Cancel from '~icons/tabler/x';
	import Check from '~icons/tabler/check';

	// provided by <Modals />

	interface Props {
		isOpen?: boolean;
		title: string;
		message: string;
		onConfirm: () => void;
		onCancel?: () => void;
		labels?: any;
	}

	let {
		isOpen,
		title,
		message,
		onConfirm,
		onCancel = () => modals.close(),
		labels = {
			cancel: { label: 'Cancel', icon: Cancel },
			confirm: { label: 'OK', icon: Check }
		}
	}: Props = $props();
</script>

{#if isOpen}
	{@const SvelteComponent = labels?.confirm.icon}
	<div
		role="dialog"
		class="pointer-events-none fixed inset-0 z-50 flex items-center justify-center max-sm:p-4"
		transition:fly={{ y: 50 }}
		use:focusTrap
	>
		<div
			class="rounded-box bg-base-100 shadow-secondary/30 pointer-events-auto flex min-w-fit max-w-md flex-col justify-between p-4 shadow-lg max-sm:min-w-0 max-sm:w-full"
		>
			<h2 class="text-base-content text-start text-2xl font-bold max-sm:text-lg">{title}</h2>
			<div class="divider my-2"></div>
			<p class="text-base-content mb-1 text-start text-base max-sm:text-sm">{@html message}</p>
			<div class="divider my-2"></div>
			<div class="flex justify-end gap-2 max-sm:flex-col-reverse">
				<button
					class="btn btn-primary inline-flex min-h-11 items-center justify-center gap-2 max-sm:w-full sm:min-h-0"
					onclick={() => {
						onCancel();
						modals.close();
					}}><labels.cancel.icon class="h-5 w-5 shrink-0" /><span class="text-center">{labels?.cancel.label}</span></button
				>
				<button
					class="btn btn-warning text-warning-content inline-flex min-h-11 items-center justify-center gap-2 max-sm:w-full sm:min-h-0"
					onclick={onConfirm}
					><SvelteComponent class="h-5 w-5 shrink-0" /><span class="text-center">{labels?.confirm.label}</span></button
				>
			</div>
		</div>
	</div>
{/if}
