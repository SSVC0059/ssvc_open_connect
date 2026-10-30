$ErrorActionPreference = 'Stop'

$enDash = [char]0x2013
$emDash = [char]0x2014

$base = $PSScriptRoot
if (-not $base) { $base = (Get-Location).Path }

$path = Join-Path $base '.workai\plans\xkc25-npn-support.plan.md'
if (-not (Test-Path $path)) { throw ('Plan not found: ' + $path) }

$bytes = [System.IO.File]::ReadAllBytes($path)
$hasBom = ($bytes.Length -ge 3 -and $bytes[0] -eq 239 -and $bytes[1] -eq 187 -and $bytes[2] -eq 191)
$t = [System.IO.File]::ReadAllText($path, [System.Text.Encoding]::UTF8)

$doneChecked = '[DONE, ' + ([regex]::Match($t, '\[DONE, ([^\]]+)\]').Groups[1].Value) + ']'
Write-Output ('done-label: ' + $doneChecked)

$rx1 = New-Object System.Text.RegularExpressions.Regex '3, 4\)\*\*'
Write-Output ('phase5 heading matches: ' + $rx1.Matches($t).Count)
$t = $rx1.Replace($t, ('3, 4) ' + $emDash + ' [DONE]**'), 1)

$rx2 = New-Object System.Text.RegularExpressions.Regex '(?m)^9\. '
Write-Output ('step 9 matches: ' + $rx2.Matches($t).Count)
$t = $rx2.Replace($t, '9. [DONE] ', 1)

$rx3 = New-Object System.Text.RegularExpressions.Regex '(?m)^(\*\*\S+ 5):\*\* (.*?)\r?$'
Write-Output ('phase5 check matches: ' + $rx3.Matches($t).Count)
$t = $rx3.Replace($t, ('$1 ' + $emDash + ' ' + $doneChecked + ':** pio run -e kincony-kc868-a6 ' + $emDash + ' SUCCESS (28.87 s); $2'), 1)

$rx4 = New-Object System.Text.RegularExpressions.Regex ('1' + $enDash + '4 \*\*')
Write-Output ('decision phases 1-4 matches: ' + $rx4.Matches($t).Count)
$t = $rx4.Replace($t, ('1' + $enDash + '5 **'), 1)

$rx5 = New-Object System.Text.RegularExpressions.Regex ('5' + $enDash + '9 ')
Write-Output ('decision phases 5-9 matches: ' + $rx5.Matches($t).Count)
$t = $rx5.Replace($t, ('6' + $enDash + '9 '), 1)

$utf8 = New-Object System.Text.UTF8Encoding($hasBom)
[System.IO.File]::WriteAllText($path, $t, $utf8)

Write-Output ('bom preserved: ' + $hasBom)
Write-Output ('written: ' + $path)
