/**
 * Fills #docs-previews-root with open PRs that have a docs preview on gh-pages.
 * Works with Material instant navigation via document$ (see mkdocs.yml extra_javascript).
 */
(function () {
  const REPO = "SSVC0059/ssvc_open_connect";
  // CI кладёт сайт в destination_dir: previews/pr-<N> (дефис, не previews/pr/N)
  const PREVIEW_BASE = "https://SSVC0059.github.io/ssvc_open_connect/previews/pr-";

  function render(root, pulls) {
    if (!pulls.length) {
      root.innerHTML =
        "<p>Открытых pull request не найдено, либо не удалось получить данные.</p>";
      return;
    }
    const ul = document.createElement("ul");
    pulls.forEach((pr) => {
      const li = document.createElement("li");
      const a = document.createElement("a");
      a.href = PREVIEW_BASE + pr.number + "/";
      a.textContent = "PR #" + pr.number + " — " + (pr.title || "(без названия)");
      a.rel = "noopener";
      a.target = "_blank";
      li.appendChild(a);
      const span = document.createElement("span");
      span.textContent =
        " — ветка " + (pr.head && pr.head.ref ? pr.head.ref : "?");
      span.style.opacity = "0.85";
      li.appendChild(span);
      ul.appendChild(li);
    });
    root.innerHTML = "";
    root.appendChild(ul);
  }

  function renderError(root, message) {
    root.innerHTML =
      '<p class="admonition warning"><strong>Не удалось загрузить список PR.</strong> ' +
      message +
      "</p>";
  }

  async function load() {
    const root = document.getElementById("docs-previews-root");
    if (!root) return;

    try {
      const url =
        "https://api.github.com/repos/" +
        REPO +
        "/pulls?state=open&per_page=100&sort=updated";
      const res = await fetch(url, { headers: { Accept: "application/vnd.github+json" } });
      if (!res.ok) {
        renderError(
          root,
          "HTTP " + res.status + ". Для публичного API действует лимит запросов; попробуйте позже."
        );
        return;
      }
      const data = await res.json();
      const sameRepo = data.filter(
        (pr) =>
          pr.head &&
          pr.head.repo &&
          pr.head.repo.full_name === REPO
      );
      render(root, sameRepo);
    } catch (e) {
      renderError(root, (e && e.message) || String(e));
    }
  }

  if (typeof document$ !== "undefined" && document$.subscribe) {
    document$.subscribe(function () {
      load();
    });
  } else if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", load);
  } else {
    load();
  }
})();
