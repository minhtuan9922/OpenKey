[CmdletBinding()]
param([ValidateSet('x64','x86')] [string]$Platform = 'x64', [switch]$Interactive, [switch]$NativeContext)
$ErrorActionPreference = 'Stop'
if ($Interactive -and $NativeContext) { throw 'Choose one native fixture mode.' }
$OpenKeyRepositoryRoot = Split-Path -Parent $PSScriptRoot
$componentRoot = Join-Path $OpenKeyRepositoryRoot "dist\text-service-tests-$Platform\component"
& (Join-Path $OpenKeyRepositoryRoot 'scripts\build-text-service.ps1') -Platform $Platform -OutputRoot $componentRoot
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installation = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1
if (-not $installation) { throw 'MSVC tools not found.' }
$vcvars = Join-Path $installation 'VC\Auxiliary\Build\vcvarsall.bat'
$output = Join-Path $OpenKeyRepositoryRoot "dist\text-service-tests-$Platform"
New-Item -ItemType Directory -Path $output -Force | Out-Null
$source = Join-Path $OpenKeyRepositoryRoot 'Tests\CaretTextServiceTests.cpp'
$command = '"' + $vcvars + '" ' + $Platform + ' >nul && cl /nologo /EHsc /std:c++17 /utf-8 /W4 /DUNICODE /D_UNICODE /Fe:CaretTextServiceTests.exe "' + $source + '" /link user32.lib advapi32.lib ole32.lib oleaut32.lib uuid.lib uiautomationcore.lib'
Push-Location $output
try {
    & $env:ComSpec /d /c $command
    if ($LASTEXITCODE -ne 0) { throw 'TSF fixture compilation failed.' }
    $arguments = @((Join-Path $componentRoot "$Platform\OpenKey.TextService.dll"))
    if ($Interactive) { $arguments += '--interactive' }
    if ($NativeContext) { $arguments += '--native-context' }
    & (Join-Path $output 'CaretTextServiceTests.exe') @arguments
    if ($LASTEXITCODE -ne 0) { throw 'TSF fixture failed.' }
} finally { Pop-Location }
