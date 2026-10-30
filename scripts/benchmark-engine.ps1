[CmdletBinding()]
param([ValidateSet('x64', 'x86')] [string]$Platform = 'x64')
$OpenKeyRepositoryRoot = Split-Path -Parent $PSScriptRoot

$ErrorActionPreference = 'Stop'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installation = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1
if (-not $installation) { throw 'MSVC x86/x64 tools not found.' }
$vcvars = Join-Path $installation 'VC\Auxiliary\Build\vcvarsall.bat'
$output = Join-Path $OpenKeyRepositoryRoot "Sources\OpenKey\win32\OpenKey\OpenKey\Release\EngineBenchmark-$Platform"
New-Item -ItemType Directory -Path $output -Force | Out-Null
$sources = @((Join-Path $OpenKeyRepositoryRoot 'Tests\EngineBenchmark.cpp'))
$engineRoot = Join-Path $OpenKeyRepositoryRoot 'Sources\OpenKey\engine'
$sources += @('Engine.cpp', 'Vietnamese.cpp', 'Macro.cpp', 'ConvertTool.cpp', 'SmartSwitchKey.cpp') | ForEach-Object { Join-Path $engineRoot $_ }
$quoted = ($sources | ForEach-Object { '"' + $_ + '"' }) -join ' '
$command = '"' + $vcvars + '" ' + $Platform + ' >nul && cl /nologo /O2 /DNDEBUG /EHsc /utf-8 /D_HAS_STD_BYTE=0 /Fe:EngineBenchmark.exe ' + $quoted
Push-Location $output
try {
    & $env:ComSpec /d /c $command
    if ($LASTEXITCODE -ne 0) { throw 'Benchmark compilation failed.' }
    $lines = @(& (Join-Path $output 'EngineBenchmark.exe'))
    if ($LASTEXITCODE -ne 0) { throw 'Engine correctness checks or benchmark failed.' }
    $jsonLine = @($lines | Where-Object { $_ -like 'BENCH_JSON *' })
    if ($jsonLine.Count -ne 1) { throw 'Missing benchmark results.' }
    $samples = $jsonLine[0].Substring(11) | ConvertFrom-Json
    if (@($samples).Count -ne 4) { throw 'Unexpected benchmark scenario count.' }
    foreach ($sample in $samples) {
        if ($sample.events -le 0 -or $sample.mean_us_per_event -le 0 -or
            $sample.p50_batch_us_per_event -gt $sample.p95_batch_us_per_event -or
            $sample.p95_batch_us_per_event -gt $sample.p99_batch_us_per_event) { throw 'Invalid benchmark metrics.' }
    }
    $reportPath = Join-Path $output ('benchmark-' + [guid]::NewGuid().ToString('N') + '.json')
    [ordered]@{
        architecture = $Platform; optimization = 'MSVC /O2 /DNDEBUG'
        scope = 'Engine handler only; no hooks, UI, clipboard, registry, startup or SendInput'
        timing = '100 warmed batch samples; percentiles describe batch averages, not individual-key latency'
        settings = 'Spelling enabled; free marks; macro/smart-switch/quick-consonants disabled; same standalone globals as regression test'
        os = [Environment]::OSVersion.Version.ToString()
        logicalProcessors = [Environment]::ProcessorCount
        samples = @($samples)
    } | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $reportPath -Encoding UTF8
    Write-Host ($lines | Where-Object { $_ -like 'Engine regression:*' })
    Write-Host "4/4 benchmark scenarios completed ($Platform). Report: $reportPath"
    Write-Host 'No historical baseline or live typing-latency claim is established.'
} finally { Pop-Location }
