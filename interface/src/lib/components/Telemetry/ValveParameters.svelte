<script lang="ts">
	import { Popover } from 'flowbite-svelte';
	import { slide } from 'svelte/transition';

	interface Props {
		n: string;
		vo?: number;
		o?: number;
		p?: number;
		s?: number;
		f?: number;
		active?: boolean;
	}

	let { n, vo = 0, o = 0, p = 0, s = 0, f = 0, active = false }: Props = $props();

	// вычисляем статус (runes mode)
	const status = $derived(active ? "active" : f > 0 ? "done" : "not-started");
</script>

{#if status !== "not-started"}
			<div class="valve-content">
				<div class="valve-title">
					<p class="valve-name">{n}</p>
				</div>

				{#if status === "done"}
					<p class="status-completed">Этап завершен</p>
				{:else if status === "active"}
					<div class="active-stage">
						<span class="valve-value">
							{#if o}
								{o.toFixed(2)} / {p}
							{:else}
								Закрыт
							{/if}
						</span>

						<Popover
							title="Параметры клапана"
							transition={slide}
							placement="left"
						>
							<div class="parameter-row">
								<p>Время открытия:</p>
								<p class="parameter-value">{o} сек</p>
							</div>
							<div class="parameter-row">
								<p>Периодичность:</p>
								<p class="parameter-value">{p} сек</p>
							</div>
							<div class="parameter-row">
								<p>Cкорость отбора:</p>
								<p class="parameter-value">{s} мл/ч</p>
							</div>
							<div class="parameter-row">
								<p>Пропускная способность:</p>
								<p class="parameter-value">{vo / 100}%</p>
							</div>
						</Popover>
					</div>
				{/if}

				<div class="collected-info">
					<p>Отобрано:</p>
					<p class="collected-value">{f ? f : '-'} мл</p>
				</div>
			</div>
{/if}

<style lang="scss">

  @use "$lib/styles/base/mixins" as *;

  .valve-content {
      display: flex;
      flex-direction: column;
      align-items: center;
      padding: 0.5rem;
      font-family: ui-sans-serif, system-ui, sans-serif;


    .valve-title {
      text-align: center;
      font-size: 2rem;
    }

    .valve-name {
      color: var(--primary-700);
      @include dark-theme-color;
    }

    .status-completed {
      margin-top: 0.25rem;
      font-weight: 600;
      color: #10b981;
      text-align: center;
    }

    .active-stage {
      display: flex;
      align-items: center;
      gap: 0.5rem;
      margin-top: 0.5rem;
      font-size: 1rem;

      @media (min-width: 1024px) {
        font-size: 1.25rem;
      }
    }

    .valve-value {
      background-color: var(--blue-500);
      color: var(--white);
      padding: 0.25rem 0.75rem;
      border-radius: 9999px;
      box-shadow: 0 1px 3px rgb(0 0 0 / 0.1);
    }

    .collected-info {
      display: flex;
      gap: 0.5rem;
      margin-top: 0.5rem;
      @include dark-theme-color;

    }

    .collected-value {
      font-weight: 700;
      color: var(--green-500);
    }

    .parameter-row {
      display: flex;
      justify-content: space-between;
      margin-bottom: 0.5rem;
      font-size: 0.875rem;
      font-weight: 300;
      color: var(--primary-500);

      &:last-child {
        margin-bottom: 0;
      }
    }

    .parameter-value {
      font-weight: 600;
    }
  }

</style>