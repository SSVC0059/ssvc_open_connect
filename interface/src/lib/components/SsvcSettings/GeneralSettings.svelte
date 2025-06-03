<script lang="ts">
	import EditNumbersModal from '$lib/components/SsvcSettings/EditNumbersModal.svelte';
	import EditBooleanModal from '$lib/components/SsvcSettings/EditBooleanModal.svelte';
	import EditTimeModal from '$lib/components/SsvcSettings/EditTimeModal.svelte';
	import type { SsvcSettings } from '$lib/types/models';

	const { settings, onSave } = $props<{
		settings: SsvcSettings;
		onSave: (field: string, value: any) => void;
	}>();
</script>

<div class="mt-4">
	<table class="settings-table">
		<thead>
			<tr class="bg-blue-500 text-white dark:bg-gray-800 dark:text-gray-100">
				<th class="px-4 py-2 text-left">Параметр</th>
				<th class="px-4 py-2 text-center">Значение</th>
			</tr>
		</thead>
		<tbody>
			<tr class="dark:bg-gray-700 dark:text-gray-100">
				<td class="px-4 py-2 text-left">Гистерезис при отборе тела</td>
				<td class="border border-gray-700 dark:border-gray-500 px-4 py-2">
					<div class="editable-container flex justify-center">
						<EditNumbersModal
							onSave={(newValues) => {
								// Используйте полный тип объекта как в компоненте
								onSave('hyst', newValues[0].value);
							}}
							values={[
								{
									name: 'Гистерезис',
									value: settings.hyst,
									step: 0.01,
									precision: 1,
									min: 0,
									max: 50
								}
							]}
						/>
					</div>
				</td>
			</tr>
			<tr class="dark:bg-gray-700 dark:text-gray-100">
				<td class="border border-gray-700 dark:border-gray-500 px-4 py-2 text-left"
					>Декремент при отборе тела, %
				</td>
				<td class="border border-gray-700 dark:border-gray-500 px-4 py-2 text-center items-center">
					<div class="editable-container flex justify-center">
						<EditNumbersModal
							onSave={(newValues) => {
								// Используйте полный тип объекта как в компоненте
								onSave('decrement', newValues[0].value);
							}}
							values={[
								{
									name: 'Декремент при отборе тела',
									value: settings.decrement,
									unit: '%',
									min: 0,
									max: 20
								}
							]}
						/>
					</div>
				</td>
			</tr>
			<tr class="dark:bg-gray-700 dark:text-gray-100">
				<td class="border border-gray-700 dark:border-gray-500 px-4 py-2 text-left"
					>Время отбора голов
				</td>
				<td class="border border-gray-700 dark:border-gray-500 px-4 py-2 text-center">
					<div class="editable-container flex justify-center">
						<EditTimeModal
							max={36000}
							onSave={(v) => onSave('heads_timer', v)}
							seconds={settings.heads_timer}
							step={300}
						/>
					</div>
				</td>
			</tr>
			{#if settings.late_heads_timer}
				<tr class="dark:bg-gray-700 dark:text-gray-100">
					<td class="border border-gray-700 dark:border-gray-500 px-4 py-2 text-left"
						>Время отбора подголовников
					</td>
					<td class="border border-gray-700 dark:border-gray-500 px-4 py-2 text-center">
						<div class="editable-container flex justify-center">
							<EditTimeModal
								seconds={settings.late_heads_timer}
								max={36000}
								step={300}
								onSave={(v) => onSave('late_heads_timer', v)}
							/>
						</div>
					</td>
				</tr>
			{/if}
			<tr class="dark:bg-gray-700 dark:text-gray-100">
				<td class="border border-gray-700 dark:border-gray-500 px-4 py-2 text-left"
					>Таймер фиксации температуры отбора тела
				</td>
				<td class="border border-gray-700 dark:border-gray-500 px-4 py-2 text-center">
					<div class="editable-container flex justify-center">
						<EditNumbersModal
							onSave={(newValues) => {
								// Используйте полный тип объекта как в компоненте
								onSave('hearts_timer', newValues[0].value);
							}}
							values={[
								{
									name: 'Таймер фиксации температуры отбора тела',
									value: settings.hearts_timer,
									unit: 'сек',
									min: 0,
									max: 30
								}
							]}
						/>
					</div>
				</td>
			</tr>
			{#if settings.tails_temp}
				<tr class="dark:bg-gray-700 dark:text-gray-100">
					<td class="border border-gray-700 dark:border-gray-500 px-4 py-2 text-left"
						>Температура завершения отбора хвостов
					</td>
					<td class="border border-gray-700 dark:border-gray-500 px-4 py-2 text-center">
						<div class="editable-container flex justify-center">
							<EditNumbersModal
								values={[
									{
										name: 'Температура завершения отбора хвостов',
										value: settings.tails_temp,
										unit: '°С',
										min: 0,
										max: 110.0,
										step: 0.1
									}
								]}
								onSave={(newValues) => {
									// Используйте полный тип объекта как в компоненте
									onSave('tails_temp', newValues[0].value);
								}}
							/>
						</div>
					</td>
				</tr>
			{/if}
			<tr class="dark:bg-gray-700 dark:text-gray-100">
				<td class="border border-gray-700 dark:border-gray-500 px-4 py-2 text-left">Звук</td>
				<td class="border border-gray-700 dark:border-gray-500 px-4 py-2 text-center"
					>{settings.sound ? 'Включен' : 'Отключен'}</td
				>
			</tr>
			<tr class="dark:bg-gray-700 dark:text-gray-100">
				<td class="border border-gray-700 dark:border-gray-500 px-4 py-2 text-left">Барометр</td>
				<td class="border border-gray-700 dark:border-gray-500 px-4 py-2 text-center"
					>{settings.pressure ? 'Включен' : 'Отключен'}</td
				>
			</tr>
			<tr class="dark:bg-gray-700 dark:text-gray-100">
				<td class="border border-gray-700 dark:border-gray-500 px-4 py-2 text-left"
					>Реле инвертировано
				</td>
				<td class="border border-gray-700 dark:border-gray-500 px-4 py-2 text-center"
					>{settings.relay_inverted ? 'Да' : 'Нет'}</td
				>
			</tr>
			<tr class="dark:bg-gray-700 dark:text-gray-100">
				<td class="border border-gray-700 dark:border-gray-500 px-4 py-2 text-left"
					>Автовключение реле
				</td>
				<td class="border border-gray-700 dark:border-gray-500 px-4 py-2 text-center"
					>{settings.relay_autostart ? 'Да' : 'Нет'}</td
				>
			</tr>
			<tr class="dark:bg-gray-700 dark:text-gray-100">
				<td class="border border-gray-700 dark:border-gray-500 px-4 py-2 text-left"
					>Автоматический переход между этапами
				</td>
				<td class="border border-gray-700 dark:border-gray-500 px-4 py-2 text-center"
					>{settings.auto_mode ? 'Да' : 'Нет'}</td
				>
			</tr>
			<tr class="dark:bg-gray-700 dark:text-gray-100">
				<td class="border border-gray-700 dark:border-gray-500 px-4 py-2 text-left"
					>Температура ТД2 завершения отбора тела
				</td>
				<td class="border border-gray-700 dark:border-gray-500 px-4 py-2 text-center">
					<div class="editable-container flex justify-center">
						<EditNumbersModal
							onSave={(newValues) => {
								// Используйте полный тип объекта как в компоненте
								onSave('hearts_finish_temp', newValues[0].value);
							}}
							values={[
								{
									name: 'Температура ТД2 завершения отбора тела',
									value: settings.hearts_finish_temp,
									unit: '°С',
									min: 0,
									max: 110.0,
									step: 0.1
								}
							]}
						/>
					</div>
				</td>
			</tr>
			<tr class="dark:bg-gray-700 dark:text-gray-100">
				<td class="border border-gray-700 dark:border-gray-500 px-4 py-2 text-left"
					>Скорость сброса (время открытого клапана)
				</td>
				<td class="border border-gray-700 dark:border-gray-500 px-4 py-2 text-center">
					<div class="editable-container flex justify-center">
						<EditNumbersModal
							onSave={(newValues) => {
								// Используйте полный тип объекта как в компоненте
								onSave('release_speed', newValues[0].value);
							}}
							values={[
								{
									name: 'Скорость сброса (время открытого клапана). Значение должно быть меньше периода этапа отбора голов.',
									value: settings.release_speed,
									unit: 'сек',
									step: 0.1
								}
							]}
						/>
					</div>
				</td>
			</tr>
			<tr class="dark:bg-gray-700 dark:text-gray-100">
				<td class="border border-gray-700 dark:border-gray-500 px-4 py-2 text-left"
					>Отложенный пуск
				</td>
				<td class="border border-gray-700 dark:border-gray-500 px-4 py-2 text-center">
					<div class="editable-container flex justify-center">
						<EditTimeModal
							max={18000}
							onSave={(v) => onSave('start_delay', v)}
							seconds={settings.start_delay}
						/>
					</div>
				</td>
			</tr>
			<tr class="dark:bg-gray-700 dark:text-gray-100">
				<td class="border border-gray-700 dark:border-gray-500 px-4 py-2 text-left">Фильтр ТД</td>
				<td class="border border-gray-700 dark:border-gray-500 px-4 py-2 text-center"
					>{settings.tp_filter}</td
				>
			</tr>
			<tr class="dark:bg-gray-700 dark:text-gray-100">
				<td class="border border-gray-700 dark:border-gray-500 px-4 py-2 text-left">Подсветка</td>
				<td class="text-center">
					<p>
						{#if settings.backlight === 'active'}
							Включена
						{:else if settings.backlight === 'always'}
							Всегда включено
						{:else if settings.backlight === 'off'}
							Отключена
						{:else}
							Статус не определен
						{/if}
					</p>
				</td>
			</tr>
			<tr class="dark:bg-gray-700 dark:text-gray-100">
				<td class="border border-gray-700 dark:border-gray-500 px-4 py-2 text-left"
					>Сдвиг температуры отбора тела на 0.07°С
				</td>
				<td class="border border-gray-700 dark:border-gray-500 px-4 py-2 text-center"
					>{settings.hearts_temp_shift ? 'Включен' : 'Отключен'}</td
				>
			</tr>
			<tr class="dark:bg-gray-700 dark:text-gray-100">
				<td class="border border-gray-700 dark:border-gray-500 px-4 py-2 text-left"
					>Пауза "на себя"
				</td>
				<td class="border border-gray-700 dark:border-gray-500 px-4 py-2 text-center"
					>{settings.hearts_pause ? 'Включен' : 'Отключен'}</td
				>
			</tr>
			<tr class="dark:bg-gray-700 dark:text-gray-100">
				<td class="border border-gray-700 dark:border-gray-500 px-4 py-2 text-left">Формула</td>
				<td class="border border-gray-700 dark:border-gray-500 px-4 py-2 text-center items-center">
					<div class="editable-container flex justify-center">
						<EditBooleanModal
							labels={{ true: 'Включено', false: 'Выключено' }}
							onSave={(v) => onSave('formula', v.value)}
							value={{ name: 'Статус', value: settings.formula }}
						/>
					</div>
				</td>
			</tr>
			<tr class="dark:bg-gray-700 dark:text-gray-100">
				<td class="border border-gray-700 dark:border-gray-500 px-4 py-2 text-left"
					>Температура начала формулы
				</td>
				<td class="border border-gray-700 dark:border-gray-500 px-4 py-2 text-center">
					<div class="editable-container flex justify-center">
						<EditNumbersModal
							onSave={(newValues) => {
								// Используйте полный тип объекта как в компоненте
								onSave('formula_start_temp', newValues[0].value);
							}}
							values={[
								{
									name: 'Температура начала формулы',
									value: settings.formula_start_temp,
									unit: '°С',
									min: 84.0,
									max: 100.0,
									step: 0.1
								}
							]}
						/>
					</div>
				</td>
			</tr>
			<tr class="dark:bg-gray-700 dark:text-gray-100">
				<td class="border border-gray-700 dark:border-gray-500 px-4 py-2 text-left"
					>Поправка давления в кубе
				</td>
				<td class="border border-gray-700 dark:border-gray-500 px-4 py-2 text-center">
					<div class="editable-container flex justify-center">
						<EditNumbersModal
							onSave={(newValues) => {
								// Используйте полный тип объекта как в компоненте
								onSave('tank_mmhg', newValues[0].value);
							}}
							values={[
								{
									name: 'Декремент при отборе тела',
									value: settings.tank_mmhg,
									unit: 'мм рт. ст.',
									min: 0,
									max: 50
								}
							]}
						/>
					</div>
				</td>
			</tr>
			<tr class="dark:bg-gray-700 dark:text-gray-100">
				<td class="border border-gray-700 dark:border-gray-500 px-4 py-2 text-left"
					>Поправка ТД2
				</td>
				<td class="border border-gray-700 dark:border-gray-500 px-4 py-2 text-center"
					>{settings.tp2_shift} °С
				</td>
			</tr>
			<tr class="dark:bg-gray-700 dark:text-gray-100">
				<td class="border border-gray-700 dark:border-gray-500 px-4 py-2 text-left"
					>Контроль ТД1 для выхода SIGNAL
				</td>
				<td class="border border-gray-700 dark:border-gray-500 px-4 py-2 text-center"
					>{settings.signal_tp1_control ? 'Включен' : 'Отключен'}</td
				>
			</tr>
			<tr class="dark:bg-gray-700 dark:text-gray-100">
				<td class="border border-gray-700 dark:border-gray-500 px-4 py-2 text-left"
					>Инвертировать SIGNAL
				</td>
				<td class="border border-gray-700 dark:border-gray-500 px-4 py-2 text-center"
					>{settings.signal_inverted ? 'Включен' : 'Отключен'}</td
				>
			</tr>
			<tr class="dark:bg-gray-700 dark:text-gray-100">
				<td class="border border-gray-700 dark:border-gray-500 px-4 py-2 text-left"
					>Целевая температура на ТД1 при контроле ТД1 для выхода SIGNAL
				</td>
				<td class="border border-gray-700 dark:border-gray-500 px-4 py-2 text-center"
					>{settings.tp1_control_temp} °С
				</td>
			</tr>
			<tr class="dark:bg-gray-700 dark:text-gray-100">
				<td class="border border-gray-700 dark:border-gray-500 px-4 py-2 text-left"
					>Старт по ТД1 при контроле ТД1 для выхода SIGNAL.
				</td>
				<td class="border border-gray-700 dark:border-gray-500 px-4 py-2 text-center"
					>{settings.tp1_control_start ? 'Включен' : 'Отключен'}</td
				>
			</tr>
			<tr class="dark:bg-gray-700 dark:text-gray-100">
				<td class="border border-gray-700 dark:border-gray-500 px-4 py-2 text-left"
					>Лимит стабилизации
				</td>
				<td class="border border-gray-700 dark:border-gray-500 px-4 py-2 text-center"
					>{settings.tp1_control_start === 0 ? 'Выключен' : settings.tp1_control_start} сек
				</td>
			</tr>
			<tr class="dark:bg-gray-700 dark:text-gray-100">
				<td class="border border-gray-700 dark:border-gray-500 px-4 py-2 text-left"
					>Завершить этап при превышении лимита стабилизации
				</td>
				<td class="border border-gray-700 dark:border-gray-500 px-4 py-2 text-center"
					>{settings.tp1_control_start ? 'Да' : 'Нет'}</td
				>
			</tr>
			<tr class="dark:bg-gray-700 dark:text-gray-100">
				<td class="border border-gray-700 dark:border-gray-500 px-4 py-2 text-left"
					>Время сброса. Включается опция "Сброс и снижение" при значении больше 0
				</td>
				<td class="border border-gray-700 dark:border-gray-500 px-4 py-2 text-center">
					<div class="editable-container flex justify-center">
						<EditNumbersModal
							onSave={(newValues) => {
								// Используйте полный тип объекта как в компоненте
								onSave('release_timer', newValues[0].value);
							}}
							values={[
								{
									name: 'Время сброса.',
									value: settings.release_timer,
									unit: 'сек',
									min: 0,
									max: 1200
								}
							]}
						/>
					</div>
				</td>
			</tr>
			<tr class="dark:bg-gray-700 dark:text-gray-100">
				<td class="border border-gray-700 dark:border-gray-500 px-4 py-2 text-left"
					>Скорость отбора к окончанию голов, с. Включается опция "Сброс и снижение" при любом
					знаении.
				</td>
				<td class="border border-gray-700 dark:border-gray-500 px-4 py-2 text-center">
					<div class="editable-container flex justify-center">
						<EditNumbersModal
							onSave={(newValues) => {
								// Используйте полный тип объекта как в компоненте
								onSave('heads_final', newValues[0].value);
							}}
							values={[
								{
									name: 'Скорость отбора к окончанию голов',
									value: settings.heads_final,
									unit: 'сек',
									min: 0,
									max: 99.9,
									step: 0.1
								}
							]}
						/>
					</div>
				</td>
			</tr>
		</tbody>
	</table>
</div>

<style>
	.settings-table {
		@apply w-full border-collapse border border-gray-700 dark:border-gray-500;
	}

	.settings-table th,
	.settings-table td {
		@apply border border-gray-700 dark:border-gray-500;
	}

	.settings-table {
		@apply w-full border-collapse border border-gray-700 dark:border-gray-500;
		table-layout: fixed;
	}

	.settings-table th,
	.settings-table td {
		@apply border border-gray-700 dark:border-gray-500;
	}

	.settings-table th:first-child,
	.settings-table td:first-child {
		width: 60%;
	}

	.settings-table th:last-child,
	.settings-table td:last-child {
		width: 40%;
	}

	.editable-container {
		min-width: 100px;
		display: flex;
	}
</style>
