[CmdletBinding()]
param([ValidateSet('All','x64','x86')] [string]$Platform = 'All', [string]$OutputRoot)
$ErrorActionPreference = 'Stop'
$OpenKeyRepositoryRoot = Split-Path -Parent $PSScriptRoot
if (-not $OutputRoot) { $OutputRoot = Join-Path $OpenKeyRepositoryRoot 'dist\text-service' }
# Resolve against PowerShell's location, which can differ from .NET's cwd.
$OutputRoot = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputRoot)
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installation = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1
if (-not $installation) { throw 'MSVC tools not found.' }
$vcvars = Join-Path $installation 'VC\Auxiliary\Build\vcvarsall.bat'
$source = Join-Path $OpenKeyRepositoryRoot 'Sources\OpenKey\win32\OpenKey\OpenKey.TextService'
foreach ($arch in $(if ($Platform -eq 'All') { @('x64','x86') } else { @($Platform) })) {
    $output = Join-Path $OutputRoot $arch
    New-Item -ItemType Directory -Path $output -Force | Out-Null
    $command = '"' + $vcvars + '" ' + $arch + ' >nul && cl /nologo /LD /MT /O2 /EHsc /std:c++17 /utf-8 /W4 /DUNICODE /D_UNICODE /Fe:OpenKey.TextService.dll "' + $source + '\CaretTextService.cpp" "' + $source + '\Registration.cpp" /link /DEF:"' + $source + '\OpenKey.TextService.def" user32.lib advapi32.lib ole32.lib oleaut32.lib uuid.lib'
    Push-Location $output
    try {
        & $env:ComSpec /d /c $command
        if ($LASTEXITCODE -ne 0) { throw "TSF build failed: $arch" }
        Write-Output "Built $output\OpenKey.TextService.dll (not registered or activated)."
    } finally { Pop-Location }
}
# The deployment helper embeds BOTH payloads, so it cannot load a substituted
# per-user DLL while elevated. A partial architecture build stays DLL-only.
if ($Platform -eq 'All') {
    $dll64 = (Join-Path $OutputRoot 'x64\OpenKey.TextService.dll').Replace('\','\\')
    $dll86 = (Join-Path $OutputRoot 'x86\OpenKey.TextService.dll').Replace('\','\\')
    foreach ($arch in @('x64','x86')) {
        $helperOutput = Join-Path $OutputRoot "helper\$arch"
        New-Item -ItemType Directory -Path $helperOutput -Force | Out-Null
        @("#include <windows.h>", "101 RCDATA `"$dll64`"", "102 RCDATA `"$dll86`"") |
            Set-Content -LiteralPath (Join-Path $helperOutput 'CaretPayload.rc') -Encoding Unicode
        $command = '"' + $vcvars + '" ' + $arch + ' >nul && rc /nologo /fo CaretPayload.res CaretPayload.rc && cl /nologo /MT /O2 /EHsc /std:c++17 /utf-8 /W4 /DUNICODE /D_UNICODE /Fe:OpenKey.CaretSetup.exe "' + $source + '\CaretSetup.cpp" CaretPayload.res /link /SUBSYSTEM:WINDOWS /MANIFEST:EMBED /MANIFESTINPUT:"' + $source + '\CaretSetup.manifest" user32.lib advapi32.lib shell32.lib ole32.lib uuid.lib bcrypt.lib'
        Push-Location $helperOutput
        try {
            & $env:ComSpec /d /c $command
            if ($LASTEXITCODE -ne 0) { throw "Caret deployment helper build failed: $arch" }
        } finally { Pop-Location }
    }
}
