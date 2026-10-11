[CmdletBinding()]
param(
    [ValidateSet('x64', 'x86')]
    [string]$Platform = 'x64'
)

$ErrorActionPreference = 'Stop'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) {
    throw 'Không tìm thấy vswhere. Hãy cài Visual Studio với workload Desktop development with C++.'
}
$installation = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1
if (-not $installation) {
    throw 'Không tìm thấy bộ công cụ MSVC x86/x64.'
}
$vcvars = Join-Path $installation 'VC\Auxiliary\Build\vcvarsall.bat'
$buildRoot = Join-Path $PSScriptRoot 'Sources\OpenKey\win32\OpenKey\OpenKey'
$outputDirectory = if ($Platform -eq 'x64') {
    Join-Path $buildRoot 'x64\Debug\EngineTests'
} else {
    Join-Path $buildRoot 'Debug\EngineTests'
}
New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
$engineRoot = Join-Path $PSScriptRoot 'Sources\OpenKey\engine'
$sources = @((Join-Path $PSScriptRoot 'Tests\EngineRegressionTests.cpp'))
$sources += @('Engine.cpp', 'Vietnamese.cpp', 'Macro.cpp', 'ConvertTool.cpp', 'SmartSwitchKey.cpp') |
    ForEach-Object { Join-Path $engineRoot $_ }
$quotedSources = ($sources | ForEach-Object { '"' + $_ + '"' }) -join ' '
$executable = Join-Path $outputDirectory 'EngineRegressionTests.exe'
# Run in the output directory so all compiler artifacts remain under ignored Debug folders.
$command = '"' + $vcvars + '" ' + $Platform + ' >nul && cl /nologo /EHsc /utf-8 /D_HAS_STD_BYTE=0 /Fe:EngineRegressionTests.exe ' + $quotedSources
Push-Location $outputDirectory
try {
    & $env:ComSpec /d /c $command
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $executable
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
} finally {
    Pop-Location
}
