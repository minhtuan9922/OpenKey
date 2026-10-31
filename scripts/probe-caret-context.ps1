[CmdletBinding()]
param([switch]$BuildOnly, [switch]$Trace, [switch]$Session, [ValidateRange(5,120)][int]$Seconds = 45)
$ErrorActionPreference = 'Stop'
if ($Trace -and $Session) { throw 'Choose Trace or Session, not both.' }
$OpenKeyRepositoryRoot = Split-Path -Parent $PSScriptRoot
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installation = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1
if (-not $installation) { throw 'MSVC tools not found.' }
$vcvars = Join-Path $installation 'VC\Auxiliary\Build\vcvarsall.bat'
$output = Join-Path $OpenKeyRepositoryRoot 'dist\caret-context-probe'
New-Item -ItemType Directory -Path $output -Force | Out-Null
$probeName = if ($Session) { 'CaretSessionProbe' } else { 'CaretContextProbe' }
$source = Join-Path $OpenKeyRepositoryRoot "Tests\$probeName.cpp"
$command = '"' + $vcvars + '" x64 >nul && cl /nologo /EHsc /std:c++17 /utf-8 /W4 /DUNICODE /D_UNICODE /Fe:' + $probeName + '.exe "' + $source + '" /link user32.lib ole32.lib oleaut32.lib uiautomationcore.lib uuid.lib'
Push-Location $output
try {
    & $env:ComSpec /d /c $command
    if ($LASTEXITCODE -ne 0) { throw 'UIA probe compilation failed.' }
    if (-not $BuildOnly) {
        if ($Session) {
            $log = Join-Path $output ("session-" + (Get-Date -Format 'yyyyMMdd-HHmmss') + '.log')
            & (Join-Path $output "$probeName.exe") $Seconds | Tee-Object -FilePath $log
            Write-Host "Session log: $log"
        } elseif ($Trace) {
            $log = Join-Path $output ("trace-" + (Get-Date -Format 'yyyyMMdd-HHmmss') + '.log')
            & (Join-Path $output 'CaretContextProbe.exe') --trace $Seconds | Tee-Object -FilePath $log
            Write-Host "Trace log: $log"
        } else { & (Join-Path $output 'CaretContextProbe.exe') }
        if ($LASTEXITCODE -ne 0) { throw 'UIA probe failed.' }
    }
} finally { Pop-Location }
