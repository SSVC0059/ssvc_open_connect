<script lang="ts">
	import Pencil from '~icons/tabler/pencil';

	let { value, onSave } = $props();

	let isEditing = $state(false);
	let draftSeconds = $state(value);

	// Разбиваем секунды на часы, минуты, секунды
	function secondsToTime(totalSeconds: number) {
		const hours = Math.floor(totalSeconds / 3600);
		const minutes = Math.floor((totalSeconds % 3600) / 60);
		const seconds = totalSeconds % 60;
		return { hours, minutes, seconds };
	}

	// Собираем секунды из часов, минут, секунд
	function timeToSeconds(h: number, m: number, s: number) {
		return h * 3600 + m * 60 + s;
	}

	// Форматируем время для отображения
	function formatTime(totalSeconds: number) {
		const { hours, minutes, seconds } = secondsToTime(totalSeconds);
		return [
			hours.toString().padStart(2, '0'),
			minutes.toString().padStart(2, '0'),
			seconds.toString().padStart(2, '0')
		].join(':');
	}

	// Состояния для редактирования
	let editHours = $state(0);
	let editMinutes = $state(0);
	let editSeconds = $state(0);

	// При входе в режим редактирования заполняем поля
	function startEditing() {
		const time = secondsToTime(draftSeconds);
		editHours = time.hours;
		editMinutes = time.minutes;
		editSeconds = time.seconds;
		isEditing = true;
	}

	// При сохранении собираем время обратно в секунды
	function saveChanges() {
		draftSeconds = timeToSeconds(editHours, editMinutes, editSeconds);
		onSave(draftSeconds);
		isEditing = false;
	}

	// При отмене возвращаем исходное значение
	function cancelEditing() {
		draftSeconds = value;
		isEditing = false;
	}
</script>

{#if isEditing}
	<div class="flex flex-col sm:flex-row gap-2 items-start sm:items-center">
		<div class="flex items-center gap-1">
			<input
				type="number"
				bind:value={editHours}
				class="w-12 px-2 py-1 border rounded text-center"
				min="0"
				max="23"
			/>
			<span>:</span>
			<input
				type="number"
				bind:value={editMinutes}
				class="w-12 px-2 py-1 border rounded text-center"
				min="0"
				max="59"
			/>
			<span>:</span>
			<input
				type="number"
				bind:value={editSeconds}
				class="w-12 px-2 py-1 border rounded text-center"
				min="0"
				max="59"
			/>
		</div>
		<div class="flex gap-2">
			<button class="px-2 py-1 bg-green-800 text-white rounded" onclick={saveChanges}> ✓ </button>
			<button class="px-2 py-1 bg-gray-200 rounded" onclick={cancelEditing}> × </button>
		</div>
	</div>
{:else}
	<div class="flex gap-2 items-center">
		<span class="font-mono">{formatTime(draftSeconds)}</span>
		<button class="text-blue-500 hover:text-blue-700" onclick={startEditing}>
			<Pencil />
		</button>
	</div>
{/if}
