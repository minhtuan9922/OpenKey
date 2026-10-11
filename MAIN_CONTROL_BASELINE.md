# Baseline màn hình chính Win32

Kiểm kê source ngày 2026-10-11 từ MainControlDialog.cpp, OpenKey.rc,
OpenKeyManager.cpp, SettingsService.h/.cpp và các trang Fluent tương ứng.
Không mở engine, đọc cấu hình thật hoặc chạy thao tác reset/startup để lập
tài liệu. Đây là baseline chức năng, chưa phải ảnh chụp hay nghiệm thu GUI.

## Cách đọc giá trị mặc định

Các giá trị dưới đây là fallback khi chưa có cấu hình, không phải trạng thái
trên máy người dùng. 1 là bật, 0 là tắt trừ các trường có ghi chú riêng.
SettingsService::load và restoreDefaults có khác biệt được ghi tường minh.
Tên registry, kiểu và dữ liệu ngoài registry xem REGISTRY_INVENTORY.md.

## Phần Điều khiển dùng chung

- IDC_COMBO_INPUT_TYPE → vInputType, mặc định 0: Telex. Legacy có ba lựa chọn
  0 Telex, 1 VNI, 2 Simple Telex. Fluent ở InputSettingsPage có chỉ số 0..3;
  lựa chọn 3 không có trong combo/tray legacy, phải nghiệm thu riêng.
- IDC_COMBO_TABLE_CODE → vCodeTable, mặc định 0: Unicode. Các chỉ số còn lại:
  1 TCVN3 (ABC), 2 VNI Windows, 3 Unicode Tổ hợp, 4 Vietnamese Locale CP 1258.
  Chuyển sang InputSettingsPage trong Fluent.
- IDC_RADIO_METHOD_VIETNAMESE / IDC_RADIO_METHOD_ENGLISH → vLanguage,
  mặc định 1: Việt. Fluent dùng LanguageToggle ở OverviewPage.
- IDC_CHECK_SWITCH_KEY_CTRL/ALT/WIN/SHIFT, IDC_SWITCH_KEY_KEY và
  IDC_CHECK_SWITCH_KEY_BEEP cùng sửa vSwitchKeyStatus. Bit modifier:
  Ctrl 0x100, Alt 0x200, Win 0x400, Shift 0x800; beep 0x8000.
  Phím chính lặp ở byte thấp/cao; Space hiển thị tên, Delete/Backspace đặt
  sentinel 0xFE và ô legacy rỗng. Fluent dùng HotkeySettingsPage với combo,
  preview và cảnh báo thiếu modifier (cảnh báo không chặn lưu).
- Default Preferences/reset là 0x5A00025A (Alt+Z, không beep); engine load
  khi thiếu giá trị riêng dùng 0x7A000206. Không đồng nhất hai fallback này
  bằng cách tự ghi registry; thử tài khoản mới và reset là hai ca khác nhau.
- Đổi bảng mã khi vRememberCode bật và đổi Việt/Anh khi vUseSmartSwitchKey bật
  còn cập nhật smartSwitchKey theo ứng dụng. Sau checkbox/combo, legacy
  cập nhật tray; thay đổi được lưu ngay, không cần bấm Đóng để áp dụng.

## Tab Bộ gõ — IDD_DIALOG_TAB_GENERAL

Mỗi dòng ghi ID control → registry, fallback; trang Fluent đích.

- IDC_CHECK_MODERN_ORTHORGRAPHY → vUseModernOrthography=0;
  SpellingSettingsPage, đặt dấu oà/uý thay vì òa/úy.
- IDC_CHECK_FIX_RECOMMEND_BROWSER → vFixRecommendBrowser=1;
  CompatibilitySettingsPage, sửa lỗi gợi ý trình duyệt/Excel.
- IDC_CHECK_SPELLING → vCheckSpelling=1; SpellingSettingsPage.
- IDC_CHECK_RESTORE_IF_WRONG_SPELLING → vRestoreIfWrongSpelling=1;
  SpellingSettingsPage, khôi phục phím với từ sai.
- IDC_CHECK_ALLOW_ZJWF → vAllowConsonantZFWJ=0; AdvancedSettingsPage.
- IDC_CHECK_TEMP_OFF_SPELLING → vTempOffSpelling=0; AdvancedSettingsPage,
  tạm bỏ kiểm tra chính tả bằng Ctrl.
- IDC_CHECK_SMART_SWITCH_KEY → vUseSmartSwitchKey=1;
  CompatibilitySettingsPage, nhớ chế độ theo ứng dụng.
