<script lang="ts">
	import { onMount } from 'svelte';
	import { Chart, registerables } from 'chart.js';
	import zoomPlugin from 'chartjs-plugin-zoom';

	Chart.register(...registerables, zoomPlugin);

	let chartElementTp1: HTMLCanvasElement;
	let chartElementTp2: HTMLCanvasElement;
	let tp1Chart: Chart;
	let tp2Chart: Chart;

	interface TemperatureData {
		time: string[];
		tp1: number[];
		tp2: number[];
	}

	let fullData: TemperatureData = { time: [], tp1: [], tp2: [] };
	const MAX_DATA_POINTS = 100;
	let totalPoints = 0;
	let periodicity = 1;

	async function fetchGraphData(point: number, periodicity: number): Promise<TemperatureData | null> {
		try {
			const response = await fetch(`/rest/tMetrics?point=${point}&periodicity=${periodicity}`);
			const json = await response.json();

			if (json?.graphData?.data) {
				const data = json.graphData.data;
				return {
					time: data.map((entry: any) => new Date(entry.time * 1000).toLocaleTimeString()),
					tp1: data.map((entry: any) => entry.tp1),
					tp2: data.map((entry: any) => entry.tp2),
				};
			}
			return null;
		} catch (error) {
			console.error('Error fetching data:', error);
			return null;
		}
	}

	function getDisplayedData(data: TemperatureData, displayedPoints: number): TemperatureData {
		const totalPoints = data.time.length;
		if (totalPoints <= displayedPoints) return { ...data };

		const step = Math.floor(totalPoints / displayedPoints);
		return {
			time: data.time.filter((_, index) => index % step === 0),
			tp1: data.tp1.filter((_, index) => index % step === 0),
			tp2: data.tp2.filter((_, index) => index % step === 0),
		};
	}

	function updateChart(chart: Chart, data: number[], labels: string[]) {
		chart.data.labels = labels;
		chart.data.datasets[0].data = data;
		chart.update();
	}

	async function loadData() {
		const newData = await fetchGraphData(totalPoints, periodicity);

		if (newData) {
			fullData.time.push(...newData.time);
			fullData.tp1.push(...newData.tp1);
			fullData.tp2.push(...newData.tp2);
			totalPoints += newData.time.length;

			const displayedData = getDisplayedData(fullData, MAX_DATA_POINTS);
			updateChart(tp1Chart, displayedData.tp1, displayedData.time);
			updateChart(tp2Chart, displayedData.tp2, displayedData.time);
		}
	}

	onMount(() => {
		tp1Chart = new Chart(chartElementTp1, {
			type: 'line',
			data: {
				labels: [],
				datasets: [{
					label: 'TP1',
					borderColor: '#ff6384',
					backgroundColor: '#ff638455',
					data: [],
				}]
			},
			options: {
				responsive: true,
				plugins: {
					zoom: {
						pan: { enabled: true, mode: 'x' },
						zoom: { wheel: { enabled: true }, mode: 'x' }
					}
				},
				scales: { x: { title: { text: 'Time', display: true } }, y: { title: { text: 'Temperature (°C)', display: true } } }
			}
		});

		tp2Chart = new Chart(chartElementTp2, {
			type: 'line',
			data: {
				labels: [],
				datasets: [{
					label: 'TP2',
					borderColor: '#36a2eb',
					backgroundColor: '#36a2eb55',
					data: [],
				}]
			},
			options: {
				responsive: true,
				plugins: {
					zoom: {
						pan: { enabled: true, mode: 'x' },
						zoom: { wheel: { enabled: true }, mode: 'x' }
					}
				},
				scales: { x: { title: { text: 'Time', display: true } }, y: { title: { text: 'Temperature (°C)', display: true } } }
			}
		});

		loadData();
		const interval = setInterval(loadData, 10000);
		return () => clearInterval(interval);
	});
</script>

<!-- Добавим класс к div-блоку -->
<div class="chart-container">
	<canvas bind:this={chartElementTp1} style="width: 100%; height: 100%;"></canvas>
</div>
<div class="chart-container">
	<canvas bind:this={chartElementTp2} style="width: 100%; height: 100%;"></canvas>
</div>

<style>
	/* Оставляем те же стили */
	.chart-container {
		width: 100%;
		height: 300px;
		margin-bottom: 20px;
		display: flex; /* Чтобы canvas точно занимал все пространство */
	}
</style>
