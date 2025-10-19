<script lang="ts">
	import type { SsvcSettings } from '$lib/types/ssvc';
	import EditNumbersModal from '$lib/components/SsvcSettings/EditNumbersModal.svelte';
	import Pencil from '~icons/tabler/pencil';

	const { settings, onSave } = $props<{
		settings: SsvcSettings;
		onSave: (field: string, value: any) => void;
	}>();
</script>

<!-- Добавляем основной контейнер -->
{#if settings}
	<div class="settings-grid">
		<div class="settings-panel">
			<div class="settings-section">
				<div class="settings-item">
					<h3 class="settings-section-title">Настройки пропускной способности клапанов</h3>
					<EditNumbersModal
						onSave={(newValues) =>
					onSave('valve_bw', [newValues[0].value, newValues[1].value, newValues[2].value])}
						showValue={false}
						values={[
					{
						name: 'Головы',
						value: settings.valve_bw[0],
						min: 0,
						max: 20000
					},
					{
						name: 'Тело',
						value: settings.valve_bw[1],
						min: 0,
						max: 20000
					},
					{
						name: 'Подголовники | Хвосты',
						value: settings.valve_bw[2],
						min: 0,
						max: 20000
					}
				]}>
						<button class="edit_button">
							<Pencil class="w-10 h-10" />
						</button>
					</EditNumbersModal>
				</div>


				<div class="settings-item">
					<span class="settings-label">Головы</span>
					<span class="settings-value">{settings.valve_bw[0]} мл/ч</span>
				</div>
				{#if settings.late_heads}
					<div id="late-heads-row" class="settings-item">
						<span class="settings-label">Подголовники</span>
						<span class="settings-value">{settings.valve_bw[2]} мл/ч</span>
					</div>
				{/if}
				<div class="settings-item">
					<span class="settings-label">Тело</span>
					<span class="settings-value">{settings.valve_bw[1]} мл/ч</span>
				</div>
				{#if settings.tails}
					<div id="tails-row" class="settings-item">
						<span class="settings-label">Хвосты</span>
						<span class="settings-value">2500 мл/ч</span>
					</div>
				{/if}
			</div>
		</div>
	</div>
{/if}




<style lang="scss">
	.settings-grid {
    grid-template-columns: 1fr
	}
</style>