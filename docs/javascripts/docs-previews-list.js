/**
 * Список открытых PR с ссылками на превью документации (GitHub Pages previews/pr-N/).
 * Работает в браузере; для Material instant navigation — подписка на document$.
 */
(function () {
	const ROOT_ID = 'docs-pr-previews-root';

	function esc(s) {
		return String(s)
			.replace(/&/g, '&amp;')
			.replace(/</g, '&lt;')
			.replace(/>/g, '&gt;')
			.replace(/"/g, '&quot;');
	}

	function renderError(root, message) {
		root.innerHTML = `<p><strong>Не удалось загрузить список.</strong> ${esc(message)}</p>`;
	}

	async function loadPreviews() {
		const root = document.getElementById(ROOT_ID);
		if (!root) return;

		const repo = root.dataset.repo || 'SSVC0059/ssvc_open_connect';
		const previewBase =
			root.dataset.previewBase || 'https://SSVC0059.github.io/ssvc_open_connect/previews/pr-';

		root.innerHTML = '<p><em>Загрузка списка PR…</em></p>';

		try {
			const res = await fetch(`https://api.github.com/repos/${repo}/pulls?state=open&per_page=100`, {
				headers: { Accept: 'application/vnd.github+json' }
			});
			if (res.status === 403) {
				renderError(
					root,
					'Превышен лимит запросов к GitHub API без авторизации. Подождите или откройте страницу позже.'
				);
				return;
			}
			if (!res.ok) {
				renderError(root, `Ответ API: ${res.status} ${res.statusText}`);
				return;
			}
			const pulls = await res.json();
			const sameRepo = pulls.filter((p) => p.head?.repo?.full_name === repo);

			if (sameRepo.length === 0) {
				root.innerHTML =
					'<p>Нет открытых pull request из веток этого репозитория — превью документации сейчас не ожидаются.</p>';
				return;
			}

			const items = sameRepo
				.map((p) => {
					const previewUrl = `${previewBase}${p.number}/`;
					const prUrl = esc(p.html_url || '');
					const title = esc(p.title || '');
					return `<li><strong><a href="${prUrl}" target="_blank" rel="noopener">#${p.number}</a></strong> — ${title}<br/><a href="${esc(previewUrl)}" target="_blank" rel="noopener">Открыть превью документации</a></li>`;
				})
				.join('');

			root.innerHTML = `<ul>${items}</ul>`;
		} catch (e) {
			renderError(root, (e && e.message) || String(e));
		}
	}

	function tick() {
		requestAnimationFrame(loadPreviews);
	}

	function schedule() {
		if (typeof document$ !== 'undefined' && document$.subscribe) {
			document$.subscribe(tick);
			tick();
			return;
		}
		if (document.readyState === 'loading') {
			document.addEventListener('DOMContentLoaded', tick);
		} else {
			tick();
		}
	}

	schedule();
})();
