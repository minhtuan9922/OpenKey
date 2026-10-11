[CmdletBinding()]
param([Parameter(Mandatory)][string]$Tag)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Installer/ReleasePolicy.ps1')
$metadata = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'version.json') -Raw | ConvertFrom-Json
$version = Assert-OpenKeyReleaseTag $Tag $metadata
& (Join-Path $PSScriptRoot 'test-ui-resources.ps1') | Out-Host
$setupResult = $null
& (Join-Path $PSScriptRoot 'build-setup.ps1') -Release -PassThru | ForEach-Object {
    if ($_ -is [pscustomobject] -and $_.Kind -eq 'OpenKeySetup') { $setupResult = $_ }
    else { $_ | Out-Host }
}
if (-not $setupResult) { throw 'No verified setup returned.' }
# Full fixture regression must pass before producing an upload directory.
& (Join-Path $PSScriptRoot 'test-all.ps1') All | Out-Host
if ($LASTEXITCODE -ne 0) { throw 'Release regression failed.' }
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
$output = Join-Path $PSScriptRoot 'dist/ci-release'
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
