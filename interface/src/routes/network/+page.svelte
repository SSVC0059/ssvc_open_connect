<script lang="ts">
	import MqttSettings from './MqttSettings.svelte';
	import NtpSettings from './NtpSettings.svelte';
</script>

<div class="network-page mx-0 my-1 sm:mx-8 sm:my-8">
	<div class="network-grid">
		<div class="network-column">
			<h2 class="network-column-title">MQTT</h2>
			<MqttSettings />
		</div>
		<div class="network-column">
			<h2 class="network-column-title">NTP</h2>
			<NtpSettings />
		</div>
	</div>
</div>

<style lang="scss">
	.network-column-title {
		font-size: 1.25rem;
		font-weight: 700;
		margin: 0 0 0.75rem 0.25rem;
	}

	.network-grid {
		display: grid;
		grid-template-columns: 1fr;
		gap: 1rem;
	}

	@media (min-width: 1024px) {
		.network-grid {
			grid-template-columns: 1fr 1fr;
		}
	}
</style>

