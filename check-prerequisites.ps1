[CmdletBinding()]
param(
    [ValidateSet('x64', 'x86')]
    [string]$Platform = 'x64',
    [version]$MinimumVersion = '14.51.36231.0',
    [switch]$ReportOnly
)
$ErrorActionPreference = 'Stop'

function Test-VcRuntimeVersion {
    param([bool]$Installed, [string]$Version, [version]$RequiredVersion)
    $parsed = $null
    return $Installed -and [version]::TryParse($Version.TrimStart('v', 'V'), [ref]$parsed) -and $parsed -ge $RequiredVersion
}

# Dot-source only for unit tests; no registry reads when importing the helper.
if ($MyInvocation.InvocationName -eq '.') { return }
$manifestPath = Join-Path $PSScriptRoot 'package-manifest.json'
if (Test-Path -LiteralPath $manifestPath) {
    $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    if (-not $PSBoundParameters.ContainsKey('Platform')) { $Platform = $manifest.architecture }
    if (-not $PSBoundParameters.ContainsKey('MinimumVersion')) {
        if (-not $manifest.vcRuntimeMinimumVersion) { throw 'Package manifest has no VC runtime minimum version.' }
        $MinimumVersion = [version]$manifest.vcRuntimeMinimumVersion
    }
    if ($Platform -notin @('x64', 'x86')) { throw 'Unsupported package architecture.' }
}
if ($MinimumVersion -lt [version]'14.0') { throw 'A valid VC v14 minimum version is required.' }
$bestVersion = $null
# Probe both registry views explicitly so results do not depend on PowerShell bitness.
foreach ($view in @([Microsoft.Win32.RegistryView]::Registry32, [Microsoft.Win32.RegistryView]::Registry64)) {
    $base = [Microsoft.Win32.RegistryKey]::OpenBaseKey([Microsoft.Win32.RegistryHive]::LocalMachine, $view)
    try {
        $key = $base.OpenSubKey("SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\$Platform", $false)
        if ($key) {
            try {
                $version = [string]$key.GetValue('Version', '')
                $installed = $key.GetValue('Installed', 0) -eq 1
                if (Test-VcRuntimeVersion $installed $version ([version]'0.0')) {
                    $parsed = [version]$version.TrimStart('v', 'V')
                    if ($null -eq $bestVersion -or $parsed -gt $bestVersion) { $bestVersion = $parsed }
                }
            } finally { $key.Dispose() }
        }
    } finally { $base.Dispose() }
}
$satisfied = $null -ne $bestVersion -and $bestVersion -ge $MinimumVersion
[pscustomobject]@{
    Architecture = $Platform
    RequiredVcRuntime = $MinimumVersion.ToString()
    InstalledVcRuntime = if ($bestVersion) { $bestVersion.ToString() } else { 'Not detected' }
    VcRuntimeSatisfied = $satisfied
    Download = "https://aka.ms/vc14/vc_redist.$Platform.exe"
    Scope = 'Registry prerequisite check only; not a clean-machine or DLL-load test'
}
if (-not $satisfied -and -not $ReportOnly) {
    throw 'Visual C++ runtime prerequisite is missing or too old. Download from Microsoft; this script does not install anything.'
}
