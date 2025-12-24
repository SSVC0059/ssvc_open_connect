import { apiFetch } from '$lib/api/ssvcApi';
import type { Profiles} from '$lib/types/ssvc';

export async function getProfiles(): Promise<Profiles> {
	const response = await apiFetch<Profiles>('/rest/profiles');
	return response.success ? response.data : [];
}

export async function getActiveProfile(): Promise<Profiles | null> {
	const response = await apiFetch<Profiles>('/rest/profiles/active');
	return response.success ? response.data : null;
}

// Объединенный метод для установки активного профиля и его применения
export async function setActiveAndApplyProfile(id: string): Promise<boolean> {
	const response = await apiFetch(`/rest/profiles/set-active`, 'POST', { id });
	return response.success;
}

export async function createProfiles(name: string): Promise<Profiles | null> {
	const response = await apiFetch<Profiles>('/rest/profiles', 'POST', { name });
	return response.success ? response.data : null;
}

export async function renameProfiles(
	id: string,
	newName: string): Promise<boolean> {
	const response = await apiFetch(`/rest/profiles/meta`, 'PUT', { id, newName });
	return response.success;
}

export async function copyProfiles(sourceId: string, newName: string): Promise<Profiles | null> {
	const response = await apiFetch<Profiles>(`/rest/profiles/copy`, 'POST', { sourceId, newName });
	return response.success ? response.data : null;
}

export async function deleteProfiles(id: string): Promise<boolean> {
	const response = await apiFetch(`/rest/profiles/delete`, 'DELETE', { id });
	return response.success;
}

export async function getProfileContent(id: string): Promise<any | null> {
	const response = await apiFetch<any>(`/rest/profiles/content`, 'GET', { id });
	return response.success ? response.data : null;
}

export async function saveCurrentSettingsToProfile(id: string): Promise<boolean> {
	const response = await apiFetch(`/rest/profiles/save`, 'POST', { id });
	return response.success;
}
