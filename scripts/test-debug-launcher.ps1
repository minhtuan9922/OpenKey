
$OpenKeyRepositoryRoot = Split-Path -Parent $PSScriptRoot
$ErrorActionPreference = 'Stop'
. (Join-Path $OpenKeyRepositoryRoot 'scripts\DebugUiLauncher.ps1')
$passed = 0
function Test-Launcher([string]$Name, [scriptblock]$Action) {
    if (-not (& $Action)) { throw "Launcher assertion failed: $Name" }
    $script:passed++; Write-Output "PASS: $Name"
}
$x64 = Get-OpenKeyDebugUiExecutable 'D:\Fixture Root' x64
$x86 = Get-OpenKeyDebugUiExecutable 'D:\Fixture Root' x86
Test-Launcher 'x64 Debug UI path' { $x64 -eq 'D:\Fixture Root\Sources\OpenKey\win32\OpenKey\OpenKey.UI\x64\Debug\OpenKey.UI\OpenKey.UI.exe' }
Test-Launcher 'x86 Debug UI path' { $x86 -eq 'D:\Fixture Root\Sources\OpenKey\win32\OpenKey\OpenKey.UI\Debug\OpenKey.UI\OpenKey.UI.exe' }
Test-Launcher 'Relative root rejected' { try { Get-OpenKeyDebugUiExecutable 'relative' x64 | Out-Null; $false } catch { $true } }
Test-Launcher 'No process is not locked' { -not (Test-OpenKeyDebugUiInUse $x64 @()) }
Test-Launcher 'Matching Debug process blocks rebuild' { Test-OpenKeyDebugUiInUse $x64 @(@{Path=$x64}) }
Test-Launcher 'Case-insensitive Windows path match' { Test-OpenKeyDebugUiInUse $x64 @(@{Path=$x64.ToLowerInvariant()}) }
Test-Launcher 'Other architecture is not this output' { -not (Test-OpenKeyDebugUiInUse $x64 @(@{Path=$x86})) }
Test-Launcher 'Missing process path not guessed' { -not (Test-OpenKeyDebugUiInUse $x64 @(@{Path=$null})) }
Test-Launcher 'Alive is not GUI acceptance' { (Get-OpenKeyDebugUiLaunchState $false) -eq 'running-unverified' }
Test-Launcher 'Zero exit is not confirmed redirection' { (Get-OpenKeyDebugUiLaunchState $true 0) -eq 'exited-zero-unverified' }
Test-Launcher 'Crash exit is an error' { (Get-OpenKeyDebugUiLaunchState $true -1073741189) -eq 'exited-error' }
Test-Launcher 'Signed crash exit formats as hexadecimal' { (Format-OpenKeyProcessExitCode -1073741189) -eq '0xC000027B' }
Test-Launcher 'Success exit formats as hexadecimal' { (Format-OpenKeyProcessExitCode 0) -eq '0x00000000' }
Test-Launcher 'Launcher/helpers parse' {
    foreach ($name in @('run-ui-debug.ps1','DebugUiLauncher.ps1','test-debug-launcher.ps1')) {
        $tokens = $null; $parseErrors = $null
        $null = [Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot $name), [ref]$tokens, [ref]$parseErrors)
        if ($parseErrors.Count -ne 0) { return $false }
    }
    return $true
}
Test-Launcher 'Production launcher stops at blocked preflight' {
    function Get-Process {
        [CmdletBinding()] param([string]$Name, [int]$Id)
        if ($Name -ne 'OpenKey.UI') { throw 'Unexpected process probe: launcher passed preflight.' }
        [pscustomobject]@{ Path = (Get-OpenKeyDebugUiExecutable $OpenKeyRepositoryRoot x64) }
    }
    try {
        & (Join-Path $OpenKeyRepositoryRoot 'scripts\run-ui-debug.ps1') x64
        return $false
    } catch {
        return $_.Exception.Message.StartsWith('UI Debug dang chay hoac bi debugger giu.')
    }
}
Write-Output "Debug UI launcher: $passed/15 passed; pure helpers/parser + blocked preflight with fake process; no build/process launch/registry changes."
