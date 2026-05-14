# Превью документации по PR

Для открытых **pull request из этого репозитория** (не из fork) в GitHub Actions собирается копия сайта и публикуется в каталог `previews/pr-<номер>/` на GitHub Pages. Подробности — workflow **Docs preview** в `.github/workflows/docs-preview.yml`.

Ниже список таких PR подгружается **в браузере** через [GitHub REST API](https://docs.github.com/en/rest/pulls/pulls#list-pull-requests). Если превью ещё не успели собраться или job упал, ссылка «Открыть превью» может вести на пустую страницу.

<div
  id="docs-pr-previews-root"
  data-repo="SSVC0059/ssvc_open_connect"
  data-preview-base="https://SSVC0059.github.io/ssvc_open_connect/previews/pr-"
></div>
