<script lang="ts">
	import { page } from '$app/state';
	import { onMount } from 'svelte';
	import { Chart, registerables } from 'chart.js';
	import { daisyColor } from '$lib/DaisyUiHelper';
	import { analytics } from '$lib/stores/analytics';
	import BatteryMetrics from './metrics/BatteryMetrics.svelte';
	import { slide } from 'svelte/transition';
	import { cubicOut } from 'svelte/easing';

	Chart.register(...registerables);

	let heapChartElement: HTMLCanvasElement = $state()!;
	let heapChart: Chart;

	let psramChartElement: HTMLCanvasElement = $state()!;
	let psramChart: Chart;

	let filesystemChartElement: HTMLCanvasElement = $state()!;
	let filesystemChart: Chart;

	let temperatureChartElement: HTMLCanvasElement = $state()!;
	let temperatureChart: Chart;

	// Check if PSRAM data is available
	let hasPsramData = $derived(Math.max(...$analytics.psram_size) > 0);

	onMount(() => {
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
						max: Math.round(Math.max(...$analytics.total_heap)),
						grid: { color: daisyColor('--color-base-content', 10) },
						ticks: { color: daisyColor('--color-base-content') },
						border: { color: daisyColor('--color-base-content', 10) }
					}
				}
			}
		});

		if (hasPsramData) {
			psramChart = new Chart(psramChartElement, {
				type: 'line',
				data: {
					labels: $analytics.uptime,
					datasets: [
						{
							label: 'Used',
							borderColor: daisyColor('--color-primary'),
							backgroundColor: daisyColor('--color-primary', 50),
							borderWidth: 2,
							data: $analytics.used_psram,
							yAxisID: 'y'
						}
					]
				},
				options: {
					maintainAspectRatio: false,
					responsive: true,
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
							max: Math.round(Math.max(...$analytics.psram_size)),
							grid: { color: daisyColor('--color-base-content', 10) },
							ticks: { color: daisyColor('--color-base-content') },
							border: { color: daisyColor('--color-base-content', 10) }
						}
					}
				}
			});
		}

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
						max: Math.round(Math.max(...$analytics.fs_total)),
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

		const intervalId = setInterval(updateData, 2000);
		return () => clearInterval(intervalId);
	});

	function updateData() {
		if (!heapChart?.data) return;
		heapChart.data.labels = $analytics.uptime;
		heapChart.data.datasets[0].data = $analytics.used_heap;
		heapChart.data.datasets[1].data = $analytics.max_alloc_heap;
		heapChart.update('none');
		(heapChart.options.scales as any).y.max = Math.round(Math.max(...$analytics.total_heap));

		if (hasPsramData && psramChart?.data) {
			psramChart.data.labels = $analytics.uptime;
			psramChart.data.datasets[0].data = $analytics.used_psram;
			psramChart.update('none');
			(psramChart.options.scales as any).y.max = Math.round(Math.max(...$analytics.psram_size));
		}

		if (filesystemChart?.data) {
			filesystemChart.data.labels = $analytics.uptime;
			filesystemChart.data.datasets[0].data = $analytics.fs_used;
			filesystemChart.update('none');
			(filesystemChart.options.scales as any).y.max = Math.round(Math.max(...$analytics.fs_total));
		}

		if (temperatureChart?.data) {
			temperatureChart.data.labels = $analytics.uptime;
			temperatureChart.data.datasets[0].data = $analytics.core_temp;
			temperatureChart.update('none');
		}
	}
</script>

<div class="system-metrics-root">
	<div class="settings-container">
		<div class="settings-grid">
			<div class="settings-panel">
				<div class="settings-section">
					{#if page.data.features.analytics}
						<div class="w-full overflow-x-auto">
							<div
								class="flex w-full flex-col space-y-1 h-60"
								transition:slide|local={{ duration: 300, easing: cubicOut }}
							>
								<canvas bind:this={heapChartElement}></canvas>
							</div>
						</div>

						{#if hasPsramData}
							<div class="w-full overflow-x-auto">
								<div
									class="flex w-full flex-col space-y-1 h-60"
									transition:slide|local={{ duration: 300, easing: cubicOut }}
								>
									<canvas bind:this={psramChartElement}></canvas>
								</div>
							</div>
						{/if}

						<div class="w-full overflow-x-auto">
							<div
								class="flex w-full flex-col space-y-1 h-52"
								transition:slide|local={{ duration: 300, easing: cubicOut }}
							>
								<canvas bind:this={filesystemChartElement}></canvas>
							</div>
						</div>

						<div class="w-full overflow-x-auto">
							<div
								class="flex w-full flex-col space-y-1 h-52"
								transition:slide|local={{ duration: 300, easing: cubicOut }}
							>
								<canvas bind:this={temperatureChartElement}></canvas>
							</div>
						</div>
					{/if}

					{#if page.data.features.battery}
						<BatteryMetrics />
					{/if}
				</div>
			</div>
		</div>
	</div>
</div>

<style lang="scss">
	:global(.system-metrics-root .settings-container) {
		width: 100%;
	}

	:global(.system-metrics-root .settings-grid) {
		grid-template-columns: 1fr;
	}

	:global(.system-metrics-root .settings-panel) {
		width: 100%;
	}
</style>
