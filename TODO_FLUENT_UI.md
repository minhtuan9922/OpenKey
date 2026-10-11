# TODO: Chuyển giao diện OpenKey Windows sang Fluent Design

## Sửa menu tray bị bảng icon ẩn che (2026-10-11)

- [x] Mở menu khi thả chuột phải (WM_RBUTTONUP), không mở giữa chuỗi nhấn/thả.
- [x] Đặt menu chính và submenu tùy biến vào lớp topmost, đưa lên trên khi
  hiển thị; submenu không lấy focus. Giữ giới hạn work area và hủy cửa sổ khi đóng.
- [x] Thêm source guard cho thứ tự hiển thị/lifecycle; 37/37 tray contract đạt.
- [x] Engine Debug x64, Release x64/x86 build 0 cảnh báo/0 lỗi; 18/18 bộ hồi quy
  đạt, report `dist/regression-4cf41d23cfee40428053f32721ee5415/summary.json`.
- [ ] Nghiệm thu thực tế từ icon trong bảng icon ẩn và icon trực tiếp trên taskbar,
  submenu, click ngoài/Escape, DPI và nhiều màn hình; chưa xác nhận bằng GUI.

## Mục tiêu

### Bộ cài offline một file (2026-10-11)

- [x] Thêm Installer/OpenKey.iss và build-setup.ps1: một EXE tự chọn x64/x86,
  cài theo tài khoản, Start Menu cho engine/Settings, Desktop và chạy app tùy chọn.
- [x] Kiểm tra VC++ đúng kiến trúc/minimum, consent và UAC khi thiếu, hash trước
  chạy runtime offline; fail closed khi hủy/lỗi, restart rồi chạy lại khi cần.
- [x] Gỡ qua Windows, giữ settings/macro/runtime; không tự kill process hoặc
  thay startup. Tiếng Việt/Anh và wizard Sáng/Tối theo hệ thống.
- [x] 24 source contract + 38 native Pascal fixtures; verify 566 input compiler,
  ZIP/hash/PE/chữ ký runtime PS7/5.1 đạt. Full hồi quy 17/17 mỗi shell.
- [x] Tạo setup khoảng 59,2 MiB; đường dẫn/hash/build và hướng dẫn ở INSTALLER.md.
- [ ] Nghiệm thu cài/gỡ/UAC/shortcut/nâng cấp thật trong Sandbox/máy sạch.
- [ ] Rà quyền phân phối runtime/dependency, ký số và phát hành công khai.

### Giảm giật khi chuyển menu tray (2026-10-11)

- [x] Vẽ vào back buffer riêng cho từng panel rồi BitBlt một lần; tái sử dụng
  DC/bitmap, giải phóng khi đóng panel, chỉ invalidate hai dòng đổi hover.
- [x] Bỏ repaint và cập nhật MSAA location khi con trỏ vẫn ở cùng một dòng.
- [x] Hover submenu chờ 200 ms; giữ submenu cũ khi rê chéo qua dòng cha,
  hủy chuyển khi vào submenu; click/bàn phím/MSAA vẫn phản hồi ngay.
- [x] 297 kiểm tra popup/kiến trúc, gồm deadline/cancel/chuyển submenu,
  300 frame dùng lại buffer và cân bằng GDI sau mỗi lifetime panel.
- [x] Người dùng xác nhận popup ổn sau sửa giật (2026-10-11).
- [ ] Nghiệm thu riêng đổi Sáng/Tối và màn hình DPI hỗn hợp; xác nhận “ổn”
  không được coi là đã chạy toàn bộ ma trận kiểm thử.

### Popup tray tùy biến bo góc (2026-10-11)

- [x] Thay popup chính bằng cửa sổ WS_POPUP bo góc 12 DIP, khoảng đệm 10 DIP,
  hover bo góc và mũi tên submenu tự vẽ đúng màu Sáng/Tối/Theo hệ thống.
- [x] Giữ snapshot HMENU và 21 ID/handler, checked/default/disabled; không đổi
  dữ liệu registry. Native menu dự phòng khi High Contrast hoặc khởi tạo lỗi.
- [x] Bàn phím lên/xuống/trái/phải/Home/End/PageUp/PageDown/Enter/Space/Escape,
  chọn/cycle chữ đầu; chuột/submenu/click ngoài, cuộn khi thấp, giới hạn work area.
- [x] Provider IAccessible cho tên/role/trạng thái/action/location, event menu/
  focus và lifetime snapshot sau đóng; COM/DPI scope cân bằng, không API private.
- [x] 169 tests popup model/MSAA/offscreen mỗi kiến trúc, xem bitmap Sáng/Tối;
  giữ 303 tests fallback renderer; ghi nhận hồi quy/build ở FLUENT_UI_QA.md.
- [ ] Người dùng kiểm thử popup thực tế: submenu/chuột/bàn phím/ngoài menu,
  Narrator, DPI hỗn hợp, Win10/11, High Contrast và resize/taskbar nhiều màn hình.
- [ ] Làm mới ZIP/Sandbox sau nghiệm thu; gói cũ chưa chứa popup tùy biến.

### Màu Sáng/Tối thực sự cho nội dung menu tray (2026-10-11)

- [x] Owner-draw nền/chữ/hover/separator/checked/default/disabled của HMENU và
  submenu; dùng uiTheme chung, mặc định theo UISettings Windows.
- [x] Giữ native menu loop/lệnh/arrow, tên MSAA; font menu theo DPI; mnemonic/
  chọn và cycle chữ đầu; High Contrast dùng menu native, không API private.
- [x] Session quản lý và khôi phục itemData/fType/brush/GDI, ngăn ModifyMenu và
  mở lại menu trong nested loop; kiểm thử offscreen tích hợp test-settings.
- [x] 303 checks menu/kiến trúc; hồi quy 16/16 trên PS7 và PS5.1 đạt, build
  engine Debug/Release x64/x86 sạch 0 cảnh báo/0 lỗi. Báo cáo ở FLUENT_UI_QA.md.
- [ ] QA trực tiếp light/dark/system, màu arrow/viền native, phím mũi tên/
  Enter/Escape, submenu, click ngoài, Narrator, màn hình DPI hỗn hợp và HC.
- [ ] Làm mới ZIP/Sandbox sau nghiệm thu; gói beta cũ chưa chứa thay đổi tray.

### Gõ tắt, theme và mở trang Fluent từ tray (2026-10-11)

- [x] Thêm vUseMacro/vUseMacroInEnglishMode vào Preferences/store/engine mapping,
  toggle song ngữ ở MacroPage; lưu chọn lọc và refresh 500 ms, giữ default/reset cũ.
- [x] Tray thêm Giao diện → Theo hệ thống/Sáng/Tối, dùng uiTheme chung với
  Cài đặt nâng cao. Mặc định khi chưa lưu theo Windows; giữ lựa chọn người dùng.
  UI đang mở refresh theme, combo đồng bộ; popup hiện dùng cửa sổ tùy biến,
  owner-draw HMENU giữ làm dự phòng như các mục trên.
- [x] Gõ tắt/Chuyển mã/Giới thiệu ưu tiên Fluent đúng trang qua --page allowlist,
  cả initial/redirected activation; fallback dialog legacy nếu launch thất bại.
- [ ] QA tray mở đúng trang khi UI đóng/đang mở/minimize, đổi theme từ hai phía,
  theo Windows/High Contrast, macro bật/tắt và gõ tắt trong chế độ Anh thực tế.
- [x] Link lại engine Debug x64 sau khi người dùng thoát OpenKey: All Debug x64
  build sạch, 0 cảnh báo/0 lỗi; cả All Debug/Release x64/x86 đã build sạch.
  Hồi quy 16/16 PS7/5.1 đạt; chưa làm mới ZIP/Sandbox.

### Chuẩn bị kiểm thử Sandbox (2026-10-10)

- [x] Đưa 13 ca tích hợp staging vào test-packaging/test-all (2026-10-11): 10 đầu vào lỗi bị từ chối trước tạo session, x64/x86 không runtime và chạy lặp tạo GUID mới; kiểm tra digest, metadata, bản sao helper, Input chỉ đọc/Logs riêng, mạng/clipboard tắt. Fixture không chạy executable/installer/guest; đạt PS7/5.1. Các thư mục sandbox sinh bởi test này chỉ chứa ZIP giả, không dùng để nghiệm thu.

- [x] Tự động chuẩn bị runtime offline khi chỉ định đủ RuntimePath/RuntimeExpectedSha256 (2026-10-11): kiểm tra SHA-256, chữ ký Microsoft, kiến trúc và minimum beta trước/sau copy; tạo hướng dẫn guest và metadata session. 54/54 fixture Sandbox PS7/5.1 đạt; tích hợp bộ cài thật x64/x86 đạt; sai kiến trúc không tạo staging; luồng không runtime giữ nguyên. Không mở Sandbox hoặc chạy bộ cài.

- [x] Bổ sung runtime offline x64/x86 cho hai session mới ngày 2026-10-11: FileVersion 14.51.36247.0, đúng ProductName kiến trúc, Authenticode Valid/Microsoft Corporation, hash trước/sau copy và kiểm tra lại PS7/5.1 đạt. x64 tái dùng bộ cài cũ, x86 tải permalink Microsoft; từng Input có OFFLINE_RUNTIME.md. Không chạy/cài installer, không đổi ZIP/.wsb, không xác nhận quyền phân phối hoặc nghiệm thu máy sạch. Tên/hash ở BETA_PACKAGING.md.

