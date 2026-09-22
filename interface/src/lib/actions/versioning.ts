import { getInfo, infoCacheVersion } from '$lib/api/ssvcApi';

/**
 * Преобразует версию API в целочисленный код major*100+minor.
 * Версию с двузначным minor передавайте строкой: число 1.10 в JS равно 1.1.
 */
export function apiVersionCode(version: number | string): number {
	const [major, minor] = String(version).split('.');
	return Number(major ?? 0) * 100 + Number(minor ?? 0);
}

/**
 * Svelte Action для блокировки элементов в зависимости от версии API.
 * Элемент будет доступен, если текущая версия API больше или равна требуемой.
 *
 * @param node - HTML-элемент, к которому применяется действие.
 * @param requiredVersion - Требуемая минимальная версия API ("1.6" или 1.6).
 */
export function requireVersion(node: HTMLElement, requiredVersion: number | string) {
	let isInitialized = false;

	// Сброс кеша /rest/oc/info (переподключение к контроллеру) должен перечитать
	// версию API: устройство могло быть заменено или перепрошито, и уже
	// применённая блокировка перестала соответствовать действительности.
	const unsubscribe = infoCacheVersion.subscribe(() => {
		void checkVersion();
	});

	const checkVersion = async () => {
		try {
			const info = await getInfo();
			if (!info || !info.ssvc.api) {
				// Если информация о версии недоступна, ничего не делаем,
				// чтобы не блокировать интерфейс при временных сбоях API.
				return;
			}

			const isSupported =
				apiVersionCode(info.ssvc.api) >= apiVersionCode(requiredVersion);

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
		update(newRequiredVersion: number | string) {
			if (isInitialized) {
				requiredVersion = newRequiredVersion;
				void checkVersion();
			}
		},
		destroy() {
			unsubscribe();
		}
	};
}
