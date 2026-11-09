<script lang="ts">
	let { settings } = $props<{ settings: { hyst?: number } | undefined }>();

	function changeSetting() {
		if (settings) settings.hyst = 1;
	}
</script>

<div data-testid="mock-step">Step</div>
<button type="button" data-testid="mock-change" onclick={changeSetting}>change</button>