- [x] Làm mới beta x64/x86 và chuẩn bị hai session Sandbox riêng ngày 2026-10-11 sau dọn helper/liên kết/khóa About workflow: Release All sạch, 281 payload/gói/hash/PE verifier PS7/5.1 đạt, resources 4/4, executable/notice khớp nguồn; 27/27 fixture Sandbox đạt hai shell. Tên/hash/.wsb mới ở BETA_PACKAGING.md. Không mở guest/cài runtime, không thay session cũ; các phiên mới không kèm VC++ Redistributable, máy sạch chưa nghiệm thu.

- [x] Tạo `.wsb` với đầu vào chỉ đọc, log riêng, mạng/clipboard host tắt.
- [x] Xác minh ZIP trước staging và trước giải nén guest; báo thiếu runtime, không tự cài.
- [x] 27 ca harness Sandbox đạt PS7/PS5.1; 13 regression suites đạt mỗi shell.
- [ ] Mở máy ảo thật và kiểm tra prerequisite/UI/engine; chưa nghiệm thu máy sạch.

### Mục tiêu tổng thể

- [ ] Thay giao diện Win32 Dialog hiện tại bằng giao diện Fluent hiện đại.
- [ ] Giữ nguyên engine gõ tiếng Việt, keyboard hook, macro và chuyển mã.
- [ ] Không làm thay đổi dữ liệu cấu hình hiện có của người dùng.
- [ ] Hỗ trợ giao diện sáng, tối và chế độ theo hệ thống.
- [ ] Giữ OpenKey nhẹ, khởi động nhanh và tiếp tục hoạt động ở system tray.
- [ ] Có thể quay lại giao diện Win32 cũ trong thời gian migration.

## Phạm vi mã nguồn hiện tại

Các thành phần nghiệp vụ cần giữ và tái sử dụng:

- `Sources/OpenKey/engine/`: engine xử lý tiếng Việt dùng chung.
- `OpenKey.cpp`: keyboard hook và gửi phím.
- `OpenKeyManager.cpp`: khởi tạo engine, cấu hình và cập nhật.
- `OpenKeyHelper.cpp`: registry, clipboard và tiện ích Windows.
- `SystemTrayHelper.cpp`: biểu tượng và menu system tray.

Các thành phần giao diện cần thay thế dần:

- `MainControlDialog.cpp`
- `MacroDialog.cpp`
- `ConvertToolDialog.cpp`
- `AboutDialog.cpp`
- `BaseDialog.cpp`
- Các dialog và control trong `OpenKey.rc` / `resource.h`
- Phần quản lý dialog trong `AppDelegate.cpp`

## Kiến trúc đề xuất

- Sử dụng **WinUI 3 + C++/WinRT** để có Fluent Design native.
- Tạo project giao diện mới trong solution, không sửa trực tiếp toàn bộ UI cũ ngay từ đầu.
- Tách thao tác cấu hình khỏi Win32 control thành một lớp API/ViewModel dùng chung.
- Trong giai đoạn migration, giữ project Win32 cũ làm bản fallback.
- Ưu tiên mô hình triển khai **unpackaged, self-contained** để tiếp tục phát hành dạng ZIP; xác nhận kích thước và yêu cầu runtime trước khi chốt.
- Mức hệ điều hành dự kiến cho UI mới: Windows 10 1809 trở lên. Cần quyết định cách duy trì bản legacy cho Windows cũ.

## Giai đoạn 0 — Khóa phạm vi và tạo baseline

- [ ] Chụp ảnh tất cả màn hình hiện tại để làm tài liệu đối chiếu.
- [x] MAIN_CONTROL_BASELINE.md kiểm kê control chung/bốn tab Win32, ID → registry/default/trang Fluent và side effect (2026-10-11); ghi rõ default load/reset khác nhau, clipboard đảo, legacy-only, dependency khác biệt và reset không đồng bộ toàn bộ startup. Chỉ baseline source, GUI chưa nghiệm thu.
- [x] Kiểm kê Macro Dialog và đối chiếu MacroPage/MacroService trong MACRO_CONVERT_BASELINE.md (2026-10-11): CRUD, nhập/xuất, tự viết hoa, buffer/UTF-8, lưu ngay, khác biệt trùng tên/đổi tên/xác nhận/bản nháp và giới hạn poll. Checklist GUI/engine vẫn chờ nghiệm thu.
- [x] Kiểm kê Convert Tool Dialog và đối chiếu ConvertPage/shared services trong MACRO_CONVERT_BASELINE.md (2026-10-11): năm bảng mã, swap, casing/bỏ dấu, clipboard-only legacy, thông báo/hotkey/tray, vùng text bổ sung ở Fluent và giới hạn UI-only. Không đánh dấu Sandbox hoặc clipboard/hotkey thật đạt.
- [x] Kiểm kê 18 lệnh tray từ source, ID/label/handler/đối số/checked/menu cha ở Tests/TrayContract.json; TRAY_LIFECYCLE_BASELINE.md ghi nhấp chuột, default, encoding submenu, Explorer recovery và các entry còn mở legacy. 29 nhóm source guard tray/lifecycle đạt PS5.1 và được đưa vào test-all; tương tác tray thực tế vẫn cần QA.
- [x] Kiểm kê registry từ source Windows: 46 giá trị khóa ứng dụng + 1 Run startup, nhóm/kiểu ở Tests/RegistryInventory.json; REGISTRY_INVENTORY.md ghi dữ liệu binary, Fluent-only, khác biệt default legacy, task/shortcut và prerequisite riêng. 10 nhóm source guard đạt PS7/5.1, tích hợp test-all; không truy cập registry thật. Nghiệm thu nâng cấp/rollback vẫn chưa hoàn tất.
- [x] TRAY_LIFECYCLE_BASELINE.md ghi startup/elevation, FindWindow single-instance, init settings/startup/macro/hook/tray, tách UI/engine, activation/fallback, IPC/update và exit cleanup; lưu ý side effect startup khi mở engine, UI crash sau launch không tự fallback và giới hạn race/IPC. Chỉ kiểm kê source, không chứng minh runtime lifecycle đạt.
- [x] Checklist thủ công nhóm màn hình chính/default/reset/startup ở MAIN_CONTROL_BASELINE.md, kết hợp MACRO_CONVERT_BASELINE.md và TRAY_LIFECYCLE_BASELINE.md cho macro/chuyển mã/tray/lifecycle. Các ca vẫn để trống, chưa đánh dấu chạy thực tế đạt.
- [ ] Build và lưu kết quả baseline cho `Debug x64`, `Release x64` và `Release x86`.
- [ ] Quyết định có tiếp tục hỗ trợ Windows trước Windows 10 1809 hay không.

Tiêu chí hoàn thành:

- [ ] Có danh sách đầy đủ tính năng và cấu hình cần bảo toàn.
- [ ] Bản Win32 hiện tại vẫn build và chạy bình thường.

## Giai đoạn 1 — Tạo nền tảng WinUI 3

- [x] Cài workload/template Windows App SDK và C++/WinRT cần thiết trong Visual Studio.
- [x] Tạo project WinUI 3 C++ mới, dự kiến đặt tên `OpenKey.UI`.
- [x] Thêm project mới vào `OpenKey.sln` mà không xóa project Win32 hiện tại.
- [x] Chọn phiên bản Windows App SDK stable và cố định phiên bản dependency.
- [x] Cấu hình build cho `Debug/Release` và `x64/x86`; cả bốn cấu hình UI đã build sạch, runtime/máy sạch vẫn cần QA riêng.
- [x] Cấu hình ứng dụng unpackaged.
- [x] Thử nghiệm self-contained deployment và đo kích thước đầu ra (Debug x64: khoảng 140 MiB, 241 tệp).
- [ ] Thiết lập icon, tên ứng dụng, DPI awareness và application manifest.
  - [x] Nhúng icon.ico gốc vào AppResources.rc (IDI_OPENKEY_UI=101), gán AppWindow icon bằng handle shared/interop lúc khởi tạo; lỗi icon không chặn startup. Không tạo asset mới hoặc yêu cầu file .ico ngoài ZIP. Tên cửa sổ OpenKey và PerMonitorV2 manifest giữ nguyên.
  - [x] Thêm test-ui-binary.ps1: kiểm tra PE architecture, icon group/sáu frame byte khớp asset và manifest PerMonitorV2 nhúng bằng LOAD_LIBRARY_AS_DATAFILE, không chạy GUI/code ứng dụng. package-beta gọi checker trước tạo stage. 4/4 đạt trên Debug/Release x64/x86 bằng PS7/5.1; build bốn cấu hình sạch, static analysis Debug x64 0 cảnh báo/0 lỗi.
  - [ ] QA icon thực tế ở Explorer/taskbar/Alt+Tab và DPI khác nhau; custom titlebar hiện vẫn hiển thị tên OpenKey, không thêm bitmap vào vùng kéo.
- [x] Tạo cửa sổ WinUI tối thiểu và xác nhận chạy được từ script build.
- [x] Cập nhật `build.ps1` để có thể chọn build UI mới hoặc UI legacy.
- [x] Thêm script chạy Debug cho UI mới.
  - [x] Bổ sung preflight executable đang chạy, log build/launch JSON riêng, probe 3 giây và exit code hex; không chạy binary cũ sau build lỗi, không tự kill debugger. 15 ca helper/parse/preflight process giả đạt PS7/PS5.1. GUI/redirect thật vẫn cần QA, bản vá High Contrast chưa link x64 khi debugger giữ exe.

Tiêu chí hoàn thành:

- [x] Cửa sổ WinUI 3 nền tảng build và chạy ổn định.
- [x] Project Win32 cũ không bị ảnh hưởng.
- [x] Có quyết định rõ ràng về packaged/unpackaged và runtime deployment.

## Giai đoạn 2 — Tách lớp nghiệp vụ khỏi UI Win32

