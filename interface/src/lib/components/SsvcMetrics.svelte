<script lang="ts">
	import { onMount } from 'svelte';
	import { Chart, registerables } from 'chart.js';
	import { temperatureStore } from '$lib/stores/ssvcOpenConnect';
	import { get } from 'svelte/store';

	Chart.register(...registerables);

	let chartElement: HTMLCanvasElement;
	let temperatureChart: Chart;

	// Подписка на хранилище
	const data = get(temperatureStore);

	onMount(() => {
		// Инициализация графика
		temperatureChart = new Chart(chartElement, {
			type: 'line',
			data: {
				labels: data.time,
				datasets: [
					{
						label: 'TP1',
						borderColor: '#ff6384',
						backgroundColor: '#ff638455',
						data: data.tp1,
						yAxisID: 'y',
					},
					{
						label: 'TP2',
						borderColor: '#36a2eb',
						backgroundColor: '#36a2eb55',
						data: data.tp2,
						yAxisID: 'y',
					},
				],
			},
			options: {
				responsive: true,
				maintainAspectRatio: false,
				plugins: {
					legend: { display: true },
					tooltip: { mode: 'index', intersect: false },
				},
				scales: {
					x: { title: { display: true, text: 'Time' } },
					y: { title: { display: true, text: 'Temperature (°C)' } },
				},
			},
		});
	});

	// Обновление данных графика при изменении store
	temperatureStore.subscribe((store) => {
		if (temperatureChart) {
			temperatureChart.data.labels = store.time;
			temperatureChart.data.datasets[0].data = store.tp1;
			temperatureChart.data.datasets[1].data = store.tp2;
			temperatureChart.update('none');
		}
	});
</script>

<div class="chart-container">
	<canvas bind:this={chartElement}></canvas>
</div>

<style>
    .chart-container {
        width: 100%;
        height: 400px;
    }
</style>
