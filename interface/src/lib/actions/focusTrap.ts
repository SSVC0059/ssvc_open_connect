const FOCUSABLE_SELECTOR =
	'a[href], button:not([disabled]), textarea:not([disabled]), input:not([disabled]), select:not([disabled]), [tabindex]:not([tabindex="-1"])';

export function focusTrap(node: HTMLElement) {
	function getFocusableElements(): HTMLElement[] {
		return Array.from(node.querySelectorAll<HTMLElement>(FOCUSABLE_SELECTOR)).filter(
			(el) => !el.hasAttribute('disabled') && el.tabIndex !== -1
		);
	}

	function onKeydown(event: KeyboardEvent) {
		if (event.key !== 'Tab') return;

		const focusables = getFocusableElements();
		if (focusables.length === 0) return;

		const first = focusables[0];
		const last = focusables[focusables.length - 1];
		const active = document.activeElement as HTMLElement | null;

		if (event.shiftKey) {
			if (!active || active === first || !node.contains(active)) {
				event.preventDefault();
				last.focus();
			}
			return;
		}

		if (!active || active === last || !node.contains(active)) {
			event.preventDefault();
			first.focus();
		}
	}

	const focusables = getFocusableElements();
	focusables[0]?.focus();

	node.addEventListener('keydown', onKeydown);

	return {
		destroy() {
			node.removeEventListener('keydown', onKeydown);
		}
	};
}
