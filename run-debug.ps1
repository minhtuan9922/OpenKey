[CmdletBinding()]
param(
    [ValidateSet('x64', 'x86')]
    [string]$Platform = 'x64'
)

$ErrorActionPreference = 'Stop'

& (Join-Path $PSScriptRoot 'build.ps1') Debug $Platform
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$outputDirectory = if ($Platform -eq 'x64') {
    Join-Path $PSScriptRoot 'Sources\OpenKey\win32\OpenKey\x64\Debug'
} else {
    Join-Path $PSScriptRoot 'Sources\OpenKey\win32\OpenKey\Debug'
}

$executable = Join-Path $outputDirectory 'OpenKey.exe'
if (-not (Test-Path -LiteralPath $executable)) {
    throw "Không tìm thấy ứng dụng sau khi build: $executable"
}

Write-Host "Running $executable"
Start-Process -FilePath $executable -WorkingDirectory $outputDirectory
