[CmdletBinding()]
param(
    [Parameter(Mandatory)] [string]$ZipPath,
    [ValidateSet('x64', 'x86')] [string]$ExpectedArchitecture,
    [ValidatePattern('^[0-9A-Fa-f]{64}$')] [string]$ExpectedSha256
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$resolved = (Resolve-Path -LiteralPath $ZipPath).Path
if ($ExpectedSha256 -and (Get-FileHash -LiteralPath $resolved -Algorithm SHA256).Hash -ne $ExpectedSha256) {
    throw 'ZIP does not match the externally supplied SHA-256.'
}
function Assert-SafeEntryName([string]$Name) {
    if (-not $Name -or $Name -match '[\\:\x00-\x1f]' -or $Name.StartsWith('/') -or
        @($Name.Split('/') | Where-Object { $_ -in @('', '.', '..') -or $_.EndsWith('.') -or $_.EndsWith(' ') }).Count) {
        throw "Unsafe ZIP path: $Name"
    }
}
$archive = [IO.Compression.ZipFile]::OpenRead($resolved)
try {
    $entries = @{}
    foreach ($entry in $archive.Entries) {
        Assert-SafeEntryName $entry.FullName
        if ($entries.ContainsKey($entry.FullName)) { throw "Duplicate ZIP entry: $($entry.FullName)" }
        $entries[$entry.FullName] = $entry
    }
    $manifestEntry = $entries['package-manifest.json']
    if (-not $manifestEntry -or $manifestEntry.Length -gt 1MB) { throw 'Missing or oversized manifest.' }
    $reader = New-Object IO.StreamReader($manifestEntry.Open())
    try { $manifest = $reader.ReadToEnd() | ConvertFrom-Json } finally { $reader.Dispose() }
    if ($manifest.architecture -notin @('x64', 'x86') -or $manifest.configuration -ne 'Release' -or
        $manifest.channel -ne 'local-beta-not-for-stable-updater') { throw 'Unsupported package metadata.' }
    if ($ExpectedArchitecture -and $manifest.architecture -ne $ExpectedArchitecture) { throw 'Unexpected package architecture.' }
    $minimum = $null
    if (-not [version]::TryParse([string]$manifest.vcRuntimeMinimumVersion, [ref]$minimum) -or
        $minimum -lt [version]'14.0') { throw 'Invalid runtime prerequisite metadata.' }
    $records = @($manifest.files)
    if ($records.Count -eq 0 -or $records.Count -gt 10000 -or $entries.Count -ne $records.Count + 1) {
        throw 'Manifest/ZIP entry count mismatch.'
    }
    $listed = @{}
    foreach ($record in $records) {
        $name = [string]$record.path
        Assert-SafeEntryName $name
        if ($name -eq 'package-manifest.json' -or $listed.ContainsKey($name)) { throw "Duplicate/invalid manifest path: $name" }
        $listed[$name] = $true
        $size = 0L
        if (-not [long]::TryParse([string]$record.bytes, [ref]$size) -or $size -lt 0 -or
            [string]$record.sha256 -notmatch '^[0-9A-Fa-f]{64}$') { throw "Invalid file metadata: $name" }
        $entry = $entries[$name]
        if (-not $entry -or $entry.Length -ne $size) { throw "Missing/wrong ZIP payload: $name" }
        $hash = [Security.Cryptography.SHA256]::Create()
        $inputStream = $entry.Open()
        try { $actual = [BitConverter]::ToString($hash.ComputeHash($inputStream)).Replace('-', '') }
        finally { $inputStream.Dispose(); $hash.Dispose() }
        if ($actual -ne $record.sha256) { throw "ZIP hash mismatch: $name" }
    }
    $engine = if ($manifest.architecture -eq 'x64') { 'OpenKey64.exe' } else { 'OpenKey32.exe' }
    $required = @($engine, 'OpenKeyUpdate.exe', 'OpenKey.UI/OpenKey.UI.exe', 'OpenKey.UI/OpenKey.UI.pri',
        'OpenKey.UI/Microsoft.UI.Xaml.dll', 'OpenKey.UI/Microsoft.WindowsAppRuntime.dll',
        'OpenKey.UI/MainWindow.xbf', 'OpenKey.UI/AboutPage.xbf', 'LICENSE', 'BETA_PACKAGING.md', 'check-prerequisites.ps1',
        'ThirdPartyNotices/inventory.json')
    foreach ($name in $required) {
        if (-not $listed.ContainsKey($name)) { throw "Required package file missing: $name" }
    }
    # Check PE headers without extracting or loading executables.
    $machine = if ($manifest.architecture -eq 'x64') { 0x8664 } else { 0x14c }
    foreach ($name in @($engine, 'OpenKeyUpdate.exe', 'OpenKey.UI/OpenKey.UI.exe')) {
        $entry = $entries[$name]
        $reader = New-Object IO.BinaryReader($entry.Open())
        try {
            $header = $reader.ReadBytes(64)
            if ($header.Length -ne 64 -or [BitConverter]::ToUInt16($header, 0) -ne 0x5a4d) { throw "Invalid PE header: $name" }
            $offset = [BitConverter]::ToInt32($header, 60)
            if ($offset -lt 64 -or $offset -gt 1MB -or $offset + 6 -gt $entry.Length) { throw "Invalid PE offset: $name" }
            if ($reader.ReadBytes($offset - 64).Length -ne $offset - 64 -or
                $reader.ReadUInt32() -ne 0x4550 -or $reader.ReadUInt16() -ne $machine) { throw "Invalid PE architecture: $name" }
        } finally { $reader.Dispose() }
    }
    $result = [pscustomobject]@{ Zip = $resolved; Architecture = $manifest.architecture; VerifiedFiles = $records.Count; VcRuntimeMinimumVersion = $minimum.ToString(); Integrity = 'Passed; authenticity is not established by an unsigned manifest' }
} finally { $archive.Dispose() }
$result
