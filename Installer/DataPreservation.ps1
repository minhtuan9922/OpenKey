# Conservative source-policy guard, not a Pascal parser or installer sandbox.
# Reject unexpected edits before building; actual upgrade acceptance stays separate.
function Assert-OpenKeyDataPreservation {
    [CmdletBinding()]
    param([Parameter(Mandatory)][string]$Source, [Parameter(Mandatory)][string]$RuntimePolicy, [string]$CaretPolicy)
    if (-not $CaretPolicy) { $CaretPolicy = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'CaretPolicy.iss') -Raw }
    $allowedSections = @('Setup','Languages','CustomMessages','Tasks','Files','Icons','Run','Code')
    $sections = @([regex]::Matches($Source, '(?im)^\s*\[([^\]\r\n]+)\]\s*$') | ForEach-Object { $_.Groups[1].Value })
    if ($sections.Count -ne $allowedSections.Count -or
        @(Compare-Object ($sections | Sort-Object) ($allowedSections | Sort-Object)).Count) {
        throw 'Data preservation: installer section layout changed; review registry/delete actions.'
    }
    if ($Source -notmatch '(?im)^AppId=\{\{88759ADC-2201-4DF1-A68A-687EB37B1A02\}\s*$') {
        throw 'Data preservation: stable installer identity changed.'
    }
    foreach ($setting in @('PrivilegesRequired=lowest','DefaultDirName={localappdata}\Programs\OpenKey-Fluent','CloseApplications=no','RestartApplications=no')) {
        if ($Source -notmatch ('(?im)^' + [regex]::Escape($setting) + '\s*$')) {
            throw "Data preservation: required setting changed: $setting"
        }
    }
    $includes = @([regex]::Matches($Source, '(?im)^\s*#include\s+(.+)$') | ForEach-Object { $_.Groups[1].Value.Trim() })
    if ($includes.Count -ne 2 -or @(Compare-Object $includes @('"RuntimePolicy.iss"','"CaretPolicy.iss"') -CaseSensitive).Count -or
        ($RuntimePolicy + "`n" + $CaretPolicy) -match '(?im)^\s*#include\b') {
        throw 'Data preservation: unexpected included installer code.'
    }
    # Intentionally conservative: even a mention in a comment requires review.
    if (($Source + "`n" + $RuntimePolicy + "`n" + $CaretPolicy) -match '(?i)\b(RegWrite\w*|RegDelete\w*|RegCreate\w*|DeleteFile|DelTree|RenameFile|FileCopy|SaveStringToFile|SaveStringsToFile|CreateDir)\s*\(') {
        throw 'Data preservation: explicit registry/file mutation requires review.'
    }
    $filesBlock = [regex]::Match($Source, '(?ims)^\[Files\]\s*$(.*?)^\[').Groups[1].Value
    $files = @($filesBlock -split '\r?\n' | Where-Object { $_.Trim() -and -not $_.TrimStart().StartsWith(';') })
    $expectedFiles = @(
        'Source: "{#PayloadX64}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs; Check: UseX64',
        'Source: "{#PayloadX86}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs; Check: UseX86',
        'Source: "{#RuntimeX64}"; DestName: "vc_redist.x64.exe"; Flags: dontcopy',
        'Source: "{#RuntimeX86}"; DestName: "vc_redist.x86.exe"; Flags: dontcopy'
    )
    if ($files.Count -ne 4 -or @(Compare-Object ($files | ForEach-Object { $_.Trim() }) $expectedFiles -CaseSensitive).Count) {
        throw 'Data preservation: payload destination/actions changed.'
    }
    $runBlock = [regex]::Match($Source, '(?ims)^\[Run\]\s*$(.*?)^\[').Groups[1].Value
    $run = @($runBlock -split '\r?\n' | Where-Object { $_.Trim() -and -not $_.TrimStart().StartsWith(';') })
    $expectedRun = 'Filename: "{app}\{code:EngineName}"; Description: "{cm:StartApp}"; WorkingDir: "{app}"; Flags: postinstall nowait skipifsilent unchecked runasoriginaluser'
    if ($run.Count -ne 1 -or $run[0].Trim() -cne $expectedRun) {
        throw 'Data preservation: app launch must remain opt-in, postinstall and skipped when silent.'
    }
}
