$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Sandbox\SandboxConfig.ps1')
. (Join-Path $PSScriptRoot 'Sandbox\SandboxSmoke.ps1')
. (Join-Path $PSScriptRoot 'Sandbox\OfflineRuntime.ps1')
$passed = 0
function Assert-Sandbox([bool]$Condition) { if (-not $Condition) { throw 'Sandbox assertion failed' } }
function Test-Sandbox([string]$Name, [scriptblock]$Action) {
    try { & $Action; $script:passed++; Write-Output "PASS: $Name" }
    catch { throw "Failed: $Name - $($_.Exception.Message)" }
}
function Assert-SandboxThrows([scriptblock]$Action) {
    $rejected = $false
    try { $null = & $Action } catch { $rejected = $true }
    Assert-Sandbox $rejected
}
$configuration = New-OpenKeySandboxConfiguration 'C:\Fixture\Input' 'C:\Fixture\Logs'
$options = $configuration.Configuration
$folders = @($options.MappedFolders.MappedFolder)
Test-Sandbox 'Exactly two bounded mappings' { Assert-Sandbox ($folders.Count -eq 2) }
Test-Sandbox 'Input read-only' { Assert-Sandbox ($folders[0].ReadOnly -eq 'true') }
Test-Sandbox 'Logs writable only' { Assert-Sandbox ($folders[1].ReadOnly -eq 'false') }
Test-Sandbox 'Fixed guest input' { Assert-Sandbox ($folders[0].SandboxFolder -eq 'C:\OpenKeySandboxInput') }
Test-Sandbox 'Fixed guest logs' { Assert-Sandbox ($folders[1].SandboxFolder -eq 'C:\OpenKeySandboxLogs') }
Test-Sandbox 'Input host path absolute' { Assert-Sandbox ($folders[0].HostFolder -eq 'C:\Fixture\Input') }
Test-Sandbox 'Logs host path absolute' { Assert-Sandbox ($folders[1].HostFolder -eq 'C:\Fixture\Logs') }
Test-Sandbox 'Network disabled' { Assert-Sandbox ($options.Networking -eq 'Disable') }
Test-Sandbox 'Host clipboard disabled' { Assert-Sandbox ($options.ClipboardRedirection -eq 'Disable') }
Test-Sandbox 'Audio input disabled' { Assert-Sandbox ($options.AudioInput -eq 'Disable') }
Test-Sandbox 'Video input disabled' { Assert-Sandbox ($options.VideoInput -eq 'Disable') }
Test-Sandbox 'Printer disabled' { Assert-Sandbox ($options.PrinterRedirection -eq 'Disable') }
Test-Sandbox 'Memory bounded at 4096MB' { Assert-Sandbox ($options.MemoryInMB -eq '4096') }
Test-Sandbox 'GPU default, not forced' { Assert-Sandbox ($options.vGPU -eq 'Default') }
Test-Sandbox 'Fixed command independent of host paths' {
    Assert-Sandbox ($options.LogonCommand.Command -eq '"C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File "C:\OpenKeySandboxInput\SandboxSmoke.ps1"')
}
Test-Sandbox 'Relative input rejected' { Assert-SandboxThrows { New-OpenKeySandboxConfiguration 'Input' 'C:\Logs' } }
Test-Sandbox 'Relative logs rejected' { Assert-SandboxThrows { New-OpenKeySandboxConfiguration 'C:\Input' 'Logs' } }
Test-Sandbox 'Same folder rejected' { Assert-SandboxThrows { New-OpenKeySandboxConfiguration 'C:\Same' 'C:\Same' } }
Test-Sandbox 'Case variant same folder rejected' { Assert-SandboxThrows { New-OpenKeySandboxConfiguration 'C:\Same' 'c:\same' } }
Test-Sandbox 'XML paths escaped, not injected' {
    $special = New-OpenKeySandboxConfiguration "C:\Fixture & 'input'" 'C:\Logs & output'
    $roundTrip = [xml]$special.OuterXml
    Assert-Sandbox ($roundTrip.Configuration.MappedFolders.MappedFolder[0].HostFolder -eq "C:\Fixture & 'input'" -and
        $roundTrip.Configuration.MappedFolders.MappedFolder.Count -eq 2)
}
Test-Sandbox 'Default guest account accepted' { Assert-Sandbox (Test-OpenKeySandboxAccount 'WDAGUtilityAccount') }
Test-Sandbox 'Host account rejected' { Assert-Sandbox (-not (Test-OpenKeySandboxAccount 'FixtureHostUser')) }
Test-Sandbox 'Empty account rejected' { Assert-Sandbox (-not (Test-OpenKeySandboxAccount '')) }
Test-Sandbox 'Runtime missing is not success' { Assert-Sandbox ((Get-OpenKeySmokeStatus $false $false) -eq 'prerequisite-missing') }
Test-Sandbox 'Process exited is not success' { Assert-Sandbox ((Get-OpenKeySmokeStatus $true $true) -eq 'ui-exited-early') }
Test-Sandbox 'Process alive is smoke only' { Assert-Sandbox ((Get-OpenKeySmokeStatus $true $false) -eq 'ui-running-smoke-only') }
$runtimeFixture = @{ Sha256 = ('A' * 64); SignatureStatus = 'Valid';
    SignerSubject = 'CN=Microsoft Corporation, O=Microsoft Corporation, L=Redmond, S=Washington, C=US';
    FileVersion = '14.51.36247.0'; ProductName = 'Microsoft Visual C++ v14 Redistributable (x64) - 14.51.36247' }
