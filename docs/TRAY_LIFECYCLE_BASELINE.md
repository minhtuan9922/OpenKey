# Baseline system tray và vòng đời OpenKey Windows

Đối chiếu source ngày 2026-10-11. Đây là danh sách hành vi hiện có, không phải
kết quả nghiệm thu tương tác Windows. Không chạy engine, gửi message, đổi registry,
cài hook hoặc khởi động updater để lập baseline này.

## Toàn bộ menu tray

[Tests/TrayContract.json](../Tests/TrayContract.json) ghi đủ 21 lệnh: ID số, tên
symbol, nhãn tiếng Việt, menu cha, handler/đối số và biểu thức checked.
Thứ tự menu hiện tại:

1. Bật Tiếng Việt: đảo `vLanguage` qua onToggleVietnamese.
2. Bật kiểm tra chính tả: đảo `vCheckSpelling`, gọi vSetCheckSpelling.
3. Bật loại trừ ứng dụng thông minh: đảo `vUseSmartSwitchKey`.
4. Bật gõ tắt: đảo `vUseMacro`.
5. Cấu hình gõ tắt...: ưu tiên Fluent --page macro, fallback MacroDialog.
6. Công cụ chuyển mã...: ưu tiên Fluent --page convert, fallback ConvertToolDialog.
7. Chuyển mã nhanh: gọi onQuickConvert, ConvertService qua cầu nối legacy;
   thông báo tray không modal khi thành công và không bật tùy chọn bỏ thông báo.
8. Kiểu gõ Telex: inputType=0.
9. Kiểu gõ VNI: inputType=1.
10. Kiểu gõ Simple Telex: inputType=2. Tray chưa có lệnh riêng cho inputType=3;
    nếu chọn kiểu đó từ Fluent, ba mục tray này không checked.
11. Unicode dựng sẵn: codeTable=0.
12. TCVN3 (ABC): codeTable=1.
13. VNI Windows: codeTable=2.
14. Trong submenu “Bảng mã khác”: Unicode tổ hợp, codeTable=3.
15. Trong cùng submenu: Vietnamese locale CP 1258, codeTable=4.
16. Bảng điều khiển...: ưu tiên Fluent, fallback MainControlDialog legacy;
    là mục mặc định của menu.
17. Giới thiệu OpenKey: ưu tiên Fluent --page about, fallback AboutDialog.
18. Giao diện → Theo hệ thống: uiTheme=0, fallback khi thiếu/sai dữ liệu.
19. Giao diện → Sáng (Light): uiTheme=1.
20. Giao diện → Tối (Dark): uiTheme=2.
21. Thoát: onOpenKeyExit, dừng engine/tray và kết thúc message loop.

Checked được đồng bộ từ globals trong updateData; icon Việt/Anh còn phụ thuộc
vUseGrayIcon. Nhãn menu tray vẫn là tiếng Việt cố định, không đổi theo ngôn ngữ
hiển thị của Fluent. Theme đọc uiTheme độc lập; refresh checked trước popup.
Popup chính là WS_POPUP tùy biến bo góc 12 DIP bằng window region, top/bottom
padding 10 DIP, hover bo góc; nền/chữ/checked/arrow/submenu tự vẽ theo uiTheme.
Snapshot immutable từ HMENU giữ toàn bộ ID/checked/default/disabled/nhãn/order.
Không thêm registry; System vẫn dùng UISettings foreground cùng phép tính UI.
Font menu Windows, thread DPI per-monitor scope, giới hạn work area và cuộn
khi thiếu chiều cao. Submenu ưu tiên bên phải, thiếu chỗ chuyển bên trái.
Phím lên/xuống/trái/phải/Home/End/PageUp/PageDown/Enter/Space/Escape và chọn/
cycle chữ đầu; mất activation/capture, click ngoài hoặc đổi màn hình đóng popup.
IAccessible cung cấp tên/role/state/action/location và event popup/focus; client
giữ provider sau đóng vẫn có snapshot, window=null/invisible, không gọi action.
COM STA/DPI scopes được cân bằng, không API private hoặc hook thêm.
High Contrast dùng HMENU native hoàn toàn; lỗi khởi tạo custom dùng fallback
HMENU owner-draw Sáng/Tối của giai đoạn trước. Native Session khôi phục data/
fType/brush trước handler. Cả hai loop chặn updateData/ModifyMenu và mở lặp,
cleanup capture/windows/GDI trước dispatch, giữ WM_QUIT cho loop ngoài.
UI theo theme chung, refresh lựa chọn tray mỗi 500 ms; mặc định theo Windows,
không ghi đè lựa chọn đã lưu. Activation chỉ nhận trang trong allowlist,
redirect sang cửa sổ Fluent đang mở và chọn đúng navigation item; fallback
legacy chỉ khi launch thất bại, không phát hiện child crash sau launch.

