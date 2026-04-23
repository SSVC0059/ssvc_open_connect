<script lang="ts">
	import { fetchRelayRuleMetadata } from '$lib/api/ssvcApi';
	import ArrowNarrowUp from '~icons/tabler/arrow-narrow-up';
	import ArrowNarrowDown from '~icons/tabler/arrow-narrow-down';
	import Copy from '~icons/tabler/copy';
	import type {
		FaultCodeDescriptor,
		OpenConnectRelayRuleRow,
		OpenConnectRelayRules,
		Profile,
		RelayDescriptor,
		RelayRuleMetadata,
		RelayCondition
	} from '$lib/types/ssvc';

	let { sourceProfile } = $props<{ sourceProfile: Profile }>();
	let selectedRuleIndex = $state(0);
	let pendingDelete = $state(false);
	let relayMetadata = $state<RelayRuleMetadata | null>(null);
	let metadataLoading = $state(false);
	let metadataError = $state('');

	const relayMinBit = 0;

	const condTypes = [
		{ id: 'sensor_alarm', label: 'Сигнал тревоги датчика', shortLabel: 'Датчик', badgeClass: 'badge-info' },
		{ id: 'hardware_fault', label: 'Аппаратный сбой устройства', shortLabel: 'Сбой', badgeClass: 'badge-error' },
		{ id: 'rectification', label: 'Этап ректификации', shortLabel: 'Этап', badgeClass: 'badge-success' },
		{ id: 'ssvc_setting', label: 'Параметр контроллера SSVC', shortLabel: 'SSVC', badgeClass: 'badge-warning' }
	] as const;

	const sensorAlarmLevels = [
		{ value: 1, label: 'Минимальный (1)' },
		{ value: 2, label: 'Опасный (2)' },
		{ value: 3, label: 'Критический (3)' }
	];

	$effect(() => {
		ensureRules();
		const len = ensureRules().rules.length;
		if (len === 0) {
			selectedRuleIndex = 0;
		} else if (selectedRuleIndex >= len) {
			selectedRuleIndex = len - 1;
		}
	});

	// Сбрасываем подтверждение удаления при смене выбранного правила
	$effect(() => {
		// eslint-disable-next-line @typescript-eslint/no-unused-expressions
		selectedRuleIndex;
		pendingDelete = false;
	});

	$effect(() => {
		if (relayMetadata || metadataLoading) return;
		void loadRelayMetadata();
	});

	async function loadRelayMetadata() {
		metadataLoading = true;
		metadataError = '';
		try {
			const data = await fetchRelayRuleMetadata();
			if (data?.supported) {
				relayMetadata = data;
			} else {
				metadataError = 'Контроллер не вернул metadata для редактора реле.';
			}
		} catch (e) {
			metadataError = e instanceof Error ? e.message : 'Ошибка загрузки metadata';
		} finally {
			metadataLoading = false;
		}
	}

	function fallbackRelayDescriptors(): RelayDescriptor[] {
		const lineCount =
			relayMetadata?.relayLineCount && relayMetadata.relayLineCount > 0
				? relayMetadata.relayLineCount
				: 8;
		return Array.from({ length: lineCount }, (_, global) => {
			const reserved = global === 0 || global === 1;
			return {
				index: global + 1,
				label: `Реле ${global + 1}`,
				editable: !reserved,
				reason: reserved ? 'Системное реле AlarmManager (только просмотр)' : undefined,
				targetBit: global
			};
		});
	}

	function relays(): RelayDescriptor[] {
		return relayMetadata?.relays?.length ? relayMetadata.relays : fallbackRelayDescriptors();
	}

	/** Upper bound for global `targetBit` from metadata or descriptor list. */
	const relayMaxBit = $derived.by(() => {
		const md = relayMetadata;
		if (md?.relayLineCount != null && md.relayLineCount > 0) {
			return md.relayLineCount - 1;
		}
		const list = relays();
		const bits = list.map((r) => r.targetBit);
		return bits.length ? Math.max(...bits) : 7;
	});

	function editableRelays(): RelayDescriptor[] {
		return relays().filter((r) => r.editable);
	}

	function relayByTargetBit(bit: number): RelayDescriptor | undefined {
		return relays().find((r) => r.targetBit === clampBit(bit));
	}

	function relayLabelByBit(bit: number): string {
		return relayByTargetBit(bit)?.label ?? `Реле ${clampBit(bit) + 1}`;
	}

	function isRelayEditable(bit: number): boolean {
		return relayByTargetBit(bit)?.editable ?? false;
	}

	function ssvcFieldLabel(key: string): string {
		return relayMetadata?.ssvcFields.find((f) => f.key === key)?.label ?? key;
	}

	function faultCodeLabel(value: number): string {
		return (
			relayMetadata?.hardwareFaultCodes.find((f: FaultCodeDescriptor) => f.value === value)?.label ??
			String(value)
		);
	}

	function rectStageLabel(value: string): string {
		return relayMetadata?.rectificationStages.find((s) => s.value === value)?.label ?? value;
	}

	function condTypeMeta(type: string) {
		return condTypes.find((c) => c.id === type) ?? condTypes[0];
	}

	function ensureRules(): OpenConnectRelayRules {
		if (!sourceProfile.openConnectRelayRules) {
			sourceProfile.openConnectRelayRules = { schemaVersion: 1, rules: [] };
		}
		return sourceProfile.openConnectRelayRules;
	}

	function clampBit(bit: number): number {
		return Math.max(relayMinBit, Math.min(relayMaxBit, Math.trunc(bit)));
	}

	function normalizePriorityAndSort() {
		const r = ensureRules();
		r.rules = [...r.rules]
			.map((rule, idx) => ({ ...rule, priority: rule.priority ?? idx + 1 }))
			.sort((a, b) => (a.priority ?? 0) - (b.priority ?? 0))
			.map((rule, idx) => ({ ...rule, priority: idx + 1 }));
	}

	function defaultRule(priority: number): OpenConnectRelayRuleRow {
		return {
			enabled: true,
			targetBit: 2,
			actionEnergize: true,
			priority,
			condition: { type: 'sensor_alarm', levels: [2, 3] }
		};
	}

	function addRule() {
		const r = ensureRules();
		const nextPriority = r.rules.length + 1;
		r.rules = [...r.rules, defaultRule(nextPriority)];
		normalizePriorityAndSort();
		selectedRuleIndex = r.rules.length - 1;
	}

	function duplicateRule(index: number) {
		const r = ensureRules();
		const original = r.rules[index];
		const copy: OpenConnectRelayRuleRow = { ...original, priority: r.rules.length + 1 };
		r.rules = [...r.rules, copy];
		normalizePriorityAndSort();
		selectedRuleIndex = r.rules.length - 1;
	}

	function removeRule(index: number) {
		const r = ensureRules();
		r.rules = r.rules.filter((_, i) => i !== index);
		normalizePriorityAndSort();
		selectedRuleIndex = Math.max(0, Math.min(selectedRuleIndex, r.rules.length - 1));
		pendingDelete = false;
	}

	function moveRule(index: number, direction: -1 | 1) {
		const r = ensureRules();
		const swapWith = index + direction;
		if (swapWith < 0 || swapWith >= r.rules.length) {
			return;
		}
		const next = [...r.rules];
		[next[index], next[swapWith]] = [next[swapWith], next[index]];
		r.rules = next.map((row, i) => ({ ...row, priority: i + 1 }));
		selectedRuleIndex = swapWith;
	}

	function selectedRule(): OpenConnectRelayRuleRow | undefined {
		const r = ensureRules();
		return r.rules[selectedRuleIndex];
	}

	function conditionType(c: RelayCondition): string {
		return c.type;
	}

	function setConditionType(row: OpenConnectRelayRuleRow, t: string) {
		if (t === 'sensor_alarm') {
			row.condition = { type: 'sensor_alarm', levels: [2, 3] };
		} else if (t === 'hardware_fault') {
			row.condition = { type: 'hardware_fault', code: -1 };
		} else if (t === 'rectification') {
			row.condition = { type: 'rectification', stageEquals: 'waiting' };
		} else {
			row.condition = { type: 'ssvc_setting', key: 'formula', boolEquals: false };
		}
	}

	function setRelay(row: OpenConnectRelayRuleRow, raw: string) {
		const relayIndex = Number.parseInt(raw, 10);
		const chosen = relays().find((r) => r.index === relayIndex && r.editable);
		if (chosen) {
			row.targetBit = chosen.targetBit;
		}
	}

	function toggleSensorLevel(row: OpenConnectRelayRuleRow, level: number, checked: boolean) {
		if (row.condition.type !== 'sensor_alarm') {
			return;
		}
		const existing = new Set(row.condition.levels ?? []);
		if (checked) {
			existing.add(level);
		} else {
			existing.delete(level);
		}
		row.condition = {
			type: 'sensor_alarm',
			levels: [...existing].sort((a, b) => a - b),
			sensorNameContains: row.condition.sensorNameContains
		};
	}

	function prettyCondition(cond: RelayCondition): string {
		if (cond.type === 'sensor_alarm') {
			const levels = cond.levels?.length
				? cond.levels
						.map((l) => sensorAlarmLevels.find((s) => s.value === l)?.label ?? String(l))
						.join(', ')
				: 'Опасный (2), Критический (3)';
			const filter = cond.sensorNameContains ? ` • "${cond.sensorNameContains}"` : '';
			return `Тревога: ${levels}${filter}`;
		}
		if (cond.type === 'hardware_fault') {
			return `Сбой: ${faultCodeLabel(cond.code ?? -1)}`;
		}
		if (cond.type === 'rectification') {
			return `Этап: ${rectStageLabel(cond.stageEquals || '')}`;
		}
		if (cond.type !== 'ssvc_setting') {
			return 'Условие';
		}
		const ssvc = cond;
		const fieldType = relayMetadata?.ssvcFields.find((f) => f.key === ssvc.key)?.valueType;
		if (fieldType === 'float') {
			return `${ssvcFieldLabel(ssvc.key)} = ${ssvc.floatEquals ?? 0}`;
		}
		if (fieldType === 'int') {
			return `${ssvcFieldLabel(ssvc.key)} = ${ssvc.intEquals ?? 0}`;
		}
		return `${ssvcFieldLabel(ssvc.key)} = ${ssvc.boolEquals ? 'Да' : 'Нет'}`;
	}

	function getSelectedSsvcFieldType(row: OpenConnectRelayRuleRow): 'bool' | 'int' | 'float' {
		if (row.condition.type !== 'ssvc_setting') {
			return 'bool';
		}
		const cond = row.condition;
		return relayMetadata?.ssvcFields.find((f) => f.key === cond.key)?.valueType ?? 'bool';
	}

	function normalizeSsvcConditionValue(row: OpenConnectRelayRuleRow) {
		if (row.condition.type !== 'ssvc_setting') {
			return;
		}
		const fieldType = getSelectedSsvcFieldType(row);
		if (fieldType === 'bool') {
			row.condition.boolEquals = row.condition.boolEquals ?? false;
			delete row.condition.intEquals;
			delete row.condition.floatEquals;
		} else if (fieldType === 'int') {
			row.condition.intEquals = row.condition.intEquals ?? 0;
			delete row.condition.boolEquals;
			delete row.condition.floatEquals;
		} else {
			row.condition.floatEquals = row.condition.floatEquals ?? 0;
			delete row.condition.boolEquals;
			delete row.condition.intEquals;
		}
	}

	function sensorOptionsCount(): number {
		return relayMetadata?.sensorNames?.length ?? 0;
	}

	function roleOptionsCount(): number {
		return relayMetadata?.hardwareRoles?.length ?? 0;
	}
