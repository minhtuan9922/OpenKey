function Assert-OpenKeyReleaseTag([string]$Tag, $Metadata) {
    if ($Tag -cnotmatch '^v(\d{1,3})\.(\d{1,3})\.(\d{1,3})$') { throw 'Expected a release tag vMAJOR.MINOR.PATCH.' }
    $major = [int]$Matches[1]; $minor = [int]$Matches[2]; $patch = [int]$Matches[3]
    if ($major -gt 255 -or $minor -gt 255 -or $patch -gt 255 -or $Tag -cne "v$major.$minor.$patch") {
        throw 'Version components must be canonical integers in 0..255.'
    }
    $packed = $major + ($minor -shl 8) + ($patch -shl 16)
    $code = $Metadata.latestWinVersion.versionCode
    if ($Metadata.latestWinVersion.versionName -cne $Tag -or
        -not ($code -is [int] -or $code -is [long]) -or $code -ne $packed) {
        throw 'Release tag does not match version.json.'
    }
    return "$major.$minor.$patch"
}
