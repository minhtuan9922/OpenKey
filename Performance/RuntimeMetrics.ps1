function Get-OpenKeyRuntimeSummary {
    param(
        [Parameter(Mandatory)] [object[]]$Samples,
        [ValidateRange(1, 65536)] [int]$LogicalProcessors = 1
    )
    if ($Samples.Count -lt 2) { throw 'At least two runtime samples are required.' }
    $previous = $null
    foreach ($sample in $Samples) {
        foreach ($field in @('elapsedSeconds', 'cpuSeconds', 'workingSetBytes', 'privateBytes')) {
            if ($null -eq $sample.$field) { throw "Missing runtime metric: $field" }
            $value = [double]$sample.$field
            if ([double]::IsNaN($value) -or [double]::IsInfinity($value) -or $value -lt 0) {
                throw "Invalid runtime metric: $field"
            }
        }
        if ($null -ne $previous -and ([double]$sample.elapsedSeconds -le [double]$previous.elapsedSeconds -or
            [double]$sample.cpuSeconds -lt [double]$previous.cpuSeconds)) { throw 'Runtime counters must be monotonic.' }
        $previous = $sample
    }
    $duration = [double]$Samples[-1].elapsedSeconds - [double]$Samples[0].elapsedSeconds
    $cpu = [double]$Samples[-1].cpuSeconds - [double]$Samples[0].cpuSeconds
    # Windows PowerShell 5.1 cannot Measure-Object properties on dictionaries.
    $workingSet = $Samples | ForEach-Object { [double]$_.workingSetBytes } | Measure-Object -Average -Maximum
    $private = $Samples | ForEach-Object { [double]$_.privateBytes } | Measure-Object -Average -Maximum
    [ordered]@{
        sampleCount = $Samples.Count
        elapsedSeconds = [Math]::Round($duration, 6)
        cpuSeconds = [Math]::Round($cpu, 6)
        cpuPercentOfOneCore = [Math]::Round(100 * $cpu / $duration, 4)
        cpuPercentOfMachine = [Math]::Round(100 * $cpu / $duration / $LogicalProcessors, 4)
        meanWorkingSetMiB = [Math]::Round($workingSet.Average / 1MB, 3)
        peakWorkingSetMiB = [Math]::Round($workingSet.Maximum / 1MB, 3)
        meanPrivateMiB = [Math]::Round($private.Average / 1MB, 3)
        peakPrivateMiB = [Math]::Round($private.Maximum / 1MB, 3)
    }
}
