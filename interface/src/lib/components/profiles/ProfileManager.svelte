<script lang="ts">
	import {Fa} from 'svelte-fa';
import {
		faArrowsRotate,
		faClone,
		faDownload,
		faEllipsisV,
		faFileImport,
		faPen,
		faPlay,
		faPlus,
		faTrash
	} from '@fortawesome/free-solid-svg-icons';
	import Cancel from '~icons/tabler/x';
	import Check from '~icons/tabler/check';
	import type {Profile, Profiles} from '$lib/types/ssvc';
	import ProfileViewer from '$lib/components/profiles/ProfileViewer.svelte';
	import ProfileEditor from '$lib/components/profiles/ProfileEditor.svelte';
	import {
		copyProfiles,
		createProfiles,
		deleteProfiles,
		getProfiles,
		updateProfileMeta,
		setActiveAndApplyProfile,
		updateProfileContent,
		saveCurrentSettingsToProfile, getProfileContent
	} from '$lib/api/Profiles';
	import { notifications } from '$lib/components/toasts/notifications';
	import { modals } from 'svelte-modals';
	import ConfirmDialog from '$lib/components/ConfirmDialog.svelte';
	import InputDialog from '$lib/components/InputDialog.svelte';
	import { fetchSettings } from '$lib/api/ssvcApi';
	import { deepDiff } from '$lib/utils/deepDiff';


let profiles: Profiles | undefined = $state(undefined);
let isLoading: boolean = $state(true);
let error: string | null = $state(null);
let selectedProfile: Profile | null = $state(null); // Profile selected for viewing/editing
let editingProfile: Profile | null = $state(null); // Profile currently being edited
let appliedProfileId: string = $state(''); // ID of the profile active on the controller
let hasInitializedSelection: boolean = $state(false); // Ensure auto-selection runs only on first load
let actualizingProfileId: string = $state(''); // ID of profile currently being actualized (for spinner)

	let fileInput: HTMLInputElement;

	$effect(() => {
		loadData();
	});

