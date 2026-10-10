$ErrorActionPreference = 'Stop'
$root = Join-Path $PSScriptRoot ('dist\notices-tests-' + [guid]::NewGuid().ToString('N'))
$cache = Join-Path $root 'cache'
$package = Join-Path $cache 'test.component\1.0.0'
New-Item -ItemType Directory -Path $package -Force | Out-Null
$spec = '<package><metadata><id>Test.Component</id><version>1.0.0</version><authors>Fixture</authors><license type="file">LICENSE.txt</license></metadata></package>'
$specPath = Join-Path $package 'test.component.nuspec'
[IO.File]::WriteAllText($specPath, $spec)
[IO.File]::WriteAllText((Join-Path $package 'LICENSE.txt'), 'fixture-license')
[IO.File]::WriteAllText((Join-Path $package 'NOTICE.txt'), 'fixture-notice')
$assets = @{ packageFolders = @{ $cache = @{} }; libraries = @{
    'Test.Component/1.0.0' = @{ type = 'package'; path = 'test.component/1.0.0'; files = @('LICENSE.txt', 'NOTICE.txt') }
} }
$assetsPath = Join-Path $root 'project.assets.json'
$assets | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $assetsPath -Encoding UTF8
$collector = Join-Path $PSScriptRoot 'collect-third-party.ps1'
$output = Join-Path $root 'valid'
& $collector -OutputDirectory $output -AssetsPath $assetsPath
if ([IO.File]::ReadAllText((Join-Path $output 'Test.Component-1.0.0\LICENSE.txt')) -ne 'fixture-license' -or
    [IO.File]::ReadAllText((Join-Path $output 'Test.Component-1.0.0\NOTICE.txt')) -ne 'fixture-notice') { throw 'Notice copy changed content.' }
$inventory = Get-Content (Join-Path $output 'inventory.json') -Raw | ConvertFrom-Json
if (@($inventory.packages).Count -ne 1 -or $inventory.packages[0].id -ne 'Test.Component') { throw 'Bad inventory metadata.' }
Write-Host 'PASS valid-copy-and-inventory'
$cases = @('existing-output', 'missing-license', 'unsafe-notice', 'outside-dist')
foreach ($case in $cases) {
    [IO.File]::WriteAllText($specPath, $spec)
    $assets.libraries['Test.Component/1.0.0'].files = @('LICENSE.txt', 'NOTICE.txt')
    $target = Join-Path $root $case
    if ($case -eq 'existing-output') { $target = $output }
    if ($case -eq 'missing-license') { [IO.File]::WriteAllText($specPath, $spec.Replace('LICENSE.txt', 'missing.txt')) }
    if ($case -eq 'unsafe-notice') { $assets.libraries['Test.Component/1.0.0'].files += '../LICENSE.txt' }
    if ($case -eq 'outside-dist') { $target = Join-Path $PSScriptRoot ('forbidden-notices-' + [guid]::NewGuid().ToString('N')) }
    $assets | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $assetsPath -Encoding UTF8
    $rejected = $false
    try { & $collector -OutputDirectory $target -AssetsPath $assetsPath } catch { $rejected = $true }
    if (-not $rejected) { throw "Invalid notices input accepted: $case" }
    if ($case -eq 'outside-dist' -and (Test-Path -LiteralPath $target)) { throw 'Created output outside dist.' }
    Write-Host "PASS $case"
}
Write-Host "5/5 notices tests passed; synthetic fixtures retained: $root"
