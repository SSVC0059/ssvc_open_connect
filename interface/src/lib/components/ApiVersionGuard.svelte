<script lang="ts">
	import { getInfo, infoCacheVersion } from '$lib/api/ssvcApi';
	import { apiVersionCode } from '$lib/actions/versioning';

	let {
		requiredVersion,
		message = 'Этот функционал не поддерживается вашей версией API.',
	} = $props<{
		// Версию с двузначным minor передавайте строкой: число 1.10 в JS равно 1.1.
		requiredVersion: number | string;
		message?: string;
	}>();

	let isLoading = $state(true);
	let currentVersion = $state<string | undefined>(undefined);

	const isSupported = $derived(
		currentVersion !== undefined &&
			apiVersionCode(currentVersion) >= apiVersionCode(requiredVersion)
	);

	$effect(() => {
		// Перечитываем версию API при сбросе кеша (переподключение к контроллеру):
		// иначе уже смонтированный гейт останется с версией прошлого подключения.
		void $infoCacheVersion;
		let isMounted = true;
		const checkVersion = async () => {
			isLoading = true;
			try {
				const info = await getInfo();
				if (isMounted && info?.ssvc?.api) {
					currentVersion = info.ssvc.api;
				}
			} catch (error) {
				console.error('Failed to check version:', error);
			} finally {
				if (isMounted) {
					isLoading = false;
				}
			}
		};

		void checkVersion();

		return () => {
			isMounted = false;
		};
	});
</script>

<div class="version-guard">
	{#if isLoading}
		<p>Проверка версии API...</p>
	{:else if !isSupported}
		<div class="unsupported-banner">
			<p>{message}</p>
			<p class="version-info">
				(Требуется версия API: {requiredVersion}, текущая: {currentVersion ?? 'недоступна'})
			</p>
		</div>
	{/if}
</div>

<style>

		.unsupported-banner {
			padding: 0.75rem 1rem;
			margin-bottom: 1rem;
			border: 1px solid var(--yellow-300);
			background-color: var(--yellow-100);
			color: var(--yellow-800);
			border-radius: 0.5rem;
			text-align: center;
			font-size: 0.9rem;
			font-weight: 500;
		}

	.unsupported-banner p {
		margin: 0;
	}

	.version-info {
		font-size: 0.8rem;
		opacity: 0.8;
		margin-top: 0.25rem;
	}
</style>
