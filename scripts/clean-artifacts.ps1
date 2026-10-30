[CmdletBinding()]
param([switch]$Apply)

$ErrorActionPreference = 'Stop'
$OpenKeyRepositoryRoot = Split-Path -Parent $PSScriptRoot
$repo = [IO.Path]::GetFullPath($OpenKeyRepositoryRoot).TrimEnd('\')
$dist = Join-Path $repo 'dist'
$native = Join-Path $repo 'Sources\OpenKey\win32\OpenKey'
$candidates = [Collections.Generic.List[string]]::new()
$keep = @(
    (Join-Path $native 'OpenKey\x64\Debug'),
    (Join-Path $native 'OpenKey\Debug'),
    (Join-Path $native 'OpenKey.UI\x64\Debug\OpenKey.UI')
)
function Add-Candidate([string]$Path) { $candidates.Add([IO.Path]::GetFullPath($Path)) }

if (Test-Path -LiteralPath $dist) {
    $latestSetup = Get-ChildItem -LiteralPath $dist -Directory |
        Where-Object Name -Match '^setup-[a-f0-9]{32}$' |
        Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName 'setup-build.json') } |
        Sort-Object LastWriteTime -Descending | Select-Object -First 1
    $keepZips = @()
    if ($latestSetup) {
        $setup = Get-Content -LiteralPath (Join-Path $latestSetup.FullName 'setup-build.json') -Raw | ConvertFrom-Json
        $keepZips = @($setup.zipX64, $setup.zipX86)
    }
    # Retain the newest passing report for each PowerShell host.
    $reports = @(Get-ChildItem -LiteralPath $dist -Directory -Filter 'regression-*' |
        Sort-Object LastWriteTime -Descending)
    $keepReports = @()
    $shells = @{}
    foreach ($folder in $reports) {
        $summaryPath = Join-Path $folder.FullName 'summary.json'
        if (-not (Test-Path -LiteralPath $summaryPath)) { $keepReports += $folder.Name; continue }
        $summary = Get-Content -LiteralPath $summaryPath -Raw | ConvertFrom-Json
        if ($summary.passed -and -not $shells.ContainsKey($summary.shellVersion)) {
            $shells[$summary.shellVersion] = $true; $keepReports += $folder.Name
        }
    }
    foreach ($item in Get-ChildItem -LiteralPath $dist -Force) {
        if ($item.PSIsContainer) {
            if ($item.Name -match '^(notices-(reference|tests)-|package-tests-|prerequisites-tests-|sandbox-[a-f0-9]{32}$|setup-policy-|installer-close-compile-|diagnostics-test-|ui-debug-)' -or
                $item.Name -match '^OpenKey-Fluent-beta-' -or
                ($item.Name -match '^setup-[a-f0-9]{32}$' -and $item.FullName -ne $latestSetup.FullName) -or
                ($item.Name -match '^regression-' -and $item.Name -notin $keepReports)) { Add-Candidate $item.FullName }
        } elseif (($item.Extension -eq '.zip' -and $item.Name -like 'OpenKey-Fluent-beta-*' -and $item.FullName -notin $keepZips) -or
            ($item.Extension -eq '.log' -and $item.Name -notlike 'layout-regression*.log') -or
            $item.Name -like 'tray-popup-*.bmp' -or $item.Name -eq 'layout-move-plan.json') { Add-Candidate $item.FullName }
    }
}

