<script lang="ts">
	import { page } from '$app/state';
	import { user } from '$lib/stores/user';
	import GithubFirmwareManager from './update/GithubFirmwareManager.svelte';
	import UploadFirmware from './update/UploadFirmware.svelte';
</script>

<div class="system-update-root">
	<div class="settings-container">
		<div class="settings-grid">
			<div class="settings-panel">
				<div class="settings-section">
					{#if page.data.features.download_firmware && (!page.data.features.security || $user.admin)}
						<GithubFirmwareManager />
					{/if}

					{#if page.data.features.upload_firmware && (!page.data.features.security || $user.admin)}
						<UploadFirmware />
					{/if}
				</div>
			</div>
		</div>
	</div>
</div>

<style lang="scss">
	:global(.system-update-root .settings-container) {
		width: 100%;
	}

	:global(.system-update-root .settings-grid) {
		grid-template-columns: 1fr;
	}

	:global(.system-update-root .settings-panel) {
		width: 100%;
	}
</style>

