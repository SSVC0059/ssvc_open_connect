<script lang="ts">
	import Pencil from '~icons/tabler/pencil';
	import { focusTrap } from 'svelte-focus-trap';
	import { fly } from 'svelte/transition';
	import Cancel from '~icons/tabler/x';
	import Check from '~icons/tabler/check';

	let {
		value: initialValue,
		onSave,
		labels = { true: 'Да', false: 'Нет' }
	} = $props<{
		value: {
			name: string;
			value: boolean;
		};
		labels?: {
			true: string;
			false: string;
		};
		onSave: (value: { name: string; value: boolean }) => void;
	}>();

	let isEditing = $state(false);
	let draftValue = $state(initialValue.value);
	let draftName = $state(initialValue.name);

	let bodyOverflow = '';
	$effect(() => {
		draftValue = initialValue.value;
		draftName = initialValue.name;
		if (isEditing) {
			bodyOverflow = document.body.style.overflow;
			document.body.style.overflow = 'hidden';
		} else {
			document.body.style.overflow = bodyOverflow;
		}
	});
</script>

{#if isEditing}
	<div class="fixed inset-0 z-40 bg-black/50 backdrop-blur-sm fade-in"></div>
	<div
		role="dialog"
		class="pointer-events-none fixed inset-0 z-50 flex items-center justify-center"
		transition:fly={{ y: 50 }}
		use:focusTrap
	>
		<div
			class="bg-base-100 shadow-secondary/30 rounded-box pointer-events-auto flex max-h-full min-w-fit max-w-md flex-col justify-between p-4 shadow-lg"
		>
			<h2 class="text-base-content text-center text-2xl font-bold">Изменение переменной</h2>
			<div class="divider my-2"></div>
			<div class="overflow-y-auto space-y-4 p-4">
				<div class="flex flex-col gap-4 items-center">
					<label class="font-medium text-center">{draftName}</label>
					<div class="join">
						<button
							class="join-item btn {draftValue ? 'btn-active' : ''}"
							onclick={() => (draftValue = true)}
						>
							{labels.true}
						</button>
						<button
							class="join-item btn {!draftValue ? 'btn-active' : ''}"
							onclick={() => (draftValue = false)}
						>
							{labels.false}
						</button>
					</div>
				</div>
			</div>
			<div class="divider my-2"></div>
			<div class="flex flex-wrap justify-end gap-2">
				<button
					class="btn btn-primary inline-flex flex-none items-center"
					onclick={() => {
						onSave({ name: draftName, value: draftValue });
						isEditing = false;
					}}
				>
					<Check class="mr-2 h-5 w-5" />
					<span>Сохранить</span>
				</button>
				<button
					class="btn btn-warning text-warning-content inline-flex flex-none items-center"
					onclick={() => {
						isEditing = false;
					}}
				>
					<Cancel class="mr-2 h-5 w-5" />
					<span>Отменить</span>
				</button>
			</div>
		</div>
	</div>
{/if}

<div class="flex gap-2 items-center">
	<div class="flex items-baseline gap-1">
		<span>{initialValue.value ? labels.true : labels.false}</span>
	</div>
	<button
		class="text-blue-500 hover:text-blue-700 flex-shrink-0"
		onclick={() => (isEditing = true)}
	>
		<Pencil />
	</button>
</div>
