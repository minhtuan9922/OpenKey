[CmdletBinding()]
param([switch]$DownloadOnly)
$OpenKeyRepositoryRoot = Split-Path -Parent $PSScriptRoot

$ErrorActionPreference = 'Stop'
if (-not $DownloadOnly -and $env:GITHUB_ACTIONS -ne 'true') {
    throw 'Compiler preparation runs only on a GitHub runner. Locally use -DownloadOnly (no installer executed).'
}
. (Join-Path $OpenKeyRepositoryRoot 'Sandbox/OfflineRuntime.ps1')
function Get-VerifiedCiInput([string]$Url, [string]$Path, [string]$Hash) {
    if ($Url -notmatch '^https://' -or $Hash -notmatch '^[A-F0-9]{64}$') { throw 'Invalid pinned CI input.' }
    New-Item -ItemType Directory -Path (Split-Path -Parent $Path) -Force | Out-Null
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        Invoke-WebRequest -Uri $Url -OutFile $Path -TimeoutSec 180
    }
    if ((Get-FileHash -LiteralPath $Path).Hash -ne $Hash) { throw "CI input hash mismatch: $Path (retained for inspection)." }
}
$runtimeInputs = Get-Content -LiteralPath (Join-Path $OpenKeyRepositoryRoot 'Installer/OfflineInputs.json') -Raw | ConvertFrom-Json
foreach ($arch in @('x64','x86')) {
    $runtimeInput = $runtimeInputs.$arch
    $path = Join-Path $OpenKeyRepositoryRoot $runtimeInput.path
    Get-VerifiedCiInput $runtimeInput.url $path $runtimeInput.sha256
    Assert-OpenKeyRuntimeMetadata (Get-OpenKeyRuntimeMetadata $path) $arch ([version]$runtimeInputs.minimumVersion) $runtimeInput.sha256
    Write-Output "Verified Microsoft offline runtime: $arch"
}
$compiler = Get-Content -LiteralPath (Join-Path $OpenKeyRepositoryRoot 'Installer/CiCompiler.json') -Raw | ConvertFrom-Json
$compilerRoot = Join-Path $OpenKeyRepositoryRoot "dist/inno-$($compiler.version)-portable"
$installer = Join-Path $compilerRoot "innosetup-$($compiler.version).exe"
Get-VerifiedCiInput $compiler.url $installer $compiler.sha256
$signature = Get-AuthenticodeSignature -LiteralPath $installer
if ($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notmatch '(^|,\s*)O=Pyrsys B\.V\.(,|$)') {
    throw 'Inno installer requires valid Pyrsys Authenticode.'
}
if ($DownloadOnly) { Write-Output 'CI inputs verified; no installers or application executed.'; return }
$compilerDestination = Join-Path $compilerRoot 'compiler'
$process = Start-Process -FilePath $installer -WindowStyle Hidden -Wait -PassThru -ArgumentList @(
    '/PORTABLE=1','/CURRENTUSER','/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/NOICONS',('/DIR="' + $compilerDestination + '"'))
if ($process.ExitCode -ne 0) { throw "Portable Inno preparation failed: $($process.ExitCode)" }
$iscc = Join-Path $compilerDestination 'ISCC.exe'
if (-not (Test-Path -LiteralPath $iscc -PathType Leaf)) { throw 'Portable compiler missing.' }
if ($env:GITHUB_ENV) { "OPENKEY_ISCC_PATH=$iscc" | Out-File -LiteralPath $env:GITHUB_ENV -Encoding utf8 -Append }
Write-Output "CI compiler prepared: $iscc; runtime installers were not executed."
