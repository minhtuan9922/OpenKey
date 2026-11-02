[CmdletBinding()]
param([switch]$Bootstrap)
$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent $PSScriptRoot
. (Join-Path $repositoryRoot 'Installer/AppRuntime.ps1')
$inputs = Get-OpenKeyAppRuntimeInputs
[xml]$project = Get-Content (Join-Path $repositoryRoot 'Sources/OpenKey/win32/OpenKey/OpenKey.UI/OpenKey.UI.vcxproj') -Raw
$package = $project.Project.ItemGroup.PackageReference | Where-Object Include -eq 'Microsoft.WindowsAppSDK.Runtime'
if (([version]$package.Version).ToString(3) -ne ([version]$inputs.version).ToString(3)) { throw 'Project runtime and installer pins differ.' }
foreach ($arch in @('x64','x86')) {
    & (Join-Path $repositoryRoot 'scripts/build-runtime-check.ps1') $arch
    $probe = Join-Path $repositoryRoot "dist/runtime-check/$arch/OpenKey.RuntimeCheck.$arch.exe"
    & $probe 'invalid'
    if ($LASTEXITCODE -ne 2) { throw 'Runtime probe accepted invalid architecture.' }
    $nativeArch = if ($arch -eq 'x64') { 'X64' } else { 'X86' }
    $expected = @(Get-AppxPackage -Name 'Microsoft.WindowsAppRuntime.2' | Where-Object {
        $_.IsFramework -and $_.Architecture.ToString() -eq $nativeArch -and
        [version]$_.Version -ge [version]$inputs.version -and $_.Status.ToString() -eq 'Ok'
    }).Count -gt 0
    & $probe $arch
    if ($LASTEXITCODE -ne $(if ($expected) {0} else {1})) { throw "Native runtime detector differs from current-user package inventory: $arch" }
    if ($Bootstrap -and $expected) {
        $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
        $installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        $vcvars = Join-Path $installation 'VC/Auxiliary/Build/vcvarsall.bat'
        $output = Join-Path $repositoryRoot "dist/runtime-check/$arch"
        $source = Join-Path $repositoryRoot 'Tests/RuntimeBootstrapTests.cpp'
        Push-Location $output
        try {
            & $env:ComSpec /d /c ('"' + $vcvars + '" ' + $arch + ' >nul && cl /nologo /std:c++20 /EHsc /MT /O2 /Fe:RuntimeBootstrapTests.exe "' + $source + '" /link runtimeobject.lib')
            if ($LASTEXITCODE -ne 0) { throw 'Bootstrap smoke fixture build failed.' }
            $uiArch = if ($arch -eq 'x64') {'x64'} else {'Win32'}
            & ./RuntimeBootstrapTests.exe (Join-Path $repositoryRoot "Sources/OpenKey/win32/OpenKey/OpenKey.UI/$uiArch/Release/FrameworkDependent/OpenKey.UI/Microsoft.WindowsAppRuntime.Bootstrap.dll")
            if ($LASTEXITCODE -ne 0) { throw "Shared XAML activation failed: $arch" }
        } finally { Pop-Location }
    }
    Write-Output "Runtime detector passed: $arch (current-user framework present: $expected). No installation."
}
