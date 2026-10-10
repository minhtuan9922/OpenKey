$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Sandbox\SandboxConfig.ps1')
. (Join-Path $PSScriptRoot 'Sandbox\SandboxSmoke.ps1')
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
Test-Sandbox 'Scripts parse on current shell' {
    foreach ($name in @('prepare-sandbox.ps1','Sandbox\SandboxConfig.ps1','Sandbox\SandboxSmoke.ps1','test-sandbox.ps1')) {
        $tokens = $null; $parseErrors = $null
        $null = [Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot $name), [ref]$tokens, [ref]$parseErrors)
        Assert-Sandbox (@($parseErrors).Count -eq 0)
    }
}
Write-Output "Sandbox config/guard/status: $passed/$passed passed; pure XML/helpers, no Sandbox/app launched, no registry writes."
