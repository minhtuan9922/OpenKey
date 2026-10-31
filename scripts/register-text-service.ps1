[CmdletBinding()]
param([switch]$Unregister, [string]$ComponentRoot)
$ErrorActionPreference = 'Stop'
$OpenKeyRepositoryRoot = Split-Path -Parent $PSScriptRoot
if (-not $ComponentRoot) { $ComponentRoot = Join-Path $OpenKeyRepositoryRoot 'dist\text-service' }
# Resolve against PowerShell's location, which can differ from .NET's cwd.
$ComponentRoot = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($ComponentRoot)
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = [Security.Principal.WindowsPrincipal]::new($identity)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw 'TSF COM/profile registration needs an Administrator PowerShell. This script does not elevate automatically.'
}
$architectures = if ([Environment]::Is64BitOperatingSystem) { @('x64','x86') } else { @('x86') }
$completed = @()
foreach ($arch in $architectures) {
    $dll = Join-Path $ComponentRoot "$arch\OpenKey.TextService.dll"
    if (-not (Test-Path -LiteralPath $dll -PathType Leaf)) { throw "Build the text service first: $dll" }
}
foreach ($arch in $architectures) {
    $dll = Join-Path $ComponentRoot "$arch\OpenKey.TextService.dll"
    $folder = if ($arch -eq 'x86' -and [Environment]::Is64BitOperatingSystem) { 'SysWOW64' }
        elseif ([Environment]::Is64BitOperatingSystem -and -not [Environment]::Is64BitProcess) { 'Sysnative' } else { 'System32' }
    $registrationTool = Join-Path $env:WINDIR "$folder\regsvr32.exe"
    $arguments = @('/s'); if ($Unregister) { $arguments += '/u' }; $arguments += '"' + $dll + '"'
    $process = Start-Process -FilePath $registrationTool -ArgumentList $arguments -WindowStyle Hidden -Wait -PassThru
    if ($process.ExitCode -ne 0) {
        throw "Registration operation failed for $arch (exit $($process.ExitCode)); completed: $completed. Rerun with -Unregister to remove installed profiles."
    }
    $completed += $arch
}
if ($Unregister) { Write-Output 'Removed OpenKey Caret Editing profiles. Restart editors before deleting loaded DLLs.' }
else { Write-Output 'Registered OpenKey Caret Editing. Restart editors, then select this Vietnamese input profile using Win+Space. The default profile was not changed.' }
