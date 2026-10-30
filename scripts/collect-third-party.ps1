[CmdletBinding()]
param(
    [Parameter(Mandatory)] [string]$OutputDirectory,
    [string]$AssetsPath = ''
)
$OpenKeyRepositoryRoot = Split-Path -Parent $PSScriptRoot

$ErrorActionPreference = 'Stop'
if (-not $AssetsPath) { $AssetsPath = Join-Path $OpenKeyRepositoryRoot 'Sources\OpenKey\win32\OpenKey\OpenKey.UI\obj\project.assets.json' }
$assets = Get-Content -LiteralPath $AssetsPath -Raw -Encoding UTF8 | ConvertFrom-Json
$output = [IO.Path]::GetFullPath($OutputDirectory)
$distRoot = [IO.Path]::GetFullPath((Join-Path $OpenKeyRepositoryRoot 'dist')) + [IO.Path]::DirectorySeparatorChar
if (-not $output.StartsWith($distRoot, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Third-party output must be a new directory inside repository dist.'
}
if (Test-Path -LiteralPath $output) { throw 'Refusing to overwrite an existing notices directory.' }
New-Item -ItemType Directory -Path $output | Out-Null
$inventory = @()
foreach ($library in ($assets.libraries.PSObject.Properties | Sort-Object Name)) {
    if ($library.Value.type -ne 'package') { continue }
    $relativePackage = [string]$library.Value.path
    $packageRoot = $null
    foreach ($folder in $assets.packageFolders.PSObject.Properties.Name) {
        $cacheRoot = [IO.Path]::GetFullPath($folder).TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
        $candidate = [IO.Path]::GetFullPath((Join-Path $cacheRoot $relativePackage))
        if (-not $candidate.StartsWith($cacheRoot, [StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe NuGet package path.' }
        if (Test-Path -LiteralPath $candidate -PathType Container) { $packageRoot = $candidate; break }
    }
    if (-not $packageRoot) { throw "Restored package not found: $($library.Name)" }
    $spec = @(Get-ChildItem -LiteralPath $packageRoot -Filter *.nuspec -File)
    if ($spec.Count -ne 1) { throw "Expected one nuspec: $($library.Name)" }
    [xml]$xml = Get-Content -LiteralPath $spec[0].FullName -Raw -Encoding UTF8
    $metadata = $xml.package.metadata
    $id = [string]$metadata.id
    $version = [string]$metadata.version
    if (($id + '/' + $version) -ine $library.Name) {
        throw "NuGet metadata does not match restored identity: $($library.Name)"
    }
    $destinationName = $id + '-' + $version
    if ($destinationName -notmatch '^[A-Za-z0-9_.+-]+$') { throw 'Unsafe package identity.' }
    $destination = Join-Path $output $destinationName
    New-Item -ItemType Directory -Path $destination | Out-Null
    Copy-Item -LiteralPath $spec[0].FullName -Destination (Join-Path $destination 'package.nuspec')
    $licenseType = [string]$metadata.license.type
    $licenseValue = [string]$metadata.license.InnerText
    $noticeNames = @($library.Value.files | Where-Object {
        [IO.Path]::GetFileName($_) -match '^(license|copying|notice|thirdpartynotices|copyright)'
    })
    if ($licenseType -eq 'file' -and $licenseValue) { $noticeNames += $licenseValue }
    $copied = @()
    foreach ($name in ($noticeNames | Sort-Object -Unique)) {
        if ([IO.Path]::IsPathRooted($name) -or $name -match ':' -or
            @($name.Split([char[]]@('/', '\')) | Where-Object { $_ -in @('', '.', '..') }).Count) {
            throw 'Unsafe relative notice path.'
        }
        $source = [IO.Path]::GetFullPath((Join-Path $packageRoot $name))
        $packagePrefix = $packageRoot.TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
        if (-not $source.StartsWith($packagePrefix, [StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe package notice path.' }
        if (-not (Test-Path -LiteralPath $source -PathType Leaf)) { throw "Declared notice file missing: $id/$name" }
        # Preserve nested package paths so same-named notices cannot overwrite one another.
        $target = Join-Path $destination $name
        New-Item -ItemType Directory -Path (Split-Path -Parent $target) -Force | Out-Null
        Copy-Item -LiteralPath $source -Destination $target
        $copied += ($destinationName + '/' + $name.Replace('\', '/'))
    }
    $inventory += [ordered]@{
        id = $id; version = $version; authors = [string]$metadata.authors
        copyright = [string]$metadata.copyright
        licenseType = $licenseType; licenseValue = $licenseValue
        licenseUrl = [string]$metadata.licenseUrl; projectUrl = [string]$metadata.projectUrl
        copiedNotices = $copied
        review = if ($licenseType -eq 'file' -and $copied.Count -gt 0) {
            'Declared license copied; redistribution obligations still require review'
        } else { 'No declared license file; manual review required' }
    }
}
if ($inventory.Count -eq 0) { throw 'No restored NuGet packages found.' }
[ordered]@{
    scope = 'All restored UI NuGet packages, including build-only tools; not a binary-to-license mapping'
    redistributionApproval = 'Not established; manual review and corresponding OpenKey source still required'
    packages = $inventory
} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $output 'inventory.json') -Encoding UTF8
Write-Host "Collected metadata/notices for $($inventory.Count) restored packages. Manual release review remains pending."
