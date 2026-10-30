[CmdletBinding()]
param(
    [ValidateSet('x64', 'x86')]
    [string]$Platform = 'x64'
)
$OpenKeyRepositoryRoot = Split-Path -Parent $PSScriptRoot


$ErrorActionPreference = 'Stop'
. (Join-Path $OpenKeyRepositoryRoot 'scripts\DebugUiLauncher.ps1')
$executable = Get-OpenKeyDebugUiExecutable $OpenKeyRepositoryRoot $Platform
$outputDirectory = Split-Path -Parent $executable
$running = @(Get-Process -Name 'OpenKey.UI' -ErrorAction SilentlyContinue)
if (Test-OpenKeyDebugUiInUse $executable $running) {
    throw 'UI Debug dang chay hoac bi debugger giu. Bam Shift+F5 trong Visual Studio/dong UI roi thu lai. Script khong tu dung process.'
}
$sessionRoot = Join-Path $OpenKeyRepositoryRoot ('dist\ui-debug-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $sessionRoot | Out-Null
$buildLog = Join-Path $sessionRoot 'build.log'
$reportPath = Join-Path $sessionRoot 'launch.json'
$report = [ordered]@{
    startedUtc = [DateTime]::UtcNow.ToString('o'); executable = $executable
    platform = $Platform; buildLog = $buildLog; status = 'building'
    debuggerAttachedByScript = $false; manualAcceptancePending = $true
}
Write-Host "Build log: $buildLog"
$uiProcess = $null
try {
    # Isolate build.ps1's exit from the launcher so failure still produces a report.
    $launcherShell = (Get-Process -Id $PID).Path
    & $launcherShell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $OpenKeyRepositoryRoot 'build.ps1') Debug $Platform UI *> $buildLog
    $report.buildExitCode = $LASTEXITCODE
    if ($LASTEXITCODE -ne 0) {
        $report.status = 'build-failed'
        throw "Build failed. See $buildLog. Neu LNK1104: dung debugger/dong UI, khong chay binary cu."
    }
    if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
        $report.status = 'executable-missing'
        throw "Debug executable missing: $executable"
    }
    Write-Host "Running $executable"
    $report.status = 'launching'
    $uiProcess = Start-Process -FilePath $executable -WorkingDirectory $outputDirectory -PassThru -WindowStyle Normal
    # Retain a process handle before a fast exit (especially Windows PowerShell).
    $null = $uiProcess.Handle
    $report.processId = $uiProcess.Id
    Start-Sleep -Seconds 3
    $uiProcess.Refresh()
    $report.probeAfterSeconds = 3
    $exited = $uiProcess.HasExited
    if ($exited) {
        $uiProcess.WaitForExit()
        if ($null -eq $uiProcess.ExitCode) {
            $report.status = 'exited-code-unavailable'
            throw 'UI exited but its exit code could not be read. Window activation is not verified.'
        }
    }
    $processExitCode = if ($exited) { $uiProcess.ExitCode } else { 0 }
    $report.status = Get-OpenKeyDebugUiLaunchState $exited $processExitCode
    if ($exited) {
        $report.exitCode = $processExitCode
        $report.exitCodeHex = Format-OpenKeyProcessExitCode $processExitCode
        if ($report.status -eq 'exited-error') {
            throw "UI exited early: $($report.exitCodeHex). Debug OpenKey.UI with F5 and inspect Call Stack."
        }
        Write-Warning 'UI process exited with code 0. Single-instance redirection is possible, but a visible window is NOT verified.'
    } else {
        Write-Host "UI process $($uiProcess.Id) is running. A visible/working window is NOT verified; no debugger attached."
    }
} catch {
    $report.error = $_.Exception.Message
    if ($report.status -in @('building','launching')) { $report.status = 'error' }
    throw
} finally {
    $report.completedUtc = [DateTime]::UtcNow.ToString('o')
    $report | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $reportPath -Encoding UTF8
    Write-Host "Launch report: $reportPath"
    if ($uiProcess) { $uiProcess.Dispose() }
}
