function New-OpenKeySandboxConfiguration([string]$InputDirectory, [string]$LogsDirectory) {
    foreach ($path in @($InputDirectory, $LogsDirectory)) {
        if (-not [IO.Path]::IsPathRooted($path)) { throw 'Mapped folders must use absolute paths.' }
    }
    if ([IO.Path]::GetFullPath($InputDirectory) -eq [IO.Path]::GetFullPath($LogsDirectory)) {
        throw 'Read-only input and writable logs must be separate folders.'
    }
    $document = [xml]@'
<Configuration>
  <vGPU>Default</vGPU>
  <Networking>Disable</Networking>
  <AudioInput>Disable</AudioInput>
  <VideoInput>Disable</VideoInput>
  <PrinterRedirection>Disable</PrinterRedirection>
  <ClipboardRedirection>Disable</ClipboardRedirection>
  <MemoryInMB>4096</MemoryInMB>
  <MappedFolders>
    <MappedFolder><HostFolder/><SandboxFolder>C:\OpenKeySandboxInput</SandboxFolder><ReadOnly>true</ReadOnly></MappedFolder>
    <MappedFolder><HostFolder/><SandboxFolder>C:\OpenKeySandboxLogs</SandboxFolder><ReadOnly>false</ReadOnly></MappedFolder>
  </MappedFolders>
  <LogonCommand><Command>"C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File "C:\OpenKeySandboxInput\SandboxSmoke.ps1"</Command></LogonCommand>
</Configuration>
'@
    $document.Configuration.MappedFolders.MappedFolder[0].HostFolder = [IO.Path]::GetFullPath($InputDirectory)
    $document.Configuration.MappedFolders.MappedFolder[1].HostFolder = [IO.Path]::GetFullPath($LogsDirectory)
    return $document
}
