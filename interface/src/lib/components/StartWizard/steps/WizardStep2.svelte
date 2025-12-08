<script lang="ts">
	import type { SsvcSettings } from '$lib/types/ssvc';
	import NumberInput from '$lib/components/NumberInput.svelte';

	let { settings = $bindable() } = $props<{
		settings: SsvcSettings;
	}>();
</script>

<div class="settings-block">
	<h2 class="settings-title">Пропускная способность клапанов</h2>
	<div class="settings-group">
		<div class="settings-item">
			<label class="input-label" for="valve_bw_head">Головы</label>
			<NumberInput
				bind:value={settings.valve_bw[0]}
				unit="мл/ч"
			/>
		</div>

		<div class="settings-item">
			<label class="input-label" for="valve_bw_hearts">Тело</label>
			<NumberInput
				bind:value={settings.valve_bw[1]}
				unit="мл/ч"
			/>
		</div>

		<div class="settings-item">
			<label class="input-label" for="valve_bw_tails"
			>Подголовники/Хвосты</label
			>
			<NumberInput
				bind:value={settings.valve_bw[2]}
				unit="мл/ч"
			/>
		</div>
	</div>

</div>
