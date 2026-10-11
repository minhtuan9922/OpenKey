$ErrorActionPreference = 'Stop'
# Text contract only: no Windows messages, hooks, registry, app, or tray activity.
$engineRoot = Join-Path $PSScriptRoot 'Sources\OpenKey\win32\OpenKey\OpenKey'
$uiRoot = Join-Path $PSScriptRoot 'Sources\OpenKey\win32\OpenKey\OpenKey.UI'
$contract = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'Tests\TrayContract.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$tray = Get-Content -LiteralPath (Join-Path $engineRoot 'SystemTrayHelper.cpp') -Raw -Encoding UTF8
$delegate = Get-Content -LiteralPath (Join-Path $engineRoot 'AppDelegate.cpp') -Raw -Encoding UTF8
$core = Get-Content -LiteralPath (Join-Path $engineRoot 'OpenKey.cpp') -Raw -Encoding UTF8
$app = Get-Content -LiteralPath (Join-Path $uiRoot 'App.xaml.cpp') -Raw -Encoding UTF8
$passed = 0
function Assert-Tray([bool]$Condition, [string]$Message) { if (-not $Condition) { throw $Message } }
function Test-Tray([string]$Name, [scriptblock]$Action) {
    & $Action; $script:passed++; Write-Output "PASS tray contract: $Name"
}
function Assert-SourcePattern([string]$Source, [string]$Pattern, [string]$Message) {
    Assert-Tray ([regex]::IsMatch($Source, $Pattern, [Text.RegularExpressions.RegexOptions]::Singleline)) $Message
}
Test-Tray 'schema and unique commands' {
    Assert-Tray ($contract.schemaVersion -eq 1 -and $contract.commands.Count -eq 21 -and
        @($contract.commands.symbol | Sort-Object -Unique).Count -eq 21 -and
        @($contract.commands.id | Sort-Object -Unique).Count -eq 21) 'Invalid tray baseline'
}
foreach ($command in $contract.commands) {
    Test-Tray $command.symbol {
        $symbol = [regex]::Escape($command.symbol)
        Assert-SourcePattern $tray ('(?m)^#define\s+' + $symbol + '\s+' + $command.id + '\s*$') 'Command ID changed'
        Assert-SourcePattern $tray ('\{\s*' + $symbol + ',\s*_T\("' + [regex]::Escape($command.label) + '"\)\s*\}') 'Command label changed'
        Assert-SourcePattern $tray ('AppendMenu\(' + [regex]::Escape($command.menu) + ',\s*MF_\w+,\s*' + $symbol + ',\s*menuData\[' + $symbol + '\]\)') 'Menu placement changed'
        Assert-SourcePattern $tray ('case\s+' + $symbol + ':\s*AppDelegate::getInstance\(\)->' + [regex]::Escape($command.handler) + '\(' + [regex]::Escape($command.argument) + '\);\s*break;') 'Command dispatch changed'
        if ($null -ne $command.checked) {
            Assert-SourcePattern $tray ('MODIFY_MENU\(' + [regex]::Escape($command.menu) + ',\s*' + $symbol + ',\s*' + [regex]::Escape($command.checked) + '\)') 'Checked state changed'
        }
    }
}
Test-Tray 'complete command coverage and append order' {
    $actual = @([regex]::Matches($tray, 'case\s+(POPUP_\w+)\s*:') | ForEach-Object { $_.Groups[1].Value })
    Assert-Tray ((($actual | Sort-Object) -join '|') -ceq (($contract.commands.symbol | Sort-Object) -join '|')) 'Unhandled or undocumented command'
    $appended = @([regex]::Matches($tray, 'AppendMenu\((?:popupMenu|otherCode|themeMenu),\s*MF_\w+,\s*(POPUP_\w+),') | ForEach-Object { $_.Groups[1].Value })
    Assert-Tray (($appended -join '|') -ceq ($contract.commands.symbol -join '|')) 'Menu order changed'
}
Test-Tray 'default action and encoding submenu' {
    Assert-SourcePattern $tray 'SetMenuDefaultItem\(popupMenu,\s*POPUP_CONTROL_PANEL,\s*false\)' 'Default item changed'
    Assert-SourcePattern $tray ('AppendMenu\(popupMenu,\s*MF_POPUP,\s*\(UINT_PTR\)otherCode,\s*_T\("' + [regex]::Escape($contract.otherCodeLabel) + '"\)\)') 'Encoding submenu changed'
}
Test-Tray 'mouse activation behavior' {
    Assert-SourcePattern $tray 'lParam == WM_LBUTTONDBLCLK\)\s*\{\s*AppDelegate::getInstance\(\)->onControlPanel\(\)' 'Double click changed'
    Assert-SourcePattern $tray 'lParam == WM_LBUTTONUP\)\s*\{\s*AppDelegate::getInstance\(\)->onToggleVietnamese\(\);\s*SystemTrayHelper::updateData\(\)' 'Left click changed'
    Assert-SourcePattern $tray 'lParam == WM_RBUTTONDOWN.*?TrackPopupMenu\(\s*popupMenu,\s*TPM_RETURNCMD \| TPM_NONOTIFY' 'Right click changed'
}
Test-Tray 'theme submenu and persistence' {
    Assert-SourcePattern $tray ('AppendMenu\(popupMenu,\s*MF_POPUP,\s*\(UINT_PTR\)themeMenu,\s*_T\("' + [regex]::Escape($contract.themeLabel) + '"\)\)') 'Theme submenu changed'
    Assert-SourcePattern $delegate 'void AppDelegate::onUiTheme\(int theme\).*?theme < 0 \|\| theme > 2.*?OpenKeyUITheme::Save' 'Theme persistence gate changed'
    Assert-SourcePattern $tray 'GetCursorPos\(&curPoint\);\s*SystemTrayHelper::updateData\(\)' 'Tray checks not refreshed before opening'
}
Test-Tray 'owner draw session lifecycle' {
    Assert-SourcePattern $tray 'case WM_MEASUREITEM:.*?activeTrayMenu->Measure' 'Measure dispatch missing'
    Assert-SourcePattern $tray 'case WM_DRAWITEM:.*?activeTrayMenu->Draw' 'Draw dispatch missing'
    Assert-SourcePattern $tray 'case WM_MENUCHAR:.*?activeTrayMenu->MenuChar' 'Keyboard dispatch missing'
    Assert-SourcePattern $tray 'OpenKeyTrayTheme::Session menuAppearance.*?activeTrayMenu = &menuAppearance;.*?TrackPopupMenu.*?activeTrayMenu = nullptr;.*?menuAppearance.Restore\(\);.*?switch \(commandId\)' 'Lifetime/restore before command dispatch changed'
    Assert-SourcePattern $tray 'void SystemTrayHelper::updateData\(\).*?if \(trayPopupOpen\) return;' 'Nested message loop must not mutate menu snapshot/data'
    Assert-SourcePattern $tray 'OpenKeyTrayTheme::ResolveDark.*?OpenKeyTrayTheme::SystemDark\(\).*?OpenKeyTrayTheme::CursorDpi.*?OpenKeyTrayTheme::HighContrast\(\)' 'Shared theme/system/DPI/accessibility selection missing'
}
Test-Tray 'custom rounded popup with native fallback' {
    Assert-SourcePattern $tray 'highContrast \? OpenKeyTrayPopup::Result\{\}.*?OpenKeyTrayPopup::Show\(popupMenu, hWnd, curPoint, dark, dpi\).*?if \(!custom.handled\).*?TrackPopupMenu' 'Custom popup/native fallback changed'
    Assert-SourcePattern $tray 'trayPopupOpen = true;.*?OpenKeyTrayPopup::Show.*?trayPopupOpen = false;.*?switch \(commandId\)' 'Popup lifetime/dispatch changed'
}
Test-Tray 'Fluent page routes before legacy fallback' {
    foreach ($route in @(@('onMacroTable','macro','macroDialog'),@('onConvertTool','convert','convertDialog'),@('onOpenKeyAbout','about','aboutDialog'))) {
        Assert-SourcePattern $delegate ('void AppDelegate::' + $route[0] + '\(\)\s*\{\s*if \(launchModernControlPanel\(L"' + $route[1] + '"\)\) return;\s*if \(' + $route[2]) 'Page route/fallback changed'
    }
    Assert-SourcePattern $delegate 'ShellExecuteW\(nullptr, L"open", fullPath.c_str\(\), arguments.c_str\(\)' 'Page arguments not passed to UI'
    Assert-SourcePattern $app 'const auto page = RequestedPage\(args\).*?TryEnqueue\(\[lifetime, page\].*?OpenPage\(page\)' 'Redirected route not forwarded'
}
Test-Tray 'legacy IPC contracts' {
    Assert-SourcePattern $tray '#define WM_PREFERENCES_CHANGED \(WM_USER \+ 2020\)' 'Preferences message changed'
    Assert-SourcePattern $tray '#define WM_OPENKEY_EXIT \(WM_USER \+ 2021\)' 'Exit message changed'
    Assert-SourcePattern $tray 'case WM_USER\+2019:\s*AppDelegate::getInstance\(\)->onControlPanel\(\)' 'Activation dispatch changed'
    Assert-SourcePattern $tray 'case WM_PREFERENCES_CHANGED:\s*AppDelegate::getInstance\(\)->onSettingsChangedFromUI\(\)' 'Settings dispatch changed'
    Assert-SourcePattern $tray 'case WM_OPENKEY_EXIT:\s*AppDelegate::getInstance\(\)->onOpenKeyExit\(\)' 'Exit dispatch changed'
}
Test-Tray 'Explorer restart recovery' {
    Assert-SourcePattern $tray 'RegisterWindowMessage\(_T\("TaskbarCreated"\)\)' 'Explorer message changed'
    Assert-SourcePattern $tray 'message == taskbarCreated.*?loadTrayIcon\(\);.*?Shell_NotifyIcon\(NIM_ADD, &nid\).*?Shell_NotifyIcon\(NIM_SETVERSION, &nid\);.*?SystemTrayHelper::updateData\(\)' 'Tray recovery changed'
}
Test-Tray 'engine single-instance before initialization' {
    Assert-SourcePattern $delegate 'FindWindow\(APP_CLASS, NULL\).*?if \(previousInstance\).*?SendMessage\(previousInstance, WM_USER \+ 2019, 0, 0\);.*?return 0;.*?OpenKeyManager::initEngine\(\)' 'Engine activation order changed'
}
Test-Tray 'engine initialization and hook cleanup' {
    Assert-SourcePattern $core 'void OpenKeyInit\(\).*?SettingsService::load\(\);.*?registerRunOnStartup\(vRunWithWindows\).*?initMacroMap.*?initSmartSwitchKey.*?SetWindowsHookEx\(WH_KEYBOARD_LL.*?SetWindowsHookEx\(WH_MOUSE_LL.*?SetWinEventHook\(EVENT_SYSTEM_FOREGROUND' 'Engine initialization changed'
    Assert-SourcePattern $core 'void OpenKeyFree\(\)\s*\{\s*UnhookWindowsHookEx\(hMouseHook\);\s*UnhookWindowsHookEx\(hKeyboardHook\);\s*UnhookWinEvent\(hSystemEvent\);' 'Hook cleanup changed'
}
Test-Tray 'Fluent launch and legacy fallback' {
    Assert-SourcePattern $delegate 'void AppDelegate::onControlPanel\(\)\s*\{\s*if \(!launchModernControlPanel\(\)\)\s*\{\s*createMainDialog\(\)' 'Control fallback changed'
    Assert-SourcePattern $delegate 'L"OPENKEY_UI_PATH"' 'Configured UI path missing'
    Assert-SourcePattern $delegate 'ShellExecuteW\(nullptr, L"open".*?if \(result > 32\)' 'Launch success gate changed'
}
Test-Tray 'independent UI activation and normal close' {
    Assert-SourcePattern $app 'FindOrRegisterForKey\(L"OpenKey.UI.MainWindow"\).*?!m_appInstance.IsCurrent\(\).*?RedirectActivationAsync\(m_appInstance\);.*?return;' 'UI activation key/redirect changed'
    Assert-Tray ($app -notmatch 'OpenKeyInit\(|initEngine\(|SetWindowsHookEx\(') 'UI app now owns engine initialization'
    $window = Get-Content -LiteralPath (Join-Path $uiRoot 'MainWindow.xaml.cpp') -Raw -Encoding UTF8
    Assert-SourcePattern $window 'AppWindow\(\).Closing\(.*?SaveWindowState\(\)' 'Closing geometry persistence changed'
    Assert-Tray ($window -notmatch 'WM_OPENKEY_EXIT|onOpenKeyExit\(|freeEngine\(') 'Normal UI close now owns engine shutdown'
}
Test-Tray 'ordered idempotent engine exit' {
    Assert-SourcePattern $delegate 'void AppDelegate::onOpenKeyExit\(\)\s*\{\s*if \(exiting\) return;\s*exiting = true;\s*stopUpdateCheck\(\);\s*OpenKeyManager::freeEngine\(\);\s*SystemTrayHelper::removeSystemTray\(\);\s*PostQuitMessage\(0\)' 'Engine exit order changed'
    Assert-SourcePattern $tray 'void SystemTrayHelper::removeSystemTray\(\)\s*\{\s*Shell_NotifyIcon\(NIM_DELETE, &nid\)' 'Tray cleanup changed'
}
Write-Output "$passed/$passed tray/lifecycle source checks passed; no messages dispatched or native acceptance claimed."
