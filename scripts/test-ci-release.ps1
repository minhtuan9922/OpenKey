[CmdletBinding()]
param()
$OpenKeyRepositoryRoot = Split-Path -Parent $PSScriptRoot

$ErrorActionPreference = 'Stop'
. (Join-Path $OpenKeyRepositoryRoot 'Installer/ReleasePolicy.ps1')
$checks = 0
foreach ($testVersion in @(@('v3.0.0',3,'3.0.0'), @('v2.0.5',327682,'2.0.5'), @('v3.1.2',131331,'3.1.2'), @('v255.255.255',16777215,'255.255.255'))) {
    $metadata = @{ latestWinVersion = @{ versionName = $testVersion[0]; versionCode = $testVersion[1] } }
    if ((Assert-OpenKeyReleaseTag $testVersion[0] $metadata) -ne $testVersion[2]) { throw 'Version policy failure.' }
    $checks++
}
foreach ($tag in @('3.0.0','V3.0.0','v3.0','v3.0.0-beta','v256.0.0','v3.256.0','v3.0.256','v03.0.0','v3.00.0','v3.0.00','../v3.0.0','v3.0.0;bad','v3.0.0`n')) {
    $rejected = $false
    try { Assert-OpenKeyReleaseTag $tag @{ latestWinVersion = @{ versionName = $tag; versionCode = 3 } } | Out-Null }
    catch { $rejected = $true }
    if (-not $rejected) { throw "Unsafe tag accepted: $tag" }
    $checks++
}
foreach ($metadata in @(@{ latestWinVersion = @{ versionName = 'v3.0.1'; versionCode = 3 } }, @{ latestWinVersion = @{ versionName = 'v3.0.0'; versionCode = 327682 } }, @{ latestWinVersion = @{ versionName = 'V3.0.0'; versionCode = 3 } }, @{ latestWinVersion = @{ versionName = 'v3.0.0'; versionCode = '3' } }, @{ latestWinVersion = @{ versionName = 'v3.0.0'; versionCode = 3.0 } }, @{})) {
    $rejected = $false
    try { Assert-OpenKeyReleaseTag 'v3.0.0' $metadata | Out-Null } catch { $rejected = $true }
    if (-not $rejected) { throw 'Mismatched metadata accepted.' }
    $checks++
}
$workflow = Get-Content -LiteralPath (Join-Path $OpenKeyRepositoryRoot '.github/workflows/release-setup.yml') -Raw
$contracts = @(
    ($workflow -match "tags: \['v\*'\]"),
    ($workflow -match 'windows-2025-vs2026'),
    ($workflow -match 'persist-credentials: false'),
    ($workflow -match 'needs: build'),
    ($workflow -match 'sha256sum --check SHA256SUMS.txt'),
    ($workflow -match 'gh release upload.*OpenKey-Setup.exe SHA256SUMS.txt'),
    ($workflow -notmatch '(?m)^\s+gh .*--clobber'),
    ($workflow -match '(?s)publish:.*contents: write'),
    ($workflow -match 'gh release create.*--draft'),
    ($workflow -notmatch 'gh release create.*--prerelease'),
    ($workflow -match 'gh release edit.*--draft=false --prerelease=false --latest')
)
foreach ($contract in $contracts) { if (-not $contract) { throw 'CI source contract failed.' }; $checks++ }
$buildSource = Get-Content -LiteralPath (Join-Path $OpenKeyRepositoryRoot 'scripts\build-ci-release.ps1') -Raw
$hostContracts = @(
    ($buildSource -match 'Get-Command pwsh.exe -ErrorAction Stop'),
    ($buildSource.Contains('System32\WindowsPowerShell\v1.0\powershell.exe')),
    ($buildSource -match '(?s)foreach \(\$regressionHost in \$regressionHosts\).*?-NoProfile -ExecutionPolicy Bypass -File.*?test-all.ps1.*?if \(\$LASTEXITCODE -ne 0\).*?throw'),
    ($buildSource.IndexOf('foreach ($regressionHost in $regressionHosts)') -ge 0 -and
        $buildSource.IndexOf('foreach ($regressionHost in $regressionHosts)') -lt $buildSource.IndexOf("New-Item -ItemType Directory -Path `$output"))
)
foreach ($contract in $hostContracts) { if (-not $contract) { throw 'Dual-shell release gate contract failed.' }; $checks++ }
Write-Output "$checks release policy/workflow contract checks passed; no network, Git writes or publication."
