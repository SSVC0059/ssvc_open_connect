<script lang="ts">
	import { modals } from 'svelte-modals';
	import Cancel from '~icons/tabler/x';
	import Check from '~icons/tabler/check';
	import ChevronLeft from '~icons/tabler/chevron-left';
	import ChevronRight from '~icons/tabler/chevron-right';

	import Step1 from '$lib/components/StartWizard/steps/WizardStep1.svelte';
	import Step2 from '$lib/components/StartWizard/steps/WizardStep2.svelte';
	import Step3 from '$lib/components/StartWizard/steps/WizardStep3.svelte';
	import Step4 from '$lib/components/StartWizard/steps/WizardStep4.svelte';
	import Step5 from '$lib/components/StartWizard/steps/WizardStep5.svelte';
	import Step6 from '$lib/components/StartWizard/steps/WizardStep6.svelte';
	import Step7 from '$lib/components/StartWizard/steps/WizardStep7.svelte';
	import LoadingSpinner from '$lib/components/LoadingSpinner.svelte';
	import type { SsvcSettings } from '$lib/types/ssvc';
	import { fetchSettings, saveSettings, sendCommand } from '$lib/api/ssvcApi';

	interface Props {
		isOpen: boolean;
	}

	interface Step {
		id: number;
		title: string;
		component: any;
		condition?: (s: SsvcSettings | undefined) => boolean;
		requiresInitialData?: boolean;
		initialData?: Record<string, unknown>; // Гарантирует, что это объект
	}

	let { isOpen }: Props = $props();
	let isLoading = $state(false);
	let settings = $state<SsvcSettings | undefined>(undefined);
	let stepData = $state<Record<number, any>>({}); // Новое состояние для данных шагов
	let cachedVisibleSteps = $state<Step[]>([]); // Кэшированные видимые шаги
	let currentIndex = $state(0);
	let isStepDataInitialized = $state(false);
	let hasFetchedInitialData = $state(false);

	// Текущий шаг из кэшированного массива
	const currentStep = $derived(cachedVisibleSteps[currentIndex] ?? null);

	async function getSsvcSetting(): Promise<void> {
		if (hasFetchedInitialData) return; // Предотвращаем повторные вызовы

		try {
			settings = await fetchSettings();
			hasFetchedInitialData = true;

			// Инициализируем stepData только один раз при первой загрузке
			if (!isStepDataInitialized) {
				const initialStepData: Record<number, any> = {};
				for (const step of steps) {
					if (step.requiresInitialData && step.initialData) {
						initialStepData[step.id] = { ...step.initialData };
					} else {
						initialStepData[step.id] = {};
					}
				}
				stepData = initialStepData;
				isStepDataInitialized = true;
			}
		} catch (error) {
			console.error('Failed to fetch settings:', error);
		}
	}

	$effect(() => {
		if (isOpen && !hasFetchedInitialData) {
			getSsvcSetting();
		}
	});

	const steps: Step[] = [
		{ id: 1,
			title: 'Основные',
			component: Step1
		},
		{ id: 2,
			title: 'Клапана',
			component: Step2
		},
		{ id: 3,
			title: 'Головы',
			component: Step3
		},
		{
			id: 4,
			title: 'Подголовники',
			component: Step4,
			condition: (s) => Boolean(s?.late_heads)
		},
		{
			id: 5,
			title: 'Тело',
			component: Step5
		},
		{
			id: 6,
			title: 'Отбор',
			component: Step6,
			condition: (s) => Boolean(s?.parallel_v3)
		},
		{
			id: 7,
			title: 'Хвосты',
			component: Step7,
			condition: (s) => Boolean(s?.tails)
		}
	];

	// Обновляем кэш когда меняются настройки
	// НОВОЕ (оптимизированное)
	$effect(() => {
		if (!settings) return;

		const newVisibleSteps = steps.filter(step => {
			if (!step.condition) return true;
			return step.condition(settings);
		});

		// Более надежная проверка изменений
		const hasChanged =
			newVisibleSteps.length !== cachedVisibleSteps.length ||
			newVisibleSteps.some((step, i) =>
				cachedVisibleSteps[i]?.id !== step.id
			);

		if (hasChanged) {
			console.log('🔄 Updating visible steps', newVisibleSteps.map(s => s.id));
			cachedVisibleSteps = newVisibleSteps;

			// Корректируем currentIndex если нужно
			if (currentIndex >= cachedVisibleSteps.length) {
				currentIndex = Math.max(0, cachedVisibleSteps.length - 1);
			}
		}
	});

	const nextStep = () => {
		if (currentIndex < cachedVisibleSteps.length - 1) currentIndex++;
	};

	const prevStep = () => {
		if (currentIndex > 0) currentIndex--;
	};

	const closeModal = () => {
		modals.close();
	};

	const submitSettings = async () => {
		isLoading = true;
		try {
			const success = await saveSettings(settings);
			console.log("success:" + success);

			if (success) {
				await sendCommand("start")
				console.log('Настройки успешно сохранены');
				closeModal();
			} else {
				console.error('Не удалось сохранить настройки');
			}
		} catch (error) {
			console.error('Ошибка при сохранении настроек:', error);
		} finally {
			isLoading = false;
		}
	};

</script>

{#if isOpen}
	<div
		role="dialog"
		class="wizard-overlay-override"
	>
		<div class="wizard"	>
			<!-- Заголовок -->
			<div class="modal-header">
				<h2 class="modal-title">Мастер запуска ректификации</h2>
				<button
					type="button"
					class="modal-close"
					onclick={closeModal}
					aria-label="Close"
					disabled={isLoading}
				>
					<Cancel class="modal-icon" />
				</button>
			</div>

			<div class="divider m-0"></div>

			<!-- Индикатор шагов -->
			<div class="steps-container">
				<div
					class="steps-wrapper"
					style:transform={`translateX(calc(-${currentIndex} * var(--step-width)))`}
				>
					{#each cachedVisibleSteps as step, i}
						<div
							class="step"
							class:current={i === currentIndex}
						>
							<div class="step-circle">
								{i + 1}
							</div>
							<span class="step-title">
                {step.title}
              </span>
						</div>
					{/each}
				</div>
			</div>

			{#if settings && currentStep && cachedVisibleSteps.length > 0}
				{@const Component = currentStep.component}
				<div class="wizard-content">
					<Component
						bind:settings
						{...stepData[currentStep.id]}
					/>
				</div>
			{/if}

			<div class="wizard-nav">
				{#if cachedVisibleSteps.length > 1}
					<button 
						type="button" 
						onclick={prevStep} 
						class="btn btn-back" disabled={isLoading}>
						<ChevronLeft />
						Назад
					</button>
				{:else}
					<div></div>
				{/if}

				{#if currentIndex < cachedVisibleSteps.length - 1}
					<button 
						type="button" 
						onclick={nextStep} 
						class="btn btn-primary" disabled={isLoading}>
						Дальше
						<ChevronRight />
					</button>
				{:else}
					<button 
						type="button" 
						onclick={submitSettings} 
						class="btn btn-success" disabled={isLoading}>
						{#if isLoading}
							<LoadingSpinner />
						{:else}
							<Check class="h-5 w-5" />
						{/if}
						Запустить
					</button>
				{/if}
			</div>
		</div>

	</div>
{/if}
