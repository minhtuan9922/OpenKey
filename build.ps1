[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',

    [ValidateSet('x64', 'x86')]
    [string]$Platform = 'x64',

    [ValidateSet('Legacy', 'UI', 'All')]
    [string]$Target = 'Legacy',

    [switch]$Analyze
)

$ErrorActionPreference = 'Stop'
$solution = Join-Path $PSScriptRoot 'Sources\OpenKey\win32\OpenKey\OpenKey.sln'
$uiProject = Join-Path $PSScriptRoot 'Sources\OpenKey\win32\OpenKey\OpenKey.UI\OpenKey.UI.vcxproj'
$msbuild = Get-Command 'msbuild.exe' -ErrorAction SilentlyContinue
$msbuildPath = if ($msbuild) { $msbuild.Source } else { $null }

if (-not $msbuildPath) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'

    if (Test-Path -LiteralPath $vswhere) {
        $msbuildPath = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild `
            -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1

    }
}

if (-not $msbuildPath) {
    throw 'Không tìm thấy MSBuild. Hãy cài Visual Studio với workload Desktop development with C++.'
}

function Invoke-MSBuild([string]$Project) {
    $projectPlatform = if ($Platform -eq 'x86' -and [IO.Path]::GetExtension($Project) -eq '.vcxproj') { 'Win32' } else { $Platform }
    $buildArguments = @($Project, '/restore', '/m', "/p:Configuration=$Configuration", "/p:Platform=$projectPlatform", '/p:PreferredToolArchitecture=x64')
    if ($Analyze) {
        # Rebuild ensures unchanged translation units are analyzed too.
        $buildArguments += @('/t:Rebuild', '/p:RunCodeAnalysis=true')
    }
    & $msbuildPath @buildArguments
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

Write-Host "Building OpenKey $Target ($Configuration | $Platform)..."

if ($Target -in @('Legacy', 'All')) {
    Invoke-MSBuild $solution
}

if ($Target -in @('UI', 'All')) {
    Invoke-MSBuild $uiProject
}
