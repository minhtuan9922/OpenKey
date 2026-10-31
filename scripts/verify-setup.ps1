[CmdletBinding()]
param([Parameter(Mandatory)][string]$BuildReport,
    [ValidatePattern('^[0-9A-Fa-f]{64}$')][string]$ExpectedSha256)
$OpenKeyRepositoryRoot = Split-Path -Parent $PSScriptRoot

$ErrorActionPreference = 'Stop'
. (Join-Path $OpenKeyRepositoryRoot 'Sandbox\OfflineRuntime.ps1')
$taskReport = Get-Content -LiteralPath $BuildReport -Raw | ConvertFrom-Json
$taskFile = Get-Item -LiteralPath $taskReport.setup
$taskHash = (Get-FileHash -LiteralPath $taskFile.FullName).Hash
if ($taskFile.Length -ne $taskReport.bytes -or $taskHash -ne $taskReport.sha256 -or
    ($ExpectedSha256 -and $taskHash -ne $ExpectedSha256)) { throw 'Setup size/hash mismatch.' }
$taskStream = [IO.File]::OpenRead($taskFile.FullName)
$taskReader = [IO.BinaryReader]::new($taskStream)
try {
    if ($taskReader.ReadUInt16() -ne 0x5a4d) { throw 'Setup is not a PE executable.' }
    $taskStream.Position = 0x3c; $taskOffset = $taskReader.ReadInt32()
    if ($taskOffset -lt 64 -or $taskOffset -gt $taskStream.Length - 6) { throw 'Invalid PE offset.' }
    $taskStream.Position = $taskOffset
    if ($taskReader.ReadUInt32() -ne 0x4550 -or $taskReader.ReadUInt16() -ne 0x14c) {
        throw 'Expected x86 bootstrap for a single x86/x64 setup.'
    }
} finally { $taskReader.Dispose() }
$taskStage = Split-Path -Parent (Resolve-Path -LiteralPath $BuildReport).Path
if ((Get-FileHash -LiteralPath $taskReport.compilerManifest).Hash -ne $taskReport.compilerManifestSha256) {
    throw 'Compiler manifest changed.'
}
$taskCompiled = @{}
foreach ($taskRow in (Import-Csv -LiteralPath $taskReport.compilerManifest -Delimiter "`t")) {
    if ($taskCompiled.ContainsKey($taskRow.SourceFilename) -or $taskRow.FirstSlice -ne '1' -or $taskRow.LastSlice -ne '1') {
        throw 'Duplicate compiler payload or non-single-file media.'
    }
    $taskCompiled[$taskRow.SourceFilename] = $taskRow
}
$taskExpectedFiles = @(Get-ChildItem -LiteralPath (Join-Path $taskStage 'x64'), (Join-Path $taskStage 'x86'),
    (Join-Path $taskStage 'runtime') -File -Recurse)
if ($taskCompiled.Count -ne $taskExpectedFiles.Count) { throw 'Compiler embedded file inventory mismatch.' }
foreach ($taskExpected in $taskExpectedFiles) {
    $taskEmbedded = $taskCompiled[$taskExpected.FullName]
    if (-not $taskEmbedded -or [long]$taskEmbedded.OriginalSize -ne $taskExpected.Length -or
        $taskEmbedded.SHA256Sum -ne (Get-FileHash -LiteralPath $taskExpected.FullName).Hash) {
        throw "Compiler payload bytes mismatch: $($taskExpected.FullName)"
    }
}
Write-Output "Compiler manifest confirms $($taskCompiled.Count) embedded inputs on one media slice."
foreach ($taskArch in @('x64','x86')) {
    $taskZip = $taskReport."zip$taskArch"
    $taskZipHash = $taskReport."zip$($taskArch)Sha256"
    & (Join-Path $OpenKeyRepositoryRoot 'scripts\verify-beta.ps1') -ZipPath $taskZip -ExpectedArchitecture $taskArch -ExpectedSha256 $taskZipHash | Out-Null
    $taskRoot = Join-Path $taskStage $taskArch
    $taskManifest = Get-Content -LiteralPath (Join-Path $taskRoot 'package-manifest.json') -Raw | ConvertFrom-Json
    if ($taskManifest.architecture -ne $taskArch -or $taskManifest.configuration -ne 'Release' -or
        $taskManifest.vcRuntimeMinimumVersion -ne $taskReport.minimumRuntime) { throw 'Staged manifest mismatch.' }
    foreach ($taskEntry in $taskManifest.files) {
        $taskTarget = [IO.Path]::GetFullPath((Join-Path $taskRoot $taskEntry.path))
        if (-not $taskTarget.StartsWith($taskRoot + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe staged payload path.' }
        $taskPayload = Get-Item -LiteralPath $taskTarget
        if ($taskPayload.Length -ne $taskEntry.bytes -or (Get-FileHash -LiteralPath $taskTarget).Hash -ne $taskEntry.sha256) {
            throw "Staged payload changed: $taskTarget"
        }
    }
    $taskRuntime = $taskReport."runtime$taskArch"
    Assert-OpenKeyRuntimeMetadata (Get-OpenKeyRuntimeMetadata $taskRuntime.Path) $taskArch ([version]$taskReport.minimumRuntime) $taskRuntime.Sha256
    Write-Output "Verified $taskArch Release ZIP, $(@($taskManifest.files).Count) staged payloads and signed Microsoft runtime."
}
$taskSources = @{
    'Installer\OpenKey.iss' = $taskReport.installerSourceSha256
    'Installer\RuntimePolicy.iss' = $taskReport.policySourceSha256
    'Installer\CaretPolicy.iss' = $taskReport.caretPolicySourceSha256
    'Installer\Vietnamese.isl' = $taskReport.translationSha256
}
foreach ($taskRelative in $taskSources.Keys) {
    if ((Get-FileHash -LiteralPath (Join-Path $OpenKeyRepositoryRoot $taskRelative)).Hash -ne $taskSources[$taskRelative]) {
        throw "Installer source changed since compile: $taskRelative"
    }
}
Write-Output "Setup PE/size/SHA-256 verified: $taskHash"
Write-Output 'Verification covers build inputs/artifact integrity, not extraction, UAC, shortcut or uninstall behavior on a clean machine.'
