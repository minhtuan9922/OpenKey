function Get-OpenKeyAppRuntimeInputs {
    $inputs = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'AppRuntimeInputs.json') -Raw | ConvertFrom-Json
    if ($inputs.version -ne '2.5.1.0' -or $inputs.packageFamily -ne 'Microsoft.WindowsAppRuntime.2_8wekyb3d8bbwe') {
        throw 'Windows App Runtime pin does not match the native runtime probe.'
    }
    foreach ($arch in @('x64','x86')) {
        if ($inputs.$arch.sha256 -notmatch '^[A-F0-9]{64}$' -or
            $inputs.$arch.url -cnotmatch ('^https://download\.microsoft\.com/download/[a-f0-9-]+/WindowsAppRuntimeInstall-' + $arch + '\.exe$')) {
            throw 'Invalid pinned Microsoft Windows App Runtime URL/hash.'
        }
    }
    return $inputs
}

function Assert-OpenKeyAppRuntime([string]$Path, [string]$ExpectedHash) {
    $file = Get-Item -LiteralPath $Path -ErrorAction Stop
    $signature = Get-AuthenticodeSignature -LiteralPath $file.FullName
    if ((Get-FileHash -LiteralPath $file.FullName).Hash -ne $ExpectedHash -or
        $signature.Status -ne 'Valid' -or
        $signature.SignerCertificate.Subject -notmatch '(^|,\s*)CN=Microsoft Corporation(,|$)' -or
        $signature.SignerCertificate.Subject -notmatch '(^|,\s*)O=Microsoft Corporation(,|$)' -or
        $file.VersionInfo.ProductName -ne 'Windows App SDK' -or $file.VersionInfo.ProductVersion -ne '2.5') {
        throw "Windows App Runtime requires pinned bytes and valid Microsoft signature: $Path"
    }
}
