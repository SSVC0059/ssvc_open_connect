import '@testing-library/jest-dom/vitest';
import { cleanup } from '@testing-library/svelte';
import { afterEach } from 'vitest';

// jsdom не реализует Element.prototype.animate (svelte/transition, Portal)
if (typeof Element !== 'undefined' && !Element.prototype.animate) {
	Element.prototype.animate = () => ({ finish: () => {}, cancel: () => {}, persist: () => {} });
}

afterEach(() => {
	cleanup();
});
