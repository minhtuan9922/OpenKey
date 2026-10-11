function Assert-OpenKeyRuntimeArguments([string]$RuntimePath, [string]$RuntimeExpectedSha256) {
    if ([bool]$RuntimePath -ne [bool]$RuntimeExpectedSha256) {
        throw 'Supply both RuntimePath and RuntimeExpectedSha256, or neither.'
    }
}

# Pure policy; tests supply metadata, never execute a redistributable.
function Assert-OpenKeyRuntimeMetadata {
    param($Metadata, [ValidateSet('x64','x86')][string]$Platform,
        [version]$MinimumVersion, [string]$ExpectedSha256)
    if ($MinimumVersion -lt [version]'14.0') { throw 'Invalid runtime minimum version.' }
    if ($ExpectedSha256 -notmatch '^[0-9A-Fa-f]{64}$' -or
        [string]$Metadata.Sha256 -notmatch '^[0-9A-Fa-f]{64}$' -or
        $Metadata.Sha256 -ne $ExpectedSha256) { throw 'Runtime SHA-256 mismatch.' }
    if ($Metadata.SignatureStatus -ne 'Valid' -or
        $Metadata.SignerSubject -notmatch '(^|,\s*)CN=Microsoft Corporation(,|$)' -or
        $Metadata.SignerSubject -notmatch '(^|,\s*)O=Microsoft Corporation(,|$)') {
        throw 'Runtime requires valid Microsoft Authenticode.'
    }
    $version = $null
    if (-not [version]::TryParse([string]$Metadata.FileVersion, [ref]$version) -or
        $version -lt $MinimumVersion) { throw 'Runtime is too old or has invalid version metadata.' }
    if ($Metadata.ProductName -notmatch
        ('^Microsoft Visual C\+\+ v14 Redistributable \(' + $Platform + '\)( - [0-9.]+)?$')) {
        throw 'Unsupported runtime product or architecture.'
    }
}

function Get-OpenKeyRuntimeMetadata([string]$Path) {
    $file = Get-Item -LiteralPath $Path -ErrorAction Stop
    if ($file.PSIsContainer) { throw 'RuntimePath must be a file.' }
    $signature = Get-AuthenticodeSignature -LiteralPath $file.FullName -ErrorAction Stop
    [pscustomobject]@{
        Path = $file.FullName
        FileVersion = $file.VersionInfo.FileVersion
        ProductName = $file.VersionInfo.ProductName
        SignatureStatus = [string]$signature.Status
        SignerSubject = if ($signature.SignerCertificate) { $signature.SignerCertificate.Subject } else { '' }
        Sha256 = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256 -ErrorAction Stop).Hash
    }
}
