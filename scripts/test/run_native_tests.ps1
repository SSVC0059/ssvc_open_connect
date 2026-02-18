# Запуск native-тестов (Windows + MSYS2)
# Требуется: MSYS2 с g++ (pacman -S mingw-w64-ucrt-x86_64-gcc)
# Запуск: .\scripts\test\run_native_tests.ps1
$ProjectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$env:PATH = "C:\msys64\ucrt64\bin;$env:PATH"
Set-Location $ProjectRoot
pio test -e native
