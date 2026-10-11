[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
function Test-OpenKeySandboxAccount([string]$UserName) {
    return $UserName -eq 'WDAGUtilityAccount'
}
function Get-OpenKeySmokeStatus([bool]$RuntimeSatisfied, [bool]$ProcessExited) {
    if (-not $RuntimeSatisfied) { return 'prerequisite-missing' }
    if ($ProcessExited) { return 'ui-exited-early' }
    return 'ui-running-smoke-only'
}
# Unit tests import helpers only; importing never probes registry or starts apps.
if ($MyInvocation.InvocationName -eq '.') { return }
if (-not (Test-OpenKeySandboxAccount $env:USERNAME)) {
    throw 'Refusing to run outside the default Windows Sandbox account. No app was started.'
}
$inputRoot = 'C:\OpenKeySandboxInput'
$logsRoot = 'C:\OpenKeySandboxLogs'
if (-not (Test-Path -LiteralPath $inputRoot -PathType Container) -or
    -not (Test-Path -LiteralPath $logsRoot -PathType Container)) { throw 'Sandbox mappings are missing.' }
$report = [ordered]@{
    startedUtc = [DateTime]::UtcNow.ToString('o'); status = 'initializing'
    osVersion = [Environment]::OSVersion.Version.ToString(); is64BitOS = [Environment]::Is64BitOperatingSystem
    shellVersion = $PSVersionTable.PSVersion.ToString(); manualAcceptancePending = $true
    engineStarted = $false; runtimeInstalledByScript = $false; workDirectory = $null
}
$reportName = 'smoke-' + [guid]::NewGuid().ToString('N') + '.json'
try {
    $session = Get-Content -LiteralPath (Join-Path $inputRoot 'session.json') -Raw -Encoding UTF8 | ConvertFrom-Json
    $null = & (Join-Path $inputRoot 'verify-beta.ps1') -ZipPath (Join-Path $inputRoot 'beta.zip') -ExpectedArchitecture $session.architecture -ExpectedSha256 $session.sha256
    $workRoot = Join-Path $env:LOCALAPPDATA ('OpenKeySandbox-' + [guid]::NewGuid().ToString('N'))
    $report.workDirectory = $workRoot
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    [IO.Compression.ZipFile]::ExtractToDirectory((Join-Path $inputRoot 'beta.zip'), $workRoot)
    $runtime = & (Join-Path $workRoot 'check-prerequisites.ps1') -ReportOnly
    $report.runtime = $runtime
    $report.packageArchitecture = $session.architecture
    if ($runtime.VcRuntimeSatisfied) {
        # Interactive guest UI for manual QA, not a background helper on the host.
        $ui = Start-Process -FilePath (Join-Path $workRoot 'OpenKey.UI\OpenKey.UI.exe') -WorkingDirectory (Join-Path $workRoot 'OpenKey.UI') -PassThru -WindowStyle Normal
        Start-Sleep -Seconds 3
        $ui.Refresh()
        $report.uiProcessId = $ui.Id
        $report.uiProbeAfterSeconds = 3
        $report.status = Get-OpenKeySmokeStatus $true $ui.HasExited
        if ($ui.HasExited) { $report.uiExitCode = $ui.ExitCode }
    } else {
        $report.status = Get-OpenKeySmokeStatus $false $false
    }
} catch {
    $report.status = 'error'
    $report.error = $_.Exception.Message
} finally {
    $report.completedUtc = [DateTime]::UtcNow.ToString('o')
    $report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $logsRoot $reportName) -Encoding UTF8
}
Write-Output ($report | ConvertTo-Json -Depth 8)
if ($report.status -ne 'ui-running-smoke-only') { exit 1 }
