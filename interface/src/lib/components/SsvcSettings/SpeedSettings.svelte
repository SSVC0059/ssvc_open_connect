<script lang="ts">
import EditTimeModal from '$lib/components/SsvcSettings/EditTimeModal.svelte';
import EditNumbersModal from '$lib/components/SsvcSettings/EditNumbersModal.svelte';

import type { SsvcSettings } from '$lib/types/ssvc';
import Pencil from '~icons/tabler/pencil';

const { settings, onSave } = $props<{
	settings: SsvcSettings;
	onSave: (field: string, value: any) => void;
}>();

</script>

{#if settings}
	<div class="settings-grid">
		<div class="settings-panel">
			<div class="settings-section">
				<div class="settings-item">
					<h3 class="settings-section-title">Параметры отбора голов</h3>
					<EditNumbersModal
						onSave={(newValues) =>
				onSave('heads', [newValues[0].value, newValues[1].value])}
						showValue={false}
						values={[
								{
									name: 'Время открытия клапана',
									value: settings.heads[0],
									step: 0.1,
									precision: 1,
									min: 0,
									max: 99.9
								},
								{
									name: 'Период',
									value: settings.heads[1],
									min: 0,
									max: 999
								}
							]}>
						<button class="edit_button">
							<Pencil class="w-10 h-10" />
						</button>
					</EditNumbersModal>
				</div>

				<div class="settings-item">
					<span class="settings-label">Время открытия клапана</span>
					<span class="settings-value">{settings.heads[0]} Сек</span>
				</div>
				<div class="settings-item">
					<span class="settings-label">Периодичность</span>
					<span class="settings-value">{settings.heads[1]} Сек</span>
				</div>
			</div>

			{#if settings.late_heads}
				<div class="settings-section">
					<div class="settings-item">
						<h3 class="settings-section-title">Параметры отбора подголовников</h3>
						<EditNumbersModal
							onSave={(newValues) =>
							onSave('heads', [newValues[0].value, newValues[1].value])}
							showValue={false}
							values={[
								{
									name: 'Время открытия клапана',
									value: settings.late_heads[0],
									step: 0.1,
									precision: 1,
									min: 0,
									max: 99.9
								},
								{
									name: 'Период',
									value: settings.late_heads[1],
									min: 0,
									max: 999
								}
							]}>
							<button class="edit_button">
								<Pencil class="w-10 h-10" />
							</button>
						</EditNumbersModal>
					</div>
					<div class="settings-item">
						<span class="settings-label">Время открытия клапана</span>
						<span class="settings-value">{settings.late_heads[0]} Сек</span>
					</div>
					<div class="settings-item">
						<span class="settings-label">Периодичность</span>
						<span class="settings-value">{settings.late_heads[1]} Сек</span>
					</div>
				</div>
			{/if}
			<div class="settings-section">
				<div class="settings-item">
					<h3 class="settings-section-title">Параметры отбора тела</h3>
					<EditNumbersModal
						onSave={(newValues) =>
							onSave('heads', [newValues[0].value, newValues[1].value])}
						showValue={false}
						values={[
								{
									name: 'Время открытия клапана',
									value: settings.hearts[0],
									step: 0.1,
									precision: 1,
									min: 0,
									max: 99.9
								},
								{
									name: 'Период',
									value: settings.hearts[1],
									min: 0,
									max: 999
								}
							]}>
						<button class="edit_button">
							<Pencil class="w-10 h-10" />
						</button>
					</EditNumbersModal>
				</div>
				<div class="settings-item">
					<span class="settings-label">Время открытия клапана</span>
					<span class="settings-value">{settings.hearts[0]} Сек</span>
				</div>
				<div class="settings-item">
					<span class="settings-label">Периодичность</span>
					<span class="settings-value">{settings.hearts[1]} Сек</span>
				</div>
			</div>
			{#if settings.tails}
				<div class="settings-section">
					<div class="settings-item">
						<h3 class="settings-section-title">Параметры отбора хвостов</h3>
						<EditNumbersModal
							onSave={(newValues) =>
							onSave('heads', [newValues[0].value, newValues[1].value])}
							showValue={false}
							values={[
								{
									name: 'Время открытия клапана',
									value: settings.tails[0],
									step: 0.1,
									precision: 1,
									min: 0,
									max: 99.9
								},
								{
									name: 'Период',
									value: settings.tails[1],
									min: 0,
									max: 999
								}
							]}>
							<button class="edit_button">
								<Pencil class="w-10 h-10" />
							</button>
						</EditNumbersModal>
					</div>
					<div class="settings-item">
						<span class="settings-label">Время открытия клапана</span>
						<span class="settings-value">{settings.tails[0]} Сек</span>
					</div>
					<div class="settings-item">
						<span class="settings-label">Периодичность</span>
						<span class="settings-value">{settings.tails[1]} Сек</span>
					</div>
				</div>
			{/if}
		</div>
	</div>
{/if}

<style lang="scss">
  .settings-grid {
    grid-template-columns: 1fr
  }
</style>
