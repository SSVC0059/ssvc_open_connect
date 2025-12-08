import { getInfo } from '$lib/api/ssvcApi';

/**
 * Svelte Action для блокировки элементов в зависимости от версии прошивки.
 * Элемент будет доступен, если текущая версия API больше или равна требуемой.
 *
 * @param node - HTML-элемент, к которому применяется действие.
 * @param requiredVersion - Требуемая минимальная версия прошивки (например, 1.6).
 */
export function requireVersion(node: HTMLElement, requiredVersion: number) {
	let isInitialized = false;

	const checkVersion = async () => {
		try {
			const info = await getInfo();
			if (!info || !info.ssvc.api) {
				// Если информация о версии недоступна, ничего не делаем,
				// чтобы не блокировать интерфейс при временных сбоях API.
				return;
			}

			const currentVersion = info.ssvc.api;
			const isSupported = currentVersion >= requiredVersion;

			if (!isSupported) {
				disableInteractions(node);
			} else {
				enableInteractions(node);
			}

			isInitialized = true;
		} catch (error) {
			console.error('Failed to check version:', error);
			// В случае ошибки оставляем элемент активным по умолчанию.
		}
	};

	const disableInteractions = (element: HTMLElement) => {
		element.style.pointerEvents = 'none';
		element.style.opacity = '0.5';

		// Рекурсивно отключаем все интерактивные дочерние элементы
		element.querySelectorAll('button, input, textarea, select, a').forEach((child) => {
			(child as HTMLElement).setAttribute('disabled', 'true');
			(child as HTMLElement).style.pointerEvents = 'none';
		});
	};

	const enableInteractions = (element: HTMLElement) => {
		element.style.pointerEvents = '';
		element.style.opacity = '';

		element.querySelectorAll('button, input, textarea, select, a').forEach((child) => {
			(child as HTMLElement).removeAttribute('disabled');
			(child as HTMLElement).style.pointerEvents = '';
		});
	};

	void checkVersion();

	return {
		// Если нужно будет динамически обновлять версию
		update(newRequiredVersion: number) {
			if (isInitialized) {
				requiredVersion = newRequiredVersion;
				void checkVersion();
			}
		}
	};
}
