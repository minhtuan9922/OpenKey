[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$uiRoot = Join-Path $PSScriptRoot 'Sources\OpenKey\win32\OpenKey\OpenKey.UI'
$xamlNamespace = 'http://schemas.microsoft.com/winfx/2006/xaml'
$documents = @{}
$passed = 0
function Assert-UI([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}
function Test-UI([string]$Name, [scriptblock]$Check) {
    & $Check
    $script:passed++
    Write-Output "PASS: $Name"
}
function Nodes([xml]$Document, [string]$LocalName) {
    @($Document.SelectNodes('//*') | Where-Object LocalName -eq $LocalName)
}
$pageNames = @('OverviewPage', 'InputSettingsPage', 'SpellingSettingsPage', 'HotkeySettingsPage',
    'CompatibilitySettingsPage', 'AdvancedSettingsPage', 'MacroPage', 'ConvertPage', 'AboutPage')
Test-UI 'Parse source XAML, resource dictionaries and RESW' {
    foreach ($name in @('App', 'MainWindow') + $pageNames) {
        $documents[$name] = [xml](Get-Content -LiteralPath (Join-Path $uiRoot "$name.xaml") -Raw -Encoding UTF8)
    }
    $documents['Fluent'] = [xml](Get-Content -LiteralPath (Join-Path $uiRoot 'Styles\FluentResources.xaml') -Raw -Encoding UTF8)
    foreach ($language in @('vi-VN','en-US')) {
        $documents[$language] = [xml](Get-Content -LiteralPath (Join-Path $uiRoot "Strings\$language\Resources.resw") -Raw -Encoding UTF8)
    }
}
Test-UI 'Matching, unique Vietnamese and English resource keys' {
    $vi = @($documents['vi-VN'].root.data.name)
    $en = @($documents['en-US'].root.data.name)
    Assert-UI ($vi.Count -gt 0 -and @(Compare-Object $vi $en).Count -eq 0) 'Resource languages differ'
    foreach ($keys in @($vi, $en)) {
        Assert-UI (@($keys | Group-Object | Where-Object Count -gt 1).Count -eq 0) 'Duplicate resource key'
    }
}
Test-UI 'Every localized XAML element has resource entries' {
    $keys = @($documents['vi-VN'].root.data.name)
    foreach ($name in @('MainWindow') + $pageNames) {
        foreach ($node in $documents[$name].SelectNodes('//*')) {
            $uid = $node.GetAttribute('Uid', $xamlNamespace)
            if ($uid) { Assert-UI (@($keys | Where-Object { $_.StartsWith($uid + '.', [StringComparison]::Ordinal) }).Count -gt 0) "Missing x:Uid resource: $uid" }
        }
    }
}
Test-UI 'Unique x:Name in each page/window' {
    foreach ($name in @('MainWindow') + $pageNames) {
        $names = @($documents[$name].SelectNodes('//*') | ForEach-Object { $_.GetAttribute('Name', $xamlNamespace) } | Where-Object { $_ })
        Assert-UI (@($names | Group-Object | Where-Object Count -gt 1).Count -eq 0) "Duplicate x:Name: $name"
    }
}
Test-UI 'Fluent dictionary merged after native WinUI resources' {
    $merged = @($documents['App'].SelectNodes('//*[local-name()="ResourceDictionary.MergedDictionaries"]/*'))
    Assert-UI ($merged.Count -eq 2 -and $merged[0].LocalName -eq 'XamlControlsResources' -and $merged[1].Source -eq 'Styles/FluentResources.xaml') 'Resource merge order changed'
}
Test-UI 'Unique app design resource keys' {
    $keys = @($documents['Fluent'].SelectNodes('//*') | ForEach-Object { $_.GetAttribute('Key', $xamlNamespace) } | Where-Object { $_ })
    Assert-UI ($keys.Count -gt 0 -and @($keys | Group-Object | Where-Object Count -gt 1).Count -eq 0) 'Duplicate design token/style'
}
Test-UI 'All 21 settings cards use shared style' {
    $count = 0
    foreach ($name in $pageNames) {
        foreach ($card in (Nodes $documents[$name] 'Border')) {
            if ($card.Style -eq '{StaticResource OpenKeySettingsCardStyle}') { $count++ }
            Assert-UI (-not $card.GetAttribute('Background').Contains('CardBackgroundFillColorDefaultBrush')) "Local card color: $name"
        }
    }
    Assert-UI ($count -eq 21) "Expected 21 cards; got $count"
}
Test-UI 'Every page uses shared title and container style' {
    foreach ($name in $pageNames) {
        $styles = @($documents[$name].SelectNodes('//*') | ForEach-Object { $_.GetAttribute('Style') })
        Assert-UI ($styles -contains '{StaticResource OpenKeyPageTitleStyle}') "Missing title style: $name"
        Assert-UI ($styles -contains '{StaticResource PageStackLayoutStyle}' -or $styles -contains '{StaticResource PageGridLayoutStyle}') "Missing container: $name"
    }
}
Test-UI 'Card colors remain dynamic WinUI theme brushes' {
    $card = @(Nodes $documents['Fluent'] 'Style' | Where-Object { $_.GetAttribute('Key', $xamlNamespace) -eq 'OpenKeySettingsCardStyle' })[0]
    $setters = @($card.ChildNodes | Where-Object LocalName -eq 'Setter')
    Assert-UI (@($setters | Where-Object { $_.Property -eq 'Background' -and $_.Value -eq '{ThemeResource CardBackgroundFillColorDefaultBrush}' }).Count -eq 1) 'Static card background'
    Assert-UI (@($setters | Where-Object { $_.Property -eq 'BorderBrush' -and $_.Value -eq '{ThemeResource CardStrokeColorDefaultBrush}' }).Count -eq 1) 'Static card stroke'
}
Test-UI 'Native control templates/states not replaced' {
    foreach ($style in (Nodes $documents['Fluent'] 'Style' | Where-Object TargetType -in @('ToggleSwitch','CheckBox','ComboBox'))) {
        foreach ($setter in @($style.ChildNodes | Where-Object LocalName -eq 'Setter')) {
            Assert-UI ($setter.Property -in @('HeaderTemplate','ContentTemplate')) 'Native control state override'
        }
    }
}
Test-UI 'Resource dictionary included by UI project' {
    $project = [xml](Get-Content -LiteralPath (Join-Path $uiRoot 'OpenKey.UI.vcxproj') -Raw -Encoding UTF8)
    Assert-UI (@($project.SelectNodes('//*[local-name()="Page"]') | Where-Object Include -eq 'Styles\FluentResources.xaml').Count -eq 1) 'Dictionary missing from project'
}
Test-UI 'Adaptive setter targets exist in their page' {
    foreach ($name in $pageNames) {
        $names = @($documents[$name].SelectNodes('//*') | ForEach-Object { $_.GetAttribute('Name', $xamlNamespace) })
        foreach ($setter in (Nodes $documents[$name] 'Setter' | Where-Object { $_.GetAttribute('Target') })) {
            $target = $setter.GetAttribute('Target').Split('.')[0]
            Assert-UI ($names -contains $target) "Unknown adaptive target: $name / $target"
        }
    }
}
Test-UI 'All nine navigation tags remain available' {
    $tags = @((Nodes $documents['MainWindow'] 'NavigationViewItem') | ForEach-Object { $_.GetAttribute('Tag') })
    Assert-UI (@(Compare-Object $tags @('overview','input','spelling','hotkeys','compatibility','advanced','macro','convert','about')).Count -eq 0) 'Navigation tags changed'
}
Test-UI 'Solid fallback covers title and content without hit-test controls' {
    $fallback = @((Nodes $documents['MainWindow'] 'Border') | Where-Object { $_.GetAttribute('Name', $xamlNamespace) -eq 'BackdropFallback' })
    Assert-UI ($fallback.Count -eq 1 -and $fallback[0].GetAttribute('Grid.RowSpan') -eq '2' -and $fallback[0].Background -eq '{ThemeResource ApplicationPageBackgroundThemeBrush}') 'Invalid backdrop fallback'
    Assert-UI (@($fallback[0].ChildNodes | Where-Object NodeType -eq 'Element').Count -eq 0) 'Fallback contains controls'
}
Test-UI 'Only draft pages require session caching' {
    foreach ($name in $pageNames) {
        $mode = $documents[$name].DocumentElement.GetAttribute('NavigationCacheMode')
        if ($name -in @('MacroPage','ConvertPage')) {
            Assert-UI ($mode -eq 'Required') "Draft page can be evicted: $name"
        } else {
            Assert-UI ([string]::IsNullOrEmpty($mode) -or $mode -eq 'Disabled') "Unexpected page cache: $name"
        }
    }
}
Test-UI 'Top-level navigation does not accumulate history' {
    $frame = @((Nodes $documents['MainWindow'] 'Frame') | Where-Object { $_.GetAttribute('Name', $xamlNamespace) -eq 'ContentFrame' })
    $navigation = @(Nodes $documents['MainWindow'] 'NavigationView')
    Assert-UI ($frame.Count -eq 1 -and $frame[0].GetAttribute('IsNavigationStackEnabled') -eq 'False') 'Unbounded navigation history'
    Assert-UI ($navigation.Count -eq 1 -and $navigation[0].IsBackButtonVisible -eq 'Collapsed') 'Back UI needs an explicit navigation policy'
}
Test-UI 'Original app icon compiled into UI executable' {
    $project = [xml](Get-Content -LiteralPath (Join-Path $uiRoot 'OpenKey.UI.vcxproj') -Raw -Encoding UTF8)
    Assert-UI (@($project.SelectNodes('//*[local-name()="ResourceCompile"]') | Where-Object Include -eq 'AppResources.rc').Count -eq 1) 'Icon resource is not compiled'
    $header = Get-Content -LiteralPath (Join-Path $uiRoot 'AppResources.h') -Raw -Encoding UTF8
    $resource = Get-Content -LiteralPath (Join-Path $uiRoot 'AppResources.rc') -Raw -Encoding UTF8
    Assert-UI ($header -match '#define\s+IDI_OPENKEY_UI\s+101\b') 'Icon id changed without reader update'
    Assert-UI ($resource.Contains('IDI_OPENKEY_UI ICON "..\\OpenKey\\icon.ico"')) 'Icon resource does not use original asset'
}
Test-UI 'UI source manifest declares PerMonitorV2' {
    $manifest = [xml](Get-Content -LiteralPath (Join-Path $uiRoot 'app.manifest') -Raw -Encoding UTF8)
    $dpi = @($manifest.SelectNodes('//*[local-name()="dpiAwareness"]'))
    Assert-UI ($dpi.Count -eq 1 -and $dpi[0].InnerText -eq 'PerMonitorV2' -and $dpi[0].NamespaceURI -eq 'http://schemas.microsoft.com/SMI/2016/WindowsSettings') 'Incorrect DPI manifest'
}
Test-UI 'Dependency warnings are separate from save status and localized' {
    foreach ($pair in @(@('CompatibilitySettingsPage','ChromiumDependencyWarning'), @('AdvancedSettingsPage','SpellingDependencyWarning'))) {
        $warning = @((Nodes $documents[$pair[0]] 'InfoBar') | Where-Object { $_.GetAttribute('Name', $xamlNamespace) -eq $pair[1] })
        Assert-UI ($warning.Count -eq 1 -and $warning[0].Severity -eq 'Warning' -and $warning[0].IsClosable -eq 'False' -and $warning[0].IsOpen -eq 'False') 'Dependency warning policy changed'
        $saveStatus = @((Nodes $documents[$pair[0]] 'InfoBar') | Where-Object { $_.GetAttribute('Name', $xamlNamespace) -eq 'SettingsStatus' })
        Assert-UI ($saveStatus.Count -eq 1 -and $saveStatus[0] -ne $warning[0]) 'Warning replaced save status'
        Assert-UI ($warning[0].GetAttribute('LiveSetting', 'using:Microsoft.UI.Xaml.Automation') -eq 'Polite' -or $warning[0].GetAttribute('AutomationProperties.LiveSetting') -eq 'Polite') 'Warning is not a polite live region'
        foreach ($language in @('vi-VN','en-US')) {
            $keys = @($documents[$language].root.data | ForEach-Object name)
            Assert-UI ($keys -contains ($pair[1] + '.Title') -and $keys -contains ($pair[1] + '.Message')) 'Warning translation missing'
        }
    }
}
Test-UI 'Optional high contrast subscription cannot abort startup' {
    $source = Get-Content -LiteralPath (Join-Path $uiRoot 'MainWindow.xaml.cpp') -Raw -Encoding UTF8
    Assert-UI ($source -match '(?s)try\s*\{\s*m_contrastChanged\s*=\s*m_accessibility\.HighContrastChanged\(auto_revoke,.*?catch\s*\(hresult_error const&\)\s*\{\s*OutputDebugStringW\(L"[^"]*"\);\s*\}\s*ConfigureInitialWindow\(\);') 'Optional subscription lacks startup fallback'
    Assert-UI ($source.Contains('RootLayout().ActualThemeChanged') -and $source.Contains('m_systemSettings.ColorValuesChanged')) 'Fallback theme/color notifications missing'
}
Test-UI 'About links identify the Fluent fork and credit the original project' {
    $buttons = @(Nodes $documents['AboutPage'] 'Button')
    foreach ($pair in @(@('AboutLabel5','https://github.com/minhtuan9922/OpenKey'), @('AboutLabel6','https://github.com/minhtuan9922/OpenKey/releases'), @('AboutLabel7','https://github.com/tuyenvm/OpenKey'))) {
        $button = @($buttons | Where-Object { $_.GetAttribute('Uid', $xamlNamespace) -eq $pair[0] })
        Assert-UI ($button.Count -eq 1 -and $button[0].Tag -eq $pair[1]) 'Incorrect About destination'
    }
    foreach ($language in @('vi-VN','en-US')) {
        $credit = ($documents[$language].root.data | Where-Object name -eq 'AboutLabel4.Text').value
        Assert-UI ($credit -match 'minhtuan9922/OpenKey' -and $credit -match 'tuyenvm/OpenKey' -and $credit -match 'Mai V\u0169 Tuy\u00ean') 'Missing fork/upstream credit'
        $copyright = ($documents[$language].root.data | Where-Object name -eq 'AboutLabel11.Text').value
        Assert-UI ($copyright -match 'Copyright.*Mai V\u0169 Tuy\u00ean') 'Original copyright removed'
    }
}
Test-UI 'Update controllers use shared fork download destination' {
    $about = Get-Content -LiteralPath (Join-Path $uiRoot 'AboutPage.xaml.cpp') -Raw -Encoding UTF8
    Assert-UI ($about.Contains('OpenKeyUpdateSource::SetupUrl(update.version)') -and -not $about.Contains('github.com/tuyenvm')) 'About update uses upstream release'
    $engine = Get-Content -LiteralPath (Join-Path $uiRoot '../OpenKey/AppDelegate.cpp') -Raw -Encoding UTF8
    Assert-UI ($engine.Contains('if (fluent) UpdateLaunchService::OpenSetupDownload(owner, outcome.version);')) 'Fluent engine does not open setup download'
    $metadata = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'version.json') -Raw | ConvertFrom-Json
    $versionText = [regex]::Match($about, 'OpenKeyVersion\[\] = L"([0-9.]+)"').Groups[1].Value
    $version = [version]$versionText
    $packed = $version.Major + ($version.Minor -shl 8) + ($version.Build -shl 16)
    Assert-UI ($metadata.latestWinVersion.versionName -eq "v$versionText" -and $metadata.latestWinVersion.versionCode -eq $packed) 'Fork metadata differs from current version'
    Assert-UI ($about -match ('OpenKeyVersionCode = 0x' + $packed.ToString('X8'))) 'UI packed version differs'
    $installer = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'Installer/OpenKey.iss') -Raw
    Assert-UI ($installer -match ('(?m)^AppVersion=' + [regex]::Escape($versionText) + '\r?$')) 'Installer version differs'
    foreach ($resource in @('../OpenKey/OpenKey.rc','../OpenKeyUpdate/OpenKeyUpdate.rc','AppResources.rc')) {
        $rc = Get-Content -LiteralPath (Join-Path $uiRoot $resource) -Raw
        Assert-UI ($rc.Contains("FILEVERSION $($version.Major),$($version.Minor),$($version.Build),0") -and $rc.Contains("PRODUCTVERSION $($version.Major),$($version.Minor),$($version.Build),0")) 'Executable resource version differs'
    }
}
Write-Output "UI resource structure: $passed/22 passed; $(@($documents['vi-VN'].root.data).Count) resource keys. Static XML/source checks only, no runtime/visual QA."
