<script lang="ts">
	import { fetchRelayRuleMetadata } from '$lib/api/ssvcApi';
	import { onMount } from 'svelte';
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
	let reorderMode = $state(false);
	let isNarrowLayout = $state(false);
	let mobilePanel = $state<'list' | 'editor'>('list');
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

	onMount(() => {
		const mediaQuery = window.matchMedia('(max-width: 960px)');
		const syncLayout = () => {
			isNarrowLayout = mediaQuery.matches;
			if (!mediaQuery.matches) {
				mobilePanel = 'list';
			}
		};
		syncLayout();
		mediaQuery.addEventListener('change', syncLayout);
		return () => mediaQuery.removeEventListener('change', syncLayout);
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
		const normalizedBit = Math.trunc(bit);
		const exact = relays().find((r) => r.targetBit === normalizedBit);
		if (exact) {
			return exact;
		}
		return relays().find((r) => r.targetBit === clampBit(normalizedBit));
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

	function cloneRelayCondition(condition: RelayCondition): RelayCondition {
		if (condition.type === 'sensor_alarm') {
			return { ...condition, levels: condition.levels ? [...condition.levels] : undefined };
		}
		return { ...condition };
	}

	function addRule() {
		const r = ensureRules();
		const nextPriority = r.rules.length + 1;
		r.rules = [...r.rules, defaultRule(nextPriority)];
		normalizePriorityAndSort();
		selectedRuleIndex = r.rules.length - 1;
		if (isNarrowLayout) {
			mobilePanel = 'editor';
		}
	}

	function duplicateRule(index: number) {
		const r = ensureRules();
		const original = r.rules[index];
		const copy: OpenConnectRelayRuleRow = {
			...original,
			priority: r.rules.length + 1,
			condition: cloneRelayCondition(original.condition)
		};
		r.rules = [...r.rules, copy];
		normalizePriorityAndSort();
		selectedRuleIndex = r.rules.length - 1;
		if (isNarrowLayout) {
			mobilePanel = 'editor';
		}
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

	function openRule(index: number) {
		selectedRuleIndex = index;
		if (isNarrowLayout) {
			mobilePanel = 'editor';
		}
	}

	function showRuleList() {
		mobilePanel = 'list';
	}

	function previousRule() {
		if (selectedRuleIndex > 0) {
			openRule(selectedRuleIndex - 1);
		}
	}

	function nextRule() {
		if (selectedRuleIndex < ensureRules().rules.length - 1) {
			openRule(selectedRuleIndex + 1);
		}
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
		const relayTargetBit = Number.parseInt(raw, 10);
		const chosen = relays().find((r) => r.targetBit === relayTargetBit && r.editable);
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

	function systemRelaysCount(): number {
		return relays().filter((r) => !r.editable).length;
	}

	function userRelaysCount(): number {
		return relays().filter((r) => r.editable).length;
	}
</script>

<div class="profile-card relay-rules-card">
	<div class="card-header relay-card-header">
		<div>
			<h3 class="card-title">Реле Open Connect</h3>
			<p class="header-subtitle">
				{ensureRules().rules.length} правил, {systemRelaysCount()} системных и {userRelaysCount()} пользовательских линий
			</p>
		</div>
		<button type="button" class="btn btn-sm btn-primary" onclick={addRule}>
			+ Добавить правило
		</button>
	</div>

	<div class="relay-summary">
		<span>Правила исполняются на ESP32 сверху вниз по приоритету.</span>
		<span>Редактирование применяется сразу после изменения поля.</span>
	</div>

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
		<p class="help-note">
			Реле 1 и 2 управляются AlarmManager и не участвуют в пользовательских правилах.
		</p>
	</details>

	{#if metadataLoading}
		<div class="alert alert-info py-2 mb-3">
			<span class="loading loading-spinner loading-xs"></span>
			<span class="text-sm">Загрузка данных редактора...</span>
		</div>
	{:else if metadataError}
		<div class="alert alert-warning py-2 mb-3">
			<span class="text-sm flex-1">{metadataError}</span>
			<button type="button" class="btn btn-sm" onclick={() => void loadRelayMetadata()}>
				Повторить
			</button>
		</div>
	{/if}

	<div class="rules-toolbar">
		<button type="button" class="btn btn-sm btn-outline" onclick={() => (reorderMode = !reorderMode)}>
			{reorderMode ? 'Завершить сортировку' : 'Изменить порядок'}
		</button>
		{#if reorderMode}
			<span class="toolbar-help">Перемещайте выбранное правило кнопками в карточке редактора.</span>
		{/if}
	</div>

	{#if isNarrowLayout && mobilePanel === 'editor' && selectedRule()}
		<div class="mobile-editor-header">
			<button type="button" class="btn btn-sm btn-ghost" onclick={showRuleList}>К списку</button>
			<span>Правило #{selectedRuleIndex + 1}</span>
			<div class="mobile-nav-actions">
				<button type="button" class="btn btn-xs btn-ghost" onclick={previousRule} disabled={selectedRuleIndex === 0}>
					Назад
				</button>
				<button
					type="button"
					class="btn btn-xs btn-ghost"
					onclick={nextRule}
					disabled={selectedRuleIndex === ensureRules().rules.length - 1}
				>
					Далее
				</button>
			</div>
		</div>
	{/if}

	<div class="relay-layout">
		{#if !isNarrowLayout || mobilePanel === 'list'}
			<div class="rules-list">
				{#each ensureRules().rules as row, idx (idx)}
					<button
						type="button"
						class="rule-list-item"
						class:active={selectedRuleIndex === idx}
						class:rule-disabled={!(row.enabled ?? true)}
						onclick={() => openRule(idx)}
						title={prettyCondition(row.condition)}
					>
						<div class="rule-list-head">
							<div class="rule-list-meta">
								<span class="rule-index">#{idx + 1}</span>
								<span class="badge badge-sm {condTypeMeta(row.condition.type).badgeClass}">
									{condTypeMeta(row.condition.type).shortLabel}
								</span>
							</div>
							<span class="rule-state">{row.enabled ?? true ? 'Активно' : 'Отключено'}</span>
						</div>
						<p class="rule-list-condition">{prettyCondition(row.condition)}</p>
						<p class="rule-list-action">
							{row.actionEnergize ?? true ? 'Включить' : 'Выключить'} {relayLabelByBit(row.targetBit)}
						</p>
					</button>
				{/each}

				{#if ensureRules().rules.length === 0}
					<div class="empty-state">
						<p class="font-medium text-sm">Пока нет правил</p>
						<p class="empty-hint">Добавьте первое правило для автоматического управления реле.</p>
						<button type="button" class="btn btn-sm btn-primary" onclick={addRule}>
							+ Добавить первое правило
						</button>
					</div>
				{/if}
			</div>
		{/if}

		{#if !isNarrowLayout || mobilePanel === 'editor'}
			<div class="rule-editor">
				{#if selectedRule()}
					{@const row = selectedRule()!}
					<div class="rule-row-head">
						<div>
							<h4 class="editor-title">Правило #{selectedRuleIndex + 1}</h4>
							<p class="help-note">Сначала условие, затем действие для выбранного реле.</p>
						</div>
						<div class="editor-actions">
							<button
								type="button"
								class="btn btn-sm btn-ghost"
								onclick={() => duplicateRule(selectedRuleIndex)}
							>
								<Copy class="h-4 w-4" />
								Копировать
							</button>
							{#if reorderMode}
								<button
									type="button"
									class="btn btn-sm btn-ghost"
									onclick={() => moveRule(selectedRuleIndex, -1)}
									disabled={selectedRuleIndex === 0}
								>
									<ArrowNarrowUp class="h-4 w-4" />
									Выше
								</button>
								<button
									type="button"
									class="btn btn-sm btn-ghost"
									onclick={() => moveRule(selectedRuleIndex, 1)}
									disabled={selectedRuleIndex === ensureRules().rules.length - 1}
								>
									<ArrowNarrowDown class="h-4 w-4" />
									Ниже
								</button>
							{/if}
							{#if pendingDelete}
								<div class="delete-confirm">
									<span>Удалить правило #{selectedRuleIndex + 1}?</span>
									<button
										type="button"
										class="btn btn-error btn-sm"
										onclick={() => removeRule(selectedRuleIndex)}
									>
										Удалить
									</button>
									<button
										type="button"
										class="btn btn-ghost btn-sm"
										onclick={() => (pendingDelete = false)}
									>
										Отмена
									</button>
								</div>
							{:else}
								<button
									type="button"
									class="btn btn-sm btn-ghost text-error"
									onclick={() => (pendingDelete = true)}
								>
									Удалить
								</button>
							{/if}
						</div>
					</div>

					<div class="editor-step">
						<div class="step-title">1. Состояние правила</div>
						<label class="label-cb">
							<input type="checkbox" bind:checked={row.enabled} />
							<span>Правило активно</span>
						</label>
					</div>

					<div class="editor-step">
						<div class="step-title">2. Если</div>
						<div class="field-row">
							<span class="field-label">Тип условия</span>
							<select
								class="select select-bordered select-sm w-full"
								value={conditionType(row.condition)}
								onchange={(e) => setConditionType(row, (e.currentTarget as HTMLSelectElement).value)}
							>
								{#each condTypes as ct}
									<option value={ct.id}>{ct.label}</option>
								{/each}
							</select>
						</div>

						{#if row.condition.type === 'sensor_alarm'}
							<div class="field-row">
								<span class="field-label">Уровни тревоги (любое совпадение)</span>
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
								<span class="field-label">Фильтр по имени датчика (опционально)</span>
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
								<select class="select select-bordered select-sm w-full" bind:value={row.condition.code}>
									{#each relayMetadata?.hardwareFaultCodes ?? [] as code (code.key)}
										<option value={code.value}>{code.label}</option>
									{/each}
								</select>
							</div>
							<div class="field-row">
								<span class="field-label">Роль устройства (содержит)</span>
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
												if (row.condition.type === 'ssvc_setting') row.condition.boolEquals = true;
											}}
										>
											Да (Вкл)
										</button>
										<button
											type="button"
											class="join-item btn btn-sm"
											class:btn-error={row.condition.boolEquals !== true}
											onclick={() => {
												if (row.condition.type === 'ssvc_setting') row.condition.boolEquals = false;
											}}
										>
											Нет (Выкл)
										</button>
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

					<div class="editor-step">
						<div class="step-title">3. Тогда</div>
						<div class="then-row">
							<div class="join flex-shrink-0">
								<button
									type="button"
									class="join-item btn btn-sm"
									class:btn-success={row.actionEnergize ?? true}
									onclick={() => (row.actionEnergize = true)}
								>
									Включить
								</button>
								<button
									type="button"
									class="join-item btn btn-sm"
									class:btn-error={!(row.actionEnergize ?? true)}
									onclick={() => (row.actionEnergize = false)}
								>
									Выключить
								</button>
							</div>
							<span class="then-label">реле</span>
							<select
								class="select select-bordered select-sm then-relay-select"
							value={String(relayByTargetBit(row.targetBit)?.targetBit ?? row.targetBit)}
								onchange={(e) => setRelay(row, (e.currentTarget as HTMLSelectElement).value)}
							>
								{#each editableRelays() as relay (relay.index)}
								<option value={relay.targetBit}>{relay.label}</option>
								{/each}
							</select>
						</div>
					</div>

					<details class="tech-details">
						<summary>Технические детали и JSON</summary>
						<div class="tech-details-body">
							<div class="tech-info text-sm">
								<div>Каналы 1–6 соответствуют физическим линиям реле расширителя PCF8574.</div>
								<div>Биты 0–1: системные (AlarmManager), биты 2–7: пользовательские.</div>
							</div>
							<div class="input-group">
								<span class="field-label">JSON правила (readonly)</span>
								<textarea class="textarea textarea-bordered h-32 font-mono text-xs" readonly
									>{JSON.stringify(row, null, 2)}</textarea
								>
							</div>
						</div>
					</details>
				{:else}
					<div class="editor-empty">
						<p>Выберите правило в списке или создайте новое.</p>
					</div>
				{/if}
			</div>
		{/if}
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
	.relay-rules-card {
		display: flex;
		flex-direction: column;
		gap: 0.75rem;
	}

	.relay-card-header {
		margin-bottom: 0;
	}

	.header-subtitle {
		margin: 0.15rem 0 0;
		font-size: 0.9rem;
		opacity: 0.82;
	}

	.relay-summary {
		display: flex;
		flex-wrap: wrap;
		gap: 0.25rem 1rem;
		font-size: 0.88rem;
		opacity: 0.86;
	}

	.system-lines-collapsible {
		border: 1px dashed oklch(var(--bc) / 0.2);
		border-radius: var(--rounded-box, 0.5rem);
		padding: 0.5rem 0.7rem;
		font-size: 0.86rem;
	}

	.system-lines-collapsible summary {
		cursor: pointer;
		font-weight: 500;
		opacity: 0.86;
		user-select: none;
		list-style: disclosure-closed;
	}

	.system-lines-collapsible[open] summary {
		list-style: disclosure-open;
		margin-bottom: 0.5rem;
	}

	.system-relays-list {
		display: flex;
		gap: 0.4rem;
		flex-wrap: wrap;
	}

	.system-relay-chip {
		border: 1px solid oklch(var(--bc) / 0.2);
		border-radius: 0.4rem;
		padding: 0.22rem 0.5rem;
		font-size: 0.82rem;
		display: flex;
		align-items: center;
		gap: 0.4rem;
	}

	.help-note {
		font-size: 0.82rem;
		opacity: 0.8;
		margin: 0.4rem 0 0;
	}

	.rules-toolbar {
		display: flex;
		align-items: center;
		gap: 0.6rem;
		flex-wrap: wrap;
	}

	.toolbar-help {
		font-size: 0.82rem;
		opacity: 0.82;
	}

	.mobile-editor-header {
		display: flex;
		align-items: center;
		justify-content: space-between;
		gap: 0.4rem;
		padding: 0.45rem 0.55rem;
		border: 1px solid oklch(var(--bc) / 0.16);
		border-radius: 0.5rem;
		background: oklch(var(--b1) / 0.72);
		position: sticky;
		top: 0;
		z-index: 4;
	}

	.mobile-nav-actions {
		display: inline-flex;
		gap: 0.25rem;
	}

	.relay-layout {
		display: grid;
		grid-template-columns: minmax(280px, 340px) minmax(0, 1fr);
		gap: 1rem;
		align-items: start;
	}

	.rules-list {
		display: flex;
		flex-direction: column;
		gap: 0.45rem;
	}

	.rule-list-item {
		border: 1px solid oklch(var(--bc) / 0.15);
		border-radius: var(--rounded-box, 0.5rem);
		padding: 0.62rem 0.7rem;
		background: transparent;
		cursor: pointer;
		transition: border-color 0.15s, background 0.15s ease-in-out;
		display: flex;
		flex-direction: column;
		gap: 0.26rem;
		text-align: left;
	}

	.rule-list-item:hover {
		border-color: oklch(var(--bc) / 0.3);
	}

	.rule-list-item.active {
		border-color: oklch(var(--p) / 0.6);
		background: oklch(var(--p) / 0.07);
	}

	.rule-list-item.rule-disabled {
		opacity: 0.64;
	}

	.rule-list-item.rule-disabled.active {
		opacity: 0.84;
	}

	.rule-list-head {
		display: flex;
		align-items: center;
		justify-content: space-between;
		gap: 0.5rem;
	}

	.rule-list-meta {
		display: flex;
		align-items: center;
		gap: 0.4rem;
	}

	.rule-index {
		font-size: 0.82rem;
		font-weight: 600;
		opacity: 0.84;
	}

	.rule-state {
		font-size: 0.8rem;
		font-weight: 500;
		opacity: 0.85;
	}

	.rule-list-condition {
		margin: 0;
		font-size: 0.9rem;
		font-weight: 600;
		line-height: 1.28;
		white-space: nowrap;
		overflow: hidden;
		text-overflow: ellipsis;
	}

	.rule-list-action {
		margin: 0;
		font-size: 0.84rem;
		opacity: 0.88;
		white-space: nowrap;
		overflow: hidden;
		text-overflow: ellipsis;
	}

	.empty-state {
		display: flex;
		flex-direction: column;
		align-items: center;
		text-align: center;
		padding: 1.6rem 0.9rem;
		gap: 0.5rem;
		border: 1px dashed oklch(var(--bc) / 0.18);
		border-radius: var(--rounded-box, 0.5rem);
	}

	.empty-hint {
		margin: 0;
		font-size: 0.9rem;
		opacity: 0.84;
	}

	.rule-editor {
		border: 1px solid oklch(var(--bc) / 0.15);
		border-radius: var(--rounded-box, 0.5rem);
		padding: 0.9rem;
		background: oklch(var(--b1) / 0.7);
		display: flex;
		flex-direction: column;
		gap: 0.75rem;
	}

	.rule-row-head {
		display: flex;
		justify-content: space-between;
		align-items: flex-start;
		gap: 0.7rem;
		flex-wrap: wrap;
	}

	.editor-title {
		margin: 0;
		font-size: 1.05rem;
	}

	.editor-actions {
		display: flex;
		gap: 0.35rem;
		flex-wrap: wrap;
		justify-content: flex-end;
	}

	.delete-confirm {
		display: flex;
		align-items: center;
		gap: 0.35rem;
		font-size: 0.85rem;
		flex-wrap: wrap;
	}

	.editor-empty {
		display: flex;
		align-items: center;
		justify-content: center;
		min-height: 120px;
		font-size: 0.94rem;
		opacity: 0.86;
	}

	.editor-step {
		padding: 0.7rem 0;
		border-top: 1px solid oklch(var(--bc) / 0.1);
	}

	.editor-step:first-of-type {
		border-top: 0;
		padding-top: 0.2rem;
	}

	.step-title {
		font-size: 0.82rem;
		font-weight: 700;
		letter-spacing: 0.03em;
		text-transform: uppercase;
		margin-bottom: 0.55rem;
		opacity: 0.86;
	}

	.field-row {
		display: flex;
		flex-direction: column;
		gap: 0.32rem;
		margin-bottom: 0.7rem;
	}

	.field-row:last-child {
		margin-bottom: 0;
	}

	.field-label {
		font-size: 0.84rem;
		opacity: 0.86;
	}

	.then-row {
		display: flex;
		align-items: center;
		gap: 0.6rem;
		flex-wrap: wrap;
	}

	.then-label {
		font-size: 0.85rem;
		opacity: 0.88;
		flex-shrink: 0;
	}

	.then-relay-select {
		flex: 1;
		min-width: 120px;
	}

	.levels-list {
		display: flex;
		gap: 0.9rem;
		flex-wrap: wrap;
	}

	.tech-details {
		border: 1px dashed oklch(var(--bc) / 0.15);
		border-radius: 0.5rem;
		padding: 0.6rem 0.7rem;
	}

	.tech-details summary {
		cursor: pointer;
		font-size: 0.84rem;
		opacity: 0.86;
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
		opacity: 0.9;
	}

	.label-cb {
		display: flex;
		align-items: center;
		gap: 0.35rem;
		font-size: 0.92rem;
		cursor: pointer;
	}

	@media (max-width: 960px) {
		.relay-layout {
			grid-template-columns: 1fr;
		}

		.rule-editor {
			padding: 0.75rem;
		}
	}

	@media (max-width: 640px) {
		.relay-card-header {
			flex-direction: column;
			align-items: flex-start;
			gap: 0.6rem;
		}

		.mobile-editor-header {
			flex-wrap: wrap;
		}

		.editor-actions {
			justify-content: flex-start;
		}

		.then-row {
			align-items: stretch;
		}
	}
</style>
