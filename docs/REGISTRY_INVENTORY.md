# Kiểm kê dữ liệu OpenKey Windows

Baseline mã nguồn ngày 2026-10-11, phục vụ giữ dữ liệu khi thử Fluent/nâng cấp/
rollback. Không phải bản xuất registry hoặc công cụ khôi phục. Danh sách đầy đủ
tên, kiểu và nhóm nằm ở [Tests/RegistryInventory.json](../Tests/RegistryInventory.json).
Có 46 giá trị trong khóa ứng dụng và 1 giá trị Run startup; không truy cập dữ
liệu thực tế của người dùng để lập tài liệu này.

## Khóa ứng dụng dùng chung

`HKEY_CURRENT_USER\SOFTWARE\TuyenMai\OpenKey` dùng chung giữa engine legacy và
Fluent của cùng tài khoản, kể cả chạy từ thư mục khác. Không có profile beta riêng.

- 37 `REG_DWORD` nhóm `shared-preferences`: chế độ Việt/Anh, kiểu gõ, bảng mã,
  chính tả/gõ nhanh, nhớ trạng thái theo ứng dụng, phím tắt, tương thích, startup,
  cập nhật và 9 tùy chọn chuyển mã. Tên đầy đủ ở JSON; đọc/ghi bởi
  `SettingsServiceStore.cpp`, dùng bởi cả engine và UI.
- 2 `REG_DWORD` nhóm `legacy-only`: `vRunAsAdmin`, `vCreateDesktopShortcut`.
  `vUseMacro` và `vUseMacroInEnglishMode` đã chuyển vào shared-preferences,
  có toggle ở MacroPage, giữ nguyên tên/default/reset. Hai trường legacy-only
  còn lại chưa thuộc model Preferences Fluent;
  vẫn được legacy load/ghi, không được bỏ khi sao lưu hoặc thay executable.
- 2 `REG_BINARY` nhóm `shared-data`: `macroData` chứa danh sách gõ tắt;
  `smartSwitchKey` chứa trạng thái theo ứng dụng. Giữ nguyên byte khi sao lưu;
  không tự sửa/giải mã/import để kiểm kê. MacroService và engine/legacy dùng
  cùng tên `macroData`; xuất macro văn bản không thay thế sao lưu binary.
- 3 `REG_DWORD` nhóm `fluent-only`: `uiDisplayLanguage` (0 Việt/1 Anh,
  thiếu hoặc không hợp lệ về Việt), `uiTheme` (0 hệ thống/1 sáng/2 tối,
  thiếu hoặc không hợp lệ về hệ thống), `uiWindowMaximized` (chỉ 1 là bật).
- 2 `REG_BINARY` nhóm `fluent-only`: `uiWindowSize` là record ba DWORD
  version=1/width/height theo đơn vị logical; fallback 1120×800, kiểm tra
  width 640–8192 và height 400–8192. `uiWindowPosition` là record
  DWORD version=1/LONG x/LONG y; tọa độ trong ±1.000.000, được điều chỉnh
  về vùng làm việc khi mở cửa sổ. Không có vị trí mặc định cố định được lưu.

Ngôn ngữ UI `uiDisplayLanguage` độc lập với chế độ gõ `vLanguage`. Model chung
chỉ ghi các trường đã đổi khi dùng saveChangedPreferences, không xóa các giá
trị khác; nhiều lần ghi không phải giao dịch nguyên tử. UI lưu riêng ngôn ngữ,
theme và geometry, không gửi reload engine từ các helper này.

Các đường registry C++ hiện tại nằm trong sáu provider:
`OpenKeyHelper.cpp`, `SettingsServiceStore.cpp`, `MacroService.cpp`,
`UiLanguageSettings.h`, `UiThemeSettings.h`, `UiWindowSizeSettings.h`.
Legacy gọi thêm qua APP_GET_DATA/APP_SET_DATA và loadSetting/saveSetting.
Helper legacy có thể tạo khóa ứng dụng chưa tồn tại ngay trong đường đọc;
không coi việc mở engine legacy là thao tác chỉ đọc tuyệt đối.

## Mặc định không được gộp nhầm

Giữ nguyên hiện trạng, chưa thay hành vi hoặc nghiệm thu các trường hợp dưới đây:

- `vSwitchKeyStatus`: Preferences thiếu giá trị dùng `0x5A00025A` (Alt+Z),
  engine loadSetting thiếu giá trị dùng `0x7A000206`; restoreDefaults dùng
  DefaultSwitchKeyStatus=`0x5A00025A`. Cần kiểm tra riêng tài khoản chưa có khóa
  so với dữ liệu đã lưu; không ghi default vào registry chỉ để đồng nhất.
- `vUseMacro`: load thiếu giá trị dùng 1; restoreDefaults ghi 0.
- `vSupportMetroApp`: Preferences/load thiếu giá trị dùng 0;
  restoreDefaults ghi 1.
