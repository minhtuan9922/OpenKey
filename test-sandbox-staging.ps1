[CmdletBinding()]
param([Parameter(Mandatory)][string]$FixtureRoot)
$ErrorActionPreference = 'Stop'
# Only synthetic archives from test-packaging; never start guest/app/installers.
$prepare = Join-Path $PSScriptRoot 'prepare-sandbox.ps1'
$dist = Join-Path $PSScriptRoot 'dist'
$fixture = (Get-Item -LiteralPath $FixtureRoot -ErrorAction Stop).FullName
if ((Split-Path $fixture) -ne [IO.Path]::GetFullPath($dist) -or
    (Split-Path $fixture -Leaf) -notmatch '^package-tests-[0-9a-f]{32}$') {
    throw 'FixtureRoot must be a generated package-tests GUID directory inside dist.'
}
$validZip = Join-Path $fixture 'valid-x64.zip'
$zipHash = (Get-FileHash -LiteralPath $validZip -Algorithm SHA256).Hash
$unsignedRuntime = Join-Path $fixture 'unsigned-runtime.exe'
# Deliberately not a runnable PE or an installer. Native signature lookup only.
$fixtureBytes = [Text.Encoding]::ASCII.GetBytes('OpenKey unsigned runtime fixture only')
$stream = [IO.File]::Open($unsignedRuntime, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write)
try { $stream.Write($fixtureBytes, 0, $fixtureBytes.Length) } finally { $stream.Dispose() }
$runtimeHash = (Get-FileHash -LiteralPath $unsignedRuntime -Algorithm SHA256).Hash
$baseArguments = @{ ZipPath = $validZip; ExpectedSha256 = $zipHash; Platform = 'x64' }
$passed = 0
function Get-StagingDirectories {
    return @(Get-ChildItem -LiteralPath $dist -Directory -Filter 'sandbox-*' | ForEach-Object FullName | Sort-Object)
}
function Assert-Staging([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}
function Test-RejectedBeforeStaging([string]$Name, [hashtable]$Overrides, [string]$ExpectedError) {
    $arguments = $baseArguments.Clone()
    foreach ($key in $Overrides.Keys) { $arguments[$key] = $Overrides[$key] }
    $before = Get-StagingDirectories
    $message = $null
    try { $null = & $prepare @arguments } catch { $message = $_.Exception.Message }
    Assert-Staging ($null -ne $message) "Accepted invalid input: $Name"
    Assert-Staging ($message -like "*$ExpectedError*") "Unexpected rejection for ${Name}: $message"
    $after = Get-StagingDirectories
    Assert-Staging (($before -join "`n") -eq ($after -join "`n")) "Invalid input created staging: $Name"
    $script:passed++
    Write-Output "PASS staging: $Name"
}
Test-RejectedBeforeStaging 'runtime path without hash' @{ RuntimePath = $unsignedRuntime } 'Supply both'
Test-RejectedBeforeStaging 'runtime hash without path' @{ RuntimeExpectedSha256 = $runtimeHash } 'Supply both'
Test-RejectedBeforeStaging 'invalid runtime hash syntax' @{ RuntimePath = $unsignedRuntime; RuntimeExpectedSha256 = 'invalid' } 'RuntimeExpectedSha256'
Test-RejectedBeforeStaging 'wrong ZIP digest' @{ ExpectedSha256 = ('0' * 64) } 'SHA'
Test-RejectedBeforeStaging 'wrong ZIP architecture' @{ Platform = 'x86' } 'architecture'
$modifiedZip = Join-Path $fixture 'modified.zip'
Test-RejectedBeforeStaging 'tampered payload with correct ZIP digest' @{ ZipPath = $modifiedZip;
    ExpectedSha256 = (Get-FileHash -LiteralPath $modifiedZip -Algorithm SHA256).Hash } 'mismatch'
Test-RejectedBeforeStaging 'missing runtime file' @{ RuntimePath = (Join-Path $fixture 'missing-runtime.exe'); RuntimeExpectedSha256 = $runtimeHash } 'does not exist'
Test-RejectedBeforeStaging 'runtime directory' @{ RuntimePath = $fixture; RuntimeExpectedSha256 = $runtimeHash } 'must be a file'
Test-RejectedBeforeStaging 'unsigned runtime with correct digest' @{ RuntimePath = $unsignedRuntime; RuntimeExpectedSha256 = $runtimeHash } 'Authenticode'
Test-RejectedBeforeStaging 'runtime digest mismatch' @{ RuntimePath = $unsignedRuntime; RuntimeExpectedSha256 = ('0' * 64) } 'SHA-256 mismatch'
$sessions = @()
foreach ($architecture in @('x64','x86','x64')) {
    $sourceZip = Join-Path $fixture ("valid-$architecture.zip")
    $sourceHash = (Get-FileHash -LiteralPath $sourceZip -Algorithm SHA256).Hash
    $session = & $prepare -ZipPath $sourceZip -ExpectedSha256 $sourceHash.ToLowerInvariant() -Platform $architecture
    $sessionRoot = Split-Path $session.Configuration
    $inputRoot = Join-Path $sessionRoot 'Input'
    $logsRoot = Join-Path $sessionRoot 'Logs'
    $record = Get-Content -LiteralPath (Join-Path $inputRoot 'session.json') -Raw -Encoding UTF8 | ConvertFrom-Json
    $configuration = [xml](Get-Content -LiteralPath $session.Configuration -Raw)
    $folders = @($configuration.Configuration.MappedFolders.MappedFolder)
    Assert-Staging ($sessions -notcontains $sessionRoot) 'Repeated preparation reused a session'
    Assert-Staging ($null -eq $session.Runtime -and $null -eq $record.runtime) 'Unexpected runtime metadata'
    Assert-Staging ($record.architecture -eq $architecture -and $record.sha256 -ceq $sourceHash) 'Session binding mismatch'
    Assert-Staging ((Get-FileHash -LiteralPath (Join-Path $inputRoot 'beta.zip')).Hash -eq $sourceHash) 'Staged ZIP mismatch'
    Assert-Staging ((Get-FileHash -LiteralPath $sourceZip).Hash -eq $sourceHash) 'Source ZIP changed'
    Assert-Staging (-not (Test-Path -LiteralPath (Join-Path $inputRoot 'OFFLINE_RUNTIME.md'))) 'Unexpected runtime instructions'
    Assert-Staging (@(Get-ChildItem -LiteralPath $inputRoot -Filter 'vc_redist.*.exe').Count -eq 0) 'Unexpected runtime copy'
    Assert-Staging ($folders.Count -eq 2 -and $folders[0].HostFolder -eq $inputRoot -and $folders[0].ReadOnly -eq 'true' -and
        $folders[1].HostFolder -eq $logsRoot -and $folders[1].ReadOnly -eq 'false') 'Incorrect staging mappings'
    Assert-Staging ($configuration.Configuration.Networking -eq 'Disable' -and
        $configuration.Configuration.ClipboardRedirection -eq 'Disable') 'Host network/clipboard exposed'
    Assert-Staging (@(Get-ChildItem -LiteralPath $logsRoot).Count -eq 0) 'Preparation produced guest logs'
    foreach ($name in @('verify-beta.ps1','SandboxSmoke.ps1')) {
        $source = if ($name -eq 'SandboxSmoke.ps1') { Join-Path $PSScriptRoot "Sandbox\$name" } else { Join-Path $PSScriptRoot $name }
        Assert-Staging ((Get-FileHash -LiteralPath (Join-Path $inputRoot $name)).Hash -eq (Get-FileHash -LiteralPath $source).Hash) "Staged helper mismatch: $name"
    }
    $sessions += $sessionRoot
    $passed++
    Write-Output "PASS staging: fresh $architecture no-runtime session $sessionRoot"
}
Write-Output "$passed/$passed staging integration checks passed. Synthetic inputs only; no guest/app/installer launched."
