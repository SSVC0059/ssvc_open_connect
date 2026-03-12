<script lang="ts">
	import { onMount } from 'svelte';
	import { page } from '$app/state';
	import { user } from '$lib/stores/user';
	import CoreDump from '~icons/tabler/bug';
	import Info from '~icons/tabler/info-circle';

	let coreDumpBlob: Blob | null = $state(null);
	let errorMessage: string | null = $state(null);

	onMount(async () => {
		try {
			const response = await fetch('/rest/coreDump', {
				method: 'GET',
				headers: {
					Authorization: page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/octet-stream'
				}
			});
			if (response.ok) {
				coreDumpBlob = await response.blob();
			} else {
				errorMessage = 'No core dump available.';
			}
		} catch (e) {
			errorMessage = 'No core dump available.';
		}
	});

	function downloadCoreDump() {
		if (coreDumpBlob) {
			const url = URL.createObjectURL(coreDumpBlob);
			const a = document.createElement('a');
			a.href = url;
			a.download = 'coredump.bin';
			document.body.appendChild(a);
			a.click();
			document.body.removeChild(a);
			URL.revokeObjectURL(url);
		}
	}
</script>

<div class="system-coredump-root">
	<div class="settings-container">
		<div class="settings-grid">
			<div class="settings-panel">
				<div class="settings-section">
					<div class="settings-section-title">Core dump</div>

					<div class="alert alert-info shadow-lg">
						<Info class="h-6 w-6 shrink-0" />
						<span
							>На этой вкладке отображается последний дамп памяти устройства.
							Дамп памяти — это моментальный снимок памяти в момент сбоя устройства.
							Эта информация полезна для отладки.</span
						>
					</div>

					{#if coreDumpBlob}
						<button class="btn btn-primary mt-4" onclick={downloadCoreDump}>
							<CoreDump class="h-5 w-5" />
							<span>Скачать дамп ядра (coredump.bin)</span>
						</button>
					{:else if errorMessage}
						<p class="text-error mt-4">{errorMessage}</p>
					{:else}
						<p class="mt-4">Загрузка дампа ядра...</p>
					{/if}
				</div>
			</div>
		</div>
	</div>
</div>

<style lang="scss">
	:global(.system-coredump-root .settings-container) {
		width: 100%;
	}

	:global(.system-coredump-root .settings-grid) {
		grid-template-columns: 1fr;
	}

	:global(.system-coredump-root .settings-panel) {
		width: 100%;
	}
</style>

