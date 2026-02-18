<script lang="ts">
	// Визуальный индикатор загрузки (используется в SensorsSettings, TelegramSettings, StartWizard)
</script>

<span class="loading-spinner" aria-hidden="true"></span>

<style>
	@keyframes spin {
		to {
			transform: rotate(360deg);
		}
	}

	.loading-spinner {
		width: 2rem;
		height: 2rem;
		border: 0.25rem solid var(--primary-200);
		border-top: 0.25rem solid var(--blue-500);
		border-radius: 50%;
		animation: spin 1s linear infinite;
		display: inline-block;
		vertical-align: middle;
	}
</style>
