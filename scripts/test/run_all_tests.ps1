# Полный запуск тестов: native + embedded
# Требуется: MSYS2 с g++ (pacman -S mingw-w64-ucrt-x86_64-gcc)
# Использование: .\scripts\test\run_all_tests.ps1 [-Port COM25]
param(
    [string]$Port = "COM25"
)
$ProjectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$env:PATH = "C:\msys64\ucrt64\bin;$env:PATH"
Set-Location $ProjectRoot

Write-Host "=== Native tests ===" -ForegroundColor Cyan
$nativeResult = pio test -e native
if ($LASTEXITCODE -ne 0) {
    Write-Host "Native tests failed. Stopping." -ForegroundColor Red
    exit $LASTEXITCODE
}

Write-Host "`n=== Embedded tests (port: $Port) ===" -ForegroundColor Cyan
pio test -e esp32-s3-devkitc-1-16m-test --upload-port $Port --test-port $Port
exit $LASTEXITCODE