- IDC_CHECK_CAPS_FIRST_CHAR → vUpperCaseFirstChar=0; AdvancedSettingsPage.
- IDC_CHECK_SMART_SWITCH_CODE → vRememberCode=1; InputSettingsPage.
- IDC_CHECK_OTHER_LANGUAGES → vOtherLanguage=1; CompatibilitySettingsPage.
- IDC_CHECK_TEMP_OFF_OPEN_KEY → vTempOffOpenKey=0;
  CompatibilitySettingsPage, tạm dừng bằng Alt.

Legacy vô hiệu hóa ba control khôi phục từ sai, cho phép ZFWJ và tạm tắt
chính tả khi vCheckSpelling tắt; không xóa giá trị đang chọn. Bật/tắt chính
tả còn gọi vSetCheckSpelling. Fluent Advanced có dependency/cảnh báo cho
ZFWJ và Ctrl; SpellingSettingsPage hiện không vô hiệu hóa toggle khôi phục
từ sai theo vCheckSpelling. Đây là khác biệt UI cần quyết định/nghiệm thu,
không tự coi là tương đương hoàn toàn.

## Tab Gõ tắt — IDD_DIALOG_TAB_MACRO

- IDC_CHECK_QUICK_TELEX → vQuickTelex=0; SpellingSettingsPage.
- IDC_CHECK_QUICK_START_CONSONANT → vQuickStartConsonant=0;
  SpellingSettingsPage, f→ph, j→gi, w→qu.
- IDC_CHECK_QUICK_END_CONSONANT → vQuickEndConsonant=0;
  SpellingSettingsPage, g→ng, h→nh, k→ch.
- IDC_CHECK_USE_MACRO → vUseMacro: fallback load=1, reset=0.
  Đã có trong Preferences và MacroPage; giữ legacy/tray bật gõ tắt.
- IDC_CHECK_USE_MACRO_IN_ENGLISH → vUseMacroInEnglishMode=0;
  đã có tùy chọn tương ứng trong MacroPage Fluent.
- IDC_CHECK_AUTO_CAPS → vAutoCapsMacro=0; MacroPage.
- IDC_BUTTON_MACRO_TABLE mở MacroDialog; dữ liệu/các thao tác riêng
  xem MACRO_CONVERT_BASELINE.md. Không nhầm gõ nhanh với bật macro.

## Tab Hệ thống — IDD_DIALOG_TAB_SYSTEM

- IDC_CHECK_SHOW_ON_STARTUP → vShowOnStartUp=1; AdvancedSettingsPage.
- IDC_CHECK_MODERN_ICON → vUseGrayIcon=0; AdvancedSettingsPage, icon đơn sắc.
- IDC_CHECK_RUN_WITH_WINDOWS → vRunWithWindows=1; AdvancedSettingsPage.
  Legacy lưu rồi gọi registerRunOnStartup ngay. UI-only không tự cài hook;
  đường reload engine mới áp dụng startup khi trạng thái thay đổi.
- IDC_CHECK_RUN_AS_ADMIN → vRunAsAdmin=0; legacy-only, không có trong Fluent.
  Lưu rồi requestRestartAsAdmin: gỡ đăng ký startup trước, có thể hỏi restart
  bằng runas; hủy lời hỏi không đồng nghĩa rollback DWORD/startup.
- IDC_CHECK_CHECK_CREATE_SHORTCUT → vCreateDesktopShortcut=0; legacy-only.
  Bật gọi createDesktopShortcut tạo OpenKey.lnk. Tắt chỉ đổi DWORD trong
  handler này, không xóa shortcut; không diễn giải thành checkbox quản lý
  đầy đủ vòng đời file.
- IDC_CHECK_CHECK_UPDATE → vCheckNewVersion=0; AdvancedSettingsPage.
  restoreDefaults không đặt lại trường này.
- IDC_CHECK_FIX_CHROMIUM → vFixChromiumBrowser=0; CompatibilitySettingsPage.
  Legacy và Fluent phụ thuộc tùy chọn sửa lỗi gợi ý, giữ giá trị khi disabled.
- IDC_CHECK_USE_CLIPBOARD → vSendKeyStepByStep=1: checkbox mặc định tắt;
  bật checkbox ghi 0, tắt ghi 1. Fluent ClipboardToggle giữ quy ước đảo.
- IDC_CHECK_SUPPORT_METRO_APP → vSupportMetroApp: fallback=0, reset=1;
  CompatibilitySettingsPage. Không gọi reset chỉ để làm UI giống nhau.
- IDC_BUTTON_CHECK_UPDATE gọi beginUpdateCheck với owner và nút hiện tại;
  Fluent chuyển chức năng kiểm tra/cập nhật sang AboutPage. Nghiệm thu mạng,
  download và đóng owner khi đang chờ là ca riêng, không chạy trong kiểm kê.

## Tab Thông tin và các nút chung

- IDC_TAB_CONTROL có bốn tab: Bộ gõ, Gõ tắt, Hệ thống, Thông tin.
  Chuyển tab chỉ show/hide các dialog con; không có nút Apply/hủy cả phiên.