Test-Sandbox 'Valid runtime at minimum' { Assert-OpenKeyRuntimeMetadata $runtimeFixture x64 ([version]'14.51.36247.0') ('A' * 64) }
Test-Sandbox 'Valid newer runtime and lowercase hash' { Assert-OpenKeyRuntimeMetadata $runtimeFixture x64 ([version]'14.51.36231.0') ('a' * 64) }
Test-Sandbox 'Valid x86 runtime' {
    $candidate = $runtimeFixture.Clone(); $candidate.ProductName = 'Microsoft Visual C++ v14 Redistributable (x86) - 14.51.36247'
    Assert-OpenKeyRuntimeMetadata $candidate x86 ([version]'14.51.36231.0') ('A' * 64)
}
foreach ($mutation in @(
    @{ Field = 'SignatureStatus'; Value = 'NotSigned' },
    @{ Field = 'SignatureStatus'; Value = 'HashMismatch' },
    @{ Field = 'SignatureStatus'; Value = 'UnknownError' },
    @{ Field = 'SignerSubject'; Value = 'CN=Other, O=Microsoft Corporation' },
    @{ Field = 'SignerSubject'; Value = 'CN=Microsoft Corporation' },
    @{ Field = 'SignerSubject'; Value = 'CN=Microsoft Corporation Evil, O=Microsoft Corporation' },
    @{ Field = 'SignerSubject'; Value = 'CN=Microsoft Corporation, O=Microsoft Corporation Evil' },
    @{ Field = 'FileVersion'; Value = '14.50.0.0' },
    @{ Field = 'FileVersion'; Value = 'invalid' },
    @{ Field = 'FileVersion'; Value = '' },
    @{ Field = 'ProductName'; Value = 'Microsoft Visual C++ v14 Redistributable (x86) - 14.51.36247' },
    @{ Field = 'ProductName'; Value = 'Other runtime (x64)' },
    @{ Field = 'Sha256'; Value = 'invalid' },
    @{ Field = 'Sha256'; Value = ('B' * 64) }
)) {
    Test-Sandbox ("Runtime rejects {0}: {1}" -f $mutation.Field, $mutation.Value) {
        $candidate = $runtimeFixture.Clone(); $candidate[$mutation.Field] = $mutation.Value
        Assert-SandboxThrows { Assert-OpenKeyRuntimeMetadata $candidate x64 ([version]'14.51.36231.0') ('A' * 64) }
    }
}
Test-Sandbox 'Runtime rejects higher package minimum' { Assert-SandboxThrows { Assert-OpenKeyRuntimeMetadata $runtimeFixture x64 ([version]'14.52.0.0') ('A' * 64) } }
Test-Sandbox 'Runtime rejects invalid minimum' { Assert-SandboxThrows { Assert-OpenKeyRuntimeMetadata $runtimeFixture x64 ([version]'13.0') ('A' * 64) } }
Test-Sandbox 'Runtime rejects missing minimum' { Assert-SandboxThrows { Assert-OpenKeyRuntimeMetadata $runtimeFixture x64 $null ('A' * 64) } }
Test-Sandbox 'Runtime rejects invalid expected hash' { Assert-SandboxThrows { Assert-OpenKeyRuntimeMetadata $runtimeFixture x64 ([version]'14.51.36231.0') 'invalid' } }
Test-Sandbox 'Runtime rejects missing metadata' { Assert-SandboxThrows { Assert-OpenKeyRuntimeMetadata $null x64 ([version]'14.51.36231.0') ('A' * 64) } }
Test-Sandbox 'Runtime arguments optional as a pair' { Assert-OpenKeyRuntimeArguments '' '' }
Test-Sandbox 'Runtime arguments accepted together' { Assert-OpenKeyRuntimeArguments 'fixture.exe' ('A' * 64) }
Test-Sandbox 'Runtime path alone rejected' { Assert-SandboxThrows { Assert-OpenKeyRuntimeArguments 'fixture.exe' '' } }
Test-Sandbox 'Runtime hash alone rejected' { Assert-SandboxThrows { Assert-OpenKeyRuntimeArguments '' ('A' * 64) } }
Test-Sandbox 'Runtime directory rejected' { Assert-SandboxThrows { Get-OpenKeyRuntimeMetadata (Join-Path $PSScriptRoot 'Sandbox') } }
Test-Sandbox 'Scripts parse on current shell' {
    foreach ($name in @('prepare-sandbox.ps1','Sandbox\SandboxConfig.ps1','Sandbox\SandboxSmoke.ps1','Sandbox\OfflineRuntime.ps1','test-sandbox-staging.ps1','test-sandbox.ps1')) {
        $tokens = $null; $parseErrors = $null
        $null = [Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot $name), [ref]$tokens, [ref]$parseErrors)
        Assert-Sandbox (@($parseErrors).Count -eq 0)
    }
}
Write-Output "Sandbox config/guard/status: $passed/$passed passed; pure XML/helpers, no Sandbox/app launched, no registry writes."