- [x] Tạo lớp `SettingsService` để tập trung thao tác load/reset cấu hình, làm cầu nối đầu tiên cho UI mới.
- [x] Giữ tương thích với các registry key hiện tại để không mất cấu hình người dùng.
- [x] Tạo model chứa trạng thái ứng dụng: ngôn ngữ, kiểu gõ, bảng mã và các tùy chọn.
- [x] Tạo API bật/tắt chế độ Việt/Anh.
- [x] Tạo API thay đổi kiểu gõ và bảng mã.
- [ ] Tạo API cho toàn bộ tùy chọn chính tả, gõ nhanh, macro và tương thích ứng dụng.
- [x] Tạo API import/export/thêm/xóa macro.
- [ ] Tạo API cho công cụ chuyển mã và quick convert.
- [ ] Tạo event/callback để UI tự cập nhật khi trạng thái thay đổi từ hotkey hoặc tray.
  - [x] Tổng quan refresh Việt/Anh mỗi 500 ms khi Loaded, dừng khi Unloaded; không lưu ngược lúc refresh. Đọc lại snapshot trước thao tác lưu. Đây là polling riêng Tổng quan, chưa hoàn thành event/callback toàn bộ UI; cần QA hotkey/tray thực tế.
  - [x] Tổng quan giữ lựa chọn sau lỗi lưu thay vì bị polling 500 ms ghi đè; lưu thành công sau đó cho phép đồng bộ lại, giống các trang cài đặt khác. Cần QA lỗi lưu/control thực tế.
  - [x] Kiểu gõ/Bảng mã refresh inputType/codeTable/rememberCode mỗi 500 ms khi Loaded, dừng khi Unloaded/destructor. Guard event chống lưu ngược; tạm ngưng lúc mở ComboBox hoặc lưu lỗi. Chưa hoàn tất callback toàn bộ UI; cần test tray/điều hướng/drop-down thực tế.
  - [x] Chính tả/Gõ nhanh refresh sáu toggle từ settings mỗi 500 ms khi Loaded, không lưu ngược, giữ pending edit nếu lưu lỗi. Nâng cao refresh riêng checkSpelling để cập nhật hai tùy chọn phụ thuộc; không đổi lựa chọn ngôn ngữ giao diện. Cần QA tray và điều hướng thực tế.
  - [x] Ứng dụng/Tương thích refresh bảy toggle từ settings mỗi 500 ms khi Loaded, dừng khi Unloaded/destructor; giữ logic Clipboard đảo và phụ thuộc Chromium, không lưu khi refresh, giữ pending edit nếu lưu lỗi. Chưa xác nhận GUI runtime.
  - [x] Phím tắt refresh switchKeyStatus mỗi 500 ms khi Loaded; cập nhật modifier/phím/âm báo/preview dưới guard không lưu. Giữ mã phím ngoài danh sách, tạm dừng refresh khi dropdown mở hoặc lưu lỗi. Chưa xác nhận hotkey/điều hướng runtime.
  - [x] Hoàn thiện Nâng cao: refresh bảy toggle nhập liệu/giao diện/startup/update, giữ riêng UI language và thông báo chờ mở lại. Sau lỗi lưu giữ pending edit nhưng vẫn cập nhật phụ thuộc checkSpelling; không tự lưu hoặc đăng ký startup khi refresh. Cần QA GUI thực tế.
  - [x] Gõ tắt refresh riêng autoCapsMacro mỗi 500 ms khi Loaded, dừng khi Unloaded/destructor và không đụng draft. Thêm Tải lại danh sách thủ công, xác nhận nếu draft khác snapshot; mặc định Hủy. 8 test draft mới đạt, tổng macro 32/32 mỗi kiến trúc trên PS7/5.1; chưa xác nhận dialog/control runtime.
  - [x] Chuyển mã refresh tùy chọn/bảng mã/phím tắt/thông báo mỗi 500 ms khi Loaded, dừng khi Unloaded/destructor; guard không lưu ngược, giữ lựa chọn khi dropdown mở hoặc lưu lỗi. Không thay input/result/clipboard; cần QA control runtime.
  - [x] Tải lại Gõ tắt dùng tryReadEntries: dữ liệu thiếu là danh sách rỗng, dữ liệu lỗi/không đọc được giữ nguyên snapshot và draft, hiện lỗi song ngữ. 8 nhóm regression mới, tổng macro 40/40 mỗi kiến trúc trên PS7/5.1; chưa thay các đường CRUD/import/export sang API đọc có kiểm tra.
  - [x] Bảo vệ ghi dữ liệu Gõ tắt: saveEntry/deleteEntry/import kiểu gộp/export dùng tryReadEntries và từ chối đọc lỗi hoặc dữ liệu hỏng trước khi ghi. Export không mở/truncate tệp đích khi đọc lỗi; import thay thế toàn bộ vẫn là thao tác khôi phục có chủ đích. Thêm 8 nhóm test, tổng 48/48 x64/x86 trên PS7/5.1; chưa giải quyết race giữa nhiều writer hoặc lỗi file xảy ra sau khi export bắt đầu ghi.
  - [x] Gõ tắt nạp ban đầu bằng API đọc có kiểm tra. Sau save/delete/import thành công, chỉ refresh/reset editor khi đọc lại thành công; đọc lại lỗi giữ snapshot/draft/bộ lọc và cảnh báo đã ghi dữ liệu, cần Tải lại trước thao tác tiếp. Thêm 3 test lỗi readback sau ghi, tổng 51/51 mỗi kiến trúc trên PS7/5.1; control/dialog runtime chưa xác nhận.
  - [x] Khóa thao tác Gõ tắt theo scope trên UI thread: ngăn thao tác chồng nhau khi có dialog/picker; nhả khóa khi hủy/lỗi/thoát scope. Mới/chọn dòng/Import xác nhận trước khi bỏ draft, mặc định Hủy; hủy chọn dòng phục hồi selection cũ. Bắt hresult_error của dialog xóa, chụp shortcut trước await, áp dụng theme cho dialog. 8 ca pure gate đạt x64/x86; chưa chứng minh control/dialog runtime hoặc giữ draft khi điều hướng sang trang khác.
  - [x] Export ghi tệp tạm cùng thư mục bằng CREATE_NEW, xử lý partial write/zero progress, flush/đóng rồi mới MoveFileExW thay đích; lỗi dọn đúng tệp tạm đã tạo, không truncate đích trước khi ghi. 12 ca export mới, tổng macro 71/71 đạt x64/x86 trên PS7/5.1; full regression 12 suites / 1160 checks mỗi shell đạt. Không chứng minh crash/power-loss, ACL/metadata được giữ, network filesystem hoặc khóa writer liên tiến trình; dọn tệp tạm có thể thất bại nếu hệ thống từ chối xóa.
  - [ ] Kiểm thử tập trung Gõ tắt: draft → Mới/chọn dòng/Import/Tải lại → Hủy/Tiếp tục, nhấn nhanh lặp thao tác, lỗi dialog, focus sau hủy/thành công và điều hướng khi đang có draft.
- [ ] Tách logic cập nhật phiên bản khỏi `AboutDialog` và `AppDelegate`.
  - [x] Giữ ticket/khóa nút kiểm tra xuyên tải→ContentDialog→Launcher, chỉ FinishUpdate khi hủy/lỗi/stale/hoàn tất; root null cũng cleanup, bắt lỗi khôi phục control khi đóng window. Trở lại UI apartment sau dialog/launch, dialog theo ActualTheme. Thêm 12 fixture state, tổng 62 shared/lifecycle; hồi quy 14/14 nhóm PS7/5.1 và static analysis UI Debug x64 sạch. Chưa QA dialog/browser thật hoặc đóng gói lại beta; chi tiết ở FLUENT_UI_QA.md.
  - [x] Bảo vệ OnLinkClicked trên About bằng epoch dùng chung với cập nhật: kết quả cũ sau link/update/unload không ghi đè InfoBar, link mới không nhả worker/check busy và vô hiệu hóa dialog continuation cũ. Bắt lỗi sender/tag/URI/Launcher và quay về UI apartment; lỗi link đặt severity Error. Thêm 13 fixture, tổng 50 shared/lifecycle; hồi quy 14/14 nhóm PS7/5.1 đạt, UI Debug x64 analysis sạch. Chưa mở browser/dialog thật, không hủy thao tác đã bắt đầu và chưa đóng gói lại beta. Xem FLUENT_UI_QA.md.
  - [x] Dọn declaration/implementation getContentOfUrl không còn caller cùng include/pragma Urlmon/fstream/sstream trong OpenKeyHelper; không còn đường metadata _OpenKey.tempf. Đổi cast REG_SZ sang BYTE để bỏ phụ thuộc typedef gián tiếp. Hồi quy 14/14 nhóm PS7/5.1 và static analysis engine Debug x64 sạch; ZIP beta trước chưa chứa lượt dọn mã này. Không xóa temp thực tế; tải ZIP updater vẫn URLDownloadToFileW. Chi tiết ở FLUENT_UI_QA.md.
  - [x] Làm mới beta x64/x86 ngày 2026-10-10 sau parser/worker background/Fluent lifecycle/metadata updater dùng chung: Release All không SkipBuild sạch, 281 payload mỗi ZIP/hash/PE đạt verifier PS7/5.1, 3 executable và notice khớp nguồn, UI resources 4/4. Tên/hash ở BETA_PACKAGING.md; giữ gói/Sandbox input cũ, không chạy/cài/ký/phát hành.
  - [x] Chuyển metadata updater độc lập sang UpdateService dùng chung, bỏ temp file/locale reader/parser riêng; giữ policy gate trước tải. Lỗi metadata dừng trước ZIP, exit code 1; parser chung chặn "."/"..". 23 fixture metadata, không chạy cập nhật thật; tải ZIP/cài đặt/chữ ký vẫn ngoài bước này. Chi tiết/log ở FLUENT_UI_QA.md.
  - [x] Thống nhất service tải/parse/so sánh cho legacy và About Fluent qua UpdateService; sửa packed version 2.0.5 của Fluent, bỏ parser find/stoul/GetStringAsync riêng. Coroutine tải trên background, trở lại UI apartment; state epoch chặn duplicate và kết quả/dialog/launcher cũ sau unload. 37 ca service/lifecycle mới; chưa thay downloader của updater riêng hoặc nghiệm thu UI/network thật. Xem FLUENT_UI_QA.md.
  - [x] Tiếp tục phần legacy: tải/parse metadata trên worker nền, một job tại một thời điểm; UI timer nhận kết quả, khôi phục nút, bỏ qua dialog đã đóng và thu dọn timer khi thoát. WinHTTP HTTPS chỉ HTTP 200/no redirect, timeout theo thao tác, body tối đa 64 KiB, không shared temp file. 37 fixture transport/background mới; QA mạng/UI thật, Fluent About và downloader updater riêng còn chờ. Chi tiết ở FLUENT_UI_QA.md.
  - [x] Tách parser/so sánh metadata legacy sang UpdateCheckService: trạng thái Available/UpToDate/DownloadFailed/InvalidMetadata; About/MainControl không báo "mới nhất" khi tải/parse lỗi, startup chỉ hỏi khi có bản mới. Giới hạn schema/size/depth/numeric, bỏ npos/atoi parser. Transport đồng bộ/temp file, Fluent About và updater riêng còn cần xử lý; chi tiết và kết quả xác minh ở FLUENT_UI_QA.md.
  - [x] Tách bước mở updater sang UpdateLaunchService chung cho About/MainControl/startup: policy chặn Fluent trước launch, dùng thư mục module/working directory tường minh, kiểm tra path/truncation và ShellExecute result; không thoát engine nếu mở thất bại, bật lại nút dialog. 77 injected launch cases mỗi x64/x86 và 14/14 nhóm PS7/PS5.1 đạt. Bốn All Debug/Release và static analysis Debug x64 sạch; beta mỗi ZIP 281 payload/hash/PE đạt hai shell, resources/notice khớp. Tên/hash ở BETA_PACKAGING.md. Network/parser vẫn ở OpenKeyManager, chưa hoàn thành toàn bộ service và cần QA launch/dialog thật.
