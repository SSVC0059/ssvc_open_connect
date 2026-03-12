# Embedded-тесты: сборка без загрузки (в Docker)
# Запуск: .\scripts\docker\docker_test_embedded_build.ps1
# Для полного запуска с загрузкой на устройство — используйте pio test локально (нужен COM-порт)
$ProjectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Set-Location $ProjectRoot
docker run --rm -v "${ProjectRoot}:/project" ssvc-open-connect test -e esp32-s3-devkitc-1-16m-test --without-uploading --without-testing
