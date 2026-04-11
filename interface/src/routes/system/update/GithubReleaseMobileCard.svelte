<script lang="ts">
	import CloudDown from '~icons/tabler/cloud-download';
	import FileUpload from '~icons/tabler/file-upload';
	import Prerelease from '~icons/tabler/test-pipe';

	type Release = {
		name: string;
		html_url: string;
		prerelease: boolean;
		published_at: string;
		assets: unknown[];
	};

	let {
		release,
		isCurrent,
		canInstallOnDevice,
		onInstallDevice,
		onDownloadBrowser,
		embedded = false
	}: {
		release: Release;
		isCurrent: boolean;
		canInstallOnDevice: boolean;
		onInstallDevice: () => void;
		onDownloadBrowser: () => void;
		/** Внутри общей мобильной рамки: без отдельной обводки каждого релиза */
		embedded?: boolean;
	} = $props();
</script>

<div
	class="{embedded
		? `px-2 py-3 ${isCurrent ? 'bg-primary text-primary-content' : 'bg-transparent'}`
		: `rounded-box border border-base-content/10 bg-base-100 p-3 ${isCurrent ? 'border-primary bg-primary text-primary-content' : ''}`}"
>
	<div class="flex min-w-0 items-start justify-between gap-2">
		<a
			href={release.html_url}
			class="link link-hover min-w-0 truncate text-sm font-semibold leading-tight {isCurrent
				? 'link-primary-content'
				: ''}"
			target="_blank"
			rel="noopener noreferrer"
		>
			{release.name}
		</a>
		{#if release.prerelease}
			<Prerelease class="h-4 w-4 shrink-0 text-accent {isCurrent ? 'text-primary-content' : ''}" />
		{/if}
	</div>
	<p class="text-base-content/70 mt-0.5 text-xs {isCurrent ? 'text-primary-content/80' : ''}">
		{new Intl.DateTimeFormat('en-GB', { dateStyle: 'medium' }).format(new Date(release.published_at))}
	</p>

	<!-- Обёртки flex-1 не .btn: в +layout для .btn задано flex:none, из‑за этого две кнопки в ряду схлопывались -->
	<div class="github-release-mobile-actions mt-2 flex w-full min-w-0 gap-2">
		{#if canInstallOnDevice}
			<div class="min-w-0 flex-1 basis-0">
				<button
					type="button"
					class="btn btn-github-install-device btn-sm flex h-11 min-h-11 w-full items-center justify-center px-0"
					title="На устройство: контроллер скачает прошивку с GitHub (нужен интернет на ESP)"
					aria-label="Установить на устройство"
					onclick={onInstallDevice}
				>
					<CloudDown class="h-5 w-5 shrink-0" />
				</button>
			</div>
		{:else}
			<div
				class="flex h-11 min-h-11 min-w-0 flex-1 basis-0 items-center justify-center rounded-btn border border-base-content/20 bg-base-200/70 text-center text-xs font-medium text-base-content {isCurrent
					? 'border-primary-content/40 bg-primary-content/15 text-primary-content'
					: ''}"
			>
				Текущая
			</div>
		{/if}
		<div class="min-w-0 flex-1 basis-0">
			<button
				type="button"
				class="btn btn-github-download-file btn-sm flex h-11 min-h-11 w-full items-center justify-center px-0 {isCurrent
					? 'is-on-primary-card'
					: ''}"
				title="Скачать .bin на компьютер, затем раздел «Загрузка обновления»"
				aria-label="Скачать релиз"
				onclick={onDownloadBrowser}
			>
				<FileUpload class="h-5 w-5 shrink-0" />
			</button>
		</div>
	</div>
</div>

<style lang="scss">
	/*
	 * +layout: :global(.btn) { background: none; border: none } перебивает DaisyUI — задаём обе кнопки явно,
	 * одинаковая высота/форма, пара «заливка + контур» в одной палитре.
	 */
	:global(.github-release-mobile-actions .btn.btn-github-install-device) {
		background: var(--blue-500);
		color: var(--white);
		border: 2px solid var(--blue-600);
		box-sizing: border-box;
	}

	:global(.github-release-mobile-actions .btn.btn-github-install-device:hover:not(:disabled)) {
		background: var(--blue-600);
		border-color: var(--blue-600);
	}

	:global(.github-release-mobile-actions .btn.btn-github-download-file) {
		background: oklch(0.99 0.01 260);
		color: var(--blue-600);
		border: 2px solid var(--blue-500);
		box-sizing: border-box;
	}

	:global(.github-release-mobile-actions .btn.btn-github-download-file:hover:not(:disabled)) {
		background: oklch(0.95 0.03 260);
		border-color: var(--blue-600);
	}

	/* Карточка текущей версии: видна только «Скачать» рядом с «Текущая» */
	:global(.github-release-mobile-actions .btn.btn-github-download-file.is-on-primary-card) {
		background: oklch(1 0 0 / 0.12);
		color: oklch(1 0 0);
		border: 2px solid oklch(1 0 0 / 0.55);
	}

	:global(
		.github-release-mobile-actions .btn.btn-github-download-file.is-on-primary-card:hover:not(:disabled)
	) {
		background: oklch(1 0 0 / 0.22);
		border-color: oklch(1 0 0 / 0.7);
	}
</style>
