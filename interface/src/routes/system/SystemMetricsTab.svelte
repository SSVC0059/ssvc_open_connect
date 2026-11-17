<script lang="ts">
	import { page } from '$app/state';
	import { onMount } from 'svelte';
	import { get } from 'svelte/store';
	import type { Chart as ChartInstance } from 'chart.js';
	import { daisyColor } from '$lib/DaisyUiHelper';
	import { analytics } from '$lib/stores/analytics';
	import BatteryMetrics from './metrics/BatteryMetrics.svelte';
	import { slide } from 'svelte/transition';
	import { cubicOut } from 'svelte/easing';
	import { loadChartJs } from '$lib/charts';

	// chart.js грузится лениво: конструктор присваивается в onMount,
	// chartReady перезапускает эффекты, которые ждали библиотеку.
	let Chart: typeof ChartInstance;
	let chartReady = $state(false);

	const chartLayout = {
		padding: { top: 2, right: 6, bottom: 0, left: 2 }
	};

	let heapChartElement: HTMLCanvasElement = $state()!;
	let heapChart: ChartInstance;

	let psramChartElement: HTMLCanvasElement = $state()!;
	let psramChart: ChartInstance | undefined;

	let filesystemChartElement: HTMLCanvasElement = $state()!;
	let filesystemChart: ChartInstance;

	let temperatureChartElement: HTMLCanvasElement = $state()!;
	let temperatureChart: ChartInstance;

	let hasPsramData = $derived.by(() => {
		const s = $analytics.psram_size;
		return s.length > 0 && Math.max(...s) > 0;
	});

	function tryInitPsramChart() {
		const data = get(analytics);
		const sizes = data.psram_size;
		const has = sizes.length > 0 && Math.max(...sizes) > 0;
		if (!has || !psramChartElement || psramChart) return;
		psramChart = new Chart(psramChartElement, {
			type: 'line',
			data: {
				labels: data.uptime,
				datasets: [
					{
						label: 'Used',
						borderColor: daisyColor('--color-primary'),
						backgroundColor: daisyColor('--color-primary', 50),
						borderWidth: 2,
						data: data.used_psram,
						yAxisID: 'y'
					}
				]
			},
			options: {
				maintainAspectRatio: false,
				responsive: true,
				layout: chartLayout,
				plugins: {
					legend: { display: true },
					tooltip: {
						mode: 'index',
						intersect: false
					}
				},
				elements: {
					point: { radius: 1 }
				},
				scales: {
					x: {
						grid: { color: daisyColor('--color-base-content', 10) },
						ticks: { color: daisyColor('--color-base-content') },
						display: false
					},
					y: {
						type: 'linear',
						title: {
							display: true,
							text: 'PSRAM [KB]',
							color: daisyColor('--color-base-content'),
							font: { size: 16, weight: 'bold' }
						},
						position: 'left',
						min: 0,
						max: Math.round(Math.max(...data.psram_size, 1)),
						grid: { color: daisyColor('--color-base-content', 10) },
						ticks: { color: daisyColor('--color-base-content') },
						border: { color: daisyColor('--color-base-content', 10) }
					}
				}
			}
		});
	}

	onMount(() => {
		void initCharts();

		const intervalId = setInterval(updateData, 2000);
		return () => {
			clearInterval(intervalId);
			heapChart?.destroy();
			psramChart?.destroy();
			filesystemChart?.destroy();
			temperatureChart?.destroy();
		};
	});

	async function initCharts() {
		Chart = (await loadChartJs()).Chart;
		chartReady = true;

		if (!heapChartElement) return;
		heapChart = new Chart(heapChartElement, {
			type: 'line',
			data: {
				labels: $analytics.uptime,
				datasets: [
					{
						label: 'Used',
						borderColor: daisyColor('--color-primary'),
						backgroundColor: daisyColor('--color-primary', 50),
						borderWidth: 2,
						data: $analytics.used_heap,
						yAxisID: 'y'
					},
					{
						label: 'Max Alloc',
						borderColor: daisyColor('--color-secondary'),
						backgroundColor: daisyColor('--color-secondary', 50),
						borderWidth: 2,
						data: $analytics.max_alloc_heap,
						yAxisID: 'y'
					}
				]
			},
			options: {
				maintainAspectRatio: false,
				responsive: true,
				layout: chartLayout,
				plugins: {
					legend: { display: true },
					tooltip: {
						mode: 'index',
						intersect: false
					}
				},
				elements: {
					point: { radius: 1 }
				},
				scales: {
					x: {
						grid: { color: daisyColor('--color-base-content', 10) },
						ticks: { color: daisyColor('--color-base-content') },
						display: false
					},
					y: {
						type: 'linear',
						title: {
							display: true,
							text: 'Memory [KB]',
							color: daisyColor('--color-base-content'),
							font: { size: 16, weight: 'bold' }
						},
						position: 'left',
						min: 0,
						max: Math.round(Math.max(...$analytics.total_heap, 1)),
						grid: { color: daisyColor('--color-base-content', 10) },
						ticks: { color: daisyColor('--color-base-content') },
						border: { color: daisyColor('--color-base-content', 10) }
					}
				}
			}
		});

		filesystemChart = new Chart(filesystemChartElement, {
			type: 'line',
			data: {
				labels: $analytics.uptime,
				datasets: [
					{
						label: 'Used',
						borderColor: daisyColor('--color-primary'),
						backgroundColor: daisyColor('--color-primary', 50),
						borderWidth: 2,
						data: $analytics.fs_used,
						yAxisID: 'y'
					}
				]
			},
			options: {
				maintainAspectRatio: false,
				responsive: true,
				layout: chartLayout,
				plugins: {
					legend: { display: true },
					tooltip: {
						mode: 'index',
						intersect: false
					}
				},
				elements: {
					point: { radius: 1 }
				},
				scales: {
					x: {
						grid: { color: daisyColor('--color-base-content', 10) },
						ticks: { color: daisyColor('--color-base-content') },
						display: false
					},
					y: {
						type: 'linear',
						title: {
							display: true,
							text: 'File System [KB]',
							color: daisyColor('--color-base-content'),
							font: { size: 16, weight: 'bold' }
						},
						position: 'left',
						min: 0,
						max: Math.round(Math.max(...$analytics.fs_total, 1)),
						grid: { color: daisyColor('--color-base-content', 10) },
						ticks: { color: daisyColor('--color-base-content') },
						border: { color: daisyColor('--color-base-content', 10) }
					}
				}
			}
		});

		temperatureChart = new Chart(temperatureChartElement, {
			type: 'line',
			data: {
				labels: $analytics.uptime,
				datasets: [
					{
						label: 'Core Temperature',
						borderColor: daisyColor('--color-primary'),
						backgroundColor: daisyColor('--color-primary', 50),
						borderWidth: 2,
						data: $analytics.core_temp,
						yAxisID: 'y'
					}
				]
			},
			options: {
				maintainAspectRatio: false,
				responsive: true,
				layout: chartLayout,
				plugins: {
					legend: { display: true },
					tooltip: {
						mode: 'index',
						intersect: false
					}
				},
				elements: {
					point: { radius: 1 }
				},
				scales: {
					x: {
						grid: { color: daisyColor('--color-base-content', 10) },
						ticks: { color: daisyColor('--color-base-content') },
						display: false
					},
					y: {
						type: 'linear',
						title: {
							display: true,
							text: 'Core Temperature [°C]',
							color: daisyColor('--color-base-content'),
							font: { size: 16, weight: 'bold' }
						},
						position: 'left',
						suggestedMin: 20,
						suggestedMax: 100,
						grid: { color: daisyColor('--color-base-content', 10) },
						ticks: { color: daisyColor('--color-base-content') },
						border: { color: daisyColor('--color-base-content', 10) }
					}
				}
			}
		});

	}


	$effect(() => {
		if (!chartReady || !hasPsramData || !psramChartElement) {
			psramChart?.destroy();
			psramChart = undefined;
			return;
		}
		if (psramChart) return;
		tryInitPsramChart();
	});

	function updateData() {
		if (!heapChart?.data) return;
		heapChart.data.labels = $analytics.uptime;
		heapChart.data.datasets[0].data = $analytics.used_heap;
		heapChart.data.datasets[1].data = $analytics.max_alloc_heap;
		heapChart.update('none');
		(heapChart.options.scales as any).y.max = Math.round(Math.max(...$analytics.total_heap, 1));

		if (hasPsramData && psramChart?.data) {
			psramChart.data.labels = $analytics.uptime;
			psramChart.data.datasets[0].data = $analytics.used_psram;
			psramChart.update('none');
			(psramChart.options.scales as any).y.max = Math.round(Math.max(...$analytics.psram_size, 1));
		}

		if (filesystemChart?.data) {
			filesystemChart.data.labels = $analytics.uptime;
			filesystemChart.data.datasets[0].data = $analytics.fs_used;
			filesystemChart.update('none');
			(filesystemChart.options.scales as any).y.max = Math.round(Math.max(...$analytics.fs_total, 1));
		}

		if (temperatureChart?.data) {
			temperatureChart.data.labels = $analytics.uptime;
			temperatureChart.data.datasets[0].data = $analytics.core_temp;
			temperatureChart.update('none');
		}
	}
