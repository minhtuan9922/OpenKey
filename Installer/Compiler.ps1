# Resolve an existing compiler only. Never downloads/installs tools or changes PATH.
function Resolve-OpenKeySetupCompiler([string]$Path) {
    if ($Path) { return (Resolve-Path -LiteralPath $Path -ErrorAction Stop).Path }
    $taskCommand = Get-Command ISCC.exe -ErrorAction SilentlyContinue
    $taskCandidates = @(
        $env:OPENKEY_ISCC_PATH,
        (Join-Path $PSScriptRoot '..\dist\inno-6.7.3-portable\compiler\ISCC.exe'),
        $(if ($taskCommand) { $taskCommand.Source }),
        $(if (${env:ProgramFiles(x86)}) { Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 6\ISCC.exe' }),
        $(if ($env:ProgramFiles) { Join-Path $env:ProgramFiles 'Inno Setup 7\ISCC.exe' })
    )
    foreach ($taskCandidate in $taskCandidates) {
        if ($taskCandidate -and (Test-Path -LiteralPath $taskCandidate -PathType Leaf)) {
            return (Resolve-Path -LiteralPath $taskCandidate).Path
        }
    }
    throw 'Inno Setup 6.7.3+ required. Supply -IsccPath or OPENKEY_ISCC_PATH; see INSTALLER.md.'
}
