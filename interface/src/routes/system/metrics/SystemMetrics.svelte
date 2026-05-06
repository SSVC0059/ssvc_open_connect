<script lang="ts">
	import { onMount } from 'svelte';
	import SettingsCard from '$lib/components/SettingsCard.svelte';
	import { slide } from 'svelte/transition';
	import { cubicOut } from 'svelte/easing';
	import { Chart, registerables } from 'chart.js';
	import Metrics from '~icons/tabler/report-analytics';
	import { daisyColor } from '$lib/DaisyUiHelper';
	import { analytics } from '$lib/stores/analytics';

	Chart.register(...registerables);

	let heapChartElement: HTMLCanvasElement | undefined = $state();
	let heapChart: Chart;

	let psramChartElement: HTMLCanvasElement | undefined = $state();
	let psramChart: Chart | undefined;

	let filesystemChartElement: HTMLCanvasElement | undefined = $state();
	let filesystemChart: Chart;

	let temperatureChartElement: HTMLCanvasElement | undefined = $state();
	let temperatureChart: Chart;

	// Check if PSRAM data is available
	let hasPsramData = $derived(Math.max(...$analytics.psram_size) > 0);

	$effect(() => {
		if (hasPsramData && psramChartElement) {
			initPsramChart();
		} else if (!hasPsramData && psramChart) {
			psramChart.destroy();
			psramChart = undefined;
		}
	});

	function initPsramChart() {
		if (psramChart || !hasPsramData || !psramChartElement) {
			return;
		}

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
					legend: {
						display: true
					},
					tooltip: {
						mode: 'index',
						intersect: false
					}
				},
				elements: {
					point: {
						radius: 1
					}
				},
				scales: {
					x: {
						grid: {
							color: daisyColor('--color-base-content', 10)
						},
						ticks: {
							color: daisyColor('--color-base-content')
						},
						display: false
					},
					y: {
						type: 'linear',
						title: {
							display: true,
							text: 'PSRAM [KB]',
							color: daisyColor('--color-base-content'),
							font: {
								size: 16,
								weight: 'bold'
							}
						},
						position: 'left',
						min: 0,
						max: Math.round(Math.max(...$analytics.psram_size)),
						grid: { color: daisyColor('--color-base-content', 10) },
						ticks: {
							color: daisyColor('--color-base-content')
						},
						border: { color: daisyColor('--color-base-content', 10) }
					}
				}
			}
		});
	}

	onMount(() => {
		heapChart = new Chart(heapChartElement!, {
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
					legend: {
						display: true
					},
					tooltip: {
						mode: 'index',
						intersect: false
					}
				},
				elements: {
					point: {
						radius: 1
					}
				},
				scales: {
					x: {
						grid: {
							color: daisyColor('--color-base-content', 10)
						},
						ticks: {
							color: daisyColor('--color-base-content')
						},
						display: false
					},
					y: {
						type: 'linear',
						title: {
							display: true,
							text: 'Memory [KB]',
							color: daisyColor('--color-base-content'),
							font: {
								size: 16,
								weight: 'bold'
							}
						},
						position: 'left',
						min: 0,
						max: Math.round(Math.max(...$analytics.total_heap)),
						grid: { color: daisyColor('--color-base-content', 10) },
						ticks: {
							color: daisyColor('--color-base-content')
						},
						border: { color: daisyColor('--color-base-content', 10) }
					}
				}
			}
		});

		initPsramChart();

		filesystemChart = new Chart(filesystemChartElement!, {
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
					legend: {
						display: true
					},
					tooltip: {
						mode: 'index',
						intersect: false
					}
				},
				elements: {
					point: {
						radius: 1
					}
				},
				scales: {
					x: {
						grid: {
							color: daisyColor('--color-base-content', 10)
						},
						ticks: {
							color: daisyColor('--color-base-content')
						},
						display: false
					},
					y: {
						type: 'linear',
						title: {
							display: true,
							text: 'File System [KB]',
							color: daisyColor('--color-base-content'),
							font: {
								size: 16,
								weight: 'bold'
							}
						},
						position: 'left',
						min: 0,
						max: Math.round(Math.max(...$analytics.fs_total)),
						grid: { color: daisyColor('--color-base-content', 10) },
						ticks: {
							color: daisyColor('--color-base-content')
						},
						border: { color: daisyColor('--color-base-content', 10) }
					}
				}
			}
		});
		temperatureChart = new Chart(temperatureChartElement!, {
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
					legend: {
						display: true
					},
					tooltip: {
						mode: 'index',
						intersect: false
					}
				},
				elements: {
					point: {
						radius: 1
					}
				},
				scales: {
					x: {
						grid: {
							color: daisyColor('--color-base-content', 10)
						},
						ticks: {
							color: daisyColor('--color-base-content')
						},
						display: false
					},
					y: {
						type: 'linear',
						title: {
							display: true,
							text: 'Core Temperature [°C]',
							color: daisyColor('--color-base-content'),
							font: {
								size: 16,
								weight: 'bold'
							}
						},
						position: 'left',
						suggestedMin: 20,
						suggestedMax: 100,
						grid: { color: daisyColor('--color-base-content', 10) },
						ticks: {
							color: daisyColor('--color-base-content')
						},
						border: { color: daisyColor('--color-base-content', 10) }
					}
				}
			}
		});
		const interval = setInterval(updateData, 2000);
		return () => {
			clearInterval(interval);
			heapChart.destroy();
			psramChart?.destroy();
			filesystemChart.destroy();
			temperatureChart.destroy();
		};
	});

	function updateData() {
		heapChart.data.labels = $analytics.uptime;
		heapChart.data.datasets[0].data = $analytics.used_heap;
		heapChart.data.datasets[1].data = $analytics.max_alloc_heap;
		const heapScaleY = heapChart.options.scales?.y;
		if (heapScaleY && typeof heapScaleY === 'object') {
			heapScaleY.max = Math.round(Math.max(...$analytics.total_heap));
		}
		heapChart.update('none');

		if (hasPsramData) {
			initPsramChart();
			if (psramChart) {
				psramChart.data.labels = $analytics.uptime;
				psramChart.data.datasets[0].data = $analytics.used_psram;
				const psramScaleY = psramChart.options.scales?.y;
				if (psramScaleY && typeof psramScaleY === 'object') {
					psramScaleY.max = Math.round(Math.max(...$analytics.psram_size));
				}
				psramChart.update('none');
			}
		}

		filesystemChart.data.labels = $analytics.uptime;
		filesystemChart.data.datasets[0].data = $analytics.fs_used;
		const fsScaleY = filesystemChart.options.scales?.y;
		if (fsScaleY && typeof fsScaleY === 'object') {
			fsScaleY.max = Math.round(Math.max(...$analytics.fs_total));
		}
		filesystemChart.update('none');

		temperatureChart.data.labels = $analytics.uptime;
		temperatureChart.data.datasets[0].data = $analytics.core_temp;
		temperatureChart.update('none');
	}

</script>

<SettingsCard collapsible={false}>
	{#snippet icon()}
		<Metrics class="lex-shrink-0 mr-2 h-6 w-6 self-end" />
	{/snippet}
	{#snippet title()}
		<span>System Metrics</span>
	{/snippet}

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
</SettingsCard>
