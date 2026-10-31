
$OpenKeyRepositoryRoot = Split-Path -Parent $PSScriptRoot
$ErrorActionPreference = 'Stop'
# Source-only inventory guard: never open, export, restore, or enumerate registry.
$inventory = Get-Content -LiteralPath (Join-Path $OpenKeyRepositoryRoot 'Tests\RegistryInventory.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$win32 = Join-Path $OpenKeyRepositoryRoot 'Sources\OpenKey\win32'
$engineRoot = Join-Path $win32 'OpenKey\OpenKey'
$uiRoot = Join-Path $win32 'OpenKey\OpenKey.UI'
$passed = 0
function Assert-Inventory([bool]$Condition, [string]$Message) { if (-not $Condition) { throw $Message } }
function Test-Inventory([string]$Name, [scriptblock]$Action) {
    & $Action
    $script:passed++
    Write-Output "PASS registry inventory: $Name"
}
function Compare-InventoryNames($Actual, $Expected, [string]$Label) {
    $left = @($Actual | Sort-Object -Unique)
    $right = @($Expected | Sort-Object -Unique)
    Assert-Inventory (($left -join '|') -ceq ($right -join '|')) "Source/inventory mismatch ($Label): actual=$($left -join ',') expected=$($right -join ',')"
}
$primary = $inventory.locations[0]
$startup = $inventory.locations[1]
$names = @($primary.groups | ForEach-Object { $_.names })
Test-Inventory 'schema and bounded locations' {
    Assert-Inventory ($inventory.schemaVersion -eq 1 -and $inventory.locations.Count -eq 2) 'Unsupported inventory schema'
    Assert-Inventory ($primary.hive -ceq 'HKEY_CURRENT_USER' -and $primary.path -ceq 'SOFTWARE\TuyenMai\OpenKey' -and
        $startup.hive -ceq 'HKEY_CURRENT_USER' -and $startup.path -ceq 'Software\Microsoft\Windows\CurrentVersion\Run') 'Registry locations changed'
}
Test-Inventory 'unique value names and type/scope groups' {
    Assert-Inventory ($names.Count -eq 46 -and @($names | Sort-Object -Unique).Count -eq 46) 'Missing or duplicate application value'
    $groupKinds = @($primary.groups | ForEach-Object { $_.scope + ':' + $_.type })
    Compare-InventoryNames $groupKinds @('shared-preferences:REG_DWORD','legacy-only:REG_DWORD','shared-data:REG_BINARY','fluent-only:REG_DWORD','fluent-only:REG_BINARY') 'groups'
    Assert-Inventory ($primary.groups.Count -eq 5) 'Duplicate scope/type group'
}
$store = Get-Content -LiteralPath (Join-Path $engineRoot 'SettingsServiceStore.cpp') -Raw -Encoding UTF8
$shared = @($primary.groups | Where-Object scope -eq 'shared-preferences' | ForEach-Object { $_.names })
Test-Inventory 'all shared fields written' {
    $actual = @([regex]::Matches($store, '\{\s*L"([^"]+)"\s*,\s*&SettingsService::Preferences::') | ForEach-Object { $_.Groups[1].Value })
    Assert-Inventory ($actual.Count -eq 37) 'Shared writer field count changed'
    Compare-InventoryNames $actual $shared 'shared writer'
}
Test-Inventory 'all shared fields read' {
    $actual = @([regex]::Matches($store, 'readDword\(key,\s*L"([^"]+)"') | ForEach-Object { $_.Groups[1].Value })
    Assert-Inventory ($actual.Count -eq 37) 'Shared reader field count changed'
    Compare-InventoryNames $actual $shared 'shared reader'
}
# Honor repository ignores: generated WinUI/SDK/build trees are not app source.
$sourcePaths = & git -C $OpenKeyRepositoryRoot -c core.quotepath=false ls-files --cached --others --exclude-standard -- Sources/OpenKey/win32
if ($LASTEXITCODE -ne 0) { throw 'Cannot enumerate repository source for registry inventory.' }
$sourceFiles = @($sourcePaths | Where-Object { [IO.Path]::GetExtension($_) -in @('.cpp','.h') } |
    ForEach-Object { Get-Item -LiteralPath (Join-Path $OpenKeyRepositoryRoot $_) })
$allSource = ($sourceFiles | ForEach-Object { Get-Content -LiteralPath $_.FullName -Raw -Encoding UTF8 }) -join "`n"
$allDwords = @($primary.groups | Where-Object type -eq 'REG_DWORD' | Where-Object scope -ne 'fluent-only' | ForEach-Object { $_.names })
Test-Inventory 'legacy literal and macro DWORD keys covered' {
    $actual = @($shared)
    $actual += @([regex]::Matches($allSource, '(?:loadSetting|saveSetting)\(L"([^"]+)"') | ForEach-Object { $_.Groups[1].Value })
    $actual += @([regex]::Matches($allSource, 'APP_(?:GET|SET)_DATA\(\s*([A-Za-z_][A-Za-z_0-9]*)\s*,') | ForEach-Object { $_.Groups[1].Value } | Where-Object { $_ -ne 'KEY' })
    $actual += @([regex]::Matches($allSource, '(?:getRegInt|setRegInt)\(\s*(?:_T|TEXT)\("([^"]+)"\)') | ForEach-Object { $_.Groups[1].Value })
    Compare-InventoryNames $actual $allDwords 'legacy DWORD coverage'
}
Test-Inventory 'shared binary keys covered' {
    $actual = @([regex]::Matches($allSource, '(?:getRegBinary|setRegBinary)\(\s*_T\("([^"]+)"\)') | ForEach-Object { $_.Groups[1].Value })
    $actual += @([regex]::Matches($allSource, 'MacroDataValue\[\]\s*=\s*L"([^"]+)"') | ForEach-Object { $_.Groups[1].Value })
    $expected = @($primary.groups | Where-Object scope -eq 'shared-data' | ForEach-Object { $_.names })
    Compare-InventoryNames $actual $expected 'shared binary coverage'
}
$observedUi = @()
foreach ($file in @('UiLanguageSettings.h','UiThemeSettings.h','UiWindowSizeSettings.h')) {
    $text = Get-Content -LiteralPath (Join-Path $uiRoot $file) -Raw -Encoding UTF8
    foreach ($match in [regex]::Matches($text, '(\w*ValueName)\[\]\s*=\s*L"([^"]+)"')) {
        $symbol = $match.Groups[1].Value
        $name = $match.Groups[2].Value
        $write = [regex]::Match($text, 'RegSetValueExW\(key,\s*' + [regex]::Escape($symbol) + ',\s*0,\s*(REG_\w+)')
        Assert-Inventory $write.Success "Missing typed UI writer: $name"
        $observedUi += [pscustomobject]@{ Name = $name; Type = $write.Groups[1].Value }
    }
}
Test-Inventory 'Fluent DWORD keys and types covered' {
    Compare-InventoryNames @($observedUi | Where-Object Type -eq 'REG_DWORD' | ForEach-Object Name) @($primary.groups | Where-Object scope -eq 'fluent-only' | Where-Object type -eq 'REG_DWORD' | ForEach-Object { $_.names }) 'Fluent DWORD'
}
Test-Inventory 'Fluent binary keys and types covered' {
    Compare-InventoryNames @($observedUi | Where-Object Type -eq 'REG_BINARY' | ForEach-Object Name) @($primary.groups | Where-Object scope -eq 'fluent-only' | Where-Object type -eq 'REG_BINARY' | ForEach-Object { $_.names }) 'Fluent binary'
}
Test-Inventory 'registry provider files stay explicit' {
    $providers = @($sourceFiles | Where-Object {
        (Get-Content -LiteralPath $_.FullName -Raw -Encoding UTF8) -match '\bReg(?:OpenKey|GetValue|CreateKey|QueryValue|SetValue|DeleteValue)'
    } | ForEach-Object { $_.FullName.Substring($win32.Length + 1).Replace('\','/') })
    Compare-InventoryNames $providers @('OpenKey/OpenKey/OpenKeyHelper.cpp','OpenKey/OpenKey/MacroService.cpp','OpenKey/OpenKey/SettingsServiceStore.cpp',
        'OpenKey/OpenKey.UI/UiLanguageSettings.h','OpenKey/OpenKey.UI/UiThemeSettings.h','OpenKey/OpenKey.UI/UiWindowSizeSettings.h',
        'OpenKey/OpenKey.TextService/CaretTextService.cpp','OpenKey/OpenKey.TextService/Registration.cpp',
        'OpenKey/OpenKey.TextService/CaretDeployment.h','OpenKey/OpenKey.TextService/CaretSetup.cpp') 'providers'
    foreach ($file in $providers) {
        $text = Get-Content -LiteralPath (Join-Path $win32 $file) -Raw -Encoding UTF8
        if ($file -eq 'OpenKey/OpenKey.TextService/CaretSetup.cpp') {
            Assert-Inventory ($text.Contains('SOFTWARE\\Microsoft\\Windows\\CurrentVersion') -and $text.Contains('ProgramFilesDir') -and
                $text.Contains('RRF_SUBKEY_WOW6464KEY') -and $text -notmatch '\bReg(?:CreateKey|SetValue|Delete)') 'Caret helper may only read native Program Files metadata directly'
            continue
        }
        if ($file -eq 'OpenKey/OpenKey.TextService/CaretDeployment.h') {
            $expectedPath = $inventory.experimentalTextServiceRegistration.comPath.Replace('\','\\')
            Assert-Inventory ($text.Contains($expectedPath) -and $text.Contains('HKEY_LOCAL_MACHINE') -and
                $text -notmatch '\bReg(?:CreateKey|SetValue|Delete)') 'Caret deployment must only read its own machine COM registration'
            continue
        }
        if ($file -eq 'OpenKey/OpenKey.TextService/Registration.cpp') {
            $expectedPath = $inventory.experimentalTextServiceRegistration.comPath.Replace('\','\\')
            Assert-Inventory ($text.Contains($expectedPath) -and $text.Contains('HKEY_LOCAL_MACHINE') -and
                $text.Contains('RegDeleteTreeW(HKEY_LOCAL_MACHINE, ClassPath)')) 'TSF COM registration scope changed'
            continue
        }
        Assert-Inventory ($text.Contains('SOFTWARE\\TuyenMai\\OpenKey') -and $text.Contains('HKEY_CURRENT_USER')) "Provider root changed: $file"
    }
}
Test-Inventory 'experimental TSF registration stays separate from application data' {
    $service = Get-Content -LiteralPath (Join-Path $win32 'OpenKey/OpenKey.TextService/CaretTextService.cpp') -Raw -Encoding UTF8
    Assert-Inventory ($service -notmatch '\bReg(?:CreateKey|SetValue|Delete)') 'Text service writes application preferences'
    $registration = Get-Content -LiteralPath (Join-Path $win32 'OpenKey/OpenKey.TextService/Registration.cpp') -Raw -Encoding UTF8
    Assert-Inventory ($registration -notmatch 'SetDefaultLanguageProfile|ActivateLanguageProfile' -and
        $registration.Contains('SubstituteKeyboardLayout') -and $registration.Contains('GUID_TFCAT_TIP_KEYBOARD')) 'TSF activation or keyboard category changed'
}
Test-Inventory 'startup Run value and type covered' {
    $text = Get-Content -LiteralPath (Join-Path $engineRoot 'OpenKeyHelper.cpp') -Raw -Encoding UTF8
    Assert-Inventory ($startup.groups.Count -eq 1 -and $startup.groups[0].type -ceq 'REG_SZ' -and $startup.groups[0].scope -ceq 'startup') 'Startup schema changed'
    Compare-InventoryNames $startup.groups[0].names @('OpenKey') 'startup value'
    Assert-Inventory ($text.Contains('Software\\Microsoft\\Windows\\CurrentVersion\\Run') -and
        $text -match 'RegSetValueEx\(runKey,\s*_T\("OpenKey"\),\s*0,\s*REG_SZ' -and
        $text -match 'RegDeleteValue\(runKey,\s*_T\("OpenKey"\)') 'Startup path/type/writer changed'
}
Write-Output "$passed/$passed source inventory checks passed: 46 application values + 1 startup value. No real registry accessed."
