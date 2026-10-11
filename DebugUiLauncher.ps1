# Pure helpers: importing does not build, start/stop processes, or write files.
function Get-OpenKeyDebugUiExecutable {
    param([string]$RepositoryRoot, [ValidateSet('x64','x86')][string]$Platform)
    if (-not [IO.Path]::IsPathRooted($RepositoryRoot)) { throw 'Repository root must be absolute.' }
    $relative = if ($Platform -eq 'x64') {
        'Sources\OpenKey\win32\OpenKey\OpenKey.UI\x64\Debug\OpenKey.UI\OpenKey.UI.exe'
    } else {
        'Sources\OpenKey\win32\OpenKey\OpenKey.UI\Debug\OpenKey.UI\OpenKey.UI.exe'
    }
    return [IO.Path]::GetFullPath((Join-Path $RepositoryRoot $relative))
}
function Test-OpenKeyDebugUiInUse {
    param([string]$Executable, [object[]]$Processes)
    foreach ($candidate in $Processes) {
        if ($candidate.Path -and [string]::Equals($candidate.Path, $Executable, [StringComparison]::OrdinalIgnoreCase)) { return $true }
    }
    return $false
}
function Get-OpenKeyDebugUiLaunchState {
    param([bool]$Exited, [int]$ExitCode = 0)
    if (-not $Exited) { return 'running-unverified' }
    if ($ExitCode -eq 0) { return 'exited-zero-unverified' }
    return 'exited-error'
}
function Format-OpenKeyProcessExitCode {
    param([int]$ExitCode)
    $unsigned = [BitConverter]::ToUInt32([BitConverter]::GetBytes($ExitCode), 0)
    return ('0x{0:X8}' -f $unsigned)
}
