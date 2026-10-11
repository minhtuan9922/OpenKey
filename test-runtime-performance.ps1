$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Performance\RuntimeMetrics.ps1')
$passed = 0
function Assert-Metric([bool]$Condition, [string]$Name) {
    if (-not $Condition) { throw "FAIL: $Name" }; $script:passed++
}
function Assert-Rejected([scriptblock]$Action, [string]$Name) {
    $rejected = $false
    try { & $Action | Out-Null } catch { $rejected = $true }
    Assert-Metric $rejected $Name
}
$samples = @(
    @{elapsedSeconds=20; cpuSeconds=8; workingSetBytes=2MB; privateBytes=1MB},
    @{elapsedSeconds=22; cpuSeconds=8.5; workingSetBytes=4MB; privateBytes=3MB}
)
$metrics = Get-OpenKeyRuntimeSummary $samples 4
Assert-Metric ($metrics.sampleCount -eq 2 -and $metrics.elapsedSeconds -eq 2) 'actual duration, not process uptime'
Assert-Metric ($metrics.cpuSeconds -eq 0.5) 'CPU delta excludes earlier activity'
Assert-Metric ($metrics.cpuPercentOfOneCore -eq 25 -and $metrics.cpuPercentOfMachine -eq 6.25) 'CPU normalization'
Assert-Metric ($metrics.meanWorkingSetMiB -eq 3 -and $metrics.peakWorkingSetMiB -eq 4) 'working-set mean and peak'
Assert-Metric ($metrics.meanPrivateMiB -eq 2 -and $metrics.peakPrivateMiB -eq 3) 'private-byte mean and peak'
$orderedSamples = @($samples | ForEach-Object { [ordered]@{
    elapsedSeconds=$_.elapsedSeconds; cpuSeconds=$_.cpuSeconds
    workingSetBytes=$_.workingSetBytes; privateBytes=$_.privateBytes
} })
Assert-Metric ((Get-OpenKeyRuntimeSummary $orderedSamples 4).meanWorkingSetMiB -eq 3) 'live sampler ordered dictionaries'
$jsonSamples = $orderedSamples | ConvertTo-Json | ConvertFrom-Json
Assert-Metric ((Get-OpenKeyRuntimeSummary $jsonSamples 4).peakPrivateMiB -eq 3) 'JSON report objects'
$idle = @(
    @{elapsedSeconds=0; cpuSeconds=0; workingSetBytes=0; privateBytes=0},
    @{elapsedSeconds=3; cpuSeconds=0; workingSetBytes=0; privateBytes=0}
)
Assert-Metric ((Get-OpenKeyRuntimeSummary $idle).cpuPercentOfOneCore -eq 0) 'zero CPU is valid'
Assert-Rejected { Get-OpenKeyRuntimeSummary @($samples[0]) } 'one sample'
foreach ($field in @('elapsedSeconds', 'cpuSeconds', 'workingSetBytes', 'privateBytes')) {
    foreach ($bad in @(-1, [double]::NaN, [double]::PositiveInfinity, $null)) {
        $invalid = @($samples[0].Clone(), $samples[1].Clone()); $invalid[1][$field] = $bad
        Assert-Rejected { Get-OpenKeyRuntimeSummary $invalid } "invalid $field"
    }
}
$invalid = @($samples[0].Clone(), $samples[1].Clone()); $invalid[1].elapsedSeconds = 20
Assert-Rejected { Get-OpenKeyRuntimeSummary $invalid } 'equal timestamps'
$invalid[1].elapsedSeconds = 19
Assert-Rejected { Get-OpenKeyRuntimeSummary $invalid } 'backward time'
$invalid[1] = $samples[1].Clone(); $invalid[1].cpuSeconds = 7
Assert-Rejected { Get-OpenKeyRuntimeSummary $invalid } 'backward CPU counter'
Assert-Rejected { Get-OpenKeyRuntimeSummary $samples 0 } 'invalid processor count'
$source = Get-Content (Join-Path $PSScriptRoot 'measure-runtime.ps1') -Raw
Assert-Metric ($source -notmatch '(?im)^\s*(Start-Process|Stop-Process|taskkill|SendInput|SendMessage|Set-ItemProperty)\b') 'no app mutations in sampler'
Assert-Metric ($source -match 'Stopwatch' -and $source -match 'finally \{ \$observed.Dispose\(\) \}') 'monotonic clock and process handle cleanup'
Write-Output "$passed runtime performance checks passed; pure fixtures only, no app observation or launch."
