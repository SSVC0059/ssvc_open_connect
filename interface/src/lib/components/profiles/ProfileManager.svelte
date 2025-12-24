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
	import ProfileViewer from './ProfileViewer.svelte';
	import ProfileEditor from './ProfileEditor.svelte';

	let profiles: Profiles | undefined = $state(undefined);
	let isLoading: boolean = $state(true);
	let error: string | null = $state(null);
	let selectedProfile: Profile | null = $state(null); // Profile selected for viewing/editing
	let editingProfile: Profile | null = $state(null); // Profile currently being edited
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
		if (editingProfile) return; // Don't change selection while editing
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

	function startEditing(profile: Profile) {
		editingProfile = { ...profile }; // Create a copy for editing
		selectedProfile = null; // Hide viewer
	}

	async function handleSaveEditing() {
		if (!editingProfile) return;

		const newName = editingProfile.name;
		const originalProfile = profiles?.find(p => p.id === editingProfile!.id);

		if (newName && originalProfile && newName !== originalProfile.name) {
			try {
				const success = await renameProfiles(editingProfile.id, newName);
				if (success) {
					await loadData();
					editingProfile = null;
				} else {
					alert('Не удалось переименовать профиль.');
				}
			} catch (err) {
				console.error('Ошибка при переименовании профиля:', err);
				alert('Ошибка при переименовании профиля.');
			}
		} else {
			// If only other fields were changed, just exit editing mode
			editingProfile = null;
		}
	}

	function cancelEditing() {
		editingProfile = null;
	}

	async function startCreating() {
		const newName = prompt('Введите имя нового профиля:');
		if (newName) {
			try {
				const newProfile = await createProfiles(newName);
				if (newProfile) {
					await loadData();
					// Optionally, start editing the new profile right away
					const created = profiles?.find(p => p.name === newName);
					if (created) {
						startEditing(created);
					}
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
					if (editingProfile?.id === profile.id) {
						editingProfile = null;
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

<div class="profile-manager-layout" class:editing={editingProfile !== null}>
	<!-- Left Column: Profile List -->
	<div class="profile-list-panel">
		<div class="profile-list-header">
			<h3>Профили</h3>
			<div class="profile-actions">
				<button class="icon-button" onclick={startCreating} title="Создать новый профиль" disabled={editingProfile !== null}>
					<Fa icon={faPlus} />
				</button>
				<button class="icon-button" title="Загрузить из файла" disabled={editingProfile !== null}>
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
					<li class="profile-list-item {selectedProfile?.id === profile.id ? 'active' : ''} {editingProfile ? 'disabled' : ''}">
						<button class="profile-name-button" onclick={() => selectProfile(profile.id)} disabled={editingProfile !== null}>
							{#if profile.isApplied}
								<Fa icon={faCheck} class="applied-icon" />
							{/if}
							<span class="profile-name">{profile.name}</span>
						</button>
						<div class="profile-item-actions">
							<button class="icon-button" onclick={() => startEditing(profile)} title="Редактировать профиль" disabled={editingProfile !== null}>
								<Fa icon={faPen} />
							</button>
							<button class="icon-button" onclick={() => confirmDelete(profile)} title="Удалить профиль" disabled={editingProfile !== null}>
								<Fa icon={faTrash} />
							</button>
							<button class="icon-button" onclick={() => handleClone(profile.id)} title="Клонировать профиль" disabled={editingProfile !== null}>
								<Fa icon={faClone} />
							</button>
							<button class="icon-button" onclick={() => handleApply(profile.id)} title="Применить профиль" disabled={profile.isApplied || editingProfile !== null}>
								<Fa icon={faPlay} />
							</button>
						</div>
					</li>
				{/each}
			</ul>
		{/if}
	</div>

	<!-- Right Column: Settings Placeholder or Selected Profile Details -->
	<div class="profile-settings-panel">
		{#if editingProfile}
			<ProfileEditor profile={editingProfile} onSave={handleSaveEditing} onCancel={cancelEditing} />
		{:else if selectedProfile}
			<ProfileViewer profile={selectedProfile} />
		{:else}
			<div class="profile-settings-placeholder">
				<h3>Редактор настроек</h3>
				<p>Выберите профиль для просмотра или редактирования его параметров.</p>
			</div>
		{/if}
	</div>
</div>
<div class="hidden">
	<input type="file" accept=".json" class="hidden" />
</div>
