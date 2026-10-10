$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$root = Join-Path $PSScriptRoot ('dist\package-tests-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $root -Force | Out-Null
function New-TestPE([string]$Architecture) {
    $bytes = New-Object byte[] 70
    $bytes[0] = 0x4d; $bytes[1] = 0x5a; $bytes[60] = 64
    $bytes[64] = 0x50; $bytes[65] = 0x45
    if ($Architecture -eq 'x64') { $bytes[68] = 0x64; $bytes[69] = 0x86 }
    else { $bytes[68] = 0x4c; $bytes[69] = 0x01 }
    return ,$bytes
}
function Add-TestEntry($Archive, [string]$Name, [byte[]]$Bytes) {
    $stream = $Archive.CreateEntry($Name).Open()
    try { $stream.Write($Bytes, 0, $Bytes.Length) } finally { $stream.Dispose() }
}
$cases = @('valid-x64', 'valid-x86', 'modified', 'missing', 'extra', 'duplicate-zip',
    'duplicate-record', 'traversal', 'invalid-arch', 'wrong-pe', 'required-missing',
    'no-manifest', 'invalid-runtime', 'invalid-hash')
foreach ($case in $cases) {
    $architecture = if ($case -eq 'valid-x86') { 'x86' } else { 'x64' }
    $engine = if ($architecture -eq 'x64') { 'OpenKey64.exe' } else { 'OpenKey32.exe' }
    $files = @{}
    foreach ($name in @($engine, 'OpenKeyUpdate.exe', 'OpenKey.UI/OpenKey.UI.exe')) { $files[$name] = New-TestPE $architecture }
    foreach ($name in @('OpenKey.UI/OpenKey.UI.pri', 'OpenKey.UI/Microsoft.UI.Xaml.dll',
        'OpenKey.UI/Microsoft.WindowsAppRuntime.dll', 'OpenKey.UI/MainWindow.xbf', 'OpenKey.UI/AboutPage.xbf',
        'LICENSE', 'BETA_PACKAGING.md', 'check-prerequisites.ps1', 'ThirdPartyNotices/inventory.json')) { $files[$name] = [Text.Encoding]::UTF8.GetBytes('fixture-only') }
    if ($case -eq 'required-missing') { $files.Remove('LICENSE') }
    if ($case -eq 'wrong-pe') { $files[$engine] = New-TestPE 'x86' }
    $records = @($files.Keys | Sort-Object | ForEach-Object {
        $hash = [Security.Cryptography.SHA256]::Create()
        try { $digest = [BitConverter]::ToString($hash.ComputeHash([byte[]]$files[$_])).Replace('-', '') } finally { $hash.Dispose() }
        @{ path = $_; bytes = $files[$_].Length; sha256 = $digest }
    })
    if ($case -eq 'duplicate-record') { $records[1] = $records[0] }
    if ($case -eq 'invalid-hash') { $records[0].sha256 = 'invalid' }
    $manifest = @{ channel = 'local-beta-not-for-stable-updater'; configuration = 'Release'; architecture = $architecture;
        vcRuntimeMinimumVersion = '14.51.36231.0'; files = $records }
    if ($case -eq 'invalid-arch') { $manifest.architecture = 'arm64' }
    if ($case -eq 'invalid-runtime') { $manifest.vcRuntimeMinimumVersion = 'invalid' }
    if ($case -eq 'modified') { $files['LICENSE'] = [Text.Encoding]::UTF8.GetBytes('tamper------') }
    if ($case -eq 'missing') { $files.Remove('LICENSE') }
    $zipPath = Join-Path $root ($case + '.zip')
    $archive = [IO.Compression.ZipFile]::Open($zipPath, [IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($name in $files.Keys) { Add-TestEntry $archive $name $files[$name] }
        if ($case -eq 'extra') { Add-TestEntry $archive 'extra.txt' ([byte[]]@(1)) }
        if ($case -eq 'duplicate-zip') { Add-TestEntry $archive 'LICENSE' $files['LICENSE'] }
        if ($case -eq 'traversal') { Add-TestEntry $archive '../escape.txt' ([byte[]]@(1)) }
        if ($case -ne 'no-manifest') { Add-TestEntry $archive 'package-manifest.json' ([Text.Encoding]::UTF8.GetBytes(($manifest | ConvertTo-Json -Depth 5))) }
    } finally { $archive.Dispose() }
    $accepted = $false
    try { & (Join-Path $PSScriptRoot 'verify-beta.ps1') -ZipPath $zipPath | Out-Null; $accepted = $true } catch { }
    if ($accepted -ne ($case -in @('valid-x64', 'valid-x86'))) { throw "Packaging test failed: $case" }
    Write-Host "PASS $case"
}
$validZip = Join-Path $root 'valid-x64.zip'
$expectedHash = (Get-FileHash -LiteralPath $validZip -Algorithm SHA256).Hash
& (Join-Path $PSScriptRoot 'verify-beta.ps1') $validZip -ExpectedSha256 $expectedHash | Out-Null
Write-Host 'PASS external-hash-match'
foreach ($testArguments in @(@{ ExpectedSha256 = ('0' * 64) }, @{ ExpectedArchitecture = 'x86' })) {
    $rejected = $false
    try { & (Join-Path $PSScriptRoot 'verify-beta.ps1') $validZip @testArguments | Out-Null } catch { $rejected = $true }
    if (-not $rejected) { throw 'External hash/architecture mismatch was accepted.' }
}
Write-Host "17/17 synthetic package checks passed. No executables loaded; fixtures retained: $root"
