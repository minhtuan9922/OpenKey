[CmdletBinding()]
param([ValidateSet('x64', 'x86')] [string]$Platform = 'x64')
$ErrorActionPreference = 'Stop'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installation = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1
if (-not $installation) { throw 'MSVC x86/x64 tools not found.' }
$vcvars = Join-Path $installation 'VC\Auxiliary\Build\vcvarsall.bat'
$output = Join-Path $PSScriptRoot "Sources\OpenKey\win32\OpenKey\OpenKey\Release\ConvertTests-$Platform"
New-Item -ItemType Directory -Path $output -Force | Out-Null
$sources = @((Join-Path $PSScriptRoot 'Tests\ConvertServiceTests.cpp'))
$engineRoot = Join-Path $PSScriptRoot 'Sources\OpenKey\engine'
$sources += @('Engine.cpp', 'Vietnamese.cpp', 'Macro.cpp', 'ConvertTool.cpp', 'SmartSwitchKey.cpp') | ForEach-Object { Join-Path $engineRoot $_ }
$quoted = ($sources | ForEach-Object { '"' + $_ + '"' }) -join ' '
$command = '"' + $vcvars + '" ' + $Platform + ' >nul && cl /nologo /std:c++20 /EHsc /utf-8 /D_HAS_STD_BYTE=0 /Fe:ConvertServiceTests.exe ' + $quoted
Push-Location $output
try {
    & $env:ComSpec /d /c $command
    if ($LASTEXITCODE -ne 0) { throw 'Convert test compilation failed.' }
    & (Join-Path $output 'ConvertServiceTests.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Convert service regression failed.' }
    $clipboardSource = Join-Path $PSScriptRoot 'Tests\ClipboardServiceTests.cpp'
    $clipboardSources = @($clipboardSource) + $sources[1..($sources.Count - 1)]
    $clipboardQuoted = ($clipboardSources | ForEach-Object { '"' + $_ + '"' }) -join ' '
    $clipboardCommand = '"' + $vcvars + '" ' + $Platform + ' >nul && cl /nologo /std:c++20 /EHsc /utf-8 /D_HAS_STD_BYTE=0 /Fe:ClipboardServiceTests.exe ' + $clipboardQuoted
    & $env:ComSpec /d /c $clipboardCommand
    if ($LASTEXITCODE -ne 0) { throw 'Clipboard test compilation failed.' }
    & (Join-Path $output 'ClipboardServiceTests.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Clipboard service regression failed.' }
} finally { Pop-Location }