- [x] Viết test cho mapping giữa model mới và các biến engine hiện tại (standalone load với API giả; runtime kiểm thử riêng).
  - [x] Thêm `test-settings.ps1 x64`/`x86`: 41/41 ca store đạt trên mỗi kiến trúc, kiểm tra 35 trường model với tên registry legacy, fallback thiếu/sai type/sai size, round-trip DWORD/hotkey bit cao, lỗi mở/tạo/ghi, đóng handle, thông báo chỉ khi toàn bộ ghi thành công và giữ macroData/khóa không liên quan. API registry/window được thay bằng fake trong bộ nhớ, không sửa cấu hình thật.
  - [x] Mở rộng `test-settings.ps1`: compile `SettingsService::load` thật với globals/helper giả; 41/41 ca mapping và 41/41 store đạt trên mỗi x64/x86, /W4 không cảnh báo. Kiểm tra 35 trường, bool/Uint8, hotkey zero/bit cao, 4 tùy chọn legacy ngoài model, free-mark và nạp lại xóa trạng thái cũ. Giữ nguyên fallback hotkey legacy `0x7A000206`, khác mặc định model `0x5A00025A`; chưa đổi hành vi.
  - [x] Thêm 38 ca `restoreDefaults` đạt trên x64/x86: kiểm tra 34 trường được ghi, free-mark, giữ khóa/macro không liên quan và các tùy chọn legacy không reset, gọi reset hai lần và nạp lại. Tổng 120 ca settings đạt, /W4 không cảnh báo, PowerShell 7/5.1. Không chạy thao tác reset thật; giữ nguyên khác biệt reset/first-load hiện có.
  - [ ] Kiểm thử nhận message và đồng bộ engine/tray/UI thực tế; harness không dispatch message, cài hook, gọi startup hoặc chứng minh ghi registry nguyên tử.
- [ ] Xác minh UI Win32 cũ vẫn dùng được qua lớp tương thích trong thời gian chuyển đổi.

Tiêu chí hoàn thành:

- [x] UI mới không cần truy cập trực tiếp Win32 HWND hoặc control ID để thao tác nghiệp vụ.
- [x] Cấu hình cũ được đọc đúng và thay đổi từ UI mới được lưu đúng.

## Giai đoạn 3 — Xây dựng Fluent Design System nội bộ

- [ ] Tạo resource dictionary cho màu sắc, typography, spacing và corner radius.
  - [x] Thêm Styles/FluentResources.xaml: page/card padding, spacing, stroke, corner radius và title wrapping; 21 card/9 trang dùng chung. Màu sắc vẫn là WinUI ThemeResource, không thay native control templates/states; giữ padding riêng của list Macro và brand About.
  - [x] Thêm test-ui-resources.ps1: 14 kiểm tra cấu trúc XML/style, bản dịch, tên control/adaptive target, navigation và fallback. Chưa chứng minh visual/DPI/Narrator.
- [ ] Dùng system accent color, không hard-code màu nhận diện cho trạng thái tương tác.
- [ ] Thiết lập Light, Dark và Default theme.
  - [x] Thêm tùy chọn Theo hệ thống/Sáng/Tối trong Nâng cao, áp dụng trên root XAML ngay và lưu riêng uiTheme; không reload engine hoặc điều hướng lại trang. Theo hệ thống đọc UISettings và theo ColorValuesChanged; cập nhật caption colors theo theme, High Contrast dùng màu hệ thống.
  - [x] Thêm 23 ca fake-registry/pure-color cho default, roundtrip, dữ liệu sai, lỗi API, retry và phân loại màu; tổng settings 245 ca mỗi kiến trúc trên PS7/5.1.
  - [ ] QA trực quan theme/Mica/caption, đổi theme Windows lúc mở app, High Contrast và nội dung draft không bị mất trên cả hai ngôn ngữ.
  - [x] Bắt hresult_error khi đăng ký HighContrastChanged trong MainWindow để lỗi subscription tùy chọn không chặn tạo cửa sổ. Giữ ActualThemeChanged/ColorValuesChanged và đọc màu High Contrast; 20 ca nguồn/XML đạt PS7/PS5.1. Cần QA mở cửa sổ và High Contrast thực tế; không suy ra event thay thế bao phủ mọi thay đổi hệ thống.
- [ ] Thêm Mica backdrop; có fallback khi hệ thống không hỗ trợ.
  - [x] Kiểm tra MicaController.IsSupported, bắt lỗi khởi tạo và dùng ApplicationPageBackgroundThemeBrush phủ title/content khi không có Mica; không phủ nền solid khi Mica khởi tạo thành công. Cần QA Win10/Win11 và chính sách hệ thống thật.
- [ ] Tạo style dùng chung cho tiêu đề trang, card cài đặt, nút, ComboBox và ToggleSwitch.
  - [x] Thêm PageStackLayoutStyle/PageGridLayoutStyle dùng chung trong App.xaml cho 7 trang người dùng yêu cầu và Chuyển mã: stretch theo viewport, padding 24,24,24,32 DIP và spacing 20 DIP, không giới hạn cột nội dung gây khoảng trống lớn bên trái. Đây mới là style page container, chưa hoàn thiện toàn bộ design system.
- [ ] Chuẩn hóa icon bằng Segoe Fluent Icons hoặc SVG riêng.
- [ ] Thiết lập trạng thái hover, pressed, focus, disabled và validation.
- [ ] Bảo đảm layout hoạt động ở 100%, 125%, 150% và 200% DPI.
- [ ] Hạn chế animation và tôn trọng thiết lập reduced motion của hệ thống.
  - [x] Mọi navigation qua NavigateTo, đọc UISettings.AnimationsEnabled tại mỗi lần điều hướng và dùng SuppressNavigationTransitionInfo khi tắt animation. Không điều hướng lại trang đang mở; không thêm animation riêng. Native control transitions và đổi thiết lập lúc chạy còn cần QA.

Tiêu chí hoàn thành:

- [ ] Có một trang gallery nội bộ hiển thị các component và trạng thái giao diện.
- [ ] Không còn màu sắc/kích thước tùy ý nằm rải rác trong từng trang.

## Giai đoạn 4 — Cửa sổ chính và điều hướng

- [ ] Tạo cửa sổ chính với custom title bar phù hợp Fluent.
  - [x] Thêm vùng titlebar XAML riêng trên NavigationView, dùng SetTitleBar/ExtendsContentIntoTitleBar và Mica hiện có; giữ caption buttons native, fallback native nếu IsCustomizationSupported=false. Chừa inset trái/phải và chiều cao theo native metrics quy đổi DPI; 3 test math mới, tổng settings 222 ca mỗi kiến trúc trên PS7/5.1. Drag/caption/Snap/High Contrast và Windows 10 runtime còn cần QA.
- [x] Dùng `NavigationView` hoặc cấu trúc điều hướng tương đương.
  - [x] Giữ MacroPage/ConvertPage bằng NavigationCacheMode=Required trong Frame hiện tại, bảo toàn draft/search/input/result khi chuyển trang; các trang khác không cache. Timer hiện có Stop khi Unloaded, Start khi Loaded; không lưu draft lên đĩa/registry, không giữ qua đóng UI. Test XML chính sách cache đạt; runtime vẫn cần kiểm thử.
  - [x] ContentFrame tắt IsNavigationStackEnabled vì menu là điều hướng ngang cấp và nút Back đã collapsed; không tích lũy BackStack/ForwardStack mỗi lần đổi trang. Test XML kiểm tra cả Frame và chính sách nút Back; chưa đo bộ nhớ/runtime.
  - [ ] Kiểm thử chuyển nhiều trang rồi quay lại Gõ tắt/Chuyển mã: draft/filter/selection/input/result còn, timer chỉ chạy khi hiện, theme/đổi trạng thái tray đúng; đóng/mở UI không khôi phục draft.
- [x] Tạo trang **Tổng quan** với trạng thái Việt/Anh và thao tác bật/tắt nhanh.
- [x] Tạo trang **Kiểu gõ & bảng mã**.
- [x] Tạo trang **Chính tả & gõ nhanh**.
- [x] Tạo trang **Phím tắt**.
- [x] Tạo trang **Ứng dụng & tương thích**.
- [x] Tạo trang **Cài đặt nâng cao**.
- [x] Nhóm tùy chọn liên quan thành các settings card dễ quét.
- [x] Thêm mô tả ngắn cho các tùy chọn kỹ thuật khó hiểu.
- [ ] Hiển thị cảnh báo khi hai tùy chọn xung đột.
  - [x] Thêm cảnh báo phụ thuộc riêng ở Tương thích/Nâng cao: Chromium đã chọn nhưng Sửa lỗi đúp từ tắt; một trong hai tùy chọn chính tả nâng cao đã chọn nhưng Kiểm tra chính tả tắt. Cảnh báo Việt/Anh, polite live region, không tự tắt lựa chọn, không thay thông báo lưu/lỗi. Dùng pure policy chung; 13 ca ma trận/giữ lựa chọn đạt x64/x86. Đây là cảnh báo tùy chọn bị khóa trong UI, không khẳng định mọi xung đột engine đã được kiểm kê.
  - [ ] QA cảnh báo khi mở trang, đổi toggle/tray, lỗi lưu và bật lại điều kiện; rà thêm các xung đột khác.
- [ ] Đồng bộ UI ngay khi người dùng đổi trạng thái bằng hotkey hoặc tray.
  - [x] Cả 8 trang lưu qua saveChangedPreferences: chỉ ghi trường khác snapshot của trang, giữ các trường không sửa đã thay đổi ở tray/hotkey/trang khác. 40 ca mới kiểm tra chọn lọc/no-op/lỗi/retry bằng API giả; đồng bộ control thời gian thực ở các trang khác vẫn cần làm và QA.
- [x] Lưu cấu hình ngay khi thay đổi, giống hành vi hiện tại.
- [ ] Giữ kích thước và vị trí cửa sổ hợp lý giữa các lần mở.
  - [x] Lưu kích thước cửa sổ thường theo DIP khi đóng qua AppWindow.Closing, khóa UI riêng uiWindowSize version 1; mặc định 1120×800 khi thiếu/lỗi, khôi phục rồi giới hạn theo work area và DPI. Không lưu lúc minimize/maximize; chưa ghi nhớ vị trí/trạng thái maximize. 21 test registry giả/DPI/work-area đạt x64/x86 trên PS7/5.1; còn cần QA đóng/mở UI và đa màn hình thật.
  - [x] Bổ sung vị trí window thường bằng khóa UI riêng uiWindowPosition version 1 (tọa độ pixel có dấu). Khi mở chọn display gần vị trí lưu, đưa HWND lên display trước tính DPI và clamp toàn bộ cửa sổ trong work area; thiếu/lỗi record thì căn giữa. Thêm 14 test, tổng window 35/35 và settings 207 ca mỗi kiến trúc trên PS7/5.1; chưa xác nhận đổi/tháo màn hình thực tế hoặc lưu trạng thái maximize.
  - [x] Thêm uiWindowMaximized và cache geometry thường trong bộ nhớ qua Changed; lưu geometry thường và maximize flag khi đóng, khôi phục geometry an toàn rồi mới maximize nếu đã lưu. Minimize không ghi hoặc được khôi phục; lỗi dữ liệu/state mặc định window thường. Thêm 12 test, tổng window 47/47 và settings 219 ca mỗi kiến trúc trên PS7/5.1; cần QA runtime maximize→đóng→mở→Restore và thứ tự Changed/DPI.
- [ ] Khi đóng cửa sổ, ứng dụng tiếp tục chạy ở tray.

Tiêu chí hoàn thành:

- [ ] Toàn bộ chức năng của `MainControlDialog` hoạt động trong cửa sổ mới.
- [ ] Không có regression khi gõ tiếng Việt trong các ứng dụng phổ biến.

## Giai đoạn 5 — Màn hình quản lý Macro

- [x] Tạo trang Macro với danh sách có tìm kiếm.
- [x] Tạo thao tác thêm, sửa và xóa macro.
- [x] Thêm xác nhận khi xóa.
- [x] Hỗ trợ import dữ liệu macro hiện có.
- [x] Hỗ trợ export/backup macro nếu định dạng hiện tại cho phép.
- [x] Hiển thị lỗi trùng từ khóa hoặc dữ liệu không hợp lệ ngay trong form.
- [x] Hỗ trợ thao tác hoàn toàn bằng bàn phím.
- [x] Kiểm thử nội dung tiếng Việt, Unicode tổ hợp và chuỗi dài.
  - [x] Thêm `test-macro.ps1 x64`/`x86`: 24/24 ca dịch vụ thật đạt trên mỗi kiến trúc (/W4 sạch, PowerShell 7/5.1). Registry giả trong bộ nhớ; file import/export riêng dưới Release bị Git bỏ qua. Bao phủ CRUD/trùng/rename, Unicode/emoji/tổ hợp, giới hạn byte UTF-8, malformed/truncated data, lỗi lưu, BOM/CRLF/merge/replace và export round-trip/multiline. Không sửa macro của người dùng hoặc kiểm thử nhận macro trong keyboard hook.

Tiêu chí hoàn thành:

- [x] Tất cả chức năng của `MacroDialog` đã được thay thế.
- [x] Dữ liệu macro cũ mở được mà không cần chuyển đổi thủ công.

## Giai đoạn 6 — Công cụ chuyển mã

- [x] Tạo trang Convert Tool theo bố cục Fluent.
- [x] Tạo lựa chọn bảng mã nguồn và bảng mã đích.
- [x] Tạo các tùy chọn viết hoa, viết thường, viết hoa đầu câu và bỏ dấu.
- [x] Thêm vùng nhập/kết quả có trạng thái rỗng và thông báo lỗi rõ ràng.
- [x] Thêm nút chuyển mã, sao chép kết quả và xóa nội dung.
- [x] Giữ quick convert qua clipboard và hotkey hiện tại.
- [x] Thay MessageBox bằng InfoBar trong WinUI và thông báo hệ thống không modal cho hotkey chạy nền.
- [x] Kiểm thử dữ liệu Unicode, TCVN3, VNI Windows và CP1258.
  - [x] Thêm `test-convert.ps1 x64`/`x86` với mã service/engine thật, 175 ca pure-text đạt trên mỗi kiến trúc và 20 ca gõ engine chạy trước. Ma trận 5 bảng mã × 6 chế độ có đối chiếu legacy, golden casing/bỏ dấu, round-trip/ký tự cuối, mã bảng không hợp lệ, NUL và chuỗi dài; API clipboard bị thay bằng trap. Chạy được PowerShell 7/5.1, không chạm clipboard/registry/hook thật.

Tiêu chí hoàn thành:

- [x] Tất cả chức năng của `ConvertToolDialog` và quick convert hoạt động đúng.
- [x] Kết quả chuyển mã giống bản legacy trên bộ dữ liệu kiểm thử.

## Giai đoạn 7 — System tray, vòng đời và About

- [x] Giữ system tray hoạt động ngay cả khi cửa sổ chính chưa được tạo.
- [x] Đồng bộ menu tray với trạng thái Việt/Anh, kiểu gõ và bảng mã.
- [x] Double-click biểu tượng tray mở hoặc focus cửa sổ WinUI.
- [x] Giữ cơ chế single-instance; lần chạy thứ hai focus cửa sổ đang có.
- [x] Kiểm thử Explorer restart và xác nhận tạo lại tray icon.
- [x] Tạo trang About theo Fluent.
- [x] Hiển thị phiên bản, build date, giấy phép và liên kết dự án.
- [x] Chuyển luồng kiểm tra cập nhật sang ContentDialog/InfoBar.
- [x] Xác minh đường dẫn `OpenKeyUpdate.exe` ở luồng cũ; Fluent beta hiện thay luồng cài bằng mở releases/cập nhật thủ công để tránh cập nhật engine-only.
- [x] Kiểm thử Run on startup với kiểu deployment mới.
- [x] Bảo đảm thoát ứng dụng giải phóng keyboard hook và tray icon.

Tiêu chí hoàn thành:

- [x] Hành vi startup, tray, update và exit tương đương hoặc tốt hơn bản legacy.

## Giai đoạn 8 — Accessibility, chất lượng và hiệu năng

