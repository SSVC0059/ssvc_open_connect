<script lang="ts">
	import type { SsvcSettings } from '$lib/types/models';

	let { settings = $bindable() } = $props<{
		settings: SsvcSettings;
	}>();
</script>

<div class="settings-block">
	<h2 class="settings-title">Пропускная способность клапанов</h2>
	<div class="settings-group">
		<div class="settings-item">
			<label class="settings-label" for="valve_bw_head">Головы</label>
			<input
				bind:value={settings.valve_bw[0]}
				class="settings-input"
				id="valve_bw_head"
				type="number"
			/>
		</div>

		<div class="settings-item">
			<label class="settings-label" for="valve_bw_hearts">Тело</label>
			<input
				bind:value={settings.valve_bw[1]}
				class="settings-input"
				id="valve_bw_hearts"
				type="number"
			/>
		</div>

		<div class="settings-item">
			<label class="settings-label" for="valve_bw_tails"
			>Подголовники/Хвосты</label
			>
			<input
				bind:value={settings.valve_bw[2]}
				class="settings-input"
				id="valve_bw_tails"
				type="number"
			/>
		</div>
	</div>

</div>
