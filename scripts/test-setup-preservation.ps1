
$OpenKeyRepositoryRoot = Split-Path -Parent $PSScriptRoot
$ErrorActionPreference = 'Stop'
. (Join-Path $OpenKeyRepositoryRoot 'Installer\DataPreservation.ps1')
$source = Get-Content -LiteralPath (Join-Path $OpenKeyRepositoryRoot 'Installer\OpenKey.iss') -Raw
$policy = Get-Content -LiteralPath (Join-Path $OpenKeyRepositoryRoot 'Installer\RuntimePolicy.iss') -Raw
$passed = 0
Assert-OpenKeyDataPreservation $source $policy
$passed++
$mutations = @(
    ($source + "`n[Registry]`n"),
    ($source + "`n[InstallDelete]`n"),
    ($source + "`n[UninstallDelete]`n"),
    ($source.Replace('PrivilegesRequired=lowest', 'PrivilegesRequired=admin')),
    ($source.Replace('CloseApplications=no', 'CloseApplications=yes')),
    ($source.Replace('RestartApplications=no', 'RestartApplications=yes')),
    ($source.Replace('88759ADC-2201-4DF1-A68A-687EB37B1A02', '88759ADC-2201-4DF1-A68A-687EB37B1A03')),
    ($source.Replace('DestDir: "{app}"', 'DestDir: "{userappdata}"')),
    ($source.Replace('postinstall nowait skipifsilent unchecked', 'postinstall nowait')),
    ($source + "`n#include `"other.iss`"`n"),
    ($source + "`nRegWriteDWordValue(HKCU, 'fixture', 'value', 1);`n"),
    ($source + "`nDeleteFile('fixture');`n"),
    ($source + "`nSaveStringToFile('fixture', 'text', False);`n")
)
foreach ($changed in $mutations) {
    $rejected = $false
    try { Assert-OpenKeyDataPreservation $changed $policy } catch { $rejected = $true }
    if (-not $rejected) { throw "Preservation mutation $passed was accepted." }
    $passed++
}
foreach ($changedPolicy in @(($policy + "`nRegDeleteKeyIncludingSubkeys(HKCU, 'fixture');"), ($policy + "`n#include `"other.iss`""))) {
    $rejected = $false
    try { Assert-OpenKeyDataPreservation $source $changedPolicy } catch { $rejected = $true }
    if (-not $rejected) { throw 'Included policy mutation was accepted.' }
    $passed++
}
$build = Get-Content -LiteralPath (Join-Path $OpenKeyRepositoryRoot 'scripts\build-setup.ps1') -Raw
if ($build.IndexOf('Assert-OpenKeyDataPreservation -Source') -lt 0 -or
    $build.IndexOf('Assert-OpenKeyDataPreservation -Source') -gt $build.IndexOf('Resolve-OpenKeySetupCompiler')) {
    throw 'Preservation guard must run before compiler/runtime/build/staging work.'
}
$passed++
Write-Output "Data preservation: $passed source-policy checks passed. No registry access or installation."