- IDD_DIALOG_TAB_INFO: icon, tiêu đề, mô tả nguồn mở/miễn phí, tác giả,
  IDC_STATIC_APP_VERSION_INFO ghi phiên bản và __DATE__ của build.
- IDC_SYSLINK_HOME_PAGE/FANPAGE/AUTHOR_EMAIL mở trang chủ, Facebook, mailto;
  IDC_BUTTON_GO_SOURCE_CODE mở GitHub. Nhãn trang chủ resource là HTTPS
  nhưng handler legacy trong MainControlDialog dùng HTTP. Không kiểm tra
  kết nối hay mở link thật trong lượt này; About Fluent là đường riêng.
- IDBUTTON_OK (Đóng, default button) đóng dialog, engine/tray tiếp tục chạy.
- IDBUTTON_EXIT (Tắt OpenKey) gọi cleanup/thoát engine; xem lifecycle baseline.
- ID_BTN_DEFAULT hỏi Yes/No trước gọi restoreDefaults, fillData và update tray.
  Đường này không phải factory reset toàn bộ: không xóa macroData,
  smartSwitchKey, UI locale/theme/geometry; không reset admin/shortcut/check
  update. onDefaultConfig không trực tiếp registerRunOnStartup hay tạo/xóa
  shortcut; DWORD reset không chứng minh đích startup đã được đồng bộ.
- Legacy có tooltip cho các control tùy chọn, title chứa phiên bản và icon.
  Fluent phân nhóm theo trang, có InfoBar lưu lỗi/thành công; chưa có một
  nút tương đương cho mọi thao tác legacy chỉ vì model đã đọc đủ trường.

## Checklist kiểm thử thủ công baseline Win32/Fluent

Chạy trên VM/snapshot hoặc tài khoản thử; backup theo MIGRATION_FLUENT_BETA.md.
Ghi build/hash, x64/x86, OS/DPI/theme/ngôn ngữ và kết quả từng bước. Checklist
này cùng MACRO_CONVERT_BASELINE.md và TRAY_LIFECYCLE_BASELINE.md bao phủ các
nhóm nghiệp vụ; ảnh chụp và kết quả GUI thực tế vẫn còn thiếu.

- [ ] Tài khoản chưa có cấu hình: chụp control chung và bốn tab, đối chiếu
  fallback; ghi riêng phím chuyển, vUseMacro và vSupportMetroApp.
- [ ] Có cấu hình cũ: tất cả control phản ánh dữ liệu lưu, không reset giá trị
  legacy-only khi chỉ mở Fluent; đối chiếu trước/sau bằng bản backup riêng.
- [ ] Đổi từng combo/radio/checkbox, đóng/mở lại; chỉ trường mong đợi thay đổi,
  UI/engine/tray đồng bộ. Gõ tiếng Việt thực tế ở app ngoài OpenKey.
- [ ] Kiểu gõ legacy 0..2, Fluent 0..3 và năm bảng mã; kiểm tra font, dấu,
  gõ nhanh, khôi phục chính tả, nhớ chế độ/bảng mã theo ứng dụng.
- [ ] Chính tả tắt/bật và sửa lỗi gợi ý tắt/bật: disabled đúng, giữ lựa chọn
  con; đánh giá khác biệt toggle khôi phục từ sai giữa hai UI.
- [ ] Phím chuyển: modifier, Space, xóa phím, beep, không modifier và phím
  đang lưu ngoài combo; không kích hoạt ngoài ý muốn trong editor.
- [ ] Đóng khác Tắt OpenKey; mở lại UI, single-instance và fallback legacy.
- [ ] Cài đặt gốc: Hủy không đổi; Yes chỉ đổi đúng nhóm, không mất macro/
  smartSwitch/UI settings; kiểm tra startup thực tế riêng sau reset.
- [ ] Startup thường/admin, UAC Yes/No, bật/tắt shortcut với file thử;
  ghi nhận side effect và đích executable, không thao tác trên host thật.
- [ ] Kiểm tra phiên bản/link/tooltip, chờ update rồi đóng owner; kiểm tra
  Tab/focus, keyboard, DPI/cửa sổ nhỏ và locale/theme của Fluent.
- [ ] Chạy các checklist macro, chuyển mã, tray/lifecycle tương ứng;
  UI-only/Sandbox smoke không thay nghiệm thu keyboard hook hoặc hotkey.

## Bằng chứng tự động

test-settings.ps1 kiểm tra mapping/load/reset, store giả, geometry, theme và
dependency state trên x64/x86. test-registry-inventory.ps1 đối chiếu tên/kiểu
source. Các test này không gửi click, chạy engine, sửa registry/task/shortcut
thật, không chứng minh GUI đã nghiệm thu. Kết quả mới ở FLUENT_UI_QA.md.
