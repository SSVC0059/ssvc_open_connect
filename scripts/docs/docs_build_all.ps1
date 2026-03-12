# Сборка полной документации: основной сайт + develop
# Результат: site/ содержит основной сайт, site/develop/ — документация для разработчиков
# Использование: .\scripts\docs\docs_build_all.ps1
$ProjectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Set-Location $ProjectRoot

Write-Host "Building main site..." -ForegroundColor Cyan
mkdocs build
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "Building develop site (output: site/develop/)..." -ForegroundColor Cyan
mkdocs build -f mkdocs-develop.yml -d site/develop
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "Done. Output: site/" -ForegroundColor Green
