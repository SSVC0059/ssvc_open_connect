# Native-тесты в Docker
# Запуск: .\scripts\docker\docker_test_native.ps1
$ProjectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Set-Location $ProjectRoot
docker run --rm -v "${ProjectRoot}:/project" ssvc-open-connect test -e native
