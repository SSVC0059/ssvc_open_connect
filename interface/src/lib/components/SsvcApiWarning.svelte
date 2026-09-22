<script lang="ts">
	import { onMount } from 'svelte';
	import { getInfo } from '$lib/api/ssvcApi';
	import { apiVersionCode } from '$lib/actions/versioning';
	import type { SsvcOpenConnectInfo } from '$lib/types/ssvc';
	import AlertTriangle from '~icons/tabler/alert-triangle';
	import PlugOff from '~icons/tabler/plug-off';

	// Страница загрузки прошивки контроллера SSVC0059.
	const FIRMWARE_URL = 'https://smartmodule.ru/portfolio/0059_v2/';

	let info = $state<SsvcOpenConnectInfo | null>(null);
	let loaded = $state(false);

	onMount(() => {
		getInfo()
			.then((value) => {
				info = value ?? null;
			})
			.catch(() => {
				info = null;
			})
			.finally(() => {
				loaded = true;
			});
	});

	const compatibility = $derived(info?.oc.api_compatibility ?? 'unknown');
	const deviceApi = $derived(info?.ssvc.api ?? '');
	const requiredApi = $derived(info?.oc.api_min ?? '1.9');
	const targetApi = $derived(info?.oc.api_target ?? '');

	// unknown — контроллер не ответил или прошивка настолько старая, что не отдаёт
	// поле api: это тоже повод показать предупреждение, а не молчать.
	const isTooOld = $derived(compatibility === 'too_old');
	const isUnknown = $derived(compatibility === 'unknown');

	// Устройство совместимо, но отстаёт от целевой версии API: openConnect умеет
	// больше, чем даёт текущая прошивка контроллера. Сравнение идёт по целому
	// коду версии, иначе "1.10" оказалось бы меньше "1.7".
	const isOutdated = $derived(
		loaded &&
			!isTooOld &&
			!isUnknown &&
			apiVersionCode(deviceApi) > 0 &&
			apiVersionCode(targetApi) > 0 &&
			apiVersionCode(deviceApi) < apiVersionCode(targetApi)
	);

	const visible = $derived(loaded && (isTooOld || isUnknown || isOutdated));

	const title = $derived(
		isTooOld
			? `Требуется обновление контроллера SSVC: устройство ${deviceApi || 'неизвестной версии'}, требуется API ${requiredApi}`
			: isOutdated
				? `Требуется обновление контроллера SSVC: устройство ${deviceApi}, доступно до API ${targetApi}`
				: 'Нет ответа от контроллера SSVC: соединение не установлено либо прошивка не поддерживает определение версии API'
	);
</script>

{#if visible}
	<a
		class="ssvc-api-warning"
		class:is-error={isTooOld}
		class:is-outdated={isOutdated}
		href={FIRMWARE_URL}
		target="_blank"
		rel="noopener noreferrer"
		{title}
		aria-label={title}
	>
		{#if isUnknown}
			<PlugOff class="h-5 w-5 flex-none" />
		{:else}
			<AlertTriangle class="h-5 w-5 flex-none" />
		{/if}
		<span class="ssvc-api-warning__text">
			{#if isUnknown}
				Нет связи с контроллером SSVC
			{:else if isTooOld}
				Требуется обновление контроллера SSVC
				<span class="ssvc-api-warning__versions">
					(устройство {deviceApi || '?'} → требуется API {requiredApi})
				</span>
			{:else}
				Требуется обновление контроллера SSVC
				<span class="ssvc-api-warning__versions">
					(устройство {deviceApi} → доступно до API {targetApi})
				</span>
			{/if}
		</span>
	</a>
{/if}

<style lang="scss">
	.ssvc-api-warning {
		display: inline-flex;
		align-items: center;
		gap: 0.35rem;
		padding: 0.15rem 0.5rem;
		border-radius: var(--border-radius, 0.5rem);
		background: rgba(255, 193, 7, 0.18);
		border: 1px solid rgba(255, 193, 7, 0.55);
		color: inherit;
		font-size: 0.7rem;
		font-weight: 600;
		line-height: 1.1;
		text-decoration: none;
		white-space: nowrap;
		transition: background 0.15s ease;

		&:hover {
			background: rgba(255, 193, 7, 0.3);
		}

		&.is-error {
			background: rgba(198, 40, 40, 0.16);
			border-color: rgba(198, 40, 40, 0.55);

			&:hover {
				background: rgba(198, 40, 40, 0.28);
			}
		}

		// Устройство совместимо, но отстаёт от целевой версии API: доступен не весь
		// функционал, поэтому предупреждение мягче, чем при несовместимости.
		&.is-outdated {
			background: rgba(255, 152, 0, 0.18);
			border-color: rgba(255, 152, 0, 0.6);

			&:hover {
				background: rgba(255, 152, 0, 0.3);
			}
		}

		&__text {
			display: inline-flex;
			align-items: baseline;
			gap: 0.25rem;
		}

		&__versions {
			opacity: 0.8;
			font-weight: 500;
		}

		// На узких экранах оставляем только иконку — статусбар и без того плотный.
		@media (max-width: 1023px) {
			&__text {
				display: none;
			}
		}
	}
</style>
