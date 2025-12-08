<script lang="ts">
	import EditNumbersModal from '$lib/components/SsvcSettings/EditNumbersModal.svelte';
	import type { SsvcSettings } from '$lib/types/ssvc';
	import Pencil from '~icons/tabler/pencil';
	import EditParallelV3 from '$lib/components/SsvcSettings/EditParallelV3.svelte';

	const { settings, onSave } = $props<{
		settings: SsvcSettings;
		onSave: (field: string, value: any) => void;
	}>();

	type ParallelPoint = [number, number, number];
</script>

{#if settings}
	<div class="settings-grid">
		<div class="settings-panel">
			{#if settings.parallel_v1}
				<div class="settings-section">
					<div class="settings-item">
						<h3 class="settings-section-title">Скорость параллельного отбора клапаном №1</h3>
						<EditNumbersModal
							onSave={(newValues) => onSave('parallel_v1', [newValues[0].value, newValues[1].value])}
							showValue={false}
							values={[
                                {
                                    name: 'Время открытия клапана',
                                    value: settings.parallel_v1[0],
                                    step: 0.1,
                                    precision: 1,
                                    min: 0,
                                    max: 10
                                },
                                {
                                    name: 'Периодичность',
                                    value: settings.parallel_v1[1],
                                    min: 0,
                                    max: 100
                                }
                            ]}>
							<button class="edit-button">
								<Pencil class="w-10 h-10" />
							</button>
						</EditNumbersModal>
					</div>

					<div class="settings-item">
						<span class="input-label">Время открытия клапана</span>
						<span class="settings-value">{settings.parallel_v1[0]} Сек</span>
					</div>
					<div class="settings-item">
						<span class="input-label">Периодичность</span>
						<span class="settings-value">{settings.parallel_v1[1]} Сек</span>
					</div>
				</div>
			{/if}

			{#if settings.parallel_v3}
				<div class="settings-section">
					<div class="settings-item">
						<h3 class="settings-section-title">Скорость параллельного отбора клапаном №3</h3>
						<EditParallelV3
							points={settings.parallel_v3}
							onSave={(newPoints: ParallelPoint[]) => onSave('parallel_v3', newPoints)}
						/>
					</div>

					<!-- Компактное отображение точек -->
					<div class="compact-points">
						<div class="points-header">
							<span class="point-label">Температура</span>
							<span class="point-label">Время открытия клапана</span>
							<span class="point-label">Периодичность</span>
						</div>
						{#each settings.parallel_v3 as point, index}
							<div class="point-row">
								<span class="point-value">{point[0]} °С</span>
								<span class="point-value">{point[1]} Сек</span>
								<span class="point-value">{point[2]} Сек</span>
							</div>
						{/each}
					</div>
				</div>
			{/if}
			{#if settings.parallel}
				<div class="settings-section">
					<div class="settings-item">
						<h3 class="settings-section-title">Скорость параллельного отбора клапаном №3</h3>
						<EditNumbersModal
							onSave={(newValues) => onSave('parallel', [newValues[0].value, newValues[1].value])}
							showValue={false}
							values={[
                                {
                                    name: 'Время открытия клапана',
                                    value: settings.parallel[0],
                                    step: 0.1,
                                    precision: 1,
                                    min: 0,
                                    max: 10
                                },
                                {
                                    name: 'Периодичность',
                                    value: settings.parallel[1],
                                    min: 0,
                                    max: 100
                                }
                            ]}>
							<button class="edit_button">
								<Pencil class="w-10 h-10" />
							</button>
						</EditNumbersModal>
					</div>

					<div class="settings-item">
						<span class="input-label">Время открытия клапана</span>
						<span class="settings-value">{settings.parallel_v1[0]} Сек</span>
					</div>
					<div class="settings-item">
						<span class="input-label">Периодичность</span>
						<span class="settings-value">{settings.parallel_v1[1]} Сек</span>
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