- [x] Gán accessible name/description cho mọi control.
- [x] Thiết lập thứ tự tab và keyboard accelerator hợp lý.
- [ ] Kiểm thử với Narrator.
- [ ] Kiểm thử High Contrast mode.
- [x] Kiểm tra contrast theo hướng dẫn Fluent.
- [ ] Kiểm thử font scaling và DPI scaling.
  - [x] Bỏ chiều rộng cố định của các trang cài đặt; cho phép nội dung co giãn và xuống dòng.
  - [x] Thêm bố cục một cột cho Tổng quan, Gõ tắt, Chuyển mã và Giới thiệu khi cửa sổ dưới 1100 DIP.
  - [x] Cho phép tiêu đề, nhãn tùy chọn và nút văn bản xuống dòng; tránh thanh cuộn ngang.
  - [x] Chỉnh layout Tổng quan, Kiểu gõ/Bảng mã, Chính tả/Gõ nhanh, Phím tắt, Tương thích, Nâng cao và Gõ tắt theo ảnh người dùng: cùng style/lề với Chuyển mã, bỏ MaxWidth=760. Build Debug/Release x64/x86 0 cảnh báo/0 lỗi; kiểm tra 8/8 container và adaptive target XML đạt. Chưa có ảnh sau sửa các trang này; giữ bước QA tiếp theo chưa thực hiện theo yêu cầu.
  - [x] Rà ảnh trang Chuyển mã tiếng Anh/theme tối người dùng gửi: chữ đọc được, nút disabled khi rỗng. Chỉnh trang stretch không MaxWidth, giảm padding và chiều cao hai vùng text, nhãn nút en-US thành Convert; Debug/Release x64 build sạch và 383 resource key khớp. Chờ ảnh sau sửa và kiểm tra cuộn/narrow/DPI; chưa đánh dấu ma trận trực quan hoàn tất.
    - [x] Build thêm Debug/Release x86 sau sửa layout/resource: 0 cảnh báo/0 lỗi. Gói ZIP beta cũ chưa chứa thay đổi layout này.
    - [x] Ảnh sau sửa do người dùng gửi xác nhận trang Chuyển mã đã hết khoảng trống lớn bên trái, nhãn Convert và chữ tiếng Việt trong input/result đọc được ở theme tối/giao diện tiếng Anh. Chưa biết DPI/cỡ chữ, chưa kiểm tra cửa sổ hẹp hoặc toàn bộ trang.
  - [ ] Kiểm thử trực quan ở DPI 100/125/150/200% và cỡ chữ 100/150/200% theo `FLUENT_UI_QA.md` (công cụ điều khiển Windows chưa kết nối được).
- [x] Tách toàn bộ chuỗi giao diện sang resource để sẵn sàng đa ngôn ngữ.
  - [x] Thiết lập PRI/RESW cho `vi-VN` và `en-US`.
  - [x] Chuyển khung điều hướng, Tổng quan và Kiểu gõ/Bảng mã sang resource.
  - [x] Chuyển Chính tả, Phím tắt, Tương thích và Nâng cao sang resource, gồm thông báo lưu động.
  - [x] Chuyển Gõ tắt, Chuyển mã, Giới thiệu và thông báo động còn lại sang resource.
  - [x] Dịch tên/mô tả trợ năng và tooltip trên toàn bộ các trang.
- [x] Hoàn thiện tiếng Việt và bản tiếng Anh tối thiểu (408 khóa khớp nhau, không trùng; cần kiểm thử trực quan cả hai ngôn ngữ).
  - [x] Mặc định vi-VN độc lập ngôn ngữ Windows; thêm lựa chọn Tiếng Việt/English tại Nâng cao. Lưu uiDisplayLanguage riêng chế độ gõ, áp dụng khi mở lại UI. 12 ca mới bằng registry giả đạt x64/x86 trên PowerShell 7/5.1; cần QA mở lại cửa sổ để xác minh XAML/thông báo/điều hướng cùng ngôn ngữ.
- [ ] Đo thời gian khởi động nguội và bộ nhớ nền khi chỉ chạy ở tray.
- [ ] Đảm bảo UI không chặn keyboard hook hoặc làm tăng độ trễ khi gõ.
  - [x] Thêm `benchmark-engine.ps1 x64`/`x86` với harness /O2 độc lập: chạy 20 ca correctness trước đo, warm-up và 100 batch cho 4 kịch bản Telex/VNI/đặt dấu cũ-hiện đại; 4/4 chạy được trên mỗi kiến trúc. Report JSON ghi phạm vi và mean/percentile của batch average, chưa có baseline so sánh hoặc đo hook/UI/SendInput thực tế.
  - [ ] Đo khởi động nguội/tray và độ trễ end-to-end, so sánh với baseline trên cùng máy/cấu hình trước khi đánh dấu tiêu chí hiệu năng đạt.
- [ ] Chạy static analysis và sửa warning mới.
  - [x] Chạy MSVC static analysis cho UI và service dùng chung trên Debug x64; sửa C6387 (đường dẫn null) và C6262 (bộ đệm stack lớn), lượt kiểm tra lại không còn cảnh báo.
  - [x] Thêm `./build.ps1 Debug x64 UI -Analyze` để chạy lại toàn bộ phân tích.
  - [x] Chạy static analysis engine/tray Debug x64; sửa 7 cảnh báo ở clipboard, mở UI và thông báo tray (84 → 77 cảnh báo).
  - [x] Khởi tạo handle ứng dụng, dialog và control Win32 về `nullptr`; static analysis Debug x64 giảm tiếp 77 → 13 cảnh báo, không thay đổi logic engine.
  - [x] Sửa startup dùng CreateProcessW/Unicode, cân bằng COM khi tạo shortcut và kích thước bộ đệm nhập/xuất Gõ tắt; Debug x64 giảm 13 → 9 cảnh báo.
  - [x] Xử lý 9 cảnh báo engine còn lại: khởi tạo hook state, sửa chỉ số nguyên âm thứ hai và so sánh Enter/Return trùng trên Windows; static analysis engine/tray Debug x64 đạt 0 cảnh báo, 0 lỗi.
  - [x] Thêm 20 ca hồi quy Telex/VNI, đặt dấu cũ/hiện đại và kiểm tra trạng thái khởi tạo; đạt trên x64/x86 bằng `./test-engine.ps1` (20 ca gõ cũng đạt trước sửa).
  - [x] Sửa 36 cảnh báo C4018 bằng chuyển kiểu rõ ràng khi so sánh chỉ số với kích thước; engine/tray rebuild kèm static analysis đạt 0 cảnh báo, 0 lỗi trên Debug/Release x64/x86.
  - [x] Chạy lại 20 ca hồi quy engine và kiểm tra hook state sau sửa C4018: đạt trên x64/x86.
  - [x] UI rebuild kèm static analysis đạt 0 cảnh báo, 0 lỗi trên Debug/Release x64/x86; chạy lại Debug x64 trên mã cuối cùng.
  - [x] Updater rebuild kèm static analysis sạch trên Debug/Release x64/x86: thay WinExec bằng CreateProcessW, chờ kết quả giải nén, dùng staging riêng và kiểm tra thay executable; đóng handle tiến trình/thread.
  - [x] Thêm `./test-updater.ps1 x64`/`x86`, gọi helper updater thật với 6 fixture: hợp lệ, thiếu executable, ZIP hỏng, đích đang khóa, staging tồn tại, đường dẫn tiếng Việt/khoảng trắng. Đạt 6/6 trên mỗi kiến trúc; ca lỗi giữ nguyên byte tệp cũ. Sửa nạp module Archive khi kế thừa môi trường PowerShell 7.
  - [x] Củng cố extractor metadata updater: kiểm tra giới hạn/section/key/tag, tránh substr(npos), giới hạn đọc 64 Ki ký tự, bỏ wsprintf và bộ đệm đường dẫn/URL cố định. Thêm 16 ca metadata; 16 metadata + 6 fixture đạt trên x64/x86. Extractor chỉ hỗ trợ section phẳng hiện có, không phải parser JSON tổng quát. Rebuild/static analysis bốn cấu hình vẫn đạt 0 cảnh báo, 0 lỗi.
  - [x] Sửa updater chọn OpenKey64.exe/OpenKey32.exe theo kiến trúc build và kiểm tra PE signature/machine trước khi thay. 10 fixture + 16 metadata đạt trên mỗi x64/x86; tệp engine kiến trúc còn lại không đổi. Rebuild/static analysis bốn cấu hình 0 cảnh báo/0 lỗi; gói beta đã chứa updater mới. Chưa cập nhật toàn bundle UI/runtime.
  - [x] Chặn updater stable/engine-only trong Fluent bundle: UI chỉ mở releases, không thoát engine; Win32 startup/About/Control và updater trực tiếp kiểm tra marker cạnh executable, updater kiểm tra thêm thư mục đích trước tải. 5 layout policy + 26 hồi quy updater đạt trên x64/x86; không sửa registry setting kiểm tra cập nhật.
    - [x] Rebuild/static analysis engine/updater/UI cả Debug/Release x64/x86: 0 cảnh báo/0 lỗi; kiểm tra encoding trong binary, 383 resource khóa khớp nhau; gói beta đã chứa policy mới.
  - [ ] Kiểm thử trực quan ContentDialog tiếng Việt/Anh, mở releases lỗi/hủy và thông báo chặn cập nhật; chưa chạy app/updater thật trong lượt sửa policy.
  - [ ] Kiểm thử giải nén chậm, giao diện updater và cập nhật thật. Updater vẫn tải kênh stable và chỉ thay engine, chưa hỗ trợ trọn gói Fluent UI/runtime.
  - [ ] Kiểm thử lại startup admin và tạo shortcut thực tế sau sửa COM/CreateProcess (đã kiểm tra parser với đường dẫn có khoảng trắng/tiếng Việt, chưa thay đổi task hay shortcut trên máy).
  - [ ] Kiểm thử hồi quy clipboard: gõ theo chế độ clipboard, chuyển mã nhanh Unicode/HTML, clipboard đang bị khóa và nội dung không phải văn bản.
    - [x] Sửa clipboard service Fluent: dùng GetActiveWindow của UI thread làm owner, RAII đóng/unlock/free, đọc theo GlobalSize và yêu cầu terminator, kiểm tra bảng mã và cấp phát toàn bộ output trước EmptyClipboard. Thêm 17 ca API giả trong test-convert.ps1 đạt x64/x86, cùng 175 ca text + 20 ca engine; không chạm clipboard thật. SetClipboardData lỗi sau khi clear có thể để lại dữ liệu thiếu, không có rollback.
      - [x] Rebuild UI kèm static analysis Debug/Release x64/x86 sau sửa; báo cáo ConvertService cả bốn cấu hình không có defect. Bộ test đạt trên PowerShell 7 và 5.1; ZIP beta cũ chưa chứa binary mới.
      - [x] Tạo lại ZIP beta bằng package-beta.ps1 không SkipBuild trên x64/x86 sau sửa clipboard: Release All 0 cảnh báo/0 lỗi, 278 payload mỗi gói, verifier PowerShell 7/5.1 đạt và hash ba executable khớp output Release. 17 ca verifier tổng hợp đạt lại trên cả hai PowerShell; gói cũ được giữ, chưa chạy/cài/phát hành beta.
    - [x] Smoke test do người dùng xác nhận: Unicode → Unicode viết hoa/bỏ dấu, Copy → Notepad, Convert Clipboard Unicode → Notepad và Clear/disabled buttons hoạt động. Ảnh có hai kết quả TIẾNG VIỆT/TIENG VIET; các ca clipboard/Clear ghi theo xác nhận người dùng, không phải agent tự thao tác.
    - [x] Sửa CF_HTML Fluent: giữ nguyên header/context/thẻ/thuộc tính, chuyển text node UTF-8 và tính lại EndHTML/EndFragment theo byte. 35 ca Clipboard giả đạt x64/x86 (thêm 18 ca); build Debug UI hai kiến trúc 0 cảnh báo/0 lỗi, đã link được cả bản vá HighContrastChanged.
    - [x] Mở rộng CF_HTML cho context -1 với cặp fragment marker chuẩn và StartSelection/EndSelection tùy chọn. Ánh xạ offset qua text/thẻ khi độ dài UTF-8 thay đổi; hỗ trợ caret/fragment rỗng và selection xuyên nhiều thẻ. Từ chối selection thiếu cặp, đảo/ngoài vùng, cắt giữa UTF-8 hoặc sentinel context không khớp trước EmptyClipboard. 56 ca Clipboard giả đạt x64/x86, thêm 21 ca; bốn build UI Debug/Release đạt 0 cảnh báo/0 lỗi.
    - [x] Giải mã entity số thập phân/hex và các tên amp/lt/gt/quot/apos/nbsp (kèm biến thể hoa hợp lệ AMP/LT/GT/QUOT), chuyển text rồi escape lại &/</>. Ánh xạ selection theo output đã escape; từ chối vị trí giữa entity, entity lỗi/chưa hỗ trợ trước EmptyClipboard. Chuẩn hóa CR/CRLF literal và giữ CR từ &#13;. Thêm 53 ca Clipboard vào 56 ca trước; parser vẫn chỉ hỗ trợ tập con.
    - [x] 109/109 Clipboard mỗi kiến trúc và full regression 14/14 nhóm đạt PS7/PS5.1 trên mã entity cuối. Build Debug/Release UI và Release All x64/x86 0 cảnh báo/0 lỗi; tạo beta mới, mỗi ZIP 280 payload/hash/PE đạt hai shell. Đường dẫn/hash ở BETA_PACKAGING.md; không chạy/cài/phát hành, cần QA HTML thật và máy sạch.
    - [x] Casing CF_HTML xuyên text node: trạng thái converter theo từng lần gọi, thẻ inline/comment không reset; paragraph/br và danh sách block tag tường minh đưa newline logic vào trạng thái nhưng không thêm ký tự vào HTML. Preview selection dùng bản sao trạng thái trước node, không thay đổi kết quả thật. 137 ca Clipboard giả đạt x64/x86; 175 ca pure-text giữ nguyên, bốn build UI Debug/Release 0 cảnh báo/0 lỗi.
    - [x] Full regression 14/14 nhóm đạt PS7/PS5.1 sau casing HTML; tạo lại beta x64/x86 từ Release All, 0 cảnh báo/0 lỗi. Mỗi ZIP 280 payload/hash/PE đạt hai shell, đường dẫn/hash ở BETA_PACKAGING.md. Không chạy/cài/phát hành; gói và Sandbox input cũ được giữ nguyên.
    - [x] Nhúng toàn bộ 2.125 named entities kết thúc bằng dấu chấm phẩy từ WHATWG (93 tên hai code point), tra cứu nhị phân phân biệt hoa/thường, dùng offline. Đối chiếu từng mapping với JSON chính thức; 2.292 ca Clipboard giả đạt x64/x86 (bao gồm toàn bảng UTF-8, goldens, supplementary/combining, selection và casing). Bốn build UI Debug/Release sạch. Attribution/BSD notice được đưa vào đóng gói.
    - [x] Hỗ trợ 106 named alias legacy thiếu dấu chấm phẩy theo longest-match trong text node; không đổi thuộc tính/comment. Preview selection dùng toàn text node kèm giới hạn byte, không nhận nhầm &not trong &notin; hoặc vị trí giữa CRLF. Thêm 160 ca, 2.452 Clipboard fake đạt x64/x86, giữ 175 pure-text/20 engine; full regression 14/14 nhóm đạt PS7/PS5.1, danh sách alias đối chiếu JSON WHATWG khớp. Static analysis UI Debug x64 và bốn build UI Debug/Release sạch.
    - [x] Tạo beta alias legacy từ Release All x64/x86 sạch; 281 payload/hash/PE đạt PS7/PS5.1, notice WHATWG khớp nguồn và binary resources 4/4 mỗi kiến trúc. Đường dẫn/hash ở BETA_PACKAGING.md; không chạy/cài/ký/phát hành và không tự cập nhật Sandbox input cũ.
    - [x] Thêm numeric recovery: thiếu dấu ;, saturate số dài không overflow, U+FFFD cho null/surrogate/ngoài Unicode, mapping C1 và giữ noncharacter/control khác. Selection dựa trên full digit run, thuộc tính không đổi; không đổi casing converter. 2.641 Clipboard fake mỗi x64/x86 đạt, cùng 175 pure-text/20 engine; 128 biến thể C1 dùng oracle Windows CP1252 độc lập. Full regression 14/14 nhóm PS7/PS5.1, static analysis UI Debug x64 và bốn build UI Debug/Release sạch. Xem FLUENT_UI_QA.md.
    - [x] Tạo beta numeric recovery từ Release All sạch x64/x86: 281 payload/hash/PE đạt PS7/PS5.1, resource binary 4/4 và notice WHATWG/C1 khớp nguồn. Tên/hash ở BETA_PACKAGING.md; không chạy/cài/ký/phát hành app hoặc đổi Sandbox input cũ.
    - [x] Recovery ampersand/tên không khớp và entity số không có digit thành text literal, escape output, không giải mã đệ quy. Selection trong literal được ánh xạ, entity hợp lệ vẫn không cho cắt giữa; 2.732 ca Clipboard fake mỗi x64/x86 và 14/14 nhóm regression PS7/PS5.1 đạt. Kết quả xác minh cuối ở FLUENT_UI_QA.md.
    - [x] Build bốn UI Debug/Release và static analysis Debug x64 sạch; beta literal recovery từ Release All sạch x64/x86, mỗi ZIP 281 payload/hash/PE đạt PS7/PS5.1, resource binary 4/4 và notice khớp nguồn. Tên/hash ở BETA_PACKAGING.md; chưa cài/chạy/ký/phát hành hoặc thay Sandbox input trước.
    - [ ] Mở rộng CF_HTML: layout/CSS-aware boundaries và bảng mã legacy. Hiện từ chối bảng mã/raw-text chưa hỗ trợ trước EmptyClipboard; không tự bỏ rich text. Context -1 hiện yêu cầu fragment marker chuẩn, parser chỉ hỗ trợ tập con, không phải HTML parser đầy đủ.
    - [x] Full regression 14/14 nhóm đạt PS7/PS5.1 sau mở rộng context/selection; tạo lại beta x64/x86 từ Release All không SkipBuild, 0 cảnh báo/0 lỗi. Mỗi gói 280 payload/hash/PE đạt hai shell, chứa cả bản vá HighContrastChanged. Đường dẫn/hash gói mới ở BETA_PACKAGING.md; không cài/chạy/phát hành, Sandbox input cũ chưa cập nhật.
    - [ ] Kiểm thử CF_HTML thật với ứng dụng đích, delayed rendering và owner WinUI khi modal/thay focus, cùng hotkey/tray engine dùng service mới. SetClipboardData lỗi sau clear vẫn chưa có rollback.
        - [x] Phân biệt format vắng/rỗng/đọc lỗi; lỗi handle/size/lock/terminator và đăng ký HTML dừng trước clear, giữ cả hai format nguyên byte. Unicode rỗng đi kèm HTML hợp lệ được giữ lại. 32 fixture mới qua Fluent/legacy, tổng 3.575 Clipboard fake mỗi x64/x86 và 14/14 nhóm PS7/PS5.1 đạt. Bốn All Debug/Release sạch, static analysis Debug x64 sạch; beta Release All mỗi ZIP 281 payload/hash/PE đạt hai shell, resources/notice khớp. Tên/hash ở BETA_PACKAGING.md. Không chứng minh delayed-rendering thật hoặc bảo toàn mọi format khác.
      - [x] Chuyển hotkey/tray/dialog Win32 sang ConvertService chung qua LegacyQuickConvert với owner engine tường minh và snapshot bảng mã/flag legacy; UI không link globals engine, không còn chuyển toàn chuỗi HTML qua convertUtil. 811 fixture bridge mới, tổng 3.543 Clipboard fake mỗi x64/x86 và 14/14 nhóm PS7/PS5.1 đạt. Bốn All Debug/Release sạch, Debug x64 All static analysis sạch; beta Release All mỗi ZIP 281 payload/hash/PE đạt hai shell, resources/notice khớp. Tên/hash ở BETA_PACKAGING.md; vẫn cần QA hotkey/tray thật và không có rollback sau clear.
