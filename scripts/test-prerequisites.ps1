
$OpenKeyRepositoryRoot = Split-Path -Parent $PSScriptRoot
$ErrorActionPreference = 'Stop'
. (Join-Path $OpenKeyRepositoryRoot 'scripts\check-prerequisites.ps1')
$minimum = [version]'14.51.36231.0'
$cases = @(
    @{ Installed = $true; Version = 'v14.51.36231.0'; Expected = $true },
    @{ Installed = $true; Version = '14.52.1.0'; Expected = $true },
    @{ Installed = $true; Version = 'v14.50.99999.0'; Expected = $false },
    @{ Installed = $false; Version = 'v14.51.36231.0'; Expected = $false },
    @{ Installed = $true; Version = ''; Expected = $false },
    @{ Installed = $true; Version = 'invalid'; Expected = $false }
)
foreach ($case in $cases) {
    if ((Test-VcRuntimeVersion $case.Installed $case.Version $minimum) -ne $case.Expected) {
        throw "Prerequisite comparison failed: $($case.Version)"
    }
}
# Verify the distributed script still reads a manifest beside itself, rather
# than in the parent of a staged package. Both branches reject before registry
# probing: the explicit fallback minimum is invalid if the manifest is missed.
$fixture = Join-Path $OpenKeyRepositoryRoot ('dist\prerequisites-tests-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $fixture -Force | Out-Null
$copiedHelper = Join-Path $fixture 'check-prerequisites.ps1'
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'check-prerequisites.ps1') -Destination $copiedHelper
'{"architecture":"arm64"}' | Set-Content -LiteralPath (Join-Path $fixture 'package-manifest.json') -Encoding utf8
$testShell = (Get-Process -Id $PID).Path
try {
    $ErrorActionPreference = 'Continue'
    $output = (& $testShell -NoProfile -File $copiedHelper -MinimumVersion 0.0 -ReportOnly 2>&1 | Out-String)
    $testExitCode = $LASTEXITCODE
} finally { $ErrorActionPreference = 'Stop' }
if ($testExitCode -eq 0 -or $output -notmatch 'arm64') {
    throw 'Staged prerequisite helper did not read its adjacent manifest.'
}
Write-Host '7/7 prerequisite checks passed, including staged package manifest lookup. No registry mutation or installation.'
