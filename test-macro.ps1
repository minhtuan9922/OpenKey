[CmdletBinding()]
param([ValidateSet('x64', 'x86')] [string]$Platform = 'x64')
$ErrorActionPreference = 'Stop'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installation = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1
if (-not $installation) { throw 'MSVC x86/x64 tools not found.' }
$vcvars = Join-Path $installation 'VC\Auxiliary\Build\vcvarsall.bat'
$output = Join-Path $PSScriptRoot "Sources\OpenKey\win32\OpenKey\OpenKey\Release\MacroTests-$Platform"
New-Item -ItemType Directory -Path $output -Force | Out-Null
$fixture = Join-Path $output ('fixture-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $fixture | Out-Null
$source = Join-Path $PSScriptRoot 'Tests\MacroServiceTests.cpp'
$command = '"' + $vcvars + '" ' + $Platform + ' >nul && cl /nologo /std:c++20 /EHsc /utf-8 /W4 /Fe:MacroServiceTests.exe "' + $source + '"'
Push-Location $output
try {
    & $env:ComSpec /d /c $command
    if ($LASTEXITCODE -ne 0) { throw 'Macro test compilation failed.' }
    & (Join-Path $output 'MacroServiceTests.exe') $fixture
    if ($LASTEXITCODE -ne 0) { throw 'Macro service regression failed.' }
    Write-Host "Isolated fixture retained: $fixture"
} finally { Pop-Location }
