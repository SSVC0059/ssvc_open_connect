<script lang="ts">
	import { modals } from 'svelte-modals';
	import { focusTrap } from 'svelte-focus-trap';
	import { fly } from 'svelte/transition';
	import Cancel from '~icons/tabler/x';
	import AlertTriangle from '~icons/tabler/alert-triangle';
	import CloudDown from '~icons/tabler/cloud-download';

	interface Props {
		isOpen?: boolean;
		title?: string;
		message?: string;
		onConfirm: () => void;
		onCancel?: () => void;
	}

	let {
		isOpen,
		title = 'Экспериментальная сборка',
		message = 'Вы собираетесь установить ночную (nightly) сборку. Она может быть нестабильной и не протестирована. Используйте только если понимаете риски.',
		onConfirm,
		onCancel = () => modals.close()
	}: Props = $props();

	let confirmed = $state(false);

	function handleConfirm() {
		if (confirmed) {
			onConfirm();
		}
	}
</script>

{#if isOpen}
	<div
		role="dialog"
		aria-labelledby="nightly-dialog-title"
		class="pointer-events-none fixed inset-0 z-50 flex items-center justify-center p-4 max-sm:px-3"
		transition:fly={{ y: 50 }}
		use:focusTrap
	>
		<div
			class="rounded-box pointer-events-auto flex max-w-lg flex-col border-2 border-warning bg-base-100 p-5 shadow-2xl max-sm:mx-auto max-sm:min-w-0 max-sm:w-full max-sm:p-4"
		>
			<div class="mb-4 flex items-start gap-3">
				<AlertTriangle class="text-warning h-12 w-12 shrink-0" />
				<div>
					<h2 id="nightly-dialog-title" class="text-warning text-xl font-bold">
						{title}
					</h2>
					<p class="text-base-content mt-2 text-sm">{message}</p>
				</div>
			</div>

			<div class="alert alert-warning mb-4">
				<AlertTriangle class="h-6 w-6 shrink-0" />
				<div>
					<p class="font-semibold">Ночная сборка не предназначена для обычных пользователей.</p>
					<p class="text-sm opacity-90">
						Возможны сбои, потеря данных, несовместимость. Устанавливайте только для тестирования.
					</p>
				</div>
			</div>

			<label class="mb-4 flex cursor-pointer items-center gap-2">
				<input
					type="checkbox"
					class="checkbox checkbox-warning checkbox-sm"
					bind:checked={confirmed}
				/>
				<span class="text-sm font-medium"
					>Я понимаю риски и хочу установить экспериментальную сборку</span
				>
			</label>

			<div class="flex justify-end gap-2 max-sm:flex-col-reverse">
				<button
					class="btn btn-ghost inline-flex min-h-11 items-center justify-center gap-2 max-sm:w-full sm:min-h-0"
					onclick={onCancel}
				>
					<Cancel class="h-5 w-5 shrink-0" />
					<span>Отмена</span>
				</button>
				<button
					class="btn btn-error inline-flex min-h-11 items-center justify-center gap-2 max-sm:w-full sm:min-h-0"
					disabled={!confirmed}
					onclick={handleConfirm}
					title="Подтвердить установку nightly"
				>
					<CloudDown class="h-5 w-5 shrink-0" />
					<span class="truncate">Установить nightly</span>
				</button>
			</div>
		</div>
	</div>
{/if}
