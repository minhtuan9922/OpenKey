[CmdletBinding()]
param(
    [ValidateSet('x64','x86')][string]$Platform = 'x64',
    [ValidateSet('Debug','Release')][string]$Configuration = 'Release'
)
$OpenKeyRepositoryRoot = Split-Path -Parent $PSScriptRoot

$ErrorActionPreference = 'Stop'
$prefix = if ($Platform -eq 'x64') { "x64\$Configuration" } else { $Configuration }
$binary = Join-Path $OpenKeyRepositoryRoot "Sources\OpenKey\win32\OpenKey\OpenKey.UI\$prefix\OpenKey.UI\OpenKey.UI.exe"
$asset = Join-Path $OpenKeyRepositoryRoot 'Sources\OpenKey\win32\OpenKey\OpenKey\icon.ico'
if (-not ('OpenKeyResourceReader' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
public static class OpenKeyResourceReader {
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
    public static extern IntPtr LoadLibraryExW(string path, IntPtr file, uint flags);
    [DllImport("kernel32.dll", SetLastError=true)] public static extern bool FreeLibrary(IntPtr module);
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
    static extern IntPtr FindResourceW(IntPtr module, IntPtr name, IntPtr type);
    [DllImport("kernel32.dll", SetLastError=true)] static extern uint SizeofResource(IntPtr module, IntPtr resource);
    [DllImport("kernel32.dll", SetLastError=true)] static extern IntPtr LoadResource(IntPtr module, IntPtr resource);
    [DllImport("kernel32.dll")] static extern IntPtr LockResource(IntPtr resource);
    public static byte[] Read(IntPtr module, int id, int type) {
        IntPtr resource = FindResourceW(module, new IntPtr(id), new IntPtr(type));
        if (resource == IntPtr.Zero) throw new Win32Exception(Marshal.GetLastWin32Error());
        uint size = SizeofResource(module, resource);
        if (size == 0 || size > 16777216) throw new InvalidOperationException("Invalid resource size");
        IntPtr loaded = LoadResource(module, resource);
        if (loaded == IntPtr.Zero) throw new Win32Exception(Marshal.GetLastWin32Error());
        IntPtr bytes = LockResource(loaded);
        if (bytes == IntPtr.Zero) throw new InvalidOperationException("Cannot access resource");
        byte[] result = new byte[(int)size]; Marshal.Copy(bytes, result, 0, result.Length); return result;
    }
}
'@
}
function Assert-Binary([bool]$Condition, [string]$Message) { if (-not $Condition) { throw $Message } }
Assert-Binary (Test-Path -LiteralPath $binary -PathType Leaf) "Build output missing: $binary"
$image = [IO.File]::ReadAllBytes($binary)
Assert-Binary ($image.Length -ge 64 -and [BitConverter]::ToUInt16($image,0) -eq 0x5a4d) 'Invalid MZ header'
$offset = [BitConverter]::ToInt32($image,60)
Assert-Binary ($offset -ge 64 -and $offset -le $image.Length - 6) 'Invalid PE offset'
$machine = if ($Platform -eq 'x64') { 0x8664 } else { 0x14c }
Assert-Binary ([BitConverter]::ToUInt32($image,$offset) -eq 0x4550 -and [BitConverter]::ToUInt16($image,$offset+4) -eq $machine) 'Wrong PE architecture'
Write-Output 'PASS: PE architecture'
# LOAD_LIBRARY_AS_DATAFILE: resource mapping only, no executable code/DllMain.
$module = [OpenKeyResourceReader]::LoadLibraryExW($binary, [IntPtr]::Zero, 2)
Assert-Binary ($module -ne [IntPtr]::Zero) 'Cannot map binary resources'
try {
    $ico = [IO.File]::ReadAllBytes($asset)
    $group = [OpenKeyResourceReader]::Read($module,101,14)
    Assert-Binary ($ico.Length -ge 6 -and $group.Length -ge 6) 'Truncated icon header'
    $count = [BitConverter]::ToUInt16($ico,4)
    Assert-Binary ($count -gt 0 -and [BitConverter]::ToUInt32($ico,0) -eq 0x10000 -and
        [BitConverter]::ToUInt32($group,0) -eq 0x10000 -and [BitConverter]::ToUInt16($group,4) -eq $count -and
        $group.Length -eq 6+14*$count -and $ico.Length -ge 6+16*$count) 'Invalid icon group'
    Write-Output 'PASS: Embedded OpenKey icon group'
    for ($index=0; $index -lt $count; $index++) {
        $entry = 6+16*$index; $groupEntry = 6+14*$index
        $size = [BitConverter]::ToUInt32($ico,$entry+8)
        $start = [BitConverter]::ToUInt32($ico,$entry+12)
        Assert-Binary ($size -gt 0 -and $start -ge 6+16*$count -and [uint64]$start+$size -le $ico.Length) 'Invalid icon frame bounds'
        $id = [BitConverter]::ToUInt16($group,$groupEntry+12)
        $frame = [OpenKeyResourceReader]::Read($module,$id,3)
        Assert-Binary ($frame.Length -eq $size -and [BitConverter]::ToUInt32($group,$groupEntry+8) -eq $size) 'Icon frame length mismatch'
        $expected = New-Object byte[] ([int]$size)
        [Array]::Copy($ico,[int]$start,$expected,0,[int]$size)
        Assert-Binary ([Convert]::ToBase64String($expected) -ceq [Convert]::ToBase64String($frame)) 'Icon pixels differ from original asset'
    }
    Write-Output "PASS: All $count embedded icon frames match original bytes"
    $manifest = [xml]([Text.Encoding]::UTF8.GetString([OpenKeyResourceReader]::Read($module,1,24)).TrimStart([char]0xfeff))
    $dpi = @($manifest.SelectNodes('//*[local-name()="dpiAwareness"]'))
    Assert-Binary ($dpi.Count -eq 1 -and $dpi[0].InnerText -eq 'PerMonitorV2') 'DPI manifest missing'
    Write-Output 'PASS: Embedded PerMonitorV2 manifest'
} finally { if (-not [OpenKeyResourceReader]::FreeLibrary($module)) { throw 'Resource mapping release failed' } }
Write-Output "UI binary resources: 4/4 passed ($Configuration $Platform); data-file mapping only, no app code or GUI executed."
