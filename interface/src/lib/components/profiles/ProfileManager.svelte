<script lang="ts">
	import {Fa} from 'svelte-fa';
	import {faCheck, faClone, faPen, faPlay, faPlus, faTrash, faUpload} from '@fortawesome/free-solid-svg-icons';
	import Spinner from '../Spinner.svelte';
	import type {Profile, Profiles} from '$lib/types/ssvc';
	import {
		copyProfiles,
		createProfiles,
		deleteProfiles,
		getProfiles, renameProfiles,
		setActiveAndApplyProfile
	} from '$lib/api/Profiles';

	let profiles: Profiles | undefined = $state(undefined);
	let isLoading: boolean = $state(true);
	let error: string | null = $state(null);
	let selectedProfile: Profile | null = $state(null); // Profile selected for viewing/editing
	let appliedProfileId: string = $state(""); // ID of the profile active on the controller

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
			}
		} catch (err) {
			console.error('Ошибка загрузки данных:', err);
			error = 'Ошибка при загрузке данных о профилях.';
		} finally {
			isLoading = false;
		}
	}

	function selectProfile(profileId: string) {
		selectedProfile = profiles?.find(p => p.id === profileId) || null;
	}

	async function handleApply(id: string) {
		try {
			const success = await setActiveAndApplyProfile(id.toString());
			if (success) {
				appliedProfileId = id;
				// Update isApplied flag in local state
				if (profiles) {
					profiles = profiles.map(p => ({ ...p, isApplied: p.id === id }));
				}
			} else {
				// Handle error applying profile
				console.error('Failed to apply profile');
			}
		} catch (err) {
			console.error('Error applying profile:', err);
		}
	}

	async function startEditing(profile: Profile) {
		const newName = prompt('Введите новое имя профиля:', profile.name);
		if (newName && newName !== profile.name) {
			try {
				const success = await renameProfiles(profile.id, newName);
				if (success) {
					await loadData();
				} else {
					alert('Не удалось переименовать профиль.');
				}
			} catch (err) {
				console.error('Ошибка при переименовании профиля:', err);
				alert('Ошибка при переименовании профиля.');
			}
		}
	}

	async function startCreating() {
		const newName = prompt('Введите имя нового профиля:');
		if (newName) {
			try {
				const newProfile = await createProfiles(newName);
				if (newProfile) {
					await loadData();
				} else {
					alert('Не удалось создать профиль.');
				}
			} catch (err) {
				console.error('Ошибка при создании профиля:', err);
				alert('Ошибка при создании профиля.');
			}
		}
	}

	async function handleClone(id: string) {
		try {
			const clonedProfile = await copyProfiles(id, "");
			if (clonedProfile) {
				await loadData();
			} else {
				alert('Не удалось клонировать профиль.');
			}
		} catch (err) {
			console.error('Ошибка при клонировании профиля:', err);
			alert('Ошибка при клонировании профиля.');
		}
	}

	async function confirmDelete(profile: Profile) {
		if (confirm(`Вы уверены, что хотите удалить профиль "${profile.name}"?`)) {
			try {
				const success = await deleteProfiles(profile.id);
				if (success) {
					if (selectedProfile?.id === profile.id) {
						selectedProfile = null;
					}
					await loadData();
				} else {
					alert('Не удалось удалить профиль.');
				}
			} catch (err) {
				console.error('Ошибка при удалении профиля:', err);
				alert('Ошибка при удалении профиля.');
			}
		}
	}

</script>

<div class="profile-manager-layout">
	<!-- Left Column: Profile List -->
	<div class="profile-list-panel">
		<div class="profile-list-header">
			<h3>Профили</h3>
			<div class="profile-actions">
				<button class="btn-icon" onclick={startCreating} title="Создать новый профиль">
					<Fa icon={faPlus} />
				</button>
				<button class="btn-icon" title="Загрузить из файла">
					<Fa icon={faUpload} />
				</button>
				<input type="file" accept=".json" class="hidden" />
			</div>
		</div>

		{#if isLoading}
			<div class="profile-loading-state">
				<Spinner />
			</div>
		{:else if error}
			<div class="profile-error-state">{error}</div>
		{:else if profiles}
			<ul class="profile-list">
				{#each profiles as profile (profile.id)}
					<li class="profile-list-item {selectedProfile?.id === profile.id ? 'active' : ''}">
						<button class="profile-name-button" onclick={() => selectProfile(profile.id)}>
							{#if profile.isApplied}
								<Fa icon={faCheck} class="applied-icon" />
							{/if}
							<span class="profile-name">{profile.name}</span>
						</button>
						<div class="profile-item-actions">
							<button class="btn-icon" onclick={() => startEditing(profile)} title="Редактировать профиль">
								<Fa icon={faPen} />
							</button>
							<button class="btn-icon" onclick={() => confirmDelete(profile)} title="Удалить профиль">
								<Fa icon={faTrash} />
							</button>
							<button class="btn-icon" onclick={() => handleClone(profile.id)} title="Клонировать профиль">
								<Fa icon={faClone} />
							</button>
							<button class="btn-icon" onclick={() => handleApply(profile.id)} title="Применить профиль" disabled={profile.isApplied}>
								<Fa icon={faPlay} />
							</button>
						</div>
					</li>
				{/each}
			</ul>
		{/if}
	</div>

	<!-- Right Column: Settings Placeholder or Selected Profile Details -->
	<div class="profile-settings-placeholder">
		{#if selectedProfile}
			<h3>Настройки профиля: {selectedProfile.name}</h3>
			<p>ID: {selectedProfile.id}</p>
			<p>This is where the detailed settings for "{selectedProfile.name}" would be displayed.</p>
		{:else}
			<h3>Редактор настроек</h3>
			<p>Выберите профиль для редактирования его параметров.</p>
		{/if}
	</div>
</div>
