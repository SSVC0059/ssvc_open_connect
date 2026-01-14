<script lang="ts">
	import {Fa} from 'svelte-fa';
	import {faCheck, faClone, faPen, faPlay, faPlus, faTrash, faUpload} from '@fortawesome/free-solid-svg-icons';
	import Spinner from '$lib/components/Spinner.svelte';
	import type {Profile, Profiles} from '$lib/types/ssvc';
	import ProfileViewer from '$lib/components/profiles/ProfileViewer.svelte';
	import ProfileEditor from '$lib/components/profiles/ProfileEditor.svelte';
	import {
		copyProfiles,
		createProfiles,
		deleteProfiles,
		getProfiles, updateProfileMeta,
		setActiveAndApplyProfile, updateProfileContent, saveCurrentSettingsToProfile
	} from '$lib/api/Profiles';
	import { notifications } from '$lib/components/toasts/notifications';
	import { modals } from 'svelte-modals';
	import ConfirmDialog from '$lib/components/ConfirmDialog.svelte';
	import InputDialog from '$lib/components/InputDialog.svelte';


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
				notifications.success('Профиль успешно применен', 5000);
			} else {
				notifications.error('Не удалось применить профиль', 5000);
			}
		} catch (err) {
			console.error('Error applying profile:', err);
			notifications.error('Ошибка при применении профиля', 5000);
		}
	}

	function startEditing(profile: Profile) {
		editingProfile = { ...profile }; // Create a copy for editing
		selectedProfile = null; // Hide viewer
	}

	async function handleSaveEditing(updatedProfile: Profile) {
		if (!updatedProfile || !updatedProfile.id) return;

		console.log('Профиль для сохранения:', updatedProfile);
		try {
			isLoading = true;

			const updateSuccess = await updateProfileContent(updatedProfile.id, updatedProfile);

			if (updateSuccess) {
				editingProfile = null;
				await loadData();
				notifications.success('Профиль успешно сохранен', 5000);

			} else {
				notifications.error('Ошибка при сохранении содержимого профиля', 5000);
			}
		} catch (err) {
			console.error('Ошибка в handleSaveEditing:', err);
			notifications.error('Произошла ошибка при сохранении профиля', 5000);
		} finally {
			isLoading = false;
		}
	}

	function cancelEditing() {
		editingProfile = null;
	}

	function startCreating() {
		modals.open(InputDialog, {
			title: 'Создать профиль',
			message: 'Введите имя нового профиля:',
			onSave: async (newName: string) => {
				if (newName) {
					try {
						const newProfile = await createProfiles(newName);
						if (newProfile) {
							await loadData();
							notifications.success(`Профиль "${newName}" создан`, 5000);
							const created = profiles?.find(p => p.name === newName);
							if (created) {
								startEditing(created);
							}
						} else {
							notifications.error('Не удалось создать профиль', 5000);
						}
					} catch (err) {
						console.error('Ошибка при создании профиля:', err);
						notifications.error('Ошибка при создании профиля', 5000);
					}
				}
			}
		});
	}

	async function handleClone(id: string) {
		modals.open(InputDialog, {
			title: 'Клонировать профиль',
			message: 'Введите имя нового профиля:',
			onSave: async (newName: string) => {
				if (newName) {
					try {
						const clonedProfile = await copyProfiles(id, newName);
						if (clonedProfile) {
							await loadData();
							notifications.success(`Профиль успешно клонирован как "${newName}"`, 5000);
						} else {
							notifications.error('Не удалось клонировать профиль', 5000);
						}
					} catch (err) {
						console.error('Ошибка при клонировании профиля:', err);
						notifications.error('Ошибка при клонировании профиля', 5000);
					}
				}
			}
		});
	}

	function confirmDelete(profile: Profile) {
		modals.open(ConfirmDialog, {
			title: 'Подтверждение удаления',
			message: `Вы уверены, что хотите удалить профиль "${profile.name}"?`,
			onConfirm: async () => {
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
						notifications.success(`Профиль "${profile.name}" удален`, 5000);
					} else {
						notifications.error('Не удалось удалить профиль', 5000);
					}
				} catch (err) {
					console.error('Ошибка при удалении профиля:', err);
					notifications.error('Ошибка при удалении профиля', 5000);
				} finally {
					modals.close();
				}
			}
		});
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
			<ProfileEditor
				profileInfo={editingProfile}
				onSave={handleSaveEditing}
				onCancel={cancelEditing}
			/>
		{:else if selectedProfile}
			<ProfileViewer profileInfo={selectedProfile} />
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
