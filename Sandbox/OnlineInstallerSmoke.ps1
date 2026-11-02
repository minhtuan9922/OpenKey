[CmdletBinding()]
param([switch]$Resume)
$ErrorActionPreference = 'Stop'
if ($env:USERNAME -ne 'WDAGUtilityAccount' -or -not (Test-Path 'C:\OpenKeySandboxInput\online-session.json') -or
    -not (Test-Path 'C:\OpenKeySandboxLogs')) { throw 'Disposable Windows Sandbox session required.' }
$inputRoot = 'C:\OpenKeySandboxInput'
$logsRoot = 'C:\OpenKeySandboxLogs'
$installRoot = Join-Path $env:LOCALAPPDATA 'Programs\OpenKey-Fluent'
$report = [ordered]@{ startedUtc=[DateTime]::UtcNow.ToString('o'); phase='starting'; checks=@(); error=$null }
function Save-Report { $report | ConvertTo-Json -Depth 8 | Set-Content "$logsRoot\online-result.json" -Encoding UTF8 }
function Check([string]$Name, [bool]$Passed) {
    $report.checks += @{name=$Name; passed=$Passed}; Save-Report
    if (-not $Passed) { throw "Failed: $Name" }
}
function Run([string]$Path, [string[]]$Arguments) {
    $process = Start-Process -FilePath $Path -ArgumentList $Arguments -WindowStyle Hidden -PassThru
    if (-not $process.WaitForExit(900000)) { throw "Guest process timed out: $Path" }
    return $process.ExitCode
}
if ($Resume) {
    $previous = Get-Content "$logsRoot\online-result.json" -Raw | ConvertFrom-Json
    foreach ($property in $previous.PSObject.Properties) { $report[$property.Name] = $property.Value }
    $report.harnessIssues = @($report.error)
    $report.error = $null
    $report.phase = 'ui-testing'
}
Save-Report
try {
    if (-not $Resume) {
    $session = Get-Content "$inputRoot\online-session.json" -Raw | ConvertFrom-Json
    $report.os = [Environment]::OSVersion.Version.ToString()
    $report.user = $env:USERNAME
    $report.admin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
    Check 'Exact online setup bytes' ((Get-FileHash "$inputRoot\OpenKey-Setup.exe").Hash -eq $session.setupSha256)
    $report.frameworkBefore = @(Get-AppxPackage -Name Microsoft.WindowsAppRuntime.2 | Select-Object Name,Version,Architecture)
    $report.phase = 'interactive-install'; Save-Report
    $setup = Start-Process -FilePath "$inputRoot\OpenKey-Setup.exe" -WindowStyle Normal -PassThru -ArgumentList @('/LANG=english',"/LOG=`"$logsRoot\online-install.log`"")
    if (-not $setup.WaitForExit(1200000)) { throw 'Interactive installer timed out.' }
    $report.setupExit = $setup.ExitCode
    Check 'Interactive online install exited successfully' ($setup.ExitCode -eq 0)
    foreach ($relative in @('OpenKey64.exe','OpenKey.UI\OpenKey.UI.exe','OpenKey.UI\Microsoft.WindowsAppRuntime.Bootstrap.dll')) {
        Check "Installed $relative" (Test-Path (Join-Path $installRoot $relative))
    }
    Check 'Large app-local XAML runtime absent' (-not (Test-Path "$installRoot\OpenKey.UI\Microsoft.UI.Xaml.dll"))
    $prerequisites = & "$installRoot\check-prerequisites.ps1" -ReportOnly
    $report.prerequisites = $prerequisites
    Check 'Both installed prerequisites ready' $prerequisites.AllPrerequisitesSatisfied
    $report.uiBytes = (Get-ChildItem "$installRoot\OpenKey.UI" -File -Recurse | Measure-Object Length -Sum).Sum
    $report.frameworkAfter = @(Get-AppxPackage -Name Microsoft.WindowsAppRuntime.2 | Select-Object Name,Version,Architecture)
    $report.phase='ready-for-ui'; Save-Report
    $ui = Start-Process -FilePath "$installRoot\OpenKey.UI\OpenKey.UI.exe" -WorkingDirectory "$installRoot\OpenKey.UI" -WindowStyle Normal -PassThru
    Start-Sleep -Seconds 5
    $ui.Refresh()
    Check 'UI alive after five seconds' (-not $ui.HasExited)
    $report.phase='ui-testing'; Save-Report
    }
    $deadline = [DateTime]::UtcNow.AddMinutes(30)
    while ([DateTime]::UtcNow -lt $deadline) {
        $commandPath = "$logsRoot\command.json"
        if (Test-Path $commandPath) {
            $command = Get-Content $commandPath -Raw | ConvertFrom-Json
            if ($command.id -ne $report.lastCommand) {
                $report.lastCommand=$command.id
                switch ($command.action) {
                    'editor' {
                        [IO.File]::WriteAllText("$logsRoot\typing.txt", '', [Text.Encoding]::Unicode)
                        Start-Process "$inputRoot\TypingEditor.exe" -WindowStyle Normal | Out-Null
                    }
                    'engine' { Start-Process "$installRoot\OpenKey64.exe" -WorkingDirectory $installRoot -WindowStyle Hidden | Out-Null }
                    'snapshot' { $report.processes=@(Get-Process 'OpenKey*','TypingEditor' -ErrorAction SilentlyContinue | Select-Object Id,ProcessName,MainWindowTitle) }
                    'reinstall' {
                        $key='HKCU:\Software\TuyenMai\OpenKey'
                        New-Item -Path $key -Force | Out-Null
                        New-ItemProperty -Path $key -Name 'SandboxOnlineProbe' -Value 'keep' -PropertyType String -Force | Out-Null
                        Check 'Reinstall with runtimes present' ((Run "$inputRoot\OpenKey-Setup.exe" @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART',"/LOG=`"$logsRoot\reinstall.log`"")) -eq 0)
                        Check 'Reinstall preserves settings' ((Get-ItemPropertyValue $key SandboxOnlineProbe) -eq 'keep')
                    }
                    'uninstall' {
                        Check 'Uninstall succeeds' ((Run "$installRoot\unins000.exe" @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART',"/LOG=`"$logsRoot\uninstall.log`"")) -eq 0)
                        Check 'Application removed' (-not (Test-Path "$installRoot\OpenKey.UI\OpenKey.UI.exe"))
                        Check 'Uninstall preserves settings' ((Get-ItemPropertyValue 'HKCU:\Software\TuyenMai\OpenKey' SandboxOnlineProbe) -eq 'keep')
                        Check 'Shared runtime retained' (@(Get-AppxPackage -Name Microsoft.WindowsAppRuntime.2).Count -gt 0)
                        $report.phase='finished'; Save-Report; return
                    }
                }
                Save-Report
            }
        }
        Start-Sleep -Milliseconds 500
    }
    throw 'Guest test command deadline exceeded.'
} catch { $report.phase='failed'; $report.error=$_.Exception.ToString() }
finally { Save-Report }
