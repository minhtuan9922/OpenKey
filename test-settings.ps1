[CmdletBinding()]
param([ValidateSet('x64', 'x86')] [string]$Platform = 'x64')
$ErrorActionPreference = 'Stop'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installation = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1
if (-not $installation) { throw 'MSVC x86/x64 tools not found.' }
$vcvars = Join-Path $installation 'VC\Auxiliary\Build\vcvarsall.bat'
$output = Join-Path $PSScriptRoot "Sources\OpenKey\win32\OpenKey\OpenKey\Release\SettingsTests-$Platform"
New-Item -ItemType Directory -Path $output -Force | Out-Null
$source = Join-Path $PSScriptRoot 'Tests\SettingsMappingTests.cpp'
$command = '"' + $vcvars + '" ' + $Platform + ' >nul && cl /nologo /EHsc /utf-8 /W4 /DUNICODE /D_UNICODE /D_HAS_STD_BYTE=0 /Fe:SettingsMappingTests.exe "' + $source + '"'
Push-Location $output
try {
    & $env:ComSpec /d /c $command
    if ($LASTEXITCODE -ne 0) { throw 'Settings test compilation failed.' }
    & (Join-Path $output 'SettingsMappingTests.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Settings store regression failed.' }
    $windowSource = Join-Path $PSScriptRoot 'Tests\UiWindowSizeTests.cpp'
    $windowCommand = '"' + $vcvars + '" ' + $Platform + ' >nul && cl /nologo /EHsc /std:c++20 /utf-8 /W4 /DUNICODE /D_UNICODE /Fe:UiWindowSizeTests.exe "' + $windowSource + '"'
    & $env:ComSpec /d /c $windowCommand
    if ($LASTEXITCODE -ne 0) { throw 'Window size test compilation failed.' }
    & (Join-Path $output 'UiWindowSizeTests.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Window size regression failed.' }
    $themeSource = Join-Path $PSScriptRoot 'Tests\UiThemeTests.cpp'
    $themeCommand = '"' + $vcvars + '" ' + $Platform + ' >nul && cl /nologo /EHsc /std:c++20 /utf-8 /W4 /DUNICODE /D_UNICODE /Fe:UiThemeTests.exe "' + $themeSource + '"'
    & $env:ComSpec /d /c $themeCommand
    if ($LASTEXITCODE -ne 0) { throw 'UI theme test compilation failed.' }
    & (Join-Path $output 'UiThemeTests.exe')
    if ($LASTEXITCODE -ne 0) { throw 'UI theme regression failed.' }
    $dependencySource = Join-Path $PSScriptRoot 'Tests\SettingsDependencyTests.cpp'
    $dependencyCommand = '"' + $vcvars + '" ' + $Platform + ' >nul && cl /nologo /EHsc /std:c++20 /utf-8 /W4 /Fe:SettingsDependencyTests.exe "' + $dependencySource + '"'
    & $env:ComSpec /d /c $dependencyCommand
    if ($LASTEXITCODE -ne 0) { throw 'Settings dependency test compilation failed.' }
    & (Join-Path $output 'SettingsDependencyTests.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Settings dependency regression failed.' }
} finally { Pop-Location }