Nhấp trái WM_LBUTTONUP đảo Việt/Anh và cập nhật tray. Nhấp đôi
WM_LBUTTONDBLCLK gọi Bảng điều khiển. Nhấp phải WM_RBUTTONDOWN đặt foreground
cửa sổ tray, custom popup (hoặc TrackPopupMenu dự phòng) trả ID để dispatch
rồi updateData. Chuỗi sự kiện
nhấp đôi còn có thể chứa nhấp trái; cần QA thực tế, không suy ra rằng nhấp đôi
không làm đổi chế độ gõ chỉ từ hai nhánh source.

Explorer khởi động lại: nhận thông điệp đã đăng ký TaskbarCreated, nạp icon,
NIM_ADD, NIM_SETVERSION và updateData nếu thêm thành công. Tạo tray ban đầu có
nhánh timer thử lại khi tạo cửa sổ thất bại và vòng retry Shell_NotifyIcon;
chưa nghiệm thu restart Explorer, lỗi tạo icon hoặc timing.

## Engine: khởi động, một instance và hoạt động nền

1. wWinMain bỏ qua command line. Release đọc vRunAsAdmin; nếu bật và chưa admin,
   gọi ShellExecute runas rồi trả về 1. Debug không có nhánh tự elevation này.
   Chưa kiểm thử UAC hủy/lỗi hoặc ngữ cảnh quyền khác nhau.
2. AppDelegate::run tìm cửa sổ class OpenKeyVietnameseInputMethod. Nếu có,
   phát beep, SendMessage WM_USER+2019 để mở bảng điều khiển của instance cũ,
   rồi trả về trước initEngine. Đây là kiểm tra cửa sổ, không phải mutex;
   không chứng minh hai lần khởi động đồng thời không race hoặc IPC vượt quyền.
3. Init engine: SettingsService::load; registerRunOnStartup(vRunWithWindows);
   vKeyInit, chuẩn bị trạng thái phím, đọc macroData/smartSwitchKey, cài
   WH_KEYBOARD_LL, WH_MOUSE_LL và WinEvent foreground.
4. Tạo cửa sổ tray ẩn/icon, updateData, mở bảng điều khiển nếu vShowOnStartUp,
   phát beep, bắt đầu kiểm tra cập nhật nếu vCheckNewVersion, vào message loop.
   Loop giữ xử lý dialog legacy và TranslateMessage/DispatchMessage.

Quan trọng: mở engine có thể tác động startup Run entry/Scheduled Task ngay
trong OpenKeyInit, không chỉ khi đổi toggle. Helper registry legacy cũng có thể
tạo khóa thiếu khi đọc. Vì vậy nghiệm thu “chỉ mở beta, không chỉnh gì” vẫn cần
snapshot VM và đối chiếu startup/config. Không chạy luồng này trên host để kiểm kê.
Xem [REGISTRY_INVENTORY.md](REGISTRY_INVENTORY.md).

## Mở và đóng Fluent UI

onControlPanel tìm lần lượt OPENKEY_UI_PATH, UI cạnh executable, thư mục
OpenKey.UI bên cạnh và đường build Debug/Release theo kiến trúc. Chuẩn hóa
đường dẫn, bỏ qua thư mục/tệp thiếu, ShellExecute open; result>32 được coi là
launch được chấp nhận. Nếu mọi candidate thất bại, mở legacy.

Chốt này không kiểm tra UI sống sau launch: UI crash sau khi ShellExecute đã
chấp nhận không tự kích hoạt fallback. run-ui-debug.ps1 có probe riêng; không
nhầm probe script với hành vi nút tray.

UI là tiến trình riêng. AppInstance dùng key OpenKey.UI.MainWindow; instance
phụ redirect activation rồi Exit, instance chính đưa ActivateMainWindow về
dispatcher, restore cửa sổ nếu minimized và đưa foreground. Trước tạo XAML,
UI đặt language override từ uiDisplayLanguage. Chạy riêng OpenKey.UI.exe
không tự khởi động engine hoặc cài hook; có UI không chứng minh đang gõ được.

