<script lang="ts">
	import { Fa } from 'svelte-fa';
	import { faCog, faPlay } from '@fortawesome/free-solid-svg-icons';
	import { goto } from '$app/navigation';
	import Spinner from '../Spinner.svelte';
	import type { Profile, Profiles } from '$lib/types/ssvc';
	import { getProfiles, setActiveAndApplyProfile } from '$lib/api/Profiles';

	let profiles: Profiles = $state([]);
	let appliedProfileId: string | null = $state(null);
	let selectedProfileId: string | null = $state(null);
	let isLoading = $state(true);
	let isSaving = $state(false);

	const hasChanged = $derived(selectedProfileId !== appliedProfileId);

	$effect(() => {
		loadData();
	});

	async function loadData() {
		isLoading = true;
		try {
			profiles = await getProfiles();
			const appliedProfile = profiles.find(p => p.isApplied);
			if (appliedProfile) {
				appliedProfileId = appliedProfile.id;
				selectedProfileId = appliedProfile.id;
			}
		} catch (error) {
			console.error('Failed to load profile data:', error);
		} finally {
			isLoading = false;
		}
	}

	async function saveProfileChange() {
		if (!hasChanged || selectedProfileId === null) return;

		isSaving = true;
		try {
			const success = await setActiveAndApplyProfile(selectedProfileId.toString());
			if (success) {
				appliedProfileId = selectedProfileId;
				// Update isApplied flag in local state
				profiles = profiles.map(p => ({ ...p, isApplied: p.id === selectedProfileId }));
			}
		} catch (error) {
			console.error('Failed to switch profile:', error);
		} finally {
			isSaving = false;
		}
	}

	function goToSettings() {
		goto('/oc/settings');
	}
</script>

<div class="input-wrapper">
	<label for="profile-select" class="input-label">Профиль</label>
	<select
		id="profile-select"
		class="input-field"
		bind:value={selectedProfileId}
		disabled={isLoading || isSaving}
	>
		{#if isLoading}
			<option>Загрузка...</option>
		{:else if profiles.length === 0}
			<option value="" disabled>Нет доступных профилей</option>
		{:else}
			{#each profiles as profile}
				<option value={profile.id}>{profile.name}</option>
			{/each}
		{/if}
	</select>
	{#if isSaving}
		<Spinner />
	{/if}
	{#if hasChanged}
		<button class="btn btn-icon" onclick={saveProfileChange} disabled={isSaving} title="Применить профиль">
			<Fa icon={faPlay} />
		</button>
	{/if}
	<button class="btn btn-icon" onclick={goToSettings} title="Управление профилями">
		<Fa icon={faCog} />
	</button>
</div>
