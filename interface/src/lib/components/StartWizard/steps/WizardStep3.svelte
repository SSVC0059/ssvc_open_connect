<script lang="ts">
	import type { SsvcSettings } from '$lib/types/models';

	let { settings = $bindable() } = $props<{
		settings: SsvcSettings;
	}>();

	function secondsToTimeString(sec: number): string {
		const hours = Math.floor(sec / 3600)
			.toString()
			.padStart(2, '0');
		const minutes = Math.floor((sec % 3600) / 60)
			.toString()
			.padStart(2, '0');
		return `${hours}:${minutes}`;
	}

	function timeStringToSeconds(timeString: string): number {
		const [hoursStr, minutesStr] = timeString.split(':');
		const hours = parseInt(hoursStr, 10);
		const minutes = parseInt(minutesStr, 10);

		if (isNaN(hours) || isNaN(minutes)) {
			return 0;
		}

		return Math.min(hours * 3600 + minutes * 60, 86400);
	}

	function handleTimeChange(value: string): string {
		const seconds = timeStringToSeconds(value);
		const roundedSeconds = Math.round(seconds / 300) * 300;
		return secondsToTimeString(roundedSeconds);
	}

	function handleInputChange(event: Event) {
		const target = event.target as HTMLInputElement;
		const correctedTime = handleTimeChange(target.value);
		settings.heads_timer = timeStringToSeconds(correctedTime);
		target.value = correctedTime;
	}
</script>

<div class="space-y-4">
	<h2 class="text-xl font-semibold text-gray-700">General Settings</h2>

	<div class="space-y-5">
		<!-- Start Delay -->
		<div>
			<label class="block text-lg font-medium text-gray-700 mb-2" for="startDelay">
				Отсрочка, сек
			</label>
			<input
				bind:value={settings.start_delay}
				class="w-full px-4 py-3 text-lg border border-gray-300 rounded-md shadow-sm focus:outline-none focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
				id="startDelay"
				max="18000"
				min="0"
				type="number"
			/>
		</div>

		<!-- Heads Timer (HH:MM) -->
		<div>
			<label class="block text-lg font-medium text-gray-700 mb-2" for="headsTimer">
				Таймер голов
			</label>

			<input
				class="w-full px-4 py-3 text-lg border border-gray-300 rounded-md shadow-sm focus:outline-none focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
				id="headsTimer"
				max="23:55"
				min="00:00"
				onchange={handleInputChange}
				step="300"
				type="time"
				value={secondsToTimeString(settings.heads_timer)}
			/>
		</div>

		<!-- Heads Timer (HH:MM) -->
		<div>
			<label class="block text-lg font-medium text-gray-700 mb-2" for="heads">
				Настройки клапана голов (время открытия / период)
			</label>

			<div class="flex flex-row gap-4">
				<input
					bind:value={settings.heads[0]}
					class="w-full px-4 py-3 text-lg border border-gray-300 rounded-md shadow-sm focus:outline-none focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
					id="heads"
					type="number"
				/>
				<input
					bind:value={settings.heads[1]}
					class="w-full px-4 py-3 text-lg border border-gray-300 rounded-md shadow-sm focus:outline-none focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
					id="heads"
					type="number"
				/>
			</div>
		</div>

		<div>
			<label class="block text-lg font-medium text-gray-700 mb-2" for="startDelay">
				Снижение, сек
			</label>
			<input
				bind:value={settings.heads_final}
				class="w-full px-4 py-3 text-lg border border-gray-300 rounded-md shadow-sm focus:outline-none focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
				id="startDelay"
				max={settings.heads[0]}
				min="0"
				step="0.1"
				type="number"
			/>
		</div>

		<div>
			<label class="block text-lg font-medium text-gray-700 mb-2" for="heads">
				Сброс (время открытия / период)
			</label>

			<div class="flex flex-row gap-4">
				<input
					bind:value={settings.release_speed}
					class="w-full px-4 py-3 text-lg border border-gray-300 rounded-md shadow-sm focus:outline-none focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
					id="heads"
					type="number"
				/>
				<div class="flex flex-row gap-4">
					<input
						bind:value={settings.release_timer}
						class="w-full px-4 py-3 text-lg border border-gray-300 rounded-md shadow-sm focus:outline-none focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
						id="heads"
						type="number"
					/>
				</div>
			</div>
		</div>
	</div>
</div>
