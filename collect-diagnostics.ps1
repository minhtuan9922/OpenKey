[CmdletBinding()]
param([Parameter(Mandatory)] [string]$AppDirectory)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Diagnostics\SupportReport.ps1')
$report = Get-OpenKeySupportReport -AppDirectory $AppDirectory
if (@($report.files | Where-Object { $_.status -eq 'read' }).Count -eq 0) {
    throw 'No readable OpenKey executables found. Select the installed app or extracted beta directory.'
}
$output = Join-Path $PSScriptRoot ('dist\diagnostics-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $output | Out-Null
$reportPath = Join-Path $output 'support-report.json'
$report | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $reportPath -Encoding UTF8
Write-Output "Diagnostic report: $reportPath"
Write-Output 'Review this file before sharing. No data was uploaded; no app was launched or stopped.'
