<script lang="ts">
	import StaSettings from './StaSettings.svelte';
	import ApSettings from './ApSettings.svelte';
</script>

<div class="wifi-page mx-0 my-1 sm:mx-8 sm:my-8">
	<div class="wifi-grid">
		<div class="wifi-column">
			<h2 class="wifi-column-title">WiFi Status</h2>
			<StaSettings />
		</div>
		<div class="wifi-column">
			<h2 class="wifi-column-title">Access Point</h2>
			<ApSettings />
		</div>
	</div>
</div>

<style lang="scss">
	@use "$lib/styles/base/variables" as v;

	.wifi-column-title {
		font-size: 1.25rem;
		font-weight: 700;
		margin: 0 0 0.75rem 0.25rem;
	}

	.wifi-grid {
		display: grid;
		grid-template-columns: 1fr;
		gap: 1rem;
	}

	@media (min-width: v.$breakpoint-lg) {
		.wifi-grid {
			grid-template-columns: 1fr 1fr;
		}
	}
</style>