</script>

<div class="system-metrics-root system-tab-mobile-flatten w-full">
	<div class="settings-container">
		<div class="settings-grid">
			<div class="settings-panel">
				<div class="settings-section">
					{#if page.data.features.analytics}
						<div
							class="metrics-charts-shell rounded-box border border-base-content/10 bg-base-100 p-2 max-md:border-base-content/10 md:border-base-content/5 md:p-3"
							transition:slide|local={{ duration: 300, easing: cubicOut }}
						>
							<div
								class="flex flex-col max-md:divide-y max-md:divide-base-content/10 md:gap-4"
							>
								<div class="metrics-chart-host max-md:py-2 md:py-0">
									<canvas bind:this={heapChartElement} class="metrics-chart-canvas"></canvas>
								</div>

								{#if hasPsramData}
									<div class="metrics-chart-host max-md:py-2 md:py-0">
										<canvas bind:this={psramChartElement} class="metrics-chart-canvas"></canvas>
									</div>
								{/if}

								<div class="metrics-chart-host max-md:py-2 md:py-0">
									<canvas bind:this={filesystemChartElement} class="metrics-chart-canvas"></canvas>
								</div>

								<div class="metrics-chart-host max-md:py-2 md:py-0">
									<canvas bind:this={temperatureChartElement} class="metrics-chart-canvas"></canvas>
								</div>
							</div>
						</div>
					{/if}

					{#if page.data.features.battery}
						<div class="mt-4 w-full md:mt-4">
							<BatteryMetrics />
						</div>
					{/if}
				</div>
			</div>
		</div>
	</div>
</div>

<style lang="scss">
	@use "$lib/styles/base/variables" as v;

	:global(.system-metrics-root .settings-container) {
		width: 100%;
	}

	:global(.system-metrics-root .settings-grid) {
		grid-template-columns: 1fr;
	}

	:global(.system-metrics-root .settings-panel) {
		width: 100%;
	}

	.metrics-charts-shell {
		width: 100%;
	}

	.metrics-chart-host {
		position: relative;
		width: 100%;
		height: 11rem;
		min-height: 10rem;
	}

	@media (min-width: v.$breakpoint-md) {
		.metrics-chart-host {
			height: 14rem;
			min-height: 12rem;
		}
	}

	.metrics-chart-canvas {
		display: block;
		width: 100% !important;
		height: 100% !important;
		max-height: 100%;
	}
</style>
