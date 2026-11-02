[CmdletBinding()]
param([string]$IsccPath = '')
$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent $PSScriptRoot
. (Join-Path $repositoryRoot 'Installer/Compiler.ps1')
$IsccPath = Resolve-OpenKeySetupCompiler $IsccPath
$inputs = Get-Content -LiteralPath (Join-Path $repositoryRoot 'Installer/OfflineInputs.json') -Raw | ConvertFrom-Json
$probeRoot = Join-Path $repositoryRoot ('dist/download-probe-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $probeRoot | Out-Null
& $IsccPath /Q "/DProbeOutput=$probeRoot" "/DRuntimeUrl=$($inputs.x86.url)" "/DRuntimeHash=$($inputs.x86.sha256)" (Join-Path $repositoryRoot 'Tests/InstallerDownloadTests.iss')
if ($LASTEXITCODE -ne 0) { throw 'Download probe compilation failed.' }
$reportPath = Join-Path $probeRoot 'report.txt'
$probe = Start-Process -FilePath (Join-Path $probeRoot 'DownloadProbe.exe') -WindowStyle Hidden -PassThru -Wait -ArgumentList @(
    '/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART',('/Report="' + $reportPath + '"'),('/LOG="' + (Join-Path $probeRoot 'probe.log') + '"'))
if ($probe.ExitCode -ne 1 -or -not (Test-Path -LiteralPath $reportPath)) { throw "Download probe failed. See $probeRoot" }
Get-Content -LiteralPath $reportPath
