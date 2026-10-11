[CmdletBinding()]
param([ValidateSet('All', 'x64', 'x86')] [string]$Platform = 'All')
$ErrorActionPreference = 'Stop'
# Each suite runs in a separate PowerShell process: exit/fixture definitions in
# one suite cannot contaminate the next. Use the same shell version as caller.
$taskShell = (Get-Process -Id $PID).Path
$taskReportRoot = Join-Path $PSScriptRoot ('dist\regression-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $taskReportRoot -Force | Out-Null
$taskSuites = [System.Collections.Generic.List[object]]::new()
foreach ($taskName in @('ui-resources', 'packaging', 'prerequisites', 'third-party', 'sandbox', 'debug-launcher', 'registry-inventory', 'tray-contract', 'setup', 'ci-release')) {
    $taskSuites.Add(@{ Name = $taskName; Arguments = @() })
}
$taskArchitectures = if ($Platform -eq 'All') { @('x64','x86') } else { @($Platform) }
foreach ($taskArch in $taskArchitectures) {
    # test-convert already runs the engine correctness suite before conversion.
    foreach ($taskName in @('convert', 'settings', 'macro', 'updater')) {
        $taskSuites.Add(@{ Name = "$taskName-$taskArch"; Script = $taskName; Arguments = @($taskArch) })
    }
}
$taskResults = [System.Collections.Generic.List[object]]::new()
$taskFailed = $false
foreach ($taskSuite in $taskSuites) {
    $taskScriptName = if ($taskSuite.Script) { $taskSuite.Script } else { $taskSuite.Name }
    $taskScript = Join-Path $PSScriptRoot "test-$taskScriptName.ps1"
    $taskLog = Join-Path $taskReportRoot ($taskSuite.Name + '.log')
    $taskStarted = [DateTime]::UtcNow
    $taskArguments = @($taskSuite.Arguments)
    Write-Output "Running $($taskSuite.Name)..."
    # Windows PowerShell treats native stderr as an ErrorRecord. Keep it in
    # the suite log instead of aborting before the failure report is written.
    try {
        $ErrorActionPreference = 'Continue'
        & $taskShell -NoProfile -ExecutionPolicy Bypass -File $taskScript @taskArguments *> $taskLog
        $taskExitCode = $LASTEXITCODE
    } finally { $ErrorActionPreference = 'Stop' }
    $taskResults.Add([ordered]@{
        suite = $taskSuite.Name; exitCode = $taskExitCode; passed = ($taskExitCode -eq 0)
        log = $taskLog; seconds = [Math]::Round(([DateTime]::UtcNow - $taskStarted).TotalSeconds, 2)
    })
    if ($taskExitCode -ne 0) {
        $taskFailed = $true
        Write-Output "FAILED: $($taskSuite.Name). See $taskLog"
        Get-Content -LiteralPath $taskLog -Tail 12
        break
    }
    Write-Output "PASS: $($taskSuite.Name)"
}
$taskReport = [ordered]@{
    completedUtc = [DateTime]::UtcNow.ToString('o'); shellVersion = $PSVersionTable.PSVersion.ToString()
    requestedPlatform = $Platform; passed = (-not $taskFailed); plannedSuites = $taskSuites.Count
    results = @($taskResults.ToArray())
    scope = 'Automated fixtures/pure logic/source XML only. No native UI, keyboard hook or clean-machine acceptance.'
}
$taskReportPath = Join-Path $taskReportRoot 'summary.json'
$taskReport | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $taskReportPath -Encoding UTF8
Write-Output "Regression report: $taskReportPath"
if ($taskFailed) { exit 1 }
Write-Output "All $($taskResults.Count) suites passed. Manual acceptance remains pending."
exit 0
