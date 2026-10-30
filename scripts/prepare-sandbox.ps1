[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$ZipPath,
    [Parameter(Mandatory)][ValidatePattern('^[0-9A-Fa-f]{64}$')][string]$ExpectedSha256,
    [ValidateSet('x64','x86')][string]$Platform = 'x64',
    [string]$RuntimePath,
    [ValidatePattern('^[0-9A-Fa-f]{64}$')][string]$RuntimeExpectedSha256
)
$OpenKeyRepositoryRoot = Split-Path -Parent $PSScriptRoot

$ErrorActionPreference = 'Stop'
. (Join-Path $OpenKeyRepositoryRoot 'Sandbox\SandboxConfig.ps1')
. (Join-Path $OpenKeyRepositoryRoot 'Sandbox\OfflineRuntime.ps1')
Assert-OpenKeyRuntimeArguments $RuntimePath $RuntimeExpectedSha256
$verified = & (Join-Path $OpenKeyRepositoryRoot 'scripts\verify-beta.ps1') -ZipPath $ZipPath -ExpectedArchitecture $Platform -ExpectedSha256 $ExpectedSha256
$runtime = $null
if ($RuntimePath) {
    $runtime = Get-OpenKeyRuntimeMetadata $RuntimePath
    Assert-OpenKeyRuntimeMetadata $runtime $Platform ([version]$verified.VcRuntimeMinimumVersion) $RuntimeExpectedSha256
}
# Only share a new, bounded staging folder, never the repository or user profile.
$sessionRoot = Join-Path $OpenKeyRepositoryRoot ('dist\sandbox-' + [guid]::NewGuid().ToString('N'))
$inputRoot = Join-Path $sessionRoot 'Input'
$logsRoot = Join-Path $sessionRoot 'Logs'
New-Item -ItemType Directory -Path $inputRoot, $logsRoot | Out-Null
Copy-Item -LiteralPath $verified.Zip -Destination (Join-Path $inputRoot 'beta.zip')
Copy-Item -LiteralPath (Join-Path $OpenKeyRepositoryRoot 'scripts\verify-beta.ps1'),
    (Join-Path $OpenKeyRepositoryRoot 'Sandbox\SandboxSmoke.ps1') -Destination $inputRoot
# Bind the staged copy, not just the original archive, to the requested digest.
if ((Get-FileHash -LiteralPath (Join-Path $inputRoot 'beta.zip') -Algorithm SHA256).Hash -ne $ExpectedSha256) {
    throw 'Staged ZIP hash changed. Sandbox configuration was not created.'
}
$runtimeRecord = $null
if ($runtime) {
    $runtimeName = "vc_redist.$Platform.exe"
    $runtimeTarget = Join-Path $inputRoot $runtimeName
    # Refuse overwrite; revalidate copied bytes before producing a .wsb.
    [IO.File]::Copy($runtime.Path, $runtimeTarget, $false)
    $stagedRuntime = Get-OpenKeyRuntimeMetadata $runtimeTarget
    Assert-OpenKeyRuntimeMetadata $stagedRuntime $Platform ([version]$verified.VcRuntimeMinimumVersion) $RuntimeExpectedSha256
    $runtimeRecord = @{
        file = $runtimeName; sha256 = $stagedRuntime.Sha256
        version = $stagedRuntime.FileVersion; minimumVersion = $verified.VcRuntimeMinimumVersion
        signatureStatus = $stagedRuntime.SignatureStatus; signerSubject = $stagedRuntime.SignerSubject
    }
    $instructions = Get-Content -LiteralPath (Join-Path $OpenKeyRepositoryRoot 'Sandbox\OFFLINE_RUNTIME_TEMPLATE.md') -Raw -Encoding UTF8
    $instructions.Replace('%ARCH%', $Platform).Replace('%VERSION%', $stagedRuntime.FileVersion).
        Replace('%MINIMUM%', $verified.VcRuntimeMinimumVersion).Replace('%SHA256%', $stagedRuntime.Sha256).
        Replace('%DATE%', [DateTime]::UtcNow.ToString('yyyy-MM-dd') + ' UTC') |
        Set-Content -LiteralPath (Join-Path $inputRoot 'OFFLINE_RUNTIME.md') -Encoding UTF8
}
@{ architecture = $Platform; sha256 = $ExpectedSha256.ToUpperInvariant(); runtime = $runtimeRecord } |
    ConvertTo-Json | Set-Content -LiteralPath (Join-Path $inputRoot 'session.json') -Encoding UTF8
$configurationPath = Join-Path $sessionRoot 'OpenKey.wsb'
$configuration = New-OpenKeySandboxConfiguration $inputRoot $logsRoot
$configuration.Save($configurationPath)
Copy-Item -LiteralPath (Join-Path $OpenKeyRepositoryRoot 'Sandbox\README.md') -Destination $sessionRoot
[pscustomobject]@{
    Configuration = $configurationPath; Logs = $logsRoot
    Architecture = $Platform; PackageSha256 = $ExpectedSha256.ToUpperInvariant()
    Runtime = $runtimeRecord
    Scope = 'Prepared only. Sandbox not launched; runtime not downloaded/installed; host settings untouched.'
}