- `vUseMacroInEnglishMode`, `vRunAsAdmin`, `vCreateDesktopShortcut`:
  load thiếu giá trị dùng 0. restoreDefaults không đặt lại hai trường cuối;
  cũng không đặt lại `vCheckNewVersion` hoặc các giá trị Fluent-only/binary.
- `convertToolHotKey`: Preferences thiếu giá trị dùng `0xFE0000FE`;
  engine còn chuẩn hóa giá trị 0 thành EmptyHotKey khi load, không tự lưu ngược.

Các mặc định còn lại lấy từ Preferences trong `SettingsService.h` và
restoreDefaults trong `SettingsService.cpp`; chúng không nhất thiết là giá trị
đang có trên máy người dùng. Kiểm kê không đánh dấu UI/control baseline đã đủ.

## Startup và dữ liệu ngoài khóa ứng dụng

`HKCU\Software\Microsoft\Windows\CurrentVersion\Run`, giá trị `OpenKey`
kiểu `REG_SZ`, lưu executable được đặt trong dấu nháy. OpenKeyHelper ghi/xóa
riêng giá trị này, không xóa toàn bộ khóa Run.

Startup admin dùng Scheduled Task tên `OpenKey`; desktop shortcut là tệp.
Chúng không nằm trong bản export khóa ứng dụng. Không tự tạo/xóa/đổi task,
shortcut hoặc Run entry khi thử giữ dữ liệu. Đích executable cần đối chiếu
riêng sau rollback; trạng thái DWORD không chứng minh đích startup đúng.

Checker prerequisite chỉ đọc `HKLM\SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64`
hoặc `x86`, các giá trị `Installed` và `Version`, trên Registry32/Registry64.
Đây là dữ liệu runtime hệ thống, không thuộc 46 giá trị ứng dụng trong JSON,
không sao lưu/import chúng để rollback OpenKey. Inventory không bao phủ
registry do Windows/COM/SDK/bộ cài tự dùng ngoài các lời gọi source ứng dụng.

Component TSF thử nghiệm có namespace đăng ký riêng, ghi trong
`experimentalTextServiceRegistration` của JSON: COM CLSID
`{727F9E3B-0491-4D5C-B33D-64D59BE28A70}` ở HKLM Software/Classes/CLSID;
profile/category do API TSF đăng ký. Không thuộc dữ liệu cấu hình/macro HKCU.
`CaretTextService.cpp` chỉ đọc các thiết lập đang có, không thêm giá trị ứng
dụng; `Registration.cpp` chỉ ghi/xóa đăng ký COM/TSF của chính component.
Chạy script đăng ký cần Administrator, không tự đổi default input profile.
Xem [thiết lập và gỡ TSF](TEXT_SERVICE.md).

## Kiểm tra tự động và nghiệm thu còn lại

`test-registry-inventory.ps1` đối chiếu tên đọc/ghi, kiểu của helper Fluent,
hai đường dẫn HKCU và danh sách provider với source do git ls-files liệt kê
(tracked và untracked, trừ ignored build/generated files). Có 11 nhóm kiểm
tra, chạy trong test-all; không mở registry, chạy engine hoặc khôi phục dữ liệu.
Scanner nhận các dạng literal/macro hiện tại, không phải parser C++ hoàn chỉnh;
khi thay cách truy cập registry cần cập nhật kiểm kê và kiểm tra coverage.

Theo [MIGRATION_FLUENT_BETA.md](MIGRATION_FLUENT_BETA.md), trên VM/tài khoản thử:

- [ ] Sao lưu đầy đủ khóa ứng dụng, ghi nhận khóa/giá trị không tồn tại và kiểu
  dữ liệu; dùng macro không nhạy cảm, ghi riêng startup task/Run/shortcut.
- [ ] Mở beta nhưng không chỉnh tùy chọn: đối chiếu dữ liệu trước/sau, gồm
  legacy-only và smartSwitchKey; tách thay đổi geometry/UI dự kiến khỏi mất dữ liệu.
- [ ] Đổi đúng một tùy chọn: chỉ các giá trị dự kiến thay đổi, giá trị chưa biết
  từ phiên bản khác vẫn còn, macro binary không bị ghi lại ngoài thao tác macro.
- [ ] Thử macro UTF-8/nhiều dòng và quay lại engine cũ, đối chiếu từng mục.
- [ ] Kiểm tra khóa thiếu, kiểu sai và các default khác nhau nêu trên;
  không tự reset/import dữ liệu khi thấy lỗi.
- [ ] Kiểm tra startup thường/admin và shortcut sau rollback, cùng x64/x86.

Caret deployment bổ sung đọc HKLM cho CLSID TSF riêng ở cả hai registry view,
và đọc `ProgramFilesDir` của Windows để xác định thư mục Program Files native.
Đây là đọc metadata triển khai, không thêm giá trị settings ứng dụng. Đăng ký/gỡ
TSF chỉ qua helper nâng quyền; kiểm tra trạng thái không ghi registry.

Chưa đánh dấu nâng cấp hoặc bảo toàn registry thực tế đạt chỉ vì source test pass.