Đóng cửa sổ Fluent lưu geometry/maximized, không gọi freeEngine hoặc gửi lệnh
thoát engine. Tray/hook của engine đang chạy không thuộc cửa sổ UI. Draft trang
không được lưu bền chỉ vì đóng UI. Ngược lại, Thoát từ tray dừng engine nhưng
source onOpenKeyExit không đóng tiến trình Fluent đang mở; cần QA riêng trạng
thái UI khi engine đã thoát.

## IPC, cập nhật và thoát

- WM_USER+2019: kích hoạt Bảng điều khiển ở engine hiện có.
- WM_USER+2020: nhận thay đổi Preferences, reload settings; cập nhật bảng mã/
  chính tả/startup khi các trường tương ứng đổi, refresh legacy và tray.
  Fluent hiện vẫn polling 500 ms khi trang Loaded để nhận thay đổi từ tray/hotkey.
- WM_USER+2021: handler tray gọi onOpenKeyExit; chưa có sender Fluent trong
  source hiện tại. Đây không phải bằng chứng một nút thoát Fluent đã hoàn tất.

Kiểm tra cập nhật legacy chạy worker và UI timer; chỉ thông báo startup khi
có bản mới. Owner/token ngăn kết quả trả về dialog đã đóng. Khi đồng ý,
UpdateLaunchService phải launch thành công mới gọi onOpenKeyExit; bundle
Fluent có policy chặn updater engine-only. Fluent About dùng service chung
và luồng xác nhận/mở releases riêng. Chưa chạy cập nhật thật; không mở updater
để nghiệm thu beta, xem MIGRATION_FLUENT_BETA.md.

onOpenKeyExit có guard exiting, đánh dấu trước khi stopUpdateCheck, freeEngine
(unhook mouse/keyboard/WinEvent), removeSystemTray (NIM_DELETE), PostQuitMessage.
Sau message loop cũng stopUpdateCheck. Source guard xác nhận thứ tự hiện tại,
không chứng minh từng API cleanup thành công hoặc không còn hook ở mọi lỗi.

## Kiểm thử tự động và checklist thực tế

test-tray-contract.ps1 đối chiếu source/JSON: 21 command mapping cùng label,
ID, vị trí, checked; coverage/thứ tự/default/submenu, click/IPC/recovery,
single-instance, init/cleanup, fallback, activation UI và thứ tự thoát.
34 nhóm chạy trong test-all. Regex chỉ khóa các dạng source hiện tại, không
thay cho C++ parser, dispatch message, GUI, UAC hoặc hook acceptance.

- [ ] Khởi động engine hai lần và nhiều lần đồng thời, cả x64/x86; đúng một
  tray/hook, instance sau mở đúng cửa sổ; thử khác mức quyền trên VM.
- [ ] UI-only không engine: không nhận nhầm thành gõ tiếng Việt; mở qua tray,
  mở lại UI, restore từ minimized; xác nhận không sinh nhiều cửa sổ chính.
- [ ] Mọi lệnh tray/checked, ba kiểu gõ/năm bảng mã và trường hợp inputType=3.
- [ ] Nhấp trái/đôi/phải, bật/tắt Việt/Anh đúng, shortcut chuyển mã và balloon
  không chặn gõ; Fluent và legacy phản ánh lựa chọn khi điều hướng.
- [ ] Các entry Gõ tắt/Chuyển mã/Giới thiệu mở đúng trang Fluent kể cả UI đang
  mở/minimize; UI thiếu/launch lỗi có fallback legacy như dự kiến.
- [ ] Theme thay từ tray/Nâng cao đồng bộ, mở lại giữ lựa chọn, Theo hệ thống
  phản ứng với Windows/High Contrast; đổi theme không mất draft/input.
- [ ] Đóng UI rồi vẫn gõ/tray hoạt động; mở lại giữ geometry nhưng không draft;
  thoát engine khi UI mở, xác nhận trạng thái thực tế và không có hook dư.
- [ ] TaskbarCreated/restart Explorer trên VM, lỗi tạo tray, sleep/resume và
  sign out. Không restart Explorer hoặc kill tiến trình host để chạy source test.
- [ ] Startup thường/admin, hủy UAC, shortcut và rollback đích cũ; đối chiếu
  cả dữ liệu registry và Scheduled Task trước/sau khởi động.

Các ô chưa hoàn tất cần nghiệm thu môi trường thật; không đánh dấu đạt từ source guard.
