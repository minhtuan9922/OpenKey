[CmdletBinding()]
param(
    [Parameter(Mandatory)] [ValidateRange(1, 2147483647)] [int]$ProcessId,
    [ValidateRange(2, 60)] [int]$Seconds = 15,
    [ValidateSet('unspecified', 'tray-idle', 'ui-idle', 'typing')] [string]$Scenario = 'unspecified'
)
$OpenKeyRepositoryRoot = Split-Path -Parent $PSScriptRoot

$ErrorActionPreference = 'Stop'
. (Join-Path $OpenKeyRepositoryRoot 'Performance\RuntimeMetrics.ps1')
# Read-only sampling. Never launch/close an app, dispatch messages or send keys.
$observed = Get-Process -Id $ProcessId -ErrorAction Stop
try {
    if ($observed.ProcessName -notin @('OpenKey', 'OpenKey32', 'OpenKey64', 'OpenKey.UI')) {
        throw 'Select an OpenKey engine or Fluent UI process.'
    }
    $imagePath = $observed.MainModule.FileName
    $processStart = $observed.StartTime.ToUniversalTime()
    $startedUtc = [DateTime]::UtcNow
    $imageHash = (Get-FileHash -LiteralPath $imagePath -Algorithm SHA256).Hash
    $imageVersion = [Diagnostics.FileVersionInfo]::GetVersionInfo($imagePath).FileVersion
    $samples = [Collections.Generic.List[object]]::new()
    $timer = [Diagnostics.Stopwatch]::StartNew()
    while ($true) {
        $observed.Refresh()
        if ($observed.HasExited -or $observed.StartTime.ToUniversalTime() -ne $processStart) {
            throw 'Observed process exited or changed. No complete runtime measurement was recorded.'
        }
        $samples.Add([ordered]@{
            elapsedSeconds = $timer.Elapsed.TotalSeconds
            cpuSeconds = $observed.TotalProcessorTime.TotalSeconds
            workingSetBytes = $observed.WorkingSet64
            privateBytes = $observed.PrivateMemorySize64
        })
        if ($timer.Elapsed.TotalSeconds -ge $Seconds) { break }
        Start-Sleep -Milliseconds 1000
    }
    $timer.Stop()
    $summary = Get-OpenKeyRuntimeSummary -Samples $samples.ToArray() -LogicalProcessors ([Environment]::ProcessorCount)
    $reportDirectory = Join-Path $OpenKeyRepositoryRoot ('dist\performance-' + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $reportDirectory | Out-Null
    $reportPath = Join-Path $reportDirectory 'runtime.json'
    [ordered]@{
        schemaVersion = 1; startedUtc = $startedUtc.ToString('o'); scenario = $Scenario
        scope = 'Read-only process CPU and memory sampling. Not cold startup, per-key latency, a baseline comparison or acceptance.'
        scenarioNote = 'Scenario is supplied by the operator; the sampler cannot prove the app is idle or whether typing occurred.'
        processId = $ProcessId; processStartUtc = $processStart.ToString('o')
        executable = $imagePath; sha256 = $imageHash; fileVersion = $imageVersion
        os = [Environment]::OSVersion.Version.ToString(); logicalProcessors = [Environment]::ProcessorCount
        shellVersion = $PSVersionTable.PSVersion.ToString()
        summary = $summary; samples = $samples.ToArray()
    } | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $reportPath -Encoding UTF8
    $summary | ConvertTo-Json
    Write-Output "Runtime report: $reportPath"
} finally { $observed.Dispose() }
