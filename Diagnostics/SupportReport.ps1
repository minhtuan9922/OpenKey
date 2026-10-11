function Get-OpenKeySupportReport {
    [CmdletBinding()]
    param([Parameter(Mandatory)] [string]$AppDirectory)
    if ($AppDirectory.StartsWith('\\')) { throw 'A local application directory is required.' }
    $root = Get-Item -LiteralPath $AppDirectory -ErrorAction Stop
    if ($root.PSProvider.Name -ne 'FileSystem' -or -not $root.PSIsContainer -or
        ($root.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        throw 'Select a local, non-linked application directory.'
    }
    $files = @('OpenKey.exe', 'OpenKey64.exe', 'OpenKey32.exe', 'OpenKeyUpdate.exe',
        'OpenKey.UI.exe', 'OpenKey.UI\OpenKey.UI.exe')
    $entries = foreach ($relative in $files) {
        $entry = [ordered]@{ file = $relative; status = 'missing' }
        $path = Join-Path $root.FullName $relative
        try {
            # Do not follow junctions/symlinks, even for allowlisted file names.
            $linked = $false
            $candidate = $path
            while ($candidate -ne $root.FullName) {
                $item = Get-Item -LiteralPath $candidate -Force -ErrorAction SilentlyContinue
                if ($item -and ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)) { $linked = $true }
                $candidate = Split-Path -Parent $candidate
            }
            if ($linked) { $entry.status = 'skipped-link' }
            elseif (Test-Path -LiteralPath $path -PathType Leaf) {
                $info = Get-Item -LiteralPath $path -ErrorAction Stop
                $entry.sha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256 -ErrorAction Stop).Hash
                $entry.bytes = $info.Length
                $entry.fileVersion = [Diagnostics.FileVersionInfo]::GetVersionInfo($path).FileVersion
                $entry.status = 'read'
            }
        } catch { $entry = [ordered]@{ file = $relative; status = 'unreadable' } }
        [pscustomobject]$entry
    }
    [ordered]@{
        schemaVersion = 1; collectedUtc = [DateTime]::UtcNow.ToString('o')
        osVersion = [Environment]::OSVersion.Version.ToString()
        os64Bit = [Environment]::Is64BitOperatingSystem
        shellVersion = $PSVersionTable.PSVersion.ToString()
        scope = 'Local executable metadata only; read is not proof of executable validity or publisher identity.'
        privacy = 'No absolute app path, user name, registry, macro, clipboard, typed text, event log or crash dump collected. No upload.'
        files = @($entries)
    }
}
