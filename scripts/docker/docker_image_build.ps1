# Сборка Docker-образа ssvc-open-connect
# Запуск: .\scripts\docker\docker_image_build.ps1
$ProjectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Set-Location $ProjectRoot
docker build -t ssvc-open-connect .
