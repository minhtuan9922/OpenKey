[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$ZipPath,
    [Parameter(Mandatory)][ValidatePattern('^[0-9A-Fa-f]{64}$')][string]$ExpectedSha256,
    [ValidateSet('x64','x86')][string]$Platform = 'x64'
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Sandbox\SandboxConfig.ps1')
$verified = & (Join-Path $PSScriptRoot 'verify-beta.ps1') -ZipPath $ZipPath -ExpectedArchitecture $Platform -ExpectedSha256 $ExpectedSha256
# Only share a new, bounded staging folder, never the repository or user profile.
$sessionRoot = Join-Path $PSScriptRoot ('dist\sandbox-' + [guid]::NewGuid().ToString('N'))
$inputRoot = Join-Path $sessionRoot 'Input'
$logsRoot = Join-Path $sessionRoot 'Logs'
New-Item -ItemType Directory -Path $inputRoot, $logsRoot | Out-Null
Copy-Item -LiteralPath $verified.Zip -Destination (Join-Path $inputRoot 'beta.zip')
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'verify-beta.ps1'),
    (Join-Path $PSScriptRoot 'Sandbox\SandboxSmoke.ps1') -Destination $inputRoot
# Bind the staged copy, not just the original archive, to the requested digest.
if ((Get-FileHash -LiteralPath (Join-Path $inputRoot 'beta.zip') -Algorithm SHA256).Hash -ne $ExpectedSha256) {
    throw 'Staged ZIP hash changed. Sandbox configuration was not created.'
}
@{ architecture = $Platform; sha256 = $ExpectedSha256.ToUpperInvariant() } |
    ConvertTo-Json | Set-Content -LiteralPath (Join-Path $inputRoot 'session.json') -Encoding UTF8
$configurationPath = Join-Path $sessionRoot 'OpenKey.wsb'
$configuration = New-OpenKeySandboxConfiguration $inputRoot $logsRoot
$configuration.Save($configurationPath)
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'Sandbox\README.md') -Destination $sessionRoot
[pscustomobject]@{
    Configuration = $configurationPath; Logs = $logsRoot
    Architecture = $Platform; PackageSha256 = $ExpectedSha256.ToUpperInvariant()
    Scope = 'Prepared only. Sandbox not launched; runtime not downloaded/installed; host settings untouched.'
}
