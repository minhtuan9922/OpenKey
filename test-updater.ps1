[CmdletBinding()]
param(
    [ValidateSet('x64', 'x86')]
    [string]$Platform = 'x64'
)
$ErrorActionPreference = 'Stop'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installation = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1
if (-not $installation) { throw 'Không tìm thấy bộ công cụ MSVC x86/x64.' }
$vcvars = Join-Path $installation 'VC\Auxiliary\Build\vcvarsall.bat'
$buildRoot = Join-Path $PSScriptRoot 'Sources\OpenKey\win32\OpenKey\OpenKeyUpdate'
$outputDirectory = if ($Platform -eq 'x64') { Join-Path $buildRoot 'x64\Debug\UpdaterTests' } else { Join-Path $buildRoot 'Debug\UpdaterTests' }
New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
# Each run uses a new fixture directory; nothing is deleted recursively.
$fixtureRoot = Join-Path $outputDirectory ('fixtures-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $fixtureRoot | Out-Null
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$engineName = if ($Platform -eq 'x64') { 'OpenKey64.exe' } else { 'OpenKey32.exe' }
$otherName = if ($Platform -eq 'x64') { 'OpenKey32.exe' } else { 'OpenKey64.exe' }
function New-UpdaterTestPE([bool]$OtherArchitecture) {
    $bytes = New-Object byte[] 81
    $bytes[0] = 0x4d; $bytes[1] = 0x5a; $bytes[60] = 64; $bytes[64] = 0x50; $bytes[65] = 0x45
    $x64 = ($Platform -eq 'x64') -xor $OtherArchitecture
    if ($x64) { $bytes[68] = 0x64; $bytes[69] = 0x86 } else { $bytes[68] = 0x4c; $bytes[69] = 0x01 }
    [Text.Encoding]::ASCII.GetBytes('new-fixture').CopyTo($bytes, 70)
    return ,$bytes
}
foreach ($caseName in @('valid', 'missing', 'corrupt', 'locked', 'existing-stage', 'Unicode tiếng Việt spaces',
    'wrong-name', 'wrong-machine', 'invalid-pe', 'both-architectures')) {
    $caseDirectory = Join-Path $fixtureRoot $caseName
    New-Item -ItemType Directory -Path $caseDirectory | Out-Null
    [IO.File]::WriteAllText((Join-Path $caseDirectory $engineName), 'old-fixture')
    [IO.File]::WriteAllText((Join-Path $caseDirectory $otherName), 'other-old-fixture')
    $zipPath = Join-Path $caseDirectory '_OpenKeyUpdate.zip'
    if ($caseName -eq 'corrupt') {
        [IO.File]::WriteAllText($zipPath, 'not-a-zip')
    } else {
        $zip = [IO.Compression.ZipFile]::Open($zipPath, [IO.Compression.ZipArchiveMode]::Create)
        try {
            $entryName = if ($caseName -eq 'missing') { 'readme.txt' } elseif ($caseName -eq 'wrong-name') { $otherName } else { $engineName }
            $bytes = if ($caseName -eq 'invalid-pe') { [Text.Encoding]::ASCII.GetBytes('not-a-pe') } else { New-UpdaterTestPE ($caseName -in @('wrong-machine', 'wrong-name')) }
            $stream = $zip.CreateEntry($entryName).Open()
            try { $stream.Write($bytes, 0, $bytes.Length) } finally { $stream.Dispose() }
            if ($caseName -eq 'both-architectures') {
                $bytes = New-UpdaterTestPE $true
                $stream = $zip.CreateEntry($otherName).Open()
                try { $stream.Write($bytes, 0, $bytes.Length) } finally { $stream.Dispose() }
            }
        } finally { $zip.Dispose() }
    }
    if ($caseName -eq 'existing-stage') {
        $staging = Join-Path $caseDirectory 'stage-test'
        New-Item -ItemType Directory -Path $staging | Out-Null
        [IO.File]::WriteAllText((Join-Path $staging $engineName), 'stale-fixture')
    }
}
$source = Join-Path $PSScriptRoot 'Tests\UpdaterRegressionTests.cpp'
$command = '"' + $vcvars + '" ' + $Platform + ' >nul && cl /nologo /EHsc /utf-8 /DUNICODE /D_UNICODE /Fe:UpdaterRegressionTests.exe "' + $source + '" /link /SUBSYSTEM:CONSOLE user32.lib'
Push-Location $outputDirectory
try {
    & $env:ComSpec /d /c $command
    if ($LASTEXITCODE -ne 0) { throw 'Build updater tests failed.' }
    & (Join-Path $outputDirectory 'UpdaterRegressionTests.exe') $fixtureRoot
    if ($LASTEXITCODE -ne 0) { throw 'Updater regression tests failed.' }
    $policies = @(
        @{ Name = 'legacy'; Marker = ''; Expected = 'allow' },
        @{ Name = 'manifest'; Marker = 'package-manifest.json'; Expected = 'block' },
        @{ Name = 'flat-ui'; Marker = 'OpenKey.UI.exe'; Expected = 'block' },
        @{ Name = 'nested-ui'; Marker = 'OpenKey.UI'; Expected = 'block' },
        @{ Name = 'unrelated'; Marker = 'readme.txt'; Expected = 'allow' }
    )
    foreach ($policy in $policies) {
        $directory = Join-Path $fixtureRoot ('policy-' + $policy.Name)
        New-Item -ItemType Directory -Path $directory | Out-Null
        if ($policy.Marker -eq 'OpenKey.UI') { New-Item -ItemType Directory -Path (Join-Path $directory $policy.Marker) | Out-Null }
        elseif ($policy.Marker) { [IO.File]::WriteAllText((Join-Path $directory $policy.Marker), 'fixture-only') }
        $testExecutable = Join-Path $directory 'UpdaterRegressionTests.exe'
        Copy-Item -LiteralPath (Join-Path $outputDirectory 'UpdaterRegressionTests.exe') -Destination $testExecutable
        # Run only the policy-test entry point. It neither downloads nor replaces files.
        & $testExecutable --policy $policy.Expected
        if ($LASTEXITCODE -ne 0) { throw "Update policy failed: $($policy.Name)" }
    }
    Write-Host "5/5 module-directory update policy cases passed ($Platform)."
    Write-Host "10 updater fixture cases + 23 metadata cases passed ($Platform). Fixtures retained: $fixtureRoot"
} finally { Pop-Location }
