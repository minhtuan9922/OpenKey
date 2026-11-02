[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
if ($env:USERNAME -ne 'WDAGUtilityAccount' -or -not (Test-Path 'C:\OpenKeySandboxInput\upgrade-session.json')) {
    throw 'Disposable Sandbox upgrade fixture required.'
}
$inputRoot='C:\OpenKeySandboxInput'
$logsRoot='C:\OpenKeySandboxLogs'
$installRoot=Join-Path $env:LOCALAPPDATA 'Programs\OpenKey-Fluent'
$result=[ordered]@{phase='running';checks=@();error=$null}
function Save { $result | ConvertTo-Json -Depth 5 | Set-Content "$logsRoot\upgrade-result.json" -Encoding UTF8 }
function Check([string]$Name,[bool]$Passed) {
    $result.checks+=@{name=$Name;passed=$Passed}; Save
    if (-not $Passed) {throw "Failed: $Name"}
}
function Install([string]$File,[string]$Log) {
    $process=Start-Process $File -WindowStyle Hidden -PassThru -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART',"/LOG=`"$logsRoot\$Log`"")
    if (-not $process.WaitForExit(600000)) {throw 'Upgrade fixture timed out.'}
    return $process.ExitCode
}
Save
try {
    $session=Get-Content "$inputRoot\upgrade-session.json" -Raw | ConvertFrom-Json
    Check 'Verified previous self-contained setup' ((Get-FileHash "$inputRoot\Previous-Setup.exe").Hash -eq $session.previousSha256)
    Check 'Verified new shared-runtime setup' ((Get-FileHash "$inputRoot\OpenKey-Setup.exe").Hash -eq $session.currentSha256)
    Check 'No engine or UI running before upgrade fixture' (@(Get-Process OpenKey64,OpenKey.UI -ErrorAction SilentlyContinue).Count -eq 0)
    Check 'Previous self-contained version installed' ((Install "$inputRoot\Previous-Setup.exe" 'previous-install.log') -eq 0)
    Check 'Previous XAML runtime present' (Test-Path "$installRoot\OpenKey.UI\Microsoft.ui.xaml.dll")
    $result.beforeUiBytes=(Get-ChildItem "$installRoot\OpenKey.UI" -File -Recurse | Measure-Object Length -Sum).Sum
    [IO.File]::WriteAllText("$installRoot\OpenKey.UI\SandboxUserNote.txt", 'preserve unrelated user file')
    Check 'Upgrade to shared runtime succeeded' ((Install "$inputRoot\OpenKey-Setup.exe" 'shared-upgrade.log') -eq 0)
    Check 'Obsolete XAML runtime removed' (-not (Test-Path "$installRoot\OpenKey.UI\Microsoft.ui.xaml.dll"))
    Check 'Unrelated user file preserved' ((Get-Content "$installRoot\OpenKey.UI\SandboxUserNote.txt" -Raw) -eq 'preserve unrelated user file')
    Check 'Saved settings preserved during upgrade' ((Get-ItemPropertyValue 'HKCU:\Software\TuyenMai\OpenKey' SandboxOnlineProbe) -eq 'keep')
    $result.afterUiBytes=(Get-ChildItem "$installRoot\OpenKey.UI" -File -Recurse | Measure-Object Length -Sum).Sum
    $ui=Start-Process "$installRoot\OpenKey.UI\OpenKey.UI.exe" -WorkingDirectory "$installRoot\OpenKey.UI" -WindowStyle Normal -PassThru
    Start-Sleep -Seconds 5; $ui.Refresh()
    Check 'Upgraded shared-runtime UI alive' (-not $ui.HasExited)
    $result.phase='passed'
} catch {$result.phase='failed';$result.error=$_.Exception.ToString()}
finally {Save}