</script>

<div class="profile-card relay-rules-card">
	<div class="card-header">
		<h3 class="card-title">Реле Open Connect</h3>
	</div>

	<!-- Краткая информация -->
	<div class="relay-summary">
		<span><strong>8 реле</strong> — 2 системных + 6 пользовательских.</span>
		<span class="opacity-70">Правила исполняются на ESP32 (RTOS) в порядке приоритета сверху вниз.</span>
	</div>

	<!-- Системные реле — свёрнутые по умолчанию -->
	<details class="system-lines-collapsible">
		<summary>Системные реле (только просмотр)</summary>
		<div class="system-relays-list">
			{#each relays().filter((r) => !r.editable) as relay (relay.index)}
				<div class="system-relay-chip">
					<span>{relay.label}</span>
					<span class="badge badge-ghost badge-sm">Только просмотр</span>
				</div>
			{/each}
		</div>
		<p class="text-xs opacity-60 mt-1">
			Реле 1 и 2 управляются AlarmManager и недоступны для редактирования пользователем.
		</p>
	</details>

	<!-- Загрузка / Ошибка metadata -->
	{#if metadataLoading}
		<div class="alert alert-info py-2 mb-3">
			<span class="loading loading-spinner loading-xs"></span>
			<span class="text-sm">Загрузка данных редактора...</span>
		</div>
	{:else if metadataError}
		<div class="alert alert-warning py-2 mb-3">
			<span class="text-sm flex-1">{metadataError}</span>
			<button type="button" class="btn btn-xs" onclick={() => void loadRelayMetadata()}>
				Повторить
			</button>
		</div>
	{/if}

	<!-- Панель инструментов -->
	<div class="rules-toolbar">
		<button type="button" class="btn btn-sm btn-primary" onclick={addRule}>
			+ Добавить правило
		</button>
		<span class="text-xs opacity-60">Верхнее правило — наивысший приоритет</span>
	</div>

	<div class="relay-layout">
		<!-- Список правил -->
		<div class="rules-list">
			{#each ensureRules().rules as row, idx (idx)}
				<div
					class="rule-card"
					class:active={selectedRuleIndex === idx}
					class:rule-disabled={!(row.enabled ?? true)}
					role="button"
					tabindex="0"
					onclick={() => (selectedRuleIndex = idx)}
					onkeydown={(e) => {
						if (e.key === 'Enter' || e.key === ' ') {
							e.preventDefault();
							selectedRuleIndex = idx;
						}
					}}
				>
					<div class="rule-card-head">
						<div class="rule-card-meta">
							<span class="rule-index">#{idx + 1}</span>
							<span class="badge badge-xs {condTypeMeta(row.condition.type).badgeClass}">
								{condTypeMeta(row.condition.type).shortLabel}
							</span>
						</div>
						<!-- Кнопки управления — stopPropagation чтобы не выбирать правило -->
						<div
							class="rule-actions"
							role="presentation"
							onclick={(e) => e.stopPropagation()}
							onkeydown={(e) => e.stopPropagation()}
						>
							<div class="rule-actions-main">
								<input
									type="checkbox"
									class="toggle toggle-xs toggle-primary"
									checked={row.enabled ?? true}
									title={row.enabled ?? true ? 'Активно — нажмите чтобы отключить' : 'Отключено — нажмите чтобы включить'}
									onchange={(e) => {
										row.enabled = (e.currentTarget as HTMLInputElement).checked;
									}}
								/>
								<span class="rule-enabled-label">{row.enabled ?? true ? 'Вкл' : 'Выкл'}</span>
							</div>
							<div class="rule-actions-secondary">
								<button
									type="button"
									class="btn btn-ghost btn-xs rule-action-btn"
									onclick={() => moveRule(idx, -1)}
									disabled={idx === 0}
									title="Поднять приоритет"
								>
									<ArrowNarrowUp class="h-4 w-4" />
									<span>Вверх</span>
								</button>
								<button
									type="button"
									class="btn btn-ghost btn-xs rule-action-btn"
									onclick={() => moveRule(idx, 1)}
									disabled={idx === ensureRules().rules.length - 1}
									title="Снизить приоритет"
								>
									<ArrowNarrowDown class="h-4 w-4" />
									<span>Вниз</span>
								</button>
								<button
									type="button"
									class="btn btn-ghost btn-xs rule-action-btn"
									onclick={() => duplicateRule(idx)}
									title="Дублировать правило"
								>
									<Copy class="h-4 w-4" />
									<span>Копия</span>
								</button>
							</div>
						</div>
					</div>

					<div class="rule-card-body">
						<div class="rule-condition-title">{prettyCondition(row.condition)}</div>
						<div class="rule-action-preview">
							<span
								class="action-chip"
								class:action-on={row.actionEnergize ?? true}
								class:action-off={!(row.actionEnergize ?? true)}
							>
								{row.actionEnergize ?? true ? 'Вкл' : 'Выкл'}
							</span>
							<span class="text-sm">{relayLabelByBit(row.targetBit)}</span>
							{#if !isRelayEditable(row.targetBit)}
								<span class="badge badge-ghost badge-xs opacity-60">системное</span>
							{/if}
						</div>
					</div>
				</div>
			{/each}

			{#if ensureRules().rules.length === 0}
				<div class="empty-state">
					<div class="empty-state-icon">
						<svg xmlns="http://www.w3.org/2000/svg" width="32" height="32" fill="none" viewBox="0 0 24 24" stroke="currentColor" stroke-width="1.5" opacity="0.4">
							<path stroke-linecap="round" stroke-linejoin="round" d="M9 12h6m-3-3v6M12 3a9 9 0 100 18A9 9 0 0012 3z" />
						</svg>
					</div>
					<p class="font-medium text-sm">Нет правил</p>
					<p class="text-xs opacity-60">Добавьте правило для автоматического управления каналами реле</p>
					<button type="button" class="btn btn-sm btn-primary mt-3" onclick={addRule}>
						+ Добавить первое правило
					</button>
				</div>
			{/if}
		</div>

		<!-- Редактор правила -->
		<div class="rule-editor">
			{#if selectedRule()}
				{@const row = selectedRule()!}

				<!-- Заголовок редактора -->
				<div class="rule-row-head">
					<div>
						<h4 class="font-semibold">Правило #{selectedRuleIndex + 1}</h4>
						<span class="text-xs opacity-50">Изменения применяются автоматически</span>
					</div>
					{#if pendingDelete}
						<div class="delete-confirm">
							<span class="text-xs text-error font-medium">Удалить правило?</span>
							<button
								type="button"
								class="btn btn-error btn-xs"
								onclick={() => removeRule(selectedRuleIndex)}
							>Да</button>
							<button
								type="button"
								class="btn btn-ghost btn-xs"
								onclick={() => (pendingDelete = false)}
							>Отмена</button>
						</div>
					{:else}
						<button
							type="button"
							class="btn btn-ghost btn-xs text-error"
							onclick={() => (pendingDelete = true)}
						>
							Удалить
						</button>
					{/if}
				</div>

				<!-- Общие параметры -->
				<div class="editor-section editor-section--inline">
					<label class="label-cb">
						<input type="checkbox" bind:checked={row.enabled} />
						<span class="text-sm">Правило активно</span>
					</label>
				</div>

				<!-- Если -->
				<div class="editor-section">
					<div class="section-title">Если</div>

					<!-- Тип условия -->
					<div class="field-row">
						<select
							class="select select-bordered select-sm w-full"
							value={conditionType(row.condition)}
							onchange={(e) =>
								setConditionType(row, (e.currentTarget as HTMLSelectElement).value)}
						>
							{#each condTypes as ct}
								<option value={ct.id}>{ct.label}</option>
							{/each}
						</select>
					</div>

					<!-- Поля специфичные для типа -->
					{#if row.condition.type === 'sensor_alarm'}
						<div class="field-row">
							<span class="field-label">Уровни тревоги <span class="opacity-50">(OR)</span></span>
							<div class="levels-list">
								{#each sensorAlarmLevels as level}
									<label class="label-cb">
										<input
											type="checkbox"
											checked={(row.condition.levels ?? [2, 3]).includes(level.value)}
											onchange={(e) =>
												toggleSensorLevel(
													row,
													level.value,
													(e.currentTarget as HTMLInputElement).checked
												)}
										/>
										{level.label}
									</label>
								{/each}
							</div>
						</div>
						<div class="field-row">
							<span class="field-label">Фильтр по имени датчика <span class="opacity-50">(опционально)</span></span>
							<input
								class="input input-bordered input-sm w-full"
								list="relay-sensor-names"
								placeholder={sensorOptionsCount() > 0
									? `Из ${sensorOptionsCount()} датчиков или введите часть имени`
									: 'Введите имя вручную'}
								bind:value={row.condition.sensorNameContains}
							/>
						</div>

					{:else if row.condition.type === 'hardware_fault'}
						<div class="field-row">
							<span class="field-label">Тип сбоя</span>
							<select
								class="select select-bordered select-sm w-full"
								bind:value={row.condition.code}
							>
								{#each relayMetadata?.hardwareFaultCodes ?? [] as code (code.key)}
									<option value={code.value}>{code.label}</option>
								{/each}
							</select>
						</div>
						<div class="field-row">
							<span class="field-label">Роль устройства <span class="opacity-50">(содержит)</span></span>
							<input
								class="input input-bordered input-sm w-full"
								list="relay-hardware-roles"
								placeholder={roleOptionsCount() > 0
									? `Из ${roleOptionsCount()} ролей или введите часть`
									: 'Введите вручную (пример: bmp581)'}
								bind:value={row.condition.roleContains}
							/>
						</div>

					{:else if row.condition.type === 'rectification'}
						<div class="field-row">
							<span class="field-label">Этап процесса</span>
							<select
								class="select select-bordered select-sm w-full"
								bind:value={row.condition.stageEquals}
							>
								{#each relayMetadata?.rectificationStages ?? [] as stage (stage.value)}
									<option value={stage.value}>{stage.label}</option>
								{/each}
							</select>
						</div>

					{:else if row.condition.type === 'ssvc_setting'}
						<div class="field-row">
							<span class="field-label">Поле SSVC</span>
							<select
								class="select select-bordered select-sm w-full"
								bind:value={row.condition.key}
								onchange={() => normalizeSsvcConditionValue(row)}
							>
								{#each relayMetadata?.ssvcFields ?? [] as field (field.key)}
									<option value={field.key}>{field.label}</option>
								{/each}
							</select>
						</div>
						{@const selectedSsvcFieldType = getSelectedSsvcFieldType(row)}
						{#if selectedSsvcFieldType === 'bool'}
							<div class="field-row">
								<span class="field-label">Ожидаемое значение</span>
								<div class="join">
									<button
										type="button"
										class="join-item btn btn-sm"
										class:btn-success={row.condition.boolEquals === true}
										onclick={() => {
											if (row.condition.type === 'ssvc_setting')
												row.condition.boolEquals = true;
										}}
									>Да (Вкл)</button>
									<button
										type="button"
										class="join-item btn btn-sm"
										class:btn-error={row.condition.boolEquals !== true}
										onclick={() => {
											if (row.condition.type === 'ssvc_setting')
												row.condition.boolEquals = false;
										}}
									>Нет (Выкл)</button>
								</div>
							</div>
						{:else if selectedSsvcFieldType === 'int'}
							<div class="field-row">
								<span class="field-label">Значение</span>
								<input
									class="input input-bordered input-sm w-full"
									type="number"
									bind:value={row.condition.intEquals}
								/>
							</div>
						{:else}
							<div class="field-row">
								<span class="field-label">Значение</span>
								<input
									class="input input-bordered input-sm w-full"
									type="number"
									step="0.01"
									bind:value={row.condition.floatEquals}
								/>
							</div>
						{/if}
					{/if}
				</div>

				<!-- Тогда — action + relay на одной строке в естественном порядке -->
				<div class="editor-section">
					<div class="section-title">Тогда</div>
					<div class="then-row">
						<div class="join flex-shrink-0">
							<button
								type="button"
								class="join-item btn btn-sm"
								class:btn-success={row.actionEnergize ?? true}
								onclick={() => (row.actionEnergize = true)}
							>Включить</button>
							<button
								type="button"
								class="join-item btn btn-sm"
								class:btn-error={!(row.actionEnergize ?? true)}
								onclick={() => (row.actionEnergize = false)}
							>Выключить</button>
						</div>
						<span class="then-label">реле</span>
						<select
							class="select select-bordered select-sm then-relay-select"
							value={String(relayByTargetBit(row.targetBit)?.index ?? 3)}
							onchange={(e) => setRelay(row, (e.currentTarget as HTMLSelectElement).value)}
						>
							{#each editableRelays() as relay (relay.index)}
								<option value={relay.index}>{relay.label}</option>
							{/each}
						</select>
					</div>
				</div>

				<!-- Технические детали -->
				<details class="tech-details">
					<summary>Технические детали</summary>
					<div class="tech-details-body">
						<div class="tech-info text-xs">
							<div>Каналы 1–6 соответствуют физическим линиям реле расширителя PCF8574.</div>
							<div>Биты 0–1: системные (AlarmManager), биты 2–7: пользовательские.</div>
						</div>
						<div class="input-group">
							<span class="input-label">JSON правила (readonly)</span>
							<textarea class="textarea textarea-bordered h-36 font-mono text-xs" readonly
								>{JSON.stringify(row, null, 2)}</textarea
							>
						</div>
					</div>
				</details>
			{:else}
				<div class="editor-empty">
					<p class="text-sm opacity-60">Выберите правило в списке слева или создайте новое.</p>
				</div>
			{/if}
		</div>
	</div>

	<datalist id="relay-sensor-names">
		{#each relayMetadata?.sensorNames ?? [] as sensorName (sensorName)}
			<option value={sensorName}></option>
		{/each}
	</datalist>

	<datalist id="relay-hardware-roles">
		{#each relayMetadata?.hardwareRoles ?? [] as role (role)}
			<option value={role}></option>
		{/each}
	</datalist>
</div>

<style>
	.relay-summary {
		display: flex;
		flex-wrap: wrap;
		gap: 0.5rem 1rem;
		font-size: 0.9rem;
		margin-bottom: 0.75rem;
	}

	/* ── Системные реле (collapsible) ── */
	.system-lines-collapsible {
		border: 1px dashed oklch(var(--bc) / 0.2);
		border-radius: var(--rounded-box, 0.5rem);
		padding: 0.5rem 0.75rem;
		margin-bottom: 0.75rem;
		font-size: 0.88rem;
	}

	.system-lines-collapsible summary {
		cursor: pointer;
		font-weight: 500;
		opacity: 0.75;
		user-select: none;
		list-style: disclosure-closed;
	}

	.system-lines-collapsible[open] summary {
		list-style: disclosure-open;
		margin-bottom: 0.5rem;
	}

	.system-relays-list {
		display: flex;
		gap: 0.5rem;
		flex-wrap: wrap;
	}

	.system-relay-chip {
		border: 1px solid oklch(var(--bc) / 0.2);
		border-radius: 0.4rem;
		padding: 0.3rem 0.5rem;
		font-size: 0.82rem;
		display: flex;
		align-items: center;
		gap: 0.4rem;
	}

	/* ── Панель инструментов ── */
	.rules-toolbar {
		display: flex;
		align-items: center;
		gap: 0.75rem;
		flex-wrap: wrap;
		margin-bottom: 0.75rem;
	}

	/* ── Основная раскладка ── */
	.relay-layout {
		display: grid;
		grid-template-columns: minmax(260px, 320px) 1fr;
		gap: 1rem;
	}

	/* ── Список правил ── */
	.rules-list {
		display: flex;
		flex-direction: column;
		gap: 0.5rem;
	}

	.rule-card {
		border: 1px solid oklch(var(--bc) / 0.15);
		border-radius: var(--rounded-box, 0.5rem);
		padding: 0.6rem 0.7rem;
		background: transparent;
		cursor: pointer;
		transition: border-color 0.15s, background 0.15s;
	}

	.rule-card:hover {
		border-color: oklch(var(--bc) / 0.3);
	}

	.rule-card.active {
		border-color: oklch(var(--p) / 0.6);
		background: oklch(var(--p) / 0.07);
	}

	/* Полупрозрачность для отключённых правил */
	.rule-card.rule-disabled {
		opacity: 0.5;
	}

	.rule-card.rule-disabled.active {
		opacity: 0.75;
	}

	.rule-card-head {
		display: flex;
		align-items: center;
		justify-content: space-between;
		margin-bottom: 0.35rem;
	}

	.rule-card-meta {
		display: flex;
		align-items: center;
		gap: 0.4rem;
	}

	.rule-index {
		font-size: 0.78rem;
		font-weight: 600;
		opacity: 0.6;
	}

	.rule-actions {
		display: flex;
		flex-direction: column;
		align-items: flex-end;
		gap: 0.2rem;
		flex-shrink: 0;
	}

	.rule-actions-main {
		display: flex;
		align-items: center;
		gap: 0.35rem;
	}

	.rule-actions-secondary {
		display: inline-flex;
		align-items: center;
		gap: 0.2rem;
		flex-wrap: wrap;
		justify-content: flex-end;
	}

	.rule-enabled-label {
		font-size: 0.72rem;
		font-weight: 600;
		opacity: 0.65;
	}

	.rule-action-btn {
		display: inline-flex;
		align-items: center;
		gap: 0.2rem;
		min-height: 1.45rem;
		padding: 0.1rem 0.35rem;
		border: 1px solid oklch(var(--bc) / 0.22);
		border-radius: 0.35rem;
		line-height: 1;
	}

	.rule-action-btn:disabled {
		opacity: 0.45;
	}

	.rule-action-btn span {
		font-size: 0.68rem;
		font-weight: 600;
		opacity: 0.9;
	}

	.rule-card-body {
		display: grid;
		gap: 0.25rem;
		font-size: 0.85rem;
	}

	.rule-condition-title {
		font-size: 0.9rem;
		font-weight: 600;
		line-height: 1.25;
		opacity: 0.9;
	}

	/* Строка действия в карточке */
	.rule-action-preview {
		display: flex;
		align-items: center;
		gap: 0.4rem;
	}

	.action-chip {
		font-size: 0.72rem;
		font-weight: 600;
		padding: 0.1rem 0.35rem;
		border-radius: 0.3rem;
		letter-spacing: 0.03em;
	}

	.action-chip.action-on {
		background: oklch(var(--su) / 0.2);
		color: oklch(var(--suc, var(--su)));
	}

	.action-chip.action-off {
		background: oklch(var(--er) / 0.15);
		color: oklch(var(--erc, var(--er)));
	}

	/* ── Empty state ── */
	.empty-state {
		display: flex;
		flex-direction: column;
		align-items: center;
		text-align: center;
		padding: 2rem 1rem;
		gap: 0.35rem;
		border: 1px dashed oklch(var(--bc) / 0.18);
		border-radius: var(--rounded-box, 0.5rem);
	}

	.empty-state-icon {
		margin-bottom: 0.25rem;
		color: oklch(var(--bc));
	}

	/* ── Редактор ── */
	.rule-editor {
		border: 1px solid oklch(var(--bc) / 0.15);
		border-radius: var(--rounded-box, 0.5rem);
		padding: 1rem;
		background: oklch(var(--b1) / 0.7);
	}

	.rule-row-head {
		display: flex;
		justify-content: space-between;
		align-items: flex-start;
		margin-bottom: 0.85rem;
		gap: 0.5rem;
	}

	.delete-confirm {
		display: flex;
		align-items: center;
		gap: 0.4rem;
		flex-shrink: 0;
	}

	.editor-empty {
		display: flex;
		align-items: center;
		justify-content: center;
		min-height: 120px;
	}

	.editor-section {
		border: 1px solid oklch(var(--bc) / 0.1);
		border-radius: 0.5rem;
		padding: 0.75rem;
		margin-bottom: 0.75rem;
		background: oklch(var(--b1) / 0.4);
	}

	.section-title {
		font-size: 0.78rem;
		text-transform: uppercase;
		letter-spacing: 0.05em;
		font-weight: 700;
		opacity: 0.6;
		margin-bottom: 0.5rem;
	}

	/* Inline section (e.g. «Правило активно» checkbox without extra padding) */
	.editor-section--inline {
		padding: 0.45rem 0.75rem;
	}

	/* Single stacked field row */
	.field-row {
		display: flex;
		flex-direction: column;
		gap: 0.25rem;
		margin-bottom: 0.55rem;
	}

	.field-row:last-child {
		margin-bottom: 0;
	}

	.field-label {
		font-size: 0.8rem;
		opacity: 0.7;
	}

	/* «Тогда» — action toggle + label + relay select on one line */
	.then-row {
		display: flex;
		align-items: center;
		gap: 0.6rem;
		flex-wrap: wrap;
	}

	.then-label {
		font-size: 0.85rem;
		opacity: 0.7;
		flex-shrink: 0;
	}

	.then-relay-select {
		flex: 1;
		min-width: 120px;
	}

	.levels-list {
		display: flex;
		gap: 1rem;
		flex-wrap: wrap;
	}

	/* ── Технические детали (collapsible) ── */
	.tech-details {
		border: 1px dashed oklch(var(--bc) / 0.15);
		border-radius: 0.5rem;
		padding: 0.6rem 0.75rem;
	}

	.tech-details summary {
		cursor: pointer;
		font-size: 0.82rem;
		opacity: 0.6;
		user-select: none;
		list-style: disclosure-closed;
	}

	.tech-details[open] summary {
		list-style: disclosure-open;
		margin-bottom: 0.6rem;
	}

	.tech-details-body {
		display: grid;
		gap: 0.65rem;
	}

	.tech-info {
		border: 1px solid oklch(var(--bc) / 0.12);
		border-radius: 0.35rem;
		padding: 0.45rem 0.6rem;
		display: grid;
		gap: 0.2rem;
		opacity: 0.75;
	}

	/* ── Общие элементы ── */
	.label-cb {
		display: flex;
		align-items: center;
		gap: 0.35rem;
		font-size: 0.875rem;
		cursor: pointer;
	}

	/* ── Адаптив ── */
	@media (max-width: 960px) {
		.relay-layout {
			grid-template-columns: 1fr;
		}
	}

	@media (max-width: 640px) {
		.rule-grid {
			grid-template-columns: 1fr;
		}

		.span-2 {
			grid-column: span 1;
		}
	}
</style>
