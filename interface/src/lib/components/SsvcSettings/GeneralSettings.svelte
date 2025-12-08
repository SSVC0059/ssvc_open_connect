<script lang="ts">
	import EditNumbersModal from '$lib/components/SsvcSettings/EditNumbersModal.svelte';
	import EditTimeModal from '$lib/components/SsvcSettings/EditTimeModal.svelte';

	import type { SsvcSettings } from '$lib/types/ssvc';

	const { settings, onSave } = $props<{
		settings: SsvcSettings;
		onSave: (field: string, value: any) => void;
	}>();

	// Состояния для управления модальными окнами
	let showNumbersModal = false;
	let showTimeModal = false;
	let currentModalData: any = null;
	let currentModalType: 'numbers' | 'time' | null = null;

</script>

{#if settings}
	<div class="settings-grid">
		<div class="settings-panel">
			<div class="settings-section">
				<h3 class="settings-section-title">Параметры отбора</h3>

				<div class="settings-item">
					<span class="input-label">Гистерезис при отборе тела</span>
					<EditNumbersModal
						onSave={(newValues) => {
								onSave('hyst', newValues[0].value);
							}}
						values={[
								{
									name: 'Гистерезис',
									value: settings.hyst,
									unit: '°С',
									step: 0.01,
									precision: 1,
									min: 0,
									max: 50
								}
							]}
					/>
				</div>

				<div class="settings-item">
					<span class="input-label">Декремент при отборе тела</span>
					<EditNumbersModal
						onSave={(newValues) => {
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

				<div class="settings-item">
					<span class="input-label">Время отбора голов</span>
					<EditTimeModal
						max={36000}
						onSave={(v) => onSave('heads_timer', v)}
						seconds={settings.heads_timer}
						step={300}
					/>
				</div>

				<div class="settings-item">
					<span class="input-label">Таймер фиксации температуры отбора тела</span>
					<EditNumbersModal
						onSave={(newValues) => {
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
			</div>

			<div class="settings-section">
				<h3 class="settings-section-title">Настройки реле и Signal</h3>
				<div class="settings-item">
					<span class="input-label">Реле инвертировано</span>
					<label class="toggle-container">
						<input type="checkbox" class="toggle-input" checked="{settings.relay_inverted}" disabled>
						<div class="toggle-slider"></div>
					</label>
				</div>

				<div class="settings-item">
					<span class="input-label">Автовключение реле</span>
					<label class="toggle-container">
						<input type="checkbox" class="toggle-input" checked="{settings.relay_autostart}" disabled>
						<div class="toggle-slider"></div>
					</label>
				</div>

				<div class="settings-item">
					<span class="input-label">Контроль ТД1 для выхода SIGNAL</span>
					<label class="toggle-container">
						<input type="checkbox" class="toggle-input" checked="{settings.signal_tp1_control}" disabled>
						<div class="toggle-slider"></div>
					</label>
				</div>

				<div class="settings-item">
					<span class="input-label">Инвертировать SIGNAL</span>
					<label class="toggle-container">
						<input type="checkbox" class="toggle-input" checked="{settings.signal_inverted}" disabled>
						<div class="toggle-slider"></div>
					</label>
				</div>

				<div class="settings-item">
					<span class="input-label">Целевая температура на ТД1 при контроле ТД1 для выхода SIGNAL</span>
					<span class="settings-value">{settings.tp1_control_temp} °С</span>
				</div>

				<div class="settings-item">
					<span class="input-label">Старт по ТД1 при контроле ТД1 для выхода SIGNAL.</span>
					<label class="toggle-container">
						<input type="checkbox" class="toggle-input" checked="{settings.tp1_control_start}" disabled>
						<div class="toggle-slider"></div>
					</label>
				</div>

			</div>

			<div class="settings-section">
				<h3 class="settings-section-title">Системные настройки</h3>

				<div class="settings-item">
					<span class="input-label">Звук</span>
					<label class="toggle-container">
						<input type="checkbox" class="toggle-input" checked="{settings.sound}" disabled>
						<div class="toggle-slider"></div>
					</label>
				</div>

				<div class="settings-item">
					<span class="input-label">Барометр</span>
					<label class="toggle-container">
						<input type="checkbox" class="toggle-input" checked="{settings.pressure}" disabled>
						<div class="toggle-slider"></div>
					</label>
				</div>

				<div class="settings-item">
					<span class="input-label">Отложенный пуск</span>
					<button class="settings-action">
						<span>00:00</span>
						<i data-feather="edit-2" class="text-xs"></i>
					</button>
				</div>

				<div class="settings-item">
					<span class="input-label">Поправка давления в кубе</span>
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

				<div class="settings-item">
					<span class="input-label">Поправка ТД2</span>
					<span class="settings-value">{settings.tp2_shift} °С</span>
				</div>

				<div class="settings-item">
					<span class="input-label">Фильтр ТД</span>
					<span class="settings-value">0</span>
				</div>
			</div>
		</div>

		<div class="settings-panel">
			<div class="settings-section">
				<h3 class="settings-section-title">Температурные настройки</h3>

				<div class="settings-item">
					<span class="input-label">Температура ТД2 завершения отбора тела</span>
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

				<div class="settings-item">
					<span class="input-label">Сдвиг температуры отбора тела на 0.07°С</span>
					<label class="toggle-container">
						<input type="checkbox" class="toggle-input" checked="{settings.hearts_temp_shift}" disabled>
						<div class="toggle-slider"></div>
					</label>
				</div>

				<div class="settings-item">
					<span class="input-label">Пауза "на себя"</span>
					<label class="toggle-container">
						<input type="checkbox" class="toggle-input" checked="{settings.hearts_pause}" disabled>
						<div class="toggle-slider"></div>
					</label>
				</div>

				<div class="settings-item">
					<span class="input-label">Формула</span>
					<label class="toggle-container">
						<input type="checkbox" class="toggle-input"
									 bind:checked={settings.formula}
									 onchange={() => {
										 onSave('formula', settings.formula);
									 }}
						>
						<div class="toggle-slider"></div>
					</label>
				</div>

				<div class="settings-item">
					<span class="input-label">Температура начала формулы</span>
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

				<div class="settings-item">
					<span class="input-label">Лимит стабилизации</span>
					<label class="toggle-container">
						<input type="checkbox" class="toggle-input" checked="{settings.tp1_control_start}" disabled>
						<div class="toggle-slider"></div>
					</label>
				</div>

				<div class="settings-item">
					<span class="input-label">Завершить этап при превышении лимита стабилизации</span>
					<label class="toggle-container">
						<input type="checkbox" class="toggle-input" checked="{settings.stab_limit_finish}" disabled>
						<div class="toggle-slider"></div>
					</label>
				</div>
			</div>

			{#if settings.release_timer}
				<div class="settings-section">
					<h3 class="settings-section-title">Сброс и снижение</h3>

					<div class="settings-item">
						<span class="input-label">Время сброса</span>
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
					<div class="settings-item">
						<span class="input-label">Скорость сброса</span>
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

					<div class="settings-item">
						<span class="input-label">Скорость отбора к окончанию голов</span>
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
				</div>
			{/if}

		</div>
	</div>
{/if}

