[CmdletBinding()]
param([Parameter(Mandatory)][string]$Tag)
$OpenKeyRepositoryRoot = Split-Path -Parent $PSScriptRoot

$ErrorActionPreference = 'Stop'
. (Join-Path $OpenKeyRepositoryRoot 'Installer/ReleasePolicy.ps1')
$metadata = Get-Content -LiteralPath (Join-Path $OpenKeyRepositoryRoot 'version.json') -Raw | ConvertFrom-Json
$version = Assert-OpenKeyReleaseTag $Tag $metadata
& (Join-Path $OpenKeyRepositoryRoot 'scripts\test-ui-resources.ps1') | Out-Host
$setupResult = $null
& (Join-Path $OpenKeyRepositoryRoot 'scripts\build-setup.ps1') -Release -PassThru | ForEach-Object {
    if ($_ -is [pscustomobject] -and $_.Kind -eq 'OpenKeySetup') { $setupResult = $_ }
    else { $_ | Out-Host }
}
if (-not $setupResult) { throw 'No verified setup returned.' }
# Both supported script hosts must pass before producing an upload directory.
# Separate processes also isolate exit codes and suite fixture definitions.
$regressionHosts = @(
    (Get-Command pwsh.exe -ErrorAction Stop).Source,
    (Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe')
)
foreach ($regressionHost in $regressionHosts) {
    if (-not (Test-Path -LiteralPath $regressionHost -PathType Leaf)) { throw 'Required regression shell is missing.' }
    & $regressionHost -NoProfile -ExecutionPolicy Bypass -File (Join-Path $OpenKeyRepositoryRoot 'test-all.ps1') All | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "Release regression failed in $regressionHost. No upload directory produced." }
}
$report = Get-Content -LiteralPath $setupResult.BuildReport -Raw | ConvertFrom-Json
$stage = Split-Path -Parent $setupResult.BuildReport
foreach ($arch in @('x64','x86')) {
    $engine = if ($arch -eq 'x64') { 'OpenKey64.exe' } else { 'OpenKey32.exe' }
    foreach ($relative in @($engine,'OpenKeyUpdate.exe','OpenKey.UI/OpenKey.UI.exe')) {
        $file = Get-Item -LiteralPath (Join-Path $stage "$arch/$relative")
        if ($file.VersionInfo.FileVersion.Trim() -ne "$version.0") { throw "Release binary version mismatch: $($file.FullName)" }
    }
}
if ((Get-Item -LiteralPath $setupResult.Setup).VersionInfo.ProductVersion.Trim() -ne $version) { throw 'Setup version mismatch.' }
$output = Join-Path $OpenKeyRepositoryRoot 'dist/ci-release'
if (Test-Path -LiteralPath $output) { throw 'CI release output already exists; refusing to overwrite.' }
New-Item -ItemType Directory -Path $output | Out-Null
Copy-Item -LiteralPath $setupResult.Setup -Destination (Join-Path $output 'OpenKey-Setup.exe')
"$($report.sha256.ToLowerInvariant())  OpenKey-Setup.exe" | Set-Content -LiteralPath (Join-Path $output 'SHA256SUMS.txt') -Encoding ascii
@"
OpenKey Fluent $version (automated beta build)

Single offline setup for Windows x64/x86; includes engine, Fluent UI and runtime prerequisites.
SHA-256: $($report.sha256)
Not code-signed. Clean-machine installation, upgrade and uninstall acceptance remains pending.
Exit OpenKey before running setup. GPLv3 and third-party notices included.
"@ | Set-Content -LiteralPath (Join-Path $output 'RELEASE_NOTES.md') -Encoding utf8
Write-Output "Release upload files: $output"
