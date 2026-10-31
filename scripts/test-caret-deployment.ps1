[CmdletBinding()]
param([string]$ComponentRoot)
$ErrorActionPreference = 'Stop'
if (-not $ComponentRoot) { $ComponentRoot = Join-Path (Split-Path -Parent $PSScriptRoot) 'dist\caret-component\Release' }
$ComponentRoot = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($ComponentRoot)
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class CaretPayloadReader {
  [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)] static extern IntPtr LoadLibraryEx(string path, IntPtr file, uint flags);
  [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)] static extern IntPtr FindResource(IntPtr module, IntPtr name, IntPtr type);
  [DllImport("kernel32.dll")] static extern IntPtr LoadResource(IntPtr module, IntPtr resource);
  [DllImport("kernel32.dll")] static extern IntPtr LockResource(IntPtr resource);
  [DllImport("kernel32.dll")] static extern uint SizeofResource(IntPtr module, IntPtr resource);
  [DllImport("kernel32.dll")] static extern bool FreeLibrary(IntPtr module);
  public static byte[] Read(string path, int id) {
    return ReadType(path, id, 10);
  }
  public static byte[] ReadType(string path, int id, int type) {
    var module = LoadLibraryEx(path, IntPtr.Zero, 0x22); // Data/image resource only; no executable code runs.
    if(module == IntPtr.Zero) throw new Exception("Cannot map helper resources");
    try {
      var resource = FindResource(module, new IntPtr(id), new IntPtr(type));
      var size = SizeofResource(module, resource);
      var data = LockResource(LoadResource(module, resource));
      if(size == 0 || data == IntPtr.Zero) throw new Exception("Missing embedded payload");
      var bytes = new byte[checked((int)size)]; Marshal.Copy(data, bytes, 0, bytes.Length); return bytes;
    } finally { FreeLibrary(module); }
  }
}
'@
function Read-Registration {
    $values = @()
    foreach ($view in @([Microsoft.Win32.RegistryView]::Registry32, [Microsoft.Win32.RegistryView]::Registry64)) {
        $base = [Microsoft.Win32.RegistryKey]::OpenBaseKey([Microsoft.Win32.RegistryHive]::LocalMachine, $view)
        $key = $null
        try {
            $key = $base.OpenSubKey('SOFTWARE\Classes\CLSID\{727F9E3B-0491-4D5C-B33D-64D59BE28A70}\InprocServer32')
            $values += if ($key) { [string]$key.GetValue('') } else { '' }
        } finally { if ($key) { $key.Dispose() }; $base.Dispose() }
    }
    return ($values -join '|')
}
function Invoke-Helper([string]$Path, [string]$Argument) {
    $process = Start-Process -FilePath $Path -ArgumentList $Argument -WindowStyle Hidden -PassThru -Wait
    return $process.ExitCode
}
$before = Read-Registration
$principal = [Security.Principal.WindowsPrincipal]::new([Security.Principal.WindowsIdentity]::GetCurrent())
$isAdmin = $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
$checks = 0
foreach ($arch in @('x64','x86')) {
    $helper = Join-Path $ComponentRoot "helper\$arch\OpenKey.CaretSetup.exe"
    if ((Invoke-Helper $helper '--verify-payload') -ne 0) { throw "$arch helper rejected its embedded DLL architectures" }; $checks++
    $manifest = [xml]([Text.Encoding]::UTF8.GetString([CaretPayloadReader]::ReadType($helper, 1, 24)).Trim([char[]]@(0,0xFEFF)))
    $level = $manifest.SelectSingleNode('//*[local-name()="requestedExecutionLevel"]')
    if ($level.level -ne 'asInvoker' -or $level.uiAccess -ne 'false') { throw "$arch helper must never elevate for status checks" }; $checks++
    $status = Invoke-Helper $helper '--status'
    if ($status -notin @(0,1168)) { throw "$arch readonly status returned unexpected code $status" }; $checks++
    $ownership = Invoke-Helper $helper '--owns'
    if ($ownership -notin @(0,1168,1306)) { throw "$arch readonly ownership returned unexpected code $ownership" }; $checks++
    foreach ($payload in @(@{Name='x64';Id=101}, @{Name='x86';Id=102})) {
        $hash = [Security.Cryptography.SHA256]::Create()
        try { $embedded = [BitConverter]::ToString($hash.ComputeHash([CaretPayloadReader]::Read($helper, $payload.Id))).Replace('-','') }
        finally { $hash.Dispose() }
        if ($embedded -ne (Get-FileHash -LiteralPath (Join-Path $ComponentRoot "$($payload.Name)\OpenKey.TextService.dll")).Hash) { throw "$arch helper payload differs from bundled DLL" }; $checks++
    }
    if ((Invoke-Helper $helper '--invalid-command') -ne 87) { throw 'Unknown helper operation was accepted' }; $checks++
    if (-not $isAdmin) {
        foreach ($operation in @('--install','--uninstall')) {
            $exitCode = Invoke-Helper $helper $operation
            if ($exitCode -ne 5) { throw "$arch $operation without admin returned $exitCode instead of access denied" }; $checks++
        }
    }
}
if ((Read-Registration) -cne $before) { throw 'Read-only/unelevated helper tests changed registration' }; $checks++
Write-Output "$checks caret deployment checks passed; embedded payloads match and registration was unchanged. No UAC or installation performed."
