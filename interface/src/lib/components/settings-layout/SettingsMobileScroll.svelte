<script lang="ts">
	import type { Component } from 'svelte';
	import Reload from '~icons/tabler/reload';

	type Item = {
		id: string;
		title: string;
		component: Component;
		props?: Record<string, unknown>;
	};

	let {
		items,
		sectionIdPrefix,
		tabFromUrl,
		pageTitle,
		onRefresh,
		denseGrid = false,
		/** Карточка вокруг тела секции (как update-card на system/update) */
		sectionBodyCard = false
	}: {
		items: Item[];
		sectionIdPrefix: string;
		tabFromUrl: string | null;
		pageTitle?: string;
		onRefresh?: () => void;
		denseGrid?: boolean;
		sectionBodyCard?: boolean;
	} = $props();
</script>

<div
	class="settings-page-mobile"
	class:settings-page-mobile--dense-grid={denseGrid}
	class:settings-page-mobile--section-cards={sectionBodyCard}
	data-testid="settings-mobile-scroll"
>
	{#if pageTitle}
		<header class="settings-page-mobile__header">
			<h1 class="text-center text-2xl font-bold text-base-content">{pageTitle}</h1>
		</header>
	{/if}

	<div class="settings-page-mobile__scroll">
		{#snippet sections()}
			{#each items as item (item.id)}
				{@const C = item.component}
				<section
					id="{sectionIdPrefix}-section-{item.id}"
					class="settings-page-mobile__section"
					class:settings-page-mobile__section--target={tabFromUrl === item.id}
					aria-labelledby="{sectionIdPrefix}-heading-{item.id}"
				>
					<h2 id="{sectionIdPrefix}-heading-{item.id}" class="settings-page-mobile__section-title">
						{item.title}
					</h2>
					<div class="settings-page-mobile__section-body">
						{#if sectionBodyCard}
							<div
								class="settings-stack-card rounded-box border border-base-content/10 bg-base-100 p-2"
							>
								<C {...(item.props ?? {})} />
							</div>
						{:else}
							<C {...(item.props ?? {})} />
						{/if}
					</div>
				</section>
			{/each}
		{/snippet}
		{#if sectionBodyCard}
			<div class="settings-page-mobile__scroll-inner">
				{@render sections()}
			</div>
		{:else}
			{@render sections()}
		{/if}
	</div>

	{#if onRefresh}
		<div class="settings-page-mobile__footer">
			<div class="flex justify-center">
				<button
					type="button"
					class="btn btn-ssvc-refresh-mobile gap-2"
					title="Перезагрузить настройки"
					onclick={onRefresh}
				>
					<Reload class="h-5 w-5" />
					<span>Обновить</span>
				</button>
			</div>
		</div>
	{/if}
</div>

<style lang="scss">
	@use '$lib/styles/components/settings-page-mobile';

	.btn-ssvc-refresh-mobile {
		background-color: var(--color-info);
		color: oklch(1 0 0);
		border: none;
	}

	.btn-ssvc-refresh-mobile:hover {
		opacity: 0.9;
	}
</style>
