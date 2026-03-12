<script lang="ts">
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

	const status = $derived(active ? "active" : f > 0 ? "done" : "not-started");

	const tooltipText = $derived(
		`Параметры клапана\nВремя открытия: ${o} сек\nПериодичность: ${p} сек\nСкорость отбора: ${s} мл/ч\nПропускная способность: ${vo / 100}%`
	);
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
						<div class="tooltip tooltip-left whitespace-pre-line" data-tip={tooltipText}>
							<span class="valve-value cursor-help">
								{#if o}
									{o.toFixed(2)} / {p}
								{:else}
									Закрыт
								{/if}
							</span>
						</div>
					</div>
				{/if}

				<div class="collected-info">
					<p>Отобрано:</p>
					<p class="collected-value">{f ? f : '-'} мл</p>
				</div>
			</div>
{/if}

<style lang="scss">
  @use "$lib/styles/base/variables" as v;
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
      color: var(--green-500);
      text-align: center;
    }

    .active-stage {
      display: flex;
      align-items: center;
      gap: 0.5rem;
      margin-top: 0.5rem;
      font-size: 1rem;

      @media (min-width: v.$breakpoint-lg) {
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
  }

</style>