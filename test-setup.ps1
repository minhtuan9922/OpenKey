[CmdletBinding()]
param([string]$IsccPath = '')
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Installer\Compiler.ps1')
$IsccPath = Resolve-OpenKeySetupCompiler $IsccPath
$taskSource = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'Installer\OpenKey.iss') -Raw
$taskChecks = 0
function Assert-Setup([bool]$Condition, [string]$Name) {
    if (-not $Condition) { throw "Setup contract failed: $Name" }
    $script:taskChecks++
}
Assert-Setup ($taskSource -match 'ArchitecturesAllowed=x86os x64os') 'explicit supported OS architectures'
Assert-Setup ($taskSource -match 'MinVersion=10.0.17763') 'minimum Windows version'
Assert-Setup ($taskSource -match 'PrivilegesRequired=lowest') 'current-user installation'
Assert-Setup ($taskSource -match 'DefaultDirName=\{localappdata\}') 'user-owned destination'
Assert-Setup ($taskSource -match 'DiskSpanning=no') 'one offline executable'
Assert-Setup ($taskSource -match 'PayloadX64.*Check: UseX64') 'x64 payload condition'
Assert-Setup ($taskSource -match 'PayloadX86.*Check: UseX86') 'x86 payload condition'
Assert-Setup (@([regex]::Matches($taskSource, 'Flags: dontcopy')).Count -eq 2) 'two temporary-only runtimes'
Assert-Setup ($taskSource -match 'userprograms.*code:EngineName') 'Start Menu starts engine'
Assert-Setup ($taskSource -match 'userdesktop.*Tasks: desktopicon') 'optional desktop engine shortcut'
Assert-Setup ($taskSource -match 'Name: "desktopicon".*Flags: unchecked') 'desktop unchecked by default'
Assert-Setup ($taskSource -match 'postinstall nowait skipifsilent unchecked runasoriginaluser') 'opt-in launch as user'
Assert-Setup ($taskSource -match 'CloseApplications=no' -and $taskSource -match 'RestartApplications=no') 'no forced process shutdown'
Assert-Setup ($taskSource -match 'if WizardSilent then Exit;.*' -and $taskSource -match 'MB_YESNO or MB_DEFBUTTON2') 'interactive app close consent defaults to No'
Assert-Setup ($taskSource -match 'PostMessage\(CloseWindows\[Index\], \$0400 \+ 2021' -and $taskSource -match 'PostMessage\(CloseWindows\[Index\], \$0010') 'graceful engine IPC and UI close'
Assert-Setup ($taskSource -match 'Remaining := 5000 - \(AppTicks - Started\)' -and $taskSource -match 'AppWait\(CloseHandles\[Index\], Remaining\)' -and $taskSource -match 'AppCloseHandle\(CloseHandles\[Index\]\)') 'bounded process exit wait and handle cleanup'
Assert-Setup ($taskSource -notmatch 'TerminateProcess|taskkill|Stop-Process') 'never force terminate'
Assert-Setup ($taskSource -match 'CurrentProcessID <> CloseProcessIDs\[Index\]') 'revalidate HWND process after user confirmation'
Assert-Setup ($taskSource.IndexOf('if not CloseInstalledApps then') -lt $taskSource.IndexOf('if RuntimeSatisfied then Exit;')) 'close before runtime/file installation'
Assert-Setup ($taskSource -match 'RuntimePage.Values\[0\] := False') 'explicit runtime consent'
Assert-Setup ($taskSource -match 'if WizardSilent or not RuntimePage.Values\[0\]') 'silent missing-runtime fails closed'
Assert-Setup ($taskSource -match 'function PrepareToInstall') 'runtime installed before app files'
Assert-Setup ($taskSource.IndexOf('GetSHA256OfFile') -lt $taskSource.IndexOf("ShellExec('runas'")) 'verify runtime hash before elevation'
Assert-Setup ($taskSource -match '/install /passive /norestart') 'no automatic runtime reboot'
Assert-Setup ($taskSource -match 'RuntimeInView\(HKLM32\)' -and $taskSource -match 'RuntimeInView\(HKLM64\)') 'both registry views queried'
Assert-Setup ($taskSource -match 'RuntimeOutcome\(ExitCode, RuntimeSatisfied\)') 'verify version after runtime process'
Assert-Setup ($taskSource -match 'InitializeUninstall' -and $taskSource -match 'not EngineRunning') 'uninstall running-app guard'
Assert-Setup ($taskSource -notmatch '(?im)^\[Registry\]|^\[InstallDelete\]|^\[UninstallDelete\]|RegDelete|RegWrite|DelTree') 'preserve settings and no broad deletes'
Assert-Setup ($taskSource -match 'Name: "vietnamese"' -and $taskSource -match 'Name: "english"') 'Vietnamese and English'
$taskViKeys = @([regex]::Matches($taskSource, '(?m)^vietnamese\.([A-Za-z]+)=') | ForEach-Object { $_.Groups[1].Value })
$taskEnKeys = @([regex]::Matches($taskSource, '(?m)^english\.([A-Za-z]+)=') | ForEach-Object { $_.Groups[1].Value })
Assert-Setup (-not @(Compare-Object $taskViKeys $taskEnKeys).Count) 'localized custom message parity'
Write-Output "$taskChecks setup source contract checks passed. Not installer GUI acceptance."
$taskBuildCommand = Get-Command (Join-Path $PSScriptRoot 'build-setup.ps1')
Assert-Setup (@($taskBuildCommand.ParameterSets | Where-Object Name -eq 'ReleaseBuild').Count -eq 1) 'Release build parameter set'
$taskReleaseSet = $taskBuildCommand.ParameterSets | Where-Object Name -eq 'ReleaseBuild'
Assert-Setup (@($taskReleaseSet.Parameters | Where-Object { $_.Name -eq 'Release' -and $_.IsMandatory }).Count -eq 1) 'explicit Release switch'
Assert-Setup (-not @($taskReleaseSet.Parameters | Where-Object Name -in @('ZipX64','ZipX86')).Count) 'Release does not require existing ZIPs'
$taskBuildSource = Get-Content -LiteralPath $taskBuildCommand.Source -Raw
Assert-Setup ($taskBuildSource -match 'package-beta.ps1.*-PassThru' -and $taskBuildSource -match 'ZipPath') 'fresh package paths returned directly'
Assert-Setup ($taskBuildSource.IndexOf('verify-setup.ps1') -lt $taskBuildSource.IndexOf('SETUP FILE:')) 'verify before quick access output'
$taskDefaults = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'Installer\OfflineInputs.json') -Raw | ConvertFrom-Json
foreach ($taskArch in @('x64','x86')) {
    Assert-Setup ($taskDefaults.$taskArch.sha256 -match '^[A-F0-9]{64}$' -and $taskDefaults.$taskArch.path -eq "dist/setup-inputs/vc_redist.$taskArch.exe") "pinned $taskArch offline input"
}
$taskPackageSource = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'package-beta.ps1') -Raw
Assert-Setup ($taskPackageSource.Contains('$engine = Join-Path $projectRoot "$prefix\$engineName"') -and $taskPackageSource.Contains('$updater = Join-Path $projectRoot "$prefix\OpenKeyUpdate.exe"')) 'solution Release output paths'
Assert-Setup ($taskPackageSource.IndexOf('Stale/mismatched Release binary:') -lt $taskPackageSource.IndexOf('Copy-Item -LiteralPath $engine') -and $taskPackageSource.Contains('$binaryVersion.FileBuildPart')) 'reject stale binary version before staging'
Write-Output '9 build shortcut/packaging contract checks passed.'
if (-not (Test-Path -LiteralPath $IsccPath -PathType Leaf)) { throw 'Inno compiler required for native Pascal policy fixture.' }
$taskProbeRoot = Join-Path $PSScriptRoot ('dist\setup-policy-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $taskProbeRoot | Out-Null
# Compile the actual installer [Code], without payloads or executing its callbacks.
# This catches API/type mistakes not covered by pure policy fixtures.
$taskCode = ($taskSource -split '(?m)^\[Code\]\s*', 2)[1]
$taskPolicyPath = Join-Path $PSScriptRoot 'Installer\RuntimePolicy.iss'
$taskCode = $taskCode.Replace('#include "RuntimePolicy.iss"', ('#include "' + $taskPolicyPath + '"'))
$taskCompileSource = @"
[Setup]
AppId=OpenKeyAppCloseCompileFixtureOnly
AppName=OpenKey app close compile fixture
AppVersion=1
PrivilegesRequired=lowest
CreateAppDir=no
Uninstallable=no
OutputDir=$taskProbeRoot
OutputBaseFilename=AppCloseCompileProbe
[Code]
$taskCode
function InitializeSetup: Boolean;
begin
  Result := False;
end;
"@
$taskCompileFile = Join-Path $taskProbeRoot 'AppCloseCompileProbe.iss'
$taskCompileSource | Set-Content -LiteralPath $taskCompileFile -Encoding UTF8
& $IsccPath /Q /DMinimumRuntime=14.51.36231.0 /DRuntimeHashX64=fixture /DRuntimeHashX86=fixture $taskCompileFile
if ($LASTEXITCODE -ne 0) { throw 'Actual installer app-close code compilation failed.' }
Write-Output 'Actual installer code compiled; app-close callbacks were not executed.'
& $IsccPath /Q "/DProbeOutput=$taskProbeRoot" (Join-Path $PSScriptRoot 'Tests\InstallerPolicyTests.iss')
if ($LASTEXITCODE -ne 0) { throw 'Native policy fixture compilation failed.' }
$taskReport = Join-Path $taskProbeRoot 'report.txt'
$taskProbe = Start-Process -FilePath (Join-Path $taskProbeRoot 'PolicyProbe.exe') -WindowStyle Hidden -PassThru -Wait -ArgumentList @(
    '/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART',('/Report="' + $taskReport + '"'),('/LOG="' + (Join-Path $taskProbeRoot 'probe.log') + '"'))
# InitializeSetup deliberately returns false: exit 1, before any installation.
if ($taskProbe.ExitCode -ne 1 -or -not (Test-Path -LiteralPath $taskReport)) { throw "Native policy fixture failed. See $taskProbeRoot" }
Get-Content -LiteralPath $taskReport
