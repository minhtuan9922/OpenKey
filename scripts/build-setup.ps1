[CmdletBinding(DefaultParameterSetName = 'ExistingPackages')]
param(
    [Parameter(Mandatory, ParameterSetName = 'ReleaseBuild')][switch]$Release,
    [Parameter(Mandatory, ParameterSetName = 'ExistingPackages')][string]$ZipX64,
    [Parameter(Mandatory, ParameterSetName = 'ExistingPackages')][string]$ZipX86,
    [string]$RuntimeX64 = '',
    [string]$RuntimeHashX64 = '',
    [string]$RuntimeX86 = '',
    [string]$RuntimeHashX86 = '',
    [string]$IsccPath = '',
    [switch]$PassThru
)
$OpenKeyRepositoryRoot = Split-Path -Parent $PSScriptRoot

$ErrorActionPreference = 'Stop'
. (Join-Path $OpenKeyRepositoryRoot 'Installer\DataPreservation.ps1')
Assert-OpenKeyDataPreservation -Source (Get-Content -LiteralPath (Join-Path $OpenKeyRepositoryRoot 'Installer\OpenKey.iss') -Raw) -RuntimePolicy (Get-Content -LiteralPath (Join-Path $OpenKeyRepositoryRoot 'Installer\RuntimePolicy.iss') -Raw)
. (Join-Path $OpenKeyRepositoryRoot 'Installer\Compiler.ps1')
$IsccPath = Resolve-OpenKeySetupCompiler $IsccPath
. (Join-Path $OpenKeyRepositoryRoot 'Sandbox\OfflineRuntime.ps1')
$taskDefaults = Get-Content -LiteralPath (Join-Path $OpenKeyRepositoryRoot 'Installer\OfflineInputs.json') -Raw | ConvertFrom-Json
Assert-OpenKeyRuntimeArguments $RuntimeX64 $RuntimeHashX64
Assert-OpenKeyRuntimeArguments $RuntimeX86 $RuntimeHashX86
if (-not $RuntimeX64) {
    $RuntimeX64 = Join-Path $OpenKeyRepositoryRoot $taskDefaults.x64.path
    $RuntimeHashX64 = $taskDefaults.x64.sha256
}
if (-not $RuntimeX86) {
    $RuntimeX86 = Join-Path $OpenKeyRepositoryRoot $taskDefaults.x86.path
    $RuntimeHashX86 = $taskDefaults.x86.sha256
}
# Validate compiler and pinned runtime inputs before spending time building.
foreach ($taskArch in @('x64','x86')) {
    $taskRuntimePath = if ($taskArch -eq 'x64') { $RuntimeX64 } else { $RuntimeX86 }
    $taskExpectedHash = if ($taskArch -eq 'x64') { $RuntimeHashX64 } else { $RuntimeHashX86 }
    if (-not (Test-Path -LiteralPath $taskRuntimePath -PathType Leaf)) {
        throw "Missing offline runtime: $taskRuntimePath. See INSTALLER.md or supply -Runtime$taskArch and -RuntimeHash$taskArch."
    }
    Assert-OpenKeyRuntimeMetadata (Get-OpenKeyRuntimeMetadata $taskRuntimePath) $taskArch ([version]$taskDefaults.minimumVersion) $taskExpectedHash
}
if ($PSCmdlet.ParameterSetName -eq 'ReleaseBuild') {
    if (-not $Release) { throw 'Use -Release to build fresh x64/x86 packages.' }
    foreach ($taskArch in @('x64','x86')) {
        Write-Host "Building and packaging Release $taskArch..."
        $taskPackage = $null
        & (Join-Path $OpenKeyRepositoryRoot 'scripts\package-beta.ps1') $taskArch -PassThru | ForEach-Object {
            if ($_ -is [pscustomobject] -and $_.Kind -eq 'OpenKeyBetaPackage') { $taskPackage = $_ }
            else { $_ | Out-Host }
        }
        if (-not $taskPackage -or $taskPackage.Architecture -ne $taskArch) { throw "No fresh $taskArch package was returned." }
        if ($taskArch -eq 'x64') { $ZipX64 = $taskPackage.ZipPath } else { $ZipX86 = $taskPackage.ZipPath }
    }
}
foreach ($taskPath in @($ZipX64, $ZipX86, $RuntimeX64, $RuntimeX86, $IsccPath)) {
    if (-not (Test-Path -LiteralPath $taskPath -PathType Leaf)) { throw "Missing input: $taskPath" }
}
$ZipX64 = (Resolve-Path -LiteralPath $ZipX64).Path
$ZipX86 = (Resolve-Path -LiteralPath $ZipX86).Path
$RuntimeX64 = (Resolve-Path -LiteralPath $RuntimeX64).Path
$RuntimeX86 = (Resolve-Path -LiteralPath $RuntimeX86).Path
$IsccPath = (Resolve-Path -LiteralPath $IsccPath).Path
# Fail before creating staging if either architecture or manifest/hash is invalid.
& (Join-Path $OpenKeyRepositoryRoot 'scripts\verify-beta.ps1') -ZipPath $ZipX64 -ExpectedArchitecture x64 | Out-Null
& (Join-Path $OpenKeyRepositoryRoot 'scripts\verify-beta.ps1') -ZipPath $ZipX86 -ExpectedArchitecture x86 | Out-Null
Add-Type -AssemblyName System.IO.Compression.FileSystem
function Read-SetupManifest([string]$Path) {
    $archive = [IO.Compression.ZipFile]::OpenRead($Path)
    try {
        $reader = [IO.StreamReader]::new($archive.GetEntry('package-manifest.json').Open())
        try { $reader.ReadToEnd() | ConvertFrom-Json } finally { $reader.Dispose() }
    } finally { $archive.Dispose() }
}
$taskManifest64 = Read-SetupManifest $ZipX64
$taskManifest86 = Read-SetupManifest $ZipX86
if ($taskManifest64.configuration -ne 'Release' -or $taskManifest86.configuration -ne 'Release' -or
    $taskManifest64.vcRuntimeMinimumVersion -ne $taskManifest86.vcRuntimeMinimumVersion) {
    throw 'Setup requires matching Release payloads and runtime minimum versions.'
}
$taskMinimum = [version]$taskManifest64.vcRuntimeMinimumVersion
$taskRuntime64 = Get-OpenKeyRuntimeMetadata $RuntimeX64
$taskRuntime86 = Get-OpenKeyRuntimeMetadata $RuntimeX86
Assert-OpenKeyRuntimeMetadata $taskRuntime64 x64 $taskMinimum $RuntimeHashX64
Assert-OpenKeyRuntimeMetadata $taskRuntime86 x86 $taskMinimum $RuntimeHashX86
$taskStage = Join-Path $OpenKeyRepositoryRoot ('dist\setup-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $taskStage | Out-Null
$taskPayload64 = Join-Path $taskStage 'x64'
$taskPayload86 = Join-Path $taskStage 'x86'
[IO.Compression.ZipFile]::ExtractToDirectory($ZipX64, $taskPayload64)
[IO.Compression.ZipFile]::ExtractToDirectory($ZipX86, $taskPayload86)
# Embed the exact verified bytes, not a mutable external runtime path.
$taskRuntimeDir = Join-Path $taskStage 'runtime'
New-Item -ItemType Directory -Path $taskRuntimeDir | Out-Null
Copy-Item -LiteralPath $RuntimeX64 -Destination (Join-Path $taskRuntimeDir 'vc_redist.x64.exe')
Copy-Item -LiteralPath $RuntimeX86 -Destination (Join-Path $taskRuntimeDir 'vc_redist.x86.exe')
$taskRuntime64 = Get-OpenKeyRuntimeMetadata (Join-Path $taskRuntimeDir 'vc_redist.x64.exe')
$taskRuntime86 = Get-OpenKeyRuntimeMetadata (Join-Path $taskRuntimeDir 'vc_redist.x86.exe')
Assert-OpenKeyRuntimeMetadata $taskRuntime64 x64 $taskMinimum $RuntimeHashX64
Assert-OpenKeyRuntimeMetadata $taskRuntime86 x86 $taskMinimum $RuntimeHashX86
$taskOutput = Join-Path $taskStage 'Output'
$taskArguments = @('/Qp', "/DPayloadX64=$taskPayload64", "/DPayloadX86=$taskPayload86",
    "/DRuntimeX64=$($taskRuntime64.Path)", "/DRuntimeX86=$($taskRuntime86.Path)",
    "/DRuntimeHashX64=$RuntimeHashX64", "/DRuntimeHashX86=$RuntimeHashX86",
    "/DMinimumRuntime=$taskMinimum", "/DSetupOutput=$taskOutput", "/DSetupManifest=$taskStage\compiler-manifest.txt",
    (Join-Path $OpenKeyRepositoryRoot 'Installer\OpenKey.iss'))
& $IsccPath @taskArguments
if ($LASTEXITCODE -ne 0) { throw "Inno Setup compilation failed: $LASTEXITCODE. Staging retained: $taskStage" }
$taskSetup = Join-Path $taskOutput 'OpenKey-Setup.exe'
if (-not (Test-Path -LiteralPath $taskSetup -PathType Leaf)) { throw 'Setup executable was not produced.' }
if (@(Get-ChildItem -LiteralPath $taskOutput -File).Count -ne 1) { throw 'Expected exactly one offline setup file.' }
$taskHash = (Get-FileHash -LiteralPath $taskSetup -Algorithm SHA256).Hash
[ordered]@{
    setup = $taskSetup; sha256 = $taskHash; bytes = (Get-Item -LiteralPath $taskSetup).Length
    architecture = 'auto x64/x86; ARM not supported'; configuration = 'Release'; channel = 'internal-beta'
    minimumRuntime = $taskMinimum.ToString(); runtimeX64 = $taskRuntime64; runtimeX86 = $taskRuntime86
    zipX64 = $ZipX64; zipX64Sha256 = (Get-FileHash -LiteralPath $ZipX64).Hash
    zipX86 = $ZipX86; zipX86Sha256 = (Get-FileHash -LiteralPath $ZipX86).Hash
    compiler = $IsccPath; compilerSha256 = (Get-FileHash -LiteralPath $IsccPath).Hash
    installerSourceSha256 = (Get-FileHash -LiteralPath (Join-Path $OpenKeyRepositoryRoot 'Installer\OpenKey.iss')).Hash
    policySourceSha256 = (Get-FileHash -LiteralPath (Join-Path $OpenKeyRepositoryRoot 'Installer\RuntimePolicy.iss')).Hash
    translationSha256 = (Get-FileHash -LiteralPath (Join-Path $OpenKeyRepositoryRoot 'Installer\Vietnamese.isl')).Hash
    compilerManifest = (Join-Path $taskStage 'compiler-manifest.txt')
    compilerManifestSha256 = (Get-FileHash -LiteralPath (Join-Path $taskStage 'compiler-manifest.txt')).Hash
    signed = $false; cleanMachineAcceptance = 'pending; setup not executed by build script'
} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $taskStage 'setup-build.json') -Encoding UTF8
Write-Host "Setup: $taskSetup"
Write-Host "SHA-256: $taskHash"
& (Join-Path $OpenKeyRepositoryRoot 'scripts\verify-setup.ps1') -BuildReport (Join-Path $taskStage 'setup-build.json') -ExpectedSha256 $taskHash | Out-Host
Write-Host ''
Write-Host "SETUP FILE: $taskSetup" -ForegroundColor Green
Write-Host "OUTPUT FOLDER: $taskOutput" -ForegroundColor Cyan
Write-Host ('Open folder: Invoke-Item -LiteralPath "' + $taskOutput + '"')
Write-Host ('Select setup: explorer.exe /select,"' + $taskSetup + '"')
Write-Host 'Build only: no application/runtime installed or started on this machine.'
if ($PassThru) {
    [pscustomobject]@{ Kind = 'OpenKeySetup'; Setup = $taskSetup; BuildReport = (Join-Path $taskStage 'setup-build.json') }
}
