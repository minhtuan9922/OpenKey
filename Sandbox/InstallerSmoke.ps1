[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
# This runner is disposable-guest-only. Never install or change host settings.
if ($env:USERNAME -ne 'WDAGUtilityAccount' -or
    -not (Test-Path 'C:\OpenKeySandboxInput\installer-session.json') -or
    -not (Test-Path 'C:\OpenKeySandboxLogs')) { throw 'Windows Sandbox mappings/account required.' }
$inputRoot = 'C:\OpenKeySandboxInput'
$logsRoot = 'C:\OpenKeySandboxLogs'
$report = [ordered]@{ startedUtc=[DateTime]::UtcNow.ToString('o'); status='running'; checks=@(); manualPending=@('Interactive UAC allow/decline', 'Overview enable/disable and editor typing'); error=$null }
function Save-Report { $report | ConvertTo-Json -Depth 8 | Set-Content "$logsRoot\installer-result.json" -Encoding UTF8 }
function Check([string]$Name, [bool]$Passed) {
    $report.checks += @{ name=$Name; passed=$Passed }; Save-Report
    if (-not $Passed) { throw "Failed: $Name" }
}
function Run([string]$File, [string[]]$Arguments) {
    $process = Start-Process -FilePath $File -ArgumentList $Arguments -PassThru -WindowStyle Hidden
    if (-not $process.WaitForExit(600000)) { throw "Timed out: $File" }
    return $process.ExitCode
}
Save-Report
try {
    $session = Get-Content "$inputRoot\installer-session.json" -Raw | ConvertFrom-Json
    $report.os = [Environment]::OSVersion.Version.ToString()
    $report.user = $env:USERNAME
    $report.admin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
    Check 'Setup digest' ((Get-FileHash "$inputRoot\OpenKey-Setup.exe").Hash -eq $session.setupSha256)
    Check 'Runtime digest' ((Get-FileHash "$inputRoot\vc_redist.x64.exe").Hash -eq $session.runtimeSha256)
    foreach ($pendingRuntime in @(Get-Process 'vc_redist.x64' -ErrorAction SilentlyContinue)) {
        if (-not $pendingRuntime.WaitForExit(600000)) { throw 'Previous guest runtime install is still running.' }
    }
    $runtimeExit = Run "$inputRoot\vc_redist.x64.exe" @('/install','/quiet','/norestart',"/log `"$logsRoot\runtime.log`"")
    $report.runtimeExit = $runtimeExit
    Check 'Offline runtime installed' ($runtimeExit -in @(0,1638,3010))
    $setup = "$inputRoot\OpenKey-Setup.exe"
    $install = Join-Path $env:LOCALAPPDATA 'Programs\OpenKey-Fluent'
    $report.installDirectory = $install
    $exitCode = Run $setup @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/TASKS="desktopicon,caretediting"',"/LOG=`"$logsRoot\install.log`"")
    $report.installExit = $exitCode
    Check 'Per-user silent install' ($exitCode -eq 0)
    foreach ($file in @('OpenKey64.exe','OpenKey.UI\OpenKey.UI.exe','CaretEditing\OpenKey.CaretSetup.exe','CaretEditing\x64\OpenKey.TextService.dll','CaretEditing\x86\OpenKey.TextService.dll')) {
        Check "Installed $file" (Test-Path -LiteralPath (Join-Path $install $file) -PathType Leaf)
    }
    Check 'Start Menu shortcut' (@(Get-ChildItem "$env:APPDATA\Microsoft\Windows\Start Menu\Programs\OpenKey Fluent" -Filter '*.lnk' -ErrorAction SilentlyContinue).Count -gt 0)
    Check 'Desktop shortcut' (@(Get-ChildItem ([Environment]::GetFolderPath('Desktop')) -Filter '*OpenKey*.lnk').Count -gt 0)
    $helper = "$install\CaretEditing\OpenKey.CaretSetup.exe"
    Check 'Silent install skips optional machine registration' ((Run $helper @('--status')) -eq 1168)
    if ($report.admin) {
        Check 'Caret registration both architectures' ((Run $helper @('--install')) -eq 0)
        Check 'Caret registered payload status' ((Run $helper @('--status')) -eq 0)
        Check 'Caret deployment ownership' ((Run $helper @('--owns')) -eq 0)
        foreach ($view in @([Microsoft.Win32.RegistryView]::Registry32,[Microsoft.Win32.RegistryView]::Registry64)) {
            $base = [Microsoft.Win32.RegistryKey]::OpenBaseKey([Microsoft.Win32.RegistryHive]::LocalMachine,$view)
            $key = $base.OpenSubKey('SOFTWARE\Classes\CLSID\{727F9E3B-0491-4D5C-B33D-64D59BE28A70}\InprocServer32')
            try { Check "TSF COM registration $view" ($key -and (Test-Path -LiteralPath ([string]$key.GetValue('')))) }
            finally { if ($key) { $key.Dispose() }; $base.Dispose() }
        }
    } else { $report.manualPending += 'Elevated Caret install/uninstall: guest token is not elevated' }
    $settings = 'HKCU:\Software\TuyenMai\OpenKey'
    New-Item -Path $settings -Force | Out-Null
    New-ItemProperty -Path $settings -Name 'SandboxPreservationProbe' -Value 'keep-on-upgrade-and-uninstall' -PropertyType String -Force | Out-Null
    Check 'Reinstall succeeds' ((Run $setup @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART',"/LOG=`"$logsRoot\reinstall.log`"")) -eq 0)
    Check 'Reinstall preserves settings' ((Get-ItemPropertyValue $settings 'SandboxPreservationProbe') -eq 'keep-on-upgrade-and-uninstall')
    $ui = Start-Process "$install\OpenKey.UI\OpenKey.UI.exe" -WorkingDirectory "$install\OpenKey.UI" -WindowStyle Hidden -PassThru
    Start-Sleep -Seconds 5
    $ui.Refresh()
    $report.uiProcessId = $ui.Id
    Check 'UI survives startup for five seconds' (-not $ui.HasExited)
    # Only our newly launched, empty guest UI. No user document or host process.
    if (-not $ui.HasExited) {
        $null = $ui.CloseMainWindow()
        if (-not $ui.WaitForExit(5000)) { Stop-Process -Id $ui.Id -Force }
    }
    if ($report.admin) {
        Check 'Caret unregister succeeds' ((Run $helper @('--uninstall')) -eq 0)
        Check 'Caret registration removed' ((Run $helper @('--status')) -eq 1168)
    }
    Check 'Uninstall succeeds' ((Run "$install\unins000.exe" @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART',"/LOG=`"$logsRoot\uninstall.log`"")) -eq 0)
    Check 'Application removed' (-not (Test-Path "$install\OpenKey.UI\OpenKey.UI.exe"))
    Check 'Uninstall preserves settings' ((Get-ItemPropertyValue $settings 'SandboxPreservationProbe') -eq 'keep-on-upgrade-and-uninstall')
    $report.status = 'passed-automated-smoke'
} catch { $report.status='failed'; $report.error=$_.Exception.ToString() }
finally { $report.completedUtc=[DateTime]::UtcNow.ToString('o'); Save-Report }