# Entire reproducible build directories, except the current runnable Debug
# outputs. Do not touch NuGet assets/inputs or editor and CodeGraph state.
foreach ($folder in Get-ChildItem -LiteralPath $native -Directory -Recurse -Force |
    Where-Object Name -in @('Debug','Release','x64','Generated Files')) {
    $protected = @($keep | Where-Object {
        $_ -eq $folder.FullName -or $_.StartsWith($folder.FullName + '\', [StringComparison]::OrdinalIgnoreCase) -or
        $folder.FullName.StartsWith($_ + '\', [StringComparison]::OrdinalIgnoreCase)
    }).Count -gt 0
    if (-not $protected) { Add-Candidate $folder.FullName }
}
foreach ($folder in @($keep[0], $keep[1])) {
    if (Test-Path -LiteralPath $folder) {
        foreach ($item in Get-ChildItem -LiteralPath $folder -Force) {
            if ($item.Name -notin @('OpenKey.exe','OpenKey.pdb')) { Add-Candidate $item.FullName }
        }
    }
}
$uiDebug = Join-Path $native 'OpenKey.UI\x64\Debug'
if (Test-Path -LiteralPath $uiDebug) {
    foreach ($item in Get-ChildItem -LiteralPath $uiDebug -Force) {
        if ($item.Name -ne 'OpenKey.UI') { Add-Candidate $item.FullName }
    }
}

# Verify the resolved absolute paths before every recursive removal. Keep all
# tracked files and reject reparse points instead of traversing other locations.
$tracked = @(& git -C $repo -c core.quotepath=false ls-files)
if ($LASTEXITCODE -ne 0) { throw 'Cannot verify tracked files.' }
$running = @(Get-Process -Name 'OpenKey','OpenKey32','OpenKey64','OpenKey.UI' -ErrorAction SilentlyContinue |
    ForEach-Object { $_.Path } | Where-Object { $_ })
$selected = [Collections.Generic.List[string]]::new()
$skipped = [Collections.Generic.List[string]]::new()
foreach ($path in ($candidates | Sort-Object Length | Select-Object -Unique)) {
    if (@($selected | Where-Object { $path.StartsWith($_ + '\', [StringComparison]::OrdinalIgnoreCase) }).Count) { continue }
    if (-not ($path.StartsWith($dist + '\', [StringComparison]::OrdinalIgnoreCase) -or
        $path.StartsWith($native + '\', [StringComparison]::OrdinalIgnoreCase))) { throw "Out-of-scope cleanup path: $path" }
    $relative = $path.Substring($repo.Length + 1).Replace('\','/')
    if (@($tracked | Where-Object { $_ -eq $relative -or $_.StartsWith($relative + '/', [StringComparison]::OrdinalIgnoreCase) }).Count) {
        throw "Tracked file in cleanup target: $relative"
    }
    & git -C $repo check-ignore -q -- $relative
    if ($LASTEXITCODE -ne 0) { throw "Cleanup target is not ignored: $relative" }
    $item = Get-Item -LiteralPath $path -Force
    $links = @($item | Where-Object { $_.Attributes -band [IO.FileAttributes]::ReparsePoint })
    if ($item.PSIsContainer) { $links += @(Get-ChildItem -LiteralPath $path -Force -Recurse -Attributes ReparsePoint) }
    # Inspect ancestors too: an apparently local target must not live below a junction.
    $ancestor = if ($item.PSIsContainer) { $item.Parent } else { $item.Directory }
    while ($ancestor -and $ancestor.FullName.StartsWith($repo, [StringComparison]::OrdinalIgnoreCase)) {
        if ($ancestor.Attributes -band [IO.FileAttributes]::ReparsePoint) { $links += $ancestor }
        $ancestor = $ancestor.Parent
    }
    if ($links.Count -or @($running | Where-Object { $_ -eq $path -or $_.StartsWith($path + '\', [StringComparison]::OrdinalIgnoreCase) }).Count) {
        $skipped.Add($relative); continue
    }
    $selected.Add($path)
}
$records = @()
$bytes = 0L
foreach ($path in $selected) {
    $item = Get-Item -LiteralPath $path -Force
    $size = if ($item.PSIsContainer) { (Get-ChildItem -LiteralPath $path -File -Recurse -Force | Measure-Object Length -Sum).Sum } else { $item.Length }
    $bytes += $size
    $records += [ordered]@{ path=$path.Substring($repo.Length + 1); bytes=$size }
}
if ($Apply) {
    $reportPath = Join-Path $dist 'cleanup-report.json'
    $previous = if (Test-Path -LiteralPath $reportPath) { Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json } else { $null }
    $previousBytes = if (-not $previous) { 0L } elseif ($previous.cumulativeBytesRemoved) { $previous.cumulativeBytesRemoved } else { $previous.bytesRemoved }
    $previousTargets = if (-not $previous) { 0 } elseif ($previous.cumulativeRemovedTargets) { $previous.cumulativeRemovedTargets } else { @($previous.removed).Count }
    $removed = @()
    $removedBytes = 0L
    for ($index = 0; $index -lt $selected.Count; $index++) {
        $path = $selected[$index]
        try {
            # Continue through independent children if an old fixture contains
            # a denied/locked file, so accessible siblings can still be cleaned.
            Remove-Item -LiteralPath $path -Recurse -Force -ErrorAction SilentlyContinue
            if (Test-Path -LiteralPath $path) { throw 'Some generated children could not be removed.' }
            $removed += $records[$index]
            $removedBytes += $records[$index].bytes
        } catch {
            # A live Sandbox mapping or locked build output may deny deletion.
            # Keep it, do not change ACLs or stop the process that owns it.
            $relative = $path.Substring($repo.Length + 1)
            $skipped.Add($relative)
            Write-Warning "Kept locked/inaccessible artifact: $relative"
        }
    }
    New-Item -ItemType Directory -Path $dist -Force | Out-Null
    [ordered]@{ completedUtc=[DateTime]::UtcNow.ToString('o'); bytesRemoved=$removedBytes; cumulativeBytesRemoved=($previousBytes + $removedBytes);
        cumulativeRemovedTargets=($previousTargets + $removed.Count); removed=$removed; skipped=@($skipped.ToArray()) } |
        ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $reportPath -Encoding utf8
    $bytes = $removedBytes
}
$targetCount = if ($Apply) { $removed.Count } else { $selected.Count }
Write-Output ("{0}: {1} targets, {2:N2} GiB; skipped {3}." -f $(if ($Apply) {'Removed'} else {'Preview'}), $targetCount, ($bytes / 1GB), $skipped.Count)
if (-not $Apply) { Write-Output 'Run scripts/clean-artifacts.ps1 -Apply to remove only these verified generated artifacts.' }
