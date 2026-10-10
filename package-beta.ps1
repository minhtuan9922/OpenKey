[CmdletBinding()]
param(
    [ValidateSet('x64', 'x86')]
    [string]$Platform = 'x64',
    # Must be reviewed if the build toolset is upgraded; current v145 build uses 14.51.36231.
    [version]$MinimumVcRuntimeVersion = '14.51.36231.0',
    [switch]$SkipBuild
)
$ErrorActionPreference = 'Stop'
if (-not $SkipBuild) {
    & (Join-Path $PSScriptRoot 'build.ps1') Release $Platform All
    if ($LASTEXITCODE -ne 0) { throw 'Release build failed.' }
}
$projectRoot = Join-Path $PSScriptRoot 'Sources\OpenKey\win32\OpenKey'
$prefix = if ($Platform -eq 'x64') { 'x64\Release' } else { 'Release' }
$engineName = if ($Platform -eq 'x64') { 'OpenKey64.exe' } else { 'OpenKey32.exe' }
$engine = Join-Path $projectRoot "OpenKey\$prefix\$engineName"
$updater = Join-Path $projectRoot "OpenKeyUpdate\$prefix\OpenKeyUpdate.exe"
$uiRoot = Join-Path $projectRoot "OpenKey.UI\$prefix\OpenKey.UI"
$requiredUI = @('OpenKey.UI.exe', 'OpenKey.UI.pri', 'Microsoft.UI.Xaml.dll',
    'Microsoft.WindowsAppRuntime.dll', 'MainWindow.xbf', 'AboutPage.xbf')
foreach ($path in @($engine, $updater) + ($requiredUI | ForEach-Object { Join-Path $uiRoot $_ })) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing Release output: $path" }
}
function Assert-PEArchitecture([string]$Path) {
    $stream = [IO.File]::OpenRead($Path)
    $reader = New-Object IO.BinaryReader($stream)
    try {
        if ($reader.ReadUInt16() -ne 0x5a4d) { throw "Not a PE file: $Path" }
        $stream.Position = 0x3c
        $peOffset = $reader.ReadInt32()
        $stream.Position = $peOffset
        if ($reader.ReadUInt32() -ne 0x4550) { throw "Invalid PE signature: $Path" }
        $expected = if ($Platform -eq 'x64') { 0x8664 } else { 0x14c }
        if ($reader.ReadUInt16() -ne $expected) { throw "Wrong architecture: $Path" }
    } finally { $reader.Dispose() }
}
foreach ($path in @($engine, $updater, (Join-Path $uiRoot 'OpenKey.UI.exe'))) { Assert-PEArchitecture $path }
& (Join-Path $PSScriptRoot 'test-ui-binary.ps1') $Platform Release
# Never overwrite a previous package or delete a build/source directory.
$packageName = 'OpenKey-Fluent-beta-' + $Platform + '-' + [guid]::NewGuid().ToString('N')
$distribution = Join-Path $PSScriptRoot 'dist'
New-Item -ItemType Directory -Path $distribution -Force | Out-Null
$stage = Join-Path $distribution $packageName
New-Item -ItemType Directory -Path $stage | Out-Null
Copy-Item -LiteralPath $engine, $updater -Destination $stage
$uiDestination = Join-Path $stage 'OpenKey.UI'
New-Item -ItemType Directory -Path $uiDestination | Out-Null
# Keep the complete self-contained runtime layout, including localized resources.
# Exclude only development artifacts; do not reduce it to just EXE/DLL files.
$excludedExtensions = @('.pdb', '.exp', '.lib', '.obj', '.pch', '.ilk', '.log', '.tlog', '.recipe', '.lastcodeanalysissucceeded')
foreach ($file in Get-ChildItem -LiteralPath $uiRoot -File -Recurse) {
    if ($file.Extension.ToLowerInvariant() -in $excludedExtensions) { continue }
    $relative = $file.FullName.Substring($uiRoot.Length + 1)
    $destination = Join-Path $uiDestination $relative
    New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
    Copy-Item -LiteralPath $file.FullName -Destination $destination
}
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'LICENSE'), (Join-Path $PSScriptRoot 'BETA_PACKAGING.md'), (Join-Path $PSScriptRoot 'HTML_ENTITIES_NOTICE.md') -Destination $stage
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'check-prerequisites.ps1') -Destination $stage
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'verify-beta.ps1') -Destination $stage
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'MIGRATION_FLUENT_BETA.md') -Destination $stage
& (Join-Path $PSScriptRoot 'collect-third-party.ps1') -OutputDirectory (Join-Path $stage 'ThirdPartyNotices')
$records = @(Get-ChildItem -LiteralPath $stage -File -Recurse | Sort-Object FullName | ForEach-Object {
    [ordered]@{
        path = $_.FullName.Substring($stage.Length + 1).Replace('\', '/')
        bytes = $_.Length
        sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
    }
})
$manifest = [ordered]@{
    channel = 'local-beta-not-for-stable-updater'
    architecture = $Platform
    configuration = 'Release'
    skippedBuild = [bool]$SkipBuild
    windowsAppSdk = 'self-contained output copied; clean-machine test pending'
    vcRuntime = 'matching Microsoft Visual C++ Redistributable required; not bundled'
    vcRuntimeMinimumVersion = $MinimumVcRuntimeVersion.ToString()
    files = $records
}
$manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $stage 'package-manifest.json') -Encoding UTF8
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zipPath = Join-Path $distribution ($packageName + '.zip')
[IO.Compression.ZipFile]::CreateFromDirectory($stage, $zipPath, [IO.Compression.CompressionLevel]::Optimal, $false)
# Use the same independent verifier shipped to testers.
& (Join-Path $PSScriptRoot 'verify-beta.ps1') -ZipPath $zipPath -ExpectedArchitecture $Platform | Out-Null
Write-Host "ZIP SHA-256: $((Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash)"
Write-Host "Verified $($records.Count) payload files ($Platform)."
Write-Host "Beta ZIP: $zipPath"
Write-Host 'Clean-machine/runtime/UI tests are still pending. Do not publish as a stable update.'
