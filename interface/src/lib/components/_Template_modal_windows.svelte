<script lang="ts">
	import { modals } from 'svelte-modals';
	import Cancel from '~icons/tabler/x';
	import Check from '~icons/tabler/check';
	import Portal from 'svelte-portal';
	import { focusTrap } from 'svelte-focus-trap';
	import { fly } from 'svelte/transition';

	// Принимаем оба колбэка
	const { onConfirm, onCancel } = $props<{
		onConfirm?: () => void;
		onCancel?: () => void;
	}>();

	let isEditing = $state(false);

	$effect(() => {
		if (!isEditing) return;

		const originalOverflow = document.body.style.overflow;
		document.body.style.overflow = 'hidden';

		return () => {
			document.body.style.overflow = originalOverflow;
		};
	});

	function handleSave() {
		isEditing = false;
		if (onConfirm) {
			onConfirm(); // Вызываем колбэк подтверждения
		} else {
			modals.close(); // Fallback
		}
	}

	// Обработчик отмены - исправлено
	function handleCancel() {
		isEditing = false;
		if (onCancel) {
			onCancel(); // Вызываем колбэк отмены
		} else {
			modals.close(); // Fallback
		}
	}

	// Обработчик клика на оверлей
	function handleOverlayClick(event: MouseEvent) {
		if (event.target === event.currentTarget) {
			handleCancel();
		}
	}

</script>

<Portal>
	<div class="modal-overlay fade-in"></div>
	<div
		role="dialog"
		class="modal-dialog"
		transition:fly={{ y: 50 }}
		use:focusTrap
	>
		<div class="modal-content">
			<h2 class="modal-title">
				Заголовок
			</h2>

			<div class="modal-body">
				Контент модального окна
			</div>
			<div class="modal-divider"></div>
			<div class="modal-actions">
				<button
					class="btn btn-primary"
					onclick={handleSave}
				>
					<Check class="btn-icon" />
					<span>Сохранить</span>
				</button>
				<button
					class="btn btn-warning"
					onclick={handleCancel}
				>
					<Cancel class="btn-icon" />
					<span>Отменить</span>
				</button>
			</div>

		</div>
	</div>

</Portal>
