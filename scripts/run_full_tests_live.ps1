<#
.SYNOPSIS
  Полный прогон тестов: PlatformIO (native + embedded), затем загрузка боевой прошивки и HTTP-смоук по сети.

.EXAMPLE
  .\scripts\run_full_tests_live.ps1 -Port COM25 -Url http://10.5.1.117

.EXAMPLE
  .\scripts\run_full_tests_live.ps1 -Port COM25 -Url http://10.5.1.117 -SkipNative

.EXAMPLE
  KC868: восстановить боевую прошивку другим env перед смоуком —
  .\scripts\run_full_tests_live.ps1 -Port COM25 -Url http://10.5.1.117 -SkipNative -ProductionEnv kincony-kc868-a6
#>
param(
    [Parameter(Mandatory = $true, HelpMessage = "COM-порт платы (загрузка и Unity, например COM25")]
    [string] $Port,

    [Parameter(Mandatory = $true, HelpMessage = "Базовый URL прошивки, например http://10.5.1.117")]
    [string] $Url,

    [string] $User = "admin",
    [string] $Password = "admin",

    [switch] $SkipNative,

    [Parameter(HelpMessage = "PlatformIO env боевой прошивки: pio run -e ... -t upload после embedded-тестов (перед HTTP-смоуком).")]
    [string] $ProductionEnv = "esp32-s3-devkitc-1-16m",

    [switch] $SkipProductionFlash,

    [Parameter(HelpMessage = "Пауза (сек) после перепрошивки перед смоуком (Wi-Fi).")]
    [int] $PostFlashDelaySeconds = 12
)

$ErrorActionPreference = "Stop"
$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
Set-Location $RepoRoot

# Cursor / IDE и отдельные оболочки часто не наследуют PATH из «MSYS2 UCRT64». Ставим ucrt64\bin вперёд,
# чтобы PlatformIO вызывал тот же g++, что и интерактивная проверка where.exe g++.
$MsysUcrtBin = "C:\msys64\ucrt64\bin"
if (Test-Path -LiteralPath $MsysUcrtBin) {
    $env:PATH = "$MsysUcrtBin;$env:PATH"
}

if ($SkipNative) {
    Write-Host "== PlatformIO: esp32-s3-devkitc-1-16m-test only ($Port); native skipped ==" -ForegroundColor Cyan
    pio test -e esp32-s3-devkitc-1-16m-test --upload-port $Port --test-port $Port
} else {
    Write-Host "== PlatformIO: native + esp32-s3-devkitc-1-16m-test ($Port) ==" -ForegroundColor Cyan
    pio test -e native -e esp32-s3-devkitc-1-16m-test --upload-port $Port --test-port $Port
}
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

if (-not $SkipProductionFlash) {
    Write-Host "== Restore production firmware: $ProductionEnv ($Port), затем смоук по HTTP ==" -ForegroundColor Cyan
    Write-Host '(После pio test на flash остаётся Unity; смоук ожидает основную прошивку.)' -ForegroundColor DarkGray
    pio run -e $ProductionEnv -t upload --upload-port $Port
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    if ($PostFlashDelaySeconds -gt 0) {
        Write-Host "Waiting ${PostFlashDelaySeconds}s for reboot / Wi-Fi ..." -ForegroundColor DarkGray
        Start-Sleep -Seconds $PostFlashDelaySeconds
    }
}

Write-Host "== smoke_rest.py: $Url --extended ==" -ForegroundColor Cyan
$Smoke = Join-Path $RepoRoot "scripts\smoke_rest.py"
python $Smoke $Url --user $User --password $Password --extended
exit $LASTEXITCODE