- [ ] Kiểm thử sleep/resume, sign out và nhiều user session.

Tiêu chí hoàn thành:

- [ ] Giao diện dùng được hoàn toàn bằng bàn phím và screen reader.
- [ ] Không có độ trễ gõ nhận thấy so với baseline.

## Giai đoạn 9 — Đóng gói và phát hành

- [ ] Hoàn thiện output folder gồm UI, engine, updater và runtime cần thiết.
  - [x] Thêm `package-beta.ps1` gom engine/updater và layout UI self-contained vào dist riêng, bỏ build artifact; xác minh PE của ba executable và SHA-256 từng payload ZIP trên x64/x86 (251 tệp mỗi gói).
  - [ ] Hoàn thiện VC++ runtime/prerequisite, thông báo giấy phép runtime và kiểm thử máy sạch trước khi phân phối.
    - [x] Thêm `collect-third-party.ps1` vào đóng gói: inventory 9 NuGet dependency, sao chép nuspec/license/notice nguyên tệp; ZIP x64/x86 có 278 payload đã xác minh. 5 ca collector đạt trên PowerShell 7/5.1. Inventory gồm cả build tools, chưa ánh xạ binary/license; Microsoft.Windows.SDK.BuildTools không khai báo license file nên cần rà soát riêng, chưa xác nhận quyền phân phối.
    - [x] Thêm checker runtime chỉ đọc registry, đọc architecture/minimumVersion từ manifest gói; 6 ca so sánh version đạt. Gói x64/x86 có 252 payload đã xác minh; checker trong từng gói nhận đúng kiến trúc. Máy phát triển đang có 14.51.36247.0 cả hai kiến trúc, đạt minimum 14.51.36231.0; chưa chứng minh máy sạch.
- [x] Luồng `package-beta.ps1` gọi `build.ps1 Release <platform> All` trước khi gom ZIP; đã chạy end-to-end cả x64 và x86, 0 cảnh báo/0 lỗi. Sau thêm checker đã xác minh lại ZIP từ output Release cả hai kiến trúc.
- [x] Thêm script tạo ZIP beta nội bộ `./package-beta.ps1 x64`/`x86`; có manifest/hash, không ghi đè gói cũ. Chưa đủ điều kiện phát hành stable.
  - [x] Làm mới beta sau cảnh báo phụ thuộc Chromium/chính tả nâng cao: Debug x64 static analysis, Debug x86 và Release All x64/x86 sạch; 13 regression suites / 1218 checks mỗi PS7/PS5.1 đạt. Hai ZIP 280 payload/hash/PE đã xác minh hai shell, tên/hash ở BETA_PACKAGING.md. Giữ gói cũ/đầu vào Sandbox, chưa nghiệm thu UI thật.
  - [x] Làm mới beta sau cache draft/history/icon ngày 2026-10-10: Release All x64/x86 sạch, checker icon/manifest đạt trước gom, 280 payload/gói (thêm icon.ico từ Image item), verifier PS7/5.1 đạt. Full regression 1164 checks + 16 binary-resource checks mỗi shell đạt; tên/hash ở BETA_PACKAGING.md. UI thật/máy sạch vẫn chờ kiểm thử tập trung.
  - [x] Làm mới beta sau khóa thao tác/bảo vệ draft và export Gõ tắt ngày 2026-10-10: Release All x64/x86 sạch, 279 payload/gói, verifier PS7/5.1 đạt, full regression 1160 checks mỗi shell. Tên/hash ở BETA_PACKAGING.md; không chạy app, không phát hành stable.
  - [x] Làm mới beta ngày 2026-10-10 sau theme/titlebar/geometry/design resources/Mica fallback/reduced motion; 279 payload mỗi gói, Release All sạch, verifier PS7/5.1 đạt. Tên/hash ở BETA_PACKAGING.md, gói cũ giữ nguyên.
  - [x] Thêm test-all.ps1 All/x64/x86: từng bộ ở process shell riêng, log/JSON riêng và fail-fast. 12 bộ / 1120 lượt kiểm tra mỗi PS7/PS5.1 đạt ngày 2026-10-10; sửa BOM fixture updater Unicode cho PS5.1. Không thay nghiệm thu UI/hook/máy sạch.
  - [x] Làm mới beta x64/x86 ngày 2026-10-09 sau layout/kích thước/ngôn ngữ/đồng bộ UI và bảo vệ reload Gõ tắt. Release All build sạch, 278 payload/gói, verifier PS7/5.1 đạt và hash 3 executable khớp output Release. Tên ZIP/hash và giới hạn QA ghi trong FLUENT_UI_QA.md; chưa chạy beta trên máy sạch hoặc phát hành.
  - [x] Thêm `verify-beta.ps1` kiểm tra ZIP độc lập (manifest/tệp bắt buộc/hash/path/entry trùng/PE), hỗ trợ hash ZIP từ kênh ngoài; tích hợp vào đóng gói. 17 ca ZIP tổng hợp đạt; ZIP Release x64/x86 có 253 payload đã kiểm tra lại. Hash nội bộ không chứng minh nhà phát hành.
- [ ] Xác minh chạy được trên máy sạch không có Visual Studio.
- [ ] Kiểm thử nâng cấp từ phiên bản OpenKey hiện tại.
- [ ] Kiểm thử giữ nguyên registry settings và macro khi nâng cấp.
- [ ] Ký số executable nếu có certificate.
- [ ] Cập nhật README với yêu cầu hệ thống và ảnh giao diện mới.
  - [x] Cập nhật README root/Windows với trạng thái beta, môi trường build đã dùng, prerequisite, lệnh build/chạy/test/package và phân biệt yêu cầu/ảnh legacy.
  - [ ] Thêm ảnh Fluent đã xác minh và chốt hỗ trợ hệ điều hành sau kiểm thử máy sạch.
- [x] Viết `MIGRATION_FLUENT_BETA.md`: settings/macro dùng chung HKCU, sao lưu trước thử, thử side-by-side không cô lập dữ liệu, rollback executable và giới hạn import registry/startup. Đây là hướng dẫn, chưa đánh dấu kiểm thử nâng cấp đạt.
- [ ] Phát hành bản beta riêng trước khi thay bản stable.
- [ ] Thu thập crash/log và phản hồi beta.
- [ ] Chỉ xóa UI Win32 cũ sau ít nhất một chu kỳ beta ổn định.

Tiêu chí hoàn thành:

- [ ] Bản phát hành chạy được trên máy sạch và nâng cấp không mất dữ liệu.
- [ ] Có phương án rollback về bản legacy.

## Ma trận kiểm thử bắt buộc

- [ ] Windows 11 x64, Light theme.
- [ ] Windows 11 x64, Dark theme.
- [ ] Windows 10 22H2 x64.
- [ ] x86 nếu vẫn nằm trong phạm vi hỗ trợ.
- [ ] DPI 100%, 125%, 150% và 200%.
- [ ] Bàn phím US và các layout bàn phím phụ đang được hỗ trợ.
- [ ] Notepad, Microsoft Office, trình duyệt Chromium và Firefox.
- [ ] Ứng dụng chạy thường và ứng dụng chạy Administrator.
- [ ] Remote Desktop và nhiều user session.
- [ ] Cài mới, nâng cấp và chạy portable từ ZIP.

## Thứ tự PR/commit đề xuất

- [ ] PR 1: inventory, baseline và test plan.
- [ ] PR 2: project WinUI 3 tối thiểu cùng build scripts.
- [ ] PR 3: `SettingsService`, model và bridge tới engine.
- [ ] PR 4: design tokens, shell và navigation.
- [ ] PR 5: trang cài đặt chính.
- [ ] PR 6: trang Macro.
- [ ] PR 7: Convert Tool.
- [ ] PR 8: tray, lifecycle, About và updater.
- [ ] PR 9: accessibility, localization và performance.
- [ ] PR 10: packaging, beta và tài liệu phát hành.

## Definition of Done toàn dự án

- [ ] Toàn bộ tính năng Windows hiện tại có mặt trong UI Fluent mới.
- [ ] Engine và keyboard hook không bị viết lại nếu không thật sự cần thiết.
- [ ] Không mất cấu hình hoặc macro của người dùng cũ.
- [ ] Build sạch cho tất cả kiến trúc được hỗ trợ.
- [ ] Không còn lỗi nghiêm trọng trong test matrix.
- [ ] UI đáp ứng Light/Dark/High Contrast và keyboard accessibility.
- [ ] Bản ZIP chạy trên máy sạch.
- [ ] README, changelog và hướng dẫn build được cập nhật.
- [ ] UI Win32 legacy chỉ được gỡ sau khi bản Fluent đã qua beta ổn định.

## Tài liệu tham khảo

- WinUI 3: <https://learn.microsoft.com/windows/apps/winui/>
- Windows App SDK: <https://learn.microsoft.com/windows/apps/windows-app-sdk/>
- Windows app design: <https://learn.microsoft.com/windows/apps/design/>
