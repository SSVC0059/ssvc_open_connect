# Сборка прошивки в Docker
# Запуск: .\scripts\docker\docker_build.ps1 [env]
# Пример: .\scripts\docker\docker_build.ps1 esp32-s3-devkitc-1-16m
param(
    [string]$Env = "esp32-s3-devkitc-1-16m"
)
$ProjectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Set-Location $ProjectRoot
docker run --rm -v "${ProjectRoot}:/project" ssvc-open-connect run -e $Env
