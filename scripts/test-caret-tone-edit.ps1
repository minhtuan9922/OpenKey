[CmdletBinding()]
param([ValidateSet('x64', 'x86')] [string]$Platform = 'x64')
$ErrorActionPreference = 'Stop'
$OpenKeyRepositoryRoot = Split-Path -Parent $PSScriptRoot
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installation = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1
if (-not $installation) { throw 'MSVC tools not found.' }
$vcvars = Join-Path $installation 'VC\Auxiliary\Build\vcvarsall.bat'
$output = Join-Path $OpenKeyRepositoryRoot "dist\caret-tone-tests-$Platform"
New-Item -ItemType Directory -Path $output -Force | Out-Null
$source = Join-Path $OpenKeyRepositoryRoot 'Tests\CaretToneEditTests.cpp'
$command = '"' + $vcvars + '" ' + $Platform + ' >nul && cl /nologo /EHsc /std:c++17 /utf-8 /W4 /DUNICODE /D_UNICODE /Fe:CaretToneEditTests.exe "' + $source + '" /link user32.lib'
Push-Location $output
try {
    & $env:ComSpec /d /c $command
    if ($LASTEXITCODE -ne 0) { throw 'Caret tone tests failed to compile.' }
    & (Join-Path $output 'CaretToneEditTests.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Caret tone regression failed.' }
} finally { Pop-Location }
