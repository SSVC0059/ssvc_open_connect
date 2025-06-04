<script lang="ts">
	import { modals } from 'svelte-modals';
	import { focusTrap } from 'svelte-focus-trap';
	import { fly } from 'svelte/transition';
	import Cancel from '~icons/tabler/x';
	import Check from '~icons/tabler/check';
	import ChevronLeft from '~icons/tabler/chevron-left';
	import ChevronRight from '~icons/tabler/chevron-right';

	import Step1 from '$lib/components/StartWizard/steps/WizardStep1.svelte';
	import Step2 from '$lib/components/StartWizard/steps/WizardStep2.svelte';
	import Step3 from '$lib/components/StartWizard/steps/WizardStep3.svelte';
	import type { SsvcSettings } from '$lib/types/models';
	import { fetchSettings } from '$lib/api/settingsApi';

	interface Props {
		isOpen: boolean;
		title?: string;
		onConfirm?: () => void;
	}

	interface Step {
		id: number;
		title: string;
		component: any;
		requiresInitialData?: boolean;
		initialData?: Record<string, unknown>; // Гарантирует, что это объект
	}

	let { isOpen, title = 'Мастер запуска ректификации', onConfirm }: Props = $props();

	let isLoading = $state(false);
	let settings = $state<SsvcSettings | null>(null); // Добавляем реактивное состояние

	async function getSsvcSetting(): Promise<void> {
		settings = await fetchSettings();
	}

	$effect(() => {
		getSsvcSetting();
	});

	const steps: Step[] = [
		{
			id: 1,
			title: 'Основные',
			component: Step1
		},
		{
			id: 2,
			title: 'Клапана',
			component: Step2
		},
		{
			id: 3,
			title: 'Головы',
			component: Step3
		}
	];

	let currentStep = $state(1);

	const nextStep = () => {
		if (currentStep < steps.length) currentStep++;
	};

	const prevStep = () => {
		if (currentStep > 1) currentStep--;
	};

	const closeModal = () => {
		modals.close();
	};

	const submitSettings = async () => {
		isLoading = true;
		try {
			// Заглушка для асинхронного запроса
			await new Promise((resolve) => {
				setTimeout(() => {
					console.log('Settings saved:', settings);
					resolve(true);
				}, 1500);
			});

			closeModal();
			if (onConfirm) onConfirm();
		} catch (error) {
			console.error('Error saving settings:', error);
		} finally {
			isLoading = false;
		}
	};

	/**
	 * Рассчитывает скорость отбора в мл/час
	 * @param openTime Время открытого клапана в секундах
	 * @param period Период открытия в секундах (общий цикл: открыто + закрыто)
	 * @param flowRate Пропускная способность клапана в мл/час (при постоянном открытии)
	 * @returns Скорость отбора в мл/час
	 */
	export function calculateSamplingRate(
		openTime: number,
		period: number,
		flowRate: number
	): number {
		console.log(openTime);

		if (period <= 0) {
			throw new Error('Период открытия должен быть больше 0');
		}

		if (openTime < 0) {
			throw new Error('Время открытия не может быть отрицательным');
		}

		if (openTime > period) {
			throw new Error('Время открытия не может превышать общий период');
		}

		// Рассчитываем коэффициент открытия (долю времени, когда клапан открыт)
		const dutyCycle = openTime / period;

		// Скорость отбора = пропускная способность * коэффициент открытия
		return flowRate * dutyCycle;
	}
</script>

{#if isOpen}
	<div
		role="dialog"
		class="pointer-events-none fixed inset-0 z-50 flex items-center justify-center p-4"
		transition:fly={{ y: 50 }}
		use:focusTrap
	>
		<div
			class="rounded-box bg-base-100 shadow-secondary/30 pointer-events-auto flex max-h-[90vh] w-full max-w-2xl flex-col overflow-hidden shadow-lg"
		>
			<!-- Заголовок -->
			<div class="flex items-center justify-between p-4">
				<h2 class="text-base-content text-2xl font-bold">{title}</h2>
				<button
					class="btn btn-ghost btn-sm"
					onclick={closeModal}
					aria-label="Close"
					disabled={isLoading}
				>
					<Cancel class="h-5 w-5" />
				</button>
			</div>

			<div class="divider m-0"></div>

			<!-- Индикатор шагов -->
			<div class="flex justify-between px-6 py-2">
				{#each steps as step}
					<div class="flex flex-col items-center">
						<div
							class={`w-8 h-8 rounded-full flex items-center justify-center transition-colors
              ${currentStep >= step.id ? 'bg-primary text-primary-content' : 'bg-base-200 text-base-content'}`}
						>
							{step.id}
						</div>
						<span
							class={`text-xs mt-1 transition-colors ${currentStep === step.id ? 'font-medium text-primary' : 'text-base-content/70'}`}
						>
							{step.title}
						</span>
					</div>
				{/each}
			</div>

			{#if settings}
				<div class="lex-1 overflow-y-auto p-6 h-[60rem]">
					{#each steps as step (step.id)}
						{#if currentStep === step.id}
							{@const Component = step.component}

							{#if step.requiresInitialData}
								<Component bind:settings {...step.initialData} />
							{:else}
								<Component bind:settings />
							{/if}
						{/if}
					{/each}
				</div>
			{/if}

			<!-- Навигация -->
			<div class="flex justify-between p-4 bg-base-200">
				{#if currentStep > 1}
					<button onclick={prevStep} class="btn btn-ghost gap-2" disabled={isLoading}>
						<ChevronLeft class="h-5 w-5" />
						Назад
					</button>
				{:else}
					<div></div>
				{/if}

				{#if currentStep < steps.length}
					<button onclick={nextStep} class="btn btn-primary gap-2" disabled={isLoading}>
						Дальше
						<ChevronRight class="h-5 w-5" />
					</button>
				{:else}
					<button onclick={submitSettings} class="btn btn-success gap-2" disabled={isLoading}>
						{#if isLoading}
							<span class="loading loading-spinner"></span>
						{:else}
							<Check class="h-5 w-5" />
						{/if}
						Запустить процесс
					</button>
				{/if}
			</div>
		</div>
	</div>
{/if}
