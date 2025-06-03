<script lang="ts">
	export let step: number;
	export let totalSteps: number;
	export let onBack: () => void;
	export let onNext: () => void;
	export let onCancel: () => void;
	export let onSave: () => void;
</script>

<div class="flex justify-end gap-4 mt-6">
	<button class="btn" onclick={onCancel}>Отмена</button>

	{#if step > 0}
		<button onclick={onBack} class="btn">Назад</button>
	{/if}

	{#if step < totalSteps - 1}
		<button onclick={onNext} class="btn btn-primary">Далее</button>
	{:else}
		<button onclick={onSave} class="btn btn-success">Сохранить</button>
	{/if}
</div>
