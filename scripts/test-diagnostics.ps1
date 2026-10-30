
$OpenKeyRepositoryRoot = Split-Path -Parent $PSScriptRoot
$ErrorActionPreference = 'Stop'
. (Join-Path $OpenKeyRepositoryRoot 'Diagnostics\SupportReport.ps1')
$passed = 0
function Assert-Report([bool]$Condition, [string]$Name) {
    if (-not $Condition) { throw "FAIL: $Name" }; $script:passed++
}
$fixture = Join-Path $OpenKeyRepositoryRoot ('dist\diagnostics-test-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $fixture | Out-Null
$empty = Get-OpenKeySupportReport $fixture
Assert-Report ($empty.files.Count -eq 6) 'fixed allowlist'
Assert-Report (@($empty.files | Where-Object { $_.status -ne 'missing' }).Count -eq 0) 'missing entries explicit'
$shell = (Get-Process -Id $PID).Path
Copy-Item -LiteralPath $shell -Destination (Join-Path $fixture 'OpenKey64.exe')
Copy-Item -LiteralPath $shell -Destination (Join-Path $fixture 'secret.exe')
New-Item -ItemType Directory -Path (Join-Path $fixture 'OpenKey.UI') | Out-Null
Copy-Item -LiteralPath $shell -Destination (Join-Path $fixture 'OpenKey.UI\OpenKey.UI.exe')
$report = Get-OpenKeySupportReport $fixture
$read = @($report.files | Where-Object { $_.status -eq 'read' })
Assert-Report ($read.Count -eq 2) 'installed UI subdirectory and engine read'
Assert-Report (@($report.files | Where-Object { $_.file -eq 'secret.exe' }).Count -eq 0) 'other files excluded'
Assert-Report ($read[0].sha256 -eq (Get-FileHash -LiteralPath $shell).Hash) 'hash matches source'
Assert-Report ($read[0].bytes -eq (Get-Item -LiteralPath $shell).Length) 'size matches source'
Assert-Report ($read[0].fileVersion -eq [Diagnostics.FileVersionInfo]::GetVersionInfo((Join-Path $fixture 'OpenKey64.exe')).FileVersion) 'version matches inspected file (MUI may differ after copy)'
$json = $report | ConvertTo-Json -Depth 6
Assert-Report (-not $json.Contains($fixture) -and -not $json.Contains($shell)) 'absolute paths excluded'
Assert-Report (($json | ConvertFrom-Json).files.Count -eq 6) 'JSON roundtrip'
foreach ($bad in @((Join-Path $fixture 'absent'), (Join-Path $fixture 'OpenKey64.exe'), '\\server\share')) {
    $rejected = $false
    try { Get-OpenKeySupportReport $bad | Out-Null } catch { $rejected = $true }
    Assert-Report $rejected 'invalid root rejected'
}
$links = Join-Path $fixture 'links'
New-Item -ItemType Directory -Path $links | Out-Null
New-Item -ItemType Junction -Path (Join-Path $links 'OpenKey.UI') -Target (Join-Path $fixture 'OpenKey.UI') | Out-Null
$linkReport = Get-OpenKeySupportReport $links
Assert-Report (($linkReport.files | Where-Object { $_.file -eq 'OpenKey.UI\OpenKey.UI.exe' }).status -eq 'skipped-link') 'nested junction skipped'
$rejected = $false
try { Get-OpenKeySupportReport (Join-Path $links 'OpenKey.UI') | Out-Null } catch { $rejected = $true }
Assert-Report $rejected 'junction root rejected'
Write-Output "Diagnostics: $passed checks passed. Fixture executables were never run."
