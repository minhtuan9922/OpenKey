[CmdletBinding()]
param(
    [Parameter(Mandatory)] [string]$NoticesDirectory,
    [string]$AssetsPath = ''
)
$ErrorActionPreference = 'Stop'
if (-not $AssetsPath) { $AssetsPath = Join-Path $PSScriptRoot 'Sources\OpenKey\win32\OpenKey\OpenKey.UI\obj\project.assets.json' }
$target = (Resolve-Path -LiteralPath $NoticesDirectory).Path.TrimEnd('\', '/')
if (-not (Test-Path -LiteralPath $target -PathType Container)) { throw 'Notices directory required.' }
# Recollect from the restored cache into a new reference directory. Do not change
# the supplied package, delete old output or claim legal redistribution approval.
$reference = Join-Path $PSScriptRoot ('dist\notices-reference-' + [guid]::NewGuid().ToString('N'))
& (Join-Path $PSScriptRoot 'collect-third-party.ps1') -OutputDirectory $reference -AssetsPath $AssetsPath
$expectedFiles = @{}
foreach ($file in Get-ChildItem -LiteralPath $reference -File -Recurse) {
    $name = $file.FullName.Substring($reference.Length + 1).Replace('\', '/')
    $expectedFiles[$name] = $file.FullName
}
$actualFiles = @{}
foreach ($file in Get-ChildItem -LiteralPath $target -File -Recurse) {
    $name = $file.FullName.Substring($target.Length + 1).Replace('\', '/')
    if ($actualFiles.ContainsKey($name) -or -not $expectedFiles.ContainsKey($name)) {
        throw "Unexpected or duplicate notice file: $name"
    }
    $actualFiles[$name] = $file.FullName
}
if ($expectedFiles.Count -ne $actualFiles.Count) { throw 'Missing packaged notices or metadata.' }
foreach ($name in $expectedFiles.Keys) {
    if ($name -eq 'inventory.json') {
        # JSON encodings/whitespace differ between PS7 and PS5.1, not semantics.
        $expected = Get-Content -LiteralPath $expectedFiles[$name] -Raw -Encoding UTF8 | ConvertFrom-Json
        $actual = Get-Content -LiteralPath $actualFiles[$name] -Raw -Encoding UTF8 | ConvertFrom-Json
        if (($expected | ConvertTo-Json -Depth 10 -Compress) -cne
            ($actual | ConvertTo-Json -Depth 10 -Compress)) { throw 'Notices inventory differs from restored dependency metadata.' }
    } elseif ((Get-FileHash -LiteralPath $expectedFiles[$name] -Algorithm SHA256).Hash -ne
        (Get-FileHash -LiteralPath $actualFiles[$name] -Algorithm SHA256).Hash) {
        throw "Packaged notice differs from restored dependency: $name"
    }
}
Write-Output "Verified $($expectedFiles.Count) notices/metadata files against restored dependency cache. Redistribution approval and binary-to-license mapping remain separate."
