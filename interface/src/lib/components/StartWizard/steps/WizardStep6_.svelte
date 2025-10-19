<script lang="ts">
	import type { SsvcSettings } from '$lib/types/models';
	import { calculateSamplingRate } from '$lib/components/StartWizard/wizardLogic';

	let { settings = $bindable() } = $props<{
		settings: SsvcSettings;
	}>();

	// Функция для расчета скорости отбора для каждой строки
	function getSamplingRate(openTime: number, period: number, valveIndex: number = 2): number {
		// Используем пропускную способность указанного клапана
		const flowRate = settings.valve_bw?.[valveIndex] || 7000;
		return calculateSamplingRate(openTime, period, flowRate);
	}
</script>

<div class="space-y-4">
	<!-- Параллельный отбор клапаном №3 -->
	<div class="bg-white rounded-lg shadow-sm border border-gray-200 overflow-hidden">
		<h2 class="text-lg font-semibold text-gray-700 p-4 bg-gray-50">Параллельный отбор клапаном №3</h2>

		{#if settings.parallel_v3?.length}
			<div class="p-4">
				<!-- Мобильная версия таблицы -->
				<div class="md:hidden space-y-3">
					{#each settings.parallel_v3 as [temp, openTime, period], i}
						<div class="border border-gray-200 rounded-lg p-3 bg-white">
							<div class="space-y-3">
								<!-- Температура -->
								<div>
									<div class="flex items-center gap-1 mb-1">
										<span class="text-sm font-medium text-gray-700">Температура</span>
										<div class="relative group">

											<div class="absolute bottom-full left-1/2 transform -translate-x-1/2 mb-2 hidden group-hover:block w-48 bg-gray-900 text-white text-xs rounded py-1 px-2 z-10">
												Диапазон температур (°C)
												<div class="absolute top-full left-1/2 transform -translate-x-1/2 border-4 border-transparent border-t-gray-900"></div>
											</div>
										</div>
									</div>
									<input
										class="w-full px-3 py-2 text-sm border border-gray-300 rounded focus:outline-none focus:ring-1 focus:ring-blue-500"
										bind:value={settings.parallel_v3[i][0]}
										max="100"
										min="0"
										step="0.1"
										type="number"
									/>
								</div>

								<div class="grid grid-cols-2 gap-3">
									<!-- Время открытия -->
									<div>
										<div class="flex items-center gap-1 mb-1">
											<span class="text-sm font-medium text-gray-700">Открытие</span>
											<div class="relative group">

												<div class="absolute bottom-full left-1/2 transform -translate-x-1/2 mb-2 hidden group-hover:block w-48 bg-gray-900 text-white text-xs rounded py-1 px-2 z-10">
													Время открытия клапана (сек)
													<div class="absolute top-full left-1/2 transform -translate-x-1/2 border-4 border-transparent border-t-gray-900"></div>
												</div>
											</div>
										</div>
										<input
											type="number"
											step="0.1"
											min="0"
											class="w-full px-3 py-2 text-sm border border-gray-300 rounded focus:outline-none focus:ring-1 focus:ring-blue-500"
											bind:value={settings.parallel_v3[i][1]}
										/>
									</div>

									<!-- Период -->
									<div>
										<div class="flex items-center gap-1 mb-1">
											<span class="text-sm font-medium text-gray-700">Период</span>
											<div class="relative group">

												<div class="absolute bottom-full left-0 mb-2 hidden group-hover:block w-48 bg-gray-900 text-white text-xs rounded py-1 px-2 z-10">
													Период между открытиями (сек)
													<div class="absolute top-full left-4 border-4 border-transparent border-t-gray-900"></div>
												</div>
											</div>
										</div>
										<input
											type="number"
											step="1"
											min="1"
											class="w-full px-3 py-2 text-sm border border-gray-300 rounded focus:outline-none focus:ring-1 focus:ring-blue-500"
											bind:value={settings.parallel_v3[i][2]}
										/>
									</div>
								</div>

								<!-- Скорость отбора -->
								<div>
									<div class="flex items-center gap-1 mb-1">
										<span class="text-sm font-medium text-gray-700">Скорость</span>
										<div class="relative group">

											<div class="absolute bottom-full left-1/2 transform -translate-x-1/2 mb-2 hidden group-hover:block w-48 bg-gray-900 text-white text-xs rounded py-1 px-2 z-10">
												Скорость отбора (мл/час)
												<div class="absolute top-full left-1/2 transform -translate-x-1/2 border-4 border-transparent border-t-gray-900"></div>
											</div>
										</div>
									</div>
									<input
										type="text"
										readonly
										class="w-full px-3 py-2 text-sm bg-gray-100 border border-gray-300 rounded"
										value={getSamplingRate(openTime, period).toFixed(1)}
									/>
								</div>
							</div>
						</div>
					{/each}
				</div>

				<!-- Десктопная версия таблицы -->
				<div class="hidden md:block overflow-x-auto">
					<table class="w-full border-collapse border border-gray-300 rounded-lg overflow-hidden">
						<thead class="bg-gray-100">
						<tr>
							<th class="px-4 py-3 text-left text-sm font-medium text-gray-700 border border-gray-300 whitespace-nowrap">
									<span class="flex items-center gap-1">
										Температура
										<div class="relative group">
											<div class="absolute bottom-full left-1/2 transform -translate-x-1/2 mb-1 hidden group-hover:block w-48 bg-gray-900 text-white text-xs rounded py-1 px-2">
												Диапазон температур (°C)
												<div class="absolute top-full left-1/2 transform -translate-x-1/2 border-4 border-transparent border-t-gray-900"></div>
											</div>
										</div>
									</span>
							</th>
							<th class="px-4 py-3 text-left text-sm font-medium text-gray-700 border border-gray-300 whitespace-nowrap">
									<span class="flex items-center gap-1">
										Открытие
										<div class="relative group">
											<div class="absolute bottom-full left-1/2 transform -translate-x-1/2 mb-1 hidden group-hover:block w-48 bg-gray-900 text-white text-xs rounded py-1 px-2">
												Время открытия клапана (сек)
												<div class="absolute top-full left-1/2 transform -translate-x-1/2 border-4 border-transparent border-t-gray-900"></div>
											</div>
										</div>
									</span>
							</th>
							<th class="px-4 py-3 text-left text-sm font-medium text-gray-700 border border-gray-300 whitespace-nowrap">
									<span class="flex items-center gap-1">
										Период
										<div class="relative group">

											<div class="absolute bottom-full left-1/2 transform -translate-x-1/2 mb-1 hidden group-hover:block w-48 bg-gray-900 text-white text-xs rounded py-1 px-2">
												Период между открытиями (сек)
												<div class="absolute top-full left-1/2 transform -translate-x-1/2 border-4 border-transparent border-t-gray-900"></div>
											</div>
										</div>
									</span>
							</th>
							<th class="px-4 py-3 text-left text-sm font-medium text-gray-700 border border-gray-300 whitespace-nowrap">
									<span class="flex items-center gap-1">
										Скорость
										<div class="relative group">

											<div class="absolute bottom-full left-1/2 transform -translate-x-1/2 mb-1 hidden group-hover:block w-48 bg-gray-900 text-white text-xs rounded py-1 px-2">
												Скорость отбора (мл/час)
												<div class="absolute top-full left-1/2 transform -translate-x-1/2 border-4 border-transparent border-t-gray-900"></div>
											</div>
										</div>
									</span>
							</th>
						</tr>
						</thead>
						<tbody>
						{#each settings.parallel_v3 as [temp, openTime, period], i}
							<tr class={i % 2 === 0 ? 'bg-white' : 'bg-gray-50'}>
								<td class="px-4 py-2 border border-gray-300">
									<input
										class="w-full px-2 py-1 text-sm border border-gray-300 rounded focus:outline-none focus:ring-1 focus:ring-blue-500"
										bind:value={settings.parallel_v3[i][0]}
										max="100"
										min="0"
										step="0.1"
										type="number"
									/>
								</td>
								<td class="px-4 py-2 border border-gray-300">
									<input
										type="number"
										step="0.1"
										min="0"
										class="w-full px-2 py-1 text-sm border border-gray-300 rounded focus:outline-none focus:ring-1 focus:ring-blue-500"
										bind:value={settings.parallel_v3[i][1]}
									/>
								</td>
								<td class="px-4 py-2 border border-gray-300">
									<input
										type="number"
										step="1"
										min="1"
										class="w-full px-2 py-1 text-sm border border-gray-300 rounded focus:outline-none focus:ring-1 focus:ring-blue-500"
										bind:value={settings.parallel_v3[i][2]}
									/>
								</td>
								<td class="px-4 py-2 border border-gray-300">
									<input
										type="text"
										readonly
										class="w-full px-2 py-1 text-sm bg-gray-100 border border-gray-300 rounded"
										value={getSamplingRate(openTime, period).toFixed(1)}
									/>
								</td>
							</tr>
						{/each}
						</tbody>
					</table>
				</div>

				<!-- Информация о пропускной способности -->
				<div class="bg-blue-50 border border-blue-200 rounded-lg p-3 mt-4">
					<p class="text-sm text-blue-700">
						<strong>Пропускная способность клапана №3:</strong> {settings.valve_bw?.[2]} мл/час
					</p>
					<p class="text-xs text-blue-600 mt-1">
						Скорость отбора рассчитывается автоматически при изменении времени открытия или периода
					</p>
				</div>
			</div>
		{/if}
	</div>

	<!-- Параллельный отбор клапаном №1 -->
	<div class="bg-white rounded-lg shadow-sm border border-gray-200 overflow-hidden">
		<h2 class="text-lg font-semibold text-gray-700 p-4 bg-gray-50">Параллельный отбор клапаном №1</h2>

		{#if settings.parallel_v1}
			<div class="p-4">
				<!-- Мобильная версия -->
				<div class="md:hidden space-y-3">
					<div class="border border-gray-200 rounded-lg p-3 bg-white">
						<div class="space-y-3">
							<div class="grid grid-cols-2 gap-3">
								<!-- Время открытия -->
								<div>
									<div class="flex items-center gap-1 mb-1">
										<span class="text-sm font-medium text-gray-700">Открытие</span>
										<div class="relative group">
											<div class="absolute bottom-full left-1/2 transform -translate-x-1/2 mb-2 hidden group-hover:block w-48 bg-gray-900 text-white text-xs rounded py-1 px-2 z-10">
												Время открытия клапана (сек)
												<div class="absolute top-full left-1/2 transform -translate-x-1/2 border-4 border-transparent border-t-gray-900"></div>
											</div>
										</div>
									</div>
									<input
										type="number"
										step="0.1"
										min="0"
										class="w-full px-3 py-2 text-sm border border-gray-300 rounded focus:outline-none focus:ring-1 focus:ring-blue-500"
										bind:value={settings.parallel_v1[0]}
									/>
								</div>

								<!-- Период -->
								<div>
									<div class="flex items-center gap-1 mb-1">
										<span class="text-sm font-medium text-gray-700">Период</span>
										<div class="relative group">
											<div class="absolute bottom-full left-0 mb-2 hidden group-hover:block w-48 bg-gray-900 text-white text-xs rounded py-1 px-2 z-10">
												Период между открытиями (сек)
												<div class="absolute top-full left-4 border-4 border-transparent border-t-gray-900"></div>
											</div>
										</div>
									</div>
									<input
										type="number"
										step="1"
										min="1"
										class="w-full px-3 py-2 text-sm border border-gray-300 rounded focus:outline-none focus:ring-1 focus:ring-blue-500"
										bind:value={settings.parallel_v1[1]}
									/>
								</div>
							</div>

							<!-- Скорость отбора -->
							<div>
								<div class="flex items-center gap-1 mb-1">
									<span class="text-sm font-medium text-gray-700">Скорость</span>
									<div class="relative group">
										<div class="absolute bottom-full left-1/2 transform -translate-x-1/2 mb-2 hidden group-hover:block w-48 bg-gray-900 text-white text-xs rounded py-1 px-2 z-10">
											Скорость отбора (мл/час)
											<div class="absolute top-full left-1/2 transform -translate-x-1/2 border-4 border-transparent border-t-gray-900"></div>
										</div>
									</div>
								</div>
								<input
									type="text"
									readonly
									class="w-full px-3 py-2 text-sm bg-gray-100 border border-gray-300 rounded"
									value={getSamplingRate(settings.parallel_v1[0], settings.parallel_v1[1], 0).toFixed(1)}
								/>
							</div>
						</div>
					</div>
				</div>

				<!-- Десктопная версия -->
				<div class="hidden md:block overflow-x-auto">
					<table class="w-full border-collapse border border-gray-300 rounded-lg overflow-hidden">
						<thead class="bg-gray-100">
						<tr>
							<th class="px-4 py-3 text-left text-sm font-medium text-gray-700 border border-gray-300 whitespace-nowrap">
									<span class="flex items-center gap-1">
										Открытие
										<div class="relative group">

											<div class="absolute bottom-full left-1/2 transform -translate-x-1/2 mb-1 hidden group-hover:block w-48 bg-gray-900 text-white text-xs rounded py-1 px-2">
												Время открытия клапана (сек)
												<div class="absolute top-full left-1/2 transform -translate-x-1/2 border-4 border-transparent border-t-gray-900"></div>
											</div>
										</div>
									</span>
							</th>
							<th class="px-4 py-3 text-left text-sm font-medium text-gray-700 border border-gray-300 whitespace-nowrap">
									<span class="flex items-center gap-1">
										Период
										<div class="relative group">

											<div class="absolute bottom-full left-1/2 transform -translate-x-1/2 mb-1 hidden group-hover:block w-48 bg-gray-900 text-white text-xs rounded py-1 px-2">
												Период между открытиями (сек)
												<div class="absolute top-full left-1/2 transform -translate-x-1/2 border-4 border-transparent border-t-gray-900"></div>
											</div>
										</div>
									</span>
							</th>
							<th class="px-4 py-3 text-left text-sm font-medium text-gray-700 border border-gray-300 whitespace-nowrap">
									<span class="flex items-center gap-1">
										Скорость
										<div class="relative group">

											<div class="absolute bottom-full left-1/2 transform -translate-x-1/2 mb-1 hidden group-hover:block w-48 bg-gray-900 text-white text-xs rounded py-1 px-2">
												Скорость отбора (мл/час)
												<div class="absolute top-full left-1/2 transform -translate-x-1/2 border-4 border-transparent border-t-gray-900"></div>
											</div>
										</div>
									</span>
							</th>
						</tr>
						</thead>
						<tbody>
						<tr class="bg-white">
							<td class="px-4 py-2 border border-gray-300">
								<input
									type="number"
									step="0.1"
									min="0"
									class="w-full px-2 py-1 text-sm border border-gray-300 rounded focus:outline-none focus:ring-1 focus:ring-blue-500"
									bind:value={settings.parallel_v1[0]}
								/>
							</td>
							<td class="px-4 py-2 border border-gray-300">
								<input
									type="number"
									step="1"
									min="1"
									class="w-full px-2 py-1 text-sm border border-gray-300 rounded focus:outline-none focus:ring-1 focus:ring-blue-500"
									bind:value={settings.parallel_v1[1]}
								/>
							</td>
							<td class="px-4 py-2 border border-gray-300">
								<input
									type="text"
									readonly
									class="w-full px-2 py-1 text-sm bg-gray-100 border border-gray-300 rounded"
									value={getSamplingRate(settings.parallel_v1[0], settings.parallel_v1[1], 0).toFixed(1)}
								/>
							</td>
						</tr>
						</tbody>
					</table>
				</div>

				<!-- Информация о пропускной способности -->
				<div class="bg-blue-50 border border-blue-200 rounded-lg p-3 mt-4">
					<p class="text-sm text-blue-700">
						<strong>Пропускная способность клапана №1:</strong> {settings.valve_bw?.[0] || 7000} мл/час
					</p>
					<p class="text-xs text-blue-600 mt-1">
						Скорость отбора рассчитывается автоматически при изменении времени открытия или периода
					</p>
				</div>
			</div>
		{/if}
	</div>
</div>