[CmdletBinding()]
param([ValidateSet('x64','x86')][string]$Platform = 'x64')
$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $installation) { throw 'Visual C++ build tools required.' }
$vcvars = Join-Path $installation 'VC/Auxiliary/Build/vcvarsall.bat'
$output = Join-Path $repositoryRoot "dist/runtime-check/$Platform"
New-Item -ItemType Directory -Path $output -Force | Out-Null
$source = Join-Path $repositoryRoot 'Sources/OpenKey/win32/OpenKey/RuntimeCheck.cpp'
Push-Location $output
try {
    & $env:ComSpec /d /c ('"' + $vcvars + '" ' + $Platform + ' >nul && cl /nologo /std:c++20 /EHsc /MT /O2 /utf-8 /DUNICODE /D_UNICODE /Fe:OpenKey.RuntimeCheck.' + $Platform + '.exe "' + $source + '" /link windowsapp.lib /OPT:REF /OPT:ICF')
    if ($LASTEXITCODE -ne 0) { throw 'Runtime probe build failed.' }
} finally { Pop-Location }