// При первом успешном получении списка профилей автоматически выбираем первый,
// чтобы при открытии вкладки «Профили» сразу открывался профиль (если он есть).
$effect(() => {
	if (!hasInitializedSelection && !editingProfile && !selectedProfile && profiles && profiles.length > 0) {
		selectedProfile = profiles[0];
		hasInitializedSelection = true;
	}
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
		const targetId = id.toString();
		const target = profiles?.find(p => p.id === targetId);

		// Перед применением профиля запрашиваем текущие настройки контроллера
		// и содержимое профиля. Если есть расхождения — показываем подтверждение.
		try {
			const [currentSettings, profileContent] = await Promise.all([
				fetchSettings(),
				target ? getProfileContent(target.id) : Promise.resolve(null)
			]);

			if (currentSettings && profileContent?.ssvcSettings) {
				const diff = deepDiff(
					profileContent.ssvcSettings as unknown as Record<string, any>,
					currentSettings as unknown as Record<string, any>
				);
				const changedKeys = Object.keys(diff);

				if (changedKeys.length > 0) {
					const preview = changedKeys.slice(0, 8).join(', ') +
						(changedKeys.length > 8 ? ` и ещё ${changedKeys.length - 8}` : '');
					const proceed = await new Promise<boolean>((resolve) => {
						modals.open(ConfirmDialog, {
							title: 'Применить профиль?',
							message: `Применение профиля «<b>${target?.name ?? targetId}</b>» изменит <b>${changedKeys.length}</b> настроек на контроллере:<br><br><code style="font-size:0.85em">${preview}</code><br><br>Текущие значения на ssvc0059v2 будут перезаписаны.`,
							labels: {
								cancel: { label: 'Отмена', icon: Cancel },
								confirm: { label: 'Применить', icon: Check }
							},
							onConfirm: () => resolve(true),
							onCancel: () => resolve(false)
						});
					});
					if (!proceed) {
						return;
					}
				}
			}
		} catch (err) {
			console.warn('Не удалось подготовить подтверждение apply, продолжаем без него:', err);
		}

		try {
			const success = await setActiveAndApplyProfile(targetId);
			if (success) {
				appliedProfileId = targetId;
				// Update isApplied flag in local state
				if (profiles) {
					profiles = profiles.map(p => ({ ...p, isApplied: p.id === targetId }));
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

	/**
	 * Актуализировать профиль текущими настройками контроллера ssvc0059v2:
	 * backend принудительно запрашивает GET_SETTINGS, ждёт ответа и затем
	 * перезаписывает JSON профиля актуальными значениями.
	 */
	async function handleActualize(profile: Profile) {
		try {
			actualizingProfileId = profile.id;
			const success = await saveCurrentSettingsToProfile(profile.id);
			if (success) {
				notifications.success(
					`Профиль «${profile.name}» актуализирован текущими настройками контроллера`,
					5000
				);
				await loadData();
			} else {
				notifications.error('Не удалось актуализировать профиль', 5000);
			}
		} catch (err) {
			console.error('Error actualizing profile:', err);
			notifications.error('Ошибка при актуализации профиля', 5000);
		} finally {
			actualizingProfileId = '';
		}
	}

	function startEditing(profile: Profile) {
		editingProfile = { ...profile }; // Create a copy for editing
		selectedProfile = null; // Hide viewer
	}

	async function handleSaveEditing(updatedProfile: Profile) {
		if (!updatedProfile || !updatedProfile.id) {
			notifications.error('Невозможно сохранить профиль: нет идентификатора.', 5000);
			return;
		}

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
						const newProfiles = await createProfiles(newName);
						if (newProfiles) {
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

	async function handleFileUpload(event: Event) {
		const input = event.target as HTMLInputElement;
		if (!input.files?.length) return;

		const file = input.files[0];
		const reader = new FileReader();

		reader.onload = async (e) => {
			const text = e.target?.result;
			if (typeof text !== 'string') return;
			const profileContent = JSON.parse(text);
			const profileName = file.name.replace('.json', '');
			try {
				// Basic validation
				if (typeof profileContent !== 'object' || profileContent === null) {
					throw new Error('Invalid profile format');
				}

				const newProfiles = await createProfiles(profileName, profileContent);
				if (newProfiles) {
					notifications.success('Профиль успешно загружен', 5000);
					await loadData();
				} else {
					notifications.error('Не удалось создать профиль для загруженных данных', 5000);
				}
			} catch (err) {
				console.error('Ошибка при загрузке или парсинге профиля:', err);
				notifications.error('Ошибка при загрузке: неверный формат файла или ошибка сервера.', 5000);
			}
		};

		reader.onerror = () => {
			notifications.error('Не удалось прочитать файл.', 5000);
		};

		reader.readAsText(file);
		input.value = ''; // Reset input for same-file uploads
	}

	async function handleDownload(profile: Profile) {
		try {
			const content = await getProfileContent(profile.id);
			if (content) {
				const blob = new Blob([JSON.stringify(content, null, 2)], { type: 'application/json' });
				const url = URL.createObjectURL(blob);
				const a = document.createElement('a');
				a.href = url;
				a.download = `${profile.name}.json`;
				document.body.appendChild(a);
				a.click();
				document.body.removeChild(a);
				URL.revokeObjectURL(url);
				notifications.success(`Профиль "${profile.name}" выгружен.`, 5000);
			} else {
				notifications.error('Не удалось получить содержимое профиля.', 5000);
			}
		} catch (err) {
			console.error('Ошибка при выгрузке профиля:', err);
			notifications.error('Ошибка при выгрузке профиля.', 5000);
		}
	}

</script>

<div class="settings-container">
	<div class="settings-grid">
		<div class="settings-panel">
			<div class="settings-section">
				<div class="profile-manager-layout" class:editing={editingProfile !== null}>
					<!-- Left Column: Profile List -->
					<div class="profile-list-panel">
						<div class="profile-list-header">
							<h3>Профили</h3>
							<div class="profile-actions">
								<button
									class="icon-button"
									onclick={startCreating}
									title="Создать новый профиль"
									disabled={editingProfile !== null}
								>
									<Fa icon={faPlus} />
								</button>
								<button
									class="icon-button"
									onclick={() => fileInput.click()}
									title="Импортировать профиль из файла"
									disabled={editingProfile !== null}
								>
									<Fa icon={faFileImport} />
								</button>
								<button
									class="icon-button"
									onclick={() => selectedProfile && handleDownload(selectedProfile)}
									title="Выгрузить выбранный профиль в файл"
									disabled={editingProfile !== null || !selectedProfile}
								>
									<Fa icon={faDownload} />
								</button>
							</div>
						</div>

						{#if appliedProfileId && profiles}
							<div class="applied-profile-summary">
								<span class="applied-profile-label">Активный профиль:</span>
								<span class="applied-profile-name">
									{profiles.find((p) => p.id === appliedProfileId)?.name}
								</span>
							</div>
						{/if}

						{#if isLoading}
							<div class="profile-loading-state flex flex-col items-center justify-center gap-2">
								<span
									class="loading loading-spinner loading-lg text-primary"
									aria-hidden="true"
								></span>
							</div>
						{:else if error}
							<div class="profile-error-state">{error}</div>
						{:else if profiles}
							<ul class="profile-list">
								{#each profiles as profile (profile.id)}
									<li
										class="profile-list-item {selectedProfile?.id === profile.id ? 'active' : ''} {profile.isApplied ? 'applied' : ''} {editingProfile ? 'disabled' : ''}"
									>
										<button
											class="profile-name-button"
											onclick={() => selectProfile(profile.id)}
											disabled={editingProfile !== null}
										>
											<span class="profile-name">{profile.name}</span>
										</button>
										<div class="profile-item-actions">
											<div class="dropdown dropdown-end max-md:dropdown-top">
												<button
													type="button"
													class="icon-button"
													tabindex="0"
													title="Действия с профилем"
													disabled={editingProfile !== null}
												>
													<Fa icon={faEllipsisV} />
												</button>
												<ul
													tabindex="-1"
													class="menu menu-sm dropdown-content mt-2 max-md:mt-0 max-md:mb-2 w-52 rounded-box bg-base-100 p-1 shadow-lg"
												>
													<li>
														<button
															type="button"
															onclick={() => startEditing(profile)}
															disabled={editingProfile !== null}
														>
															<Fa icon={faPen} />
															<span>Редактировать</span>
														</button>
													</li>
													<li>
														<button
															type="button"
															onclick={() => handleApply(profile.id)}
															disabled={profile.isApplied || editingProfile !== null}
														>
															<Fa icon={faPlay} />
															<span>Применить</span>
														</button>
													</li>
													<li>
														<button
															type="button"
															onclick={() => handleActualize(profile)}
															disabled={editingProfile !== null || actualizingProfileId === profile.id}
															title="Записать в профиль текущие настройки контроллера ssvc0059v2"
														>
															<Fa icon={faArrowsRotate} spin={actualizingProfileId === profile.id} />
															<span>Актуализировать</span>
														</button>
													</li>
													<li>
														<button
															type="button"
															onclick={() => handleClone(profile.id)}
															disabled={editingProfile !== null}
														>
															<Fa icon={faClone} />
															<span>Клонировать</span>
														</button>
													</li>
													<li>
														<button
															type="button"
															onclick={() => handleDownload(profile)}
															disabled={editingProfile !== null}
														>
															<Fa icon={faDownload} />
															<span>Выгрузить</span>
														</button>
													</li>
													<li>
														<button
															type="button"
															onclick={() => confirmDelete(profile)}
															disabled={editingProfile !== null}
														>
															<Fa icon={faTrash} />
															<span>Удалить</span>
														</button>
													</li>
												</ul>
											</div>
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
								<p>
									Выберите профиль для просмотра или редактирования его параметров.
								</p>
							</div>
						{/if}
					</div>
				</div>
			</div>
		</div>
	</div>
</div>
<div class="hidden">
	<input
		type="file"
		accept=".json"
		bind:this={fileInput}
		onchange={handleFileUpload}
		class="input-field"
	/>
</div>

<style lang="scss">
	@use '$lib/styles/base/variables' as v;
	@use '$lib/styles/components/profile-manager';

	/* Профили во вкладке настроек отображаются как одна широкая карточка */
	.settings-grid {
		grid-template-columns: 1fr;

		@media (min-width: v.$breakpoint-md) {
			grid-template-columns: 1fr;
		}
	}
</style>
