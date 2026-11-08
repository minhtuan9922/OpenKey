# OpenKey cho Linux — Fcitx5 proof-of-concept

Bản thử tiếp theo: **NativeDirect** (addon 0.2.39 / alpha.48, UI preview.8) cho
chữ hiện ngay bằng phím gốc, sửa dấu khi surrounding-text được xác nhận đầy đủ,
không tự fallback ô nổi. Thiếu hỗ trợ thì giữ nguyên phím gõ; chưa nghiệm thu
desktop/mọi ứng dụng. Alpha.38 thêm xử lý phần gợi ý đuôi được chọn trong ô URL
khi toàn bộ phần đã gõ khớp notification; alpha.37 còn đi raw ở các từ đó.
Ô trong trang web đã được người dùng xác nhận trên alpha.36.
Xem [cài thử và giới hạn](../../../docs/UBUNTU_NATIVE_DIRECT.md).

Mới từ source: **CommitOnly** (addon 0.2.26 / alpha.35) giữ từ trong bảng bộ gõ,
gửi cả từ khi kết thúc; không gửi client preedit hoặc sửa chữ đã ghi. Đây là
thử nghiệm cho Ubuntu 26, chưa nghiệm thu desktop/mọi ứng dụng. Gợi ý code
không nhận chữ trong lúc soạn từ. Xem [cách chọn và kiểm tra](../../../docs/UBUNTU_COMMIT_ONLY.md).

Nhánh `fix/ubuntu-direct-focus` có module Fcitx5 thử nghiệm với hai input
method **OpenKey Telex** và **OpenKey VNI**, dùng engine C++ chung, cùng gói `.deb`
alpha. Chưa có UI cấu hình hoàn chỉnh hoặc macro trong addon. Session service tùy chọn có
tray riêng và [autostart opt-in](../../../docs/UBUNTU_SERVICE.md#tự-khởi-động-tray-sau-đăng-nhập--opt-in),
gói addon hiện tại alpha.27; alpha.11; build/install không tự bật autostart.

Đã có [UI preview Qt Quick/QML v0.4](../../../docs/UBUNTU_UI.md) với sidebar
theo Windows và theme sáng/tối. Build opt-in `OPENKEY_BUILD_UI=ON`; giao diện
đọc trạng thái lấy mẫu từ service và cấu hình người dùng đã lưu; có Apply
Preedit/Direct và DirectDiagnostics qua Fcitx5 và xác nhận runtime, chưa bảo đảm lưu bền;
trang Chính tả có CheckSpelling và UseModernOrthography khi addon hỗ trợ schema
(từ alpha.10), mặc định đều bật; addon cũ vẫn dùng được mode/diagnostics;
Gõ nhanh Telex dùng schema QuickTelex từ alpha.11, mặc định tắt, VNI không đổi;
trang Phím tắt đọc runtime toàn cục Fcitx5, không sửa hay đăng ký hotkey;
trang Tương thích hiển thị môi trường riêng UI, chia sẻ điều khiển mode với Tổng
quan và hướng dẫn giới hạn Direct/VS Code, chưa có chính sách theo ứng dụng;
trang Chuyển mã có chuẩn hóa Unicode/hoa-thường/bỏ dấu thủ công, không tự đọc
clipboard hoặc sửa tài liệu, chưa hỗ trợ chuyển bảng mã legacy;
chuyển đổi chạy bằng worker Qt Concurrent, nguồn tối đa 64 Ki đơn vị UTF-16,
có Hủy và không nhận kết quả cũ sau khi sửa nguồn/tùy chọn;
hai nút clipboard Dán/Sao chép chỉ chạy khi người dùng bấm, không theo dõi nền;
trang Giới thiệu có metadata riêng UI/Qt và giấy phép offline, không kiểm tra
cập nhật hoặc suy ra phiên bản addon đang nạp;
Nâng cao có JSON sao lưu theme/sidebar, xem trước rồi xác nhận khôi phục,
không sao lưu cấu hình Fcitx5, macro hoặc văn bản và không tự ghi file sao lưu;
Tổng quan phân biệt lỗi service/API runtime/file đã lưu, có Đọc lại chỉ đọc,
không coi dữ liệu đã lấy mẫu là kiểm tra sức khỏe engine;
mở từ menu tray sau khi cài component UI hoặc gói riêng `openkey-settings`
preview.1 (không kèm addon/tray). UI normal dùng
một cửa sổ/session bus; có `--offline` để xem UI không đọc dữ liệu.
Điều hướng trong cửa sổ dùng Ctrl+Tab/Ctrl+Shift+Tab và F1, không đăng ký
hotkey hệ thống; xem [phạm vi và giới hạn](../../../docs/UBUNTU_UI.md#điều-hướng-bàn-phím-trong-ui).

Workflow Linux có job UI headless riêng trên Ubuntu 24.04 (Qt từ APT), tắt
addon/service, chạy CTest và kiểm tra staged install; không phát hành UI trong
gói addon; có bước tạo/kiểm tra gói UI độc lập. [Phạm vi CI và giới hạn](../../../docs/UBUNTU_UI.md#ci-giao-diện-độc-lập).
Chưa nghiệm thu job này trên remote; kết quả local không thay kiểm thử runner.

Alpha.3 thêm lựa chọn **Preedit / Direct** trong cấu hình Fcitx5. Direct commit
từng ký tự, không gửi preedit khi client hỗ trợ surrounding-text; Preedit vẫn là
mặc định. Xem [cách bật và giới hạn](../../../docs/UBUNTU_INSTALL.md#chọn-cókhông-gạch-chân).

Alpha.4 sửa khởi tạo Direct khi ô nhập có selection sẵn và đổi context lúc idle,
không cần toggle EN–VI để reset từ cũ. Đây là kết quả test headless theo phản hồi
desktop; chưa khẳng định VS Code đã hoạt động. Có diagnostics tùy chọn không ghi
nội dung để phân biệt thiếu capability, context chưa hợp lệ và timeout.

## Gói cài Ubuntu

Chạy `bash scripts/build-linux-deb.sh` từ root repository để tạo `.deb` và checksum
trong `dist/linux/`. Script build Release, chạy test và kiểm tra addon từ gói đã
giải nén, không cài vào hệ thống. Xem [hướng dẫn cài/gỡ](../../../docs/UBUNTU_INSTALL.md).
Bản build trên Ubuntu 26.04 yêu cầu runtime của máy build; không mặc nhiên hỗ trợ
Ubuntu cũ hơn. CI có job build riêng trên Ubuntu 24.04, chưa chạy trên remote.

## Build và test

Session service v0.3 (single-instance, monitor và tray trạng thái tùy chọn;
có menu chuyển Telex/VNI/tiếng Anh (US), mở cấu hình Fcitx5, mở UI OpenKey nếu đã
cài component UI và thoát riêng tray):
[hướng dẫn service](../../../docs/UBUNTU_SERVICE.md). Build tùy chọn, tách khỏi addon;
không cần cài lại alpha.9 để dùng bộ gõ đang hoạt động.
Hotkey mở cấu hình X11 là prototype opt-in `OPENKEY_BUILD_X11_HOTKEY=ON` và
`--run-tray --x11-settings-hotkey`; mặc định không bắt phím, không hỗ trợ Wayland.
Xem [giới hạn và kiểm thử Xvfb](../../../docs/UBUNTU_SERVICE.md#hotkey-mở-cấu-hình-trên-x11--thử-nghiệm-opt-in).

Công cụ: compiler C++20 cho addon, CMake >= 3.16, Ninja và SDK Fcitx5 >= 5.0.14.
CTest Linux có thêm kiểm thử trình chạy profile VS Code bằng Bash/coreutils/grep,
dùng stub nên không cần cài hoặc mở VS Code. Xem [hồi quy launcher](../../../docs/UBUNTU_VSCODE_INPUT_INVESTIGATION.md#hồi-quy-trình-chạy--cập-nhật-2026-11-04).
Engine vẫn dùng C++17. Trên Ubuntu, tên gói SDK core là `libfcitx5core-dev`:

```bash
sudo apt update
sudo apt install build-essential cmake ninja-build libfcitx5core-dev
cmake -S . -B build/ubuntu-v0.2 -G Ninja \
  -DOPENKEY_BUILD_FCITX5=ON -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build/ubuntu-v0.2 --parallel 2
ctest --test-dir build/ubuntu-v0.2 --output-on-failure
```

Chạy từ root repository. Mặc định addon tắt để engine có thể build độc lập.
Target `openkey_fcitx5` tạo `openkey.so`; không khởi động hoặc thay cấu hình
Fcitx5 của người dùng khi build/test.

## Kiểm tra gói trước khi cài

```bash
DESTDIR="$PWD/build/ubuntu-v0.2/stage" cmake --install build/ubuntu-v0.2
```

Trong staging có module dưới `usr/lib/.../fcitx5/`, addon descriptor dưới
`usr/share/fcitx5/addon/` và hai input method dưới
`usr/share/fcitx5/inputmethod/`. Thư mục lib cụ thể phụ thuộc cấu hình CMake;
đường dẫn đầy đủ được ghi trong `build/ubuntu-v0.2/install_manifest.txt`.

Nếu muốn cài thử vào hệ thống, sau khi kiểm tra staging:

```bash
sudo cmake --install build/ubuntu-v0.2
```

Không cần chạy lệnh trên để chạy các test tự động. Nó chỉ cài các file OpenKey,
không đổi input framework, không restart daemon và không thêm OpenKey vào profile.
Khi đang dùng Fcitx5 trong desktop session, khởi động lại Fcitx5 theo hướng dẫn
desktop của bạn, mở công cụ cấu hình Fcitx5, bỏ lọc ngôn ngữ nếu cần và thêm
**OpenKey Telex (Experimental)** hoặc **OpenKey VNI (Experimental)**.
Không dùng `fcitx5 -r` một cách mặc định trên KDE Wayland: Fcitx5 có thể được KWin
khởi chạy bằng socket riêng. Xem [hướng dẫn Wayland chính thức](https://fcitx-im.org/wiki/Using_Fcitx_5_on_Wayland/en).

Để gỡ bản cài thử, chuyển sang input method khác, xem danh sách trong
`install_manifest.txt` rồi chỉ xóa các file OpenKey được liệt kê ở đó. Không xóa
cả thư mục Fcitx5 hoặc profile người dùng.

## Hành vi và giới hạn

- Chỉ xuất Unicode dựng sẵn. Dùng keysym để ánh xạ chữ/số; không coi keycode
  Wayland/evdev là keycode X11 trong engine.
- Ở chế độ Preedit, từ đang gõ nằm trong preedit; Space, Enter, dấu câu hoặc điều hướng commit từ
  rồi chuyển phím gốc cho ứng dụng. Không dùng clipboard, giả lập phím hoặc xóa
  văn bản đã commit. Chưa sửa dấu ở từ cũ ngoài preedit.
- Backspace ở Preedit xóa ký tự hiển thị, không chỉ xóa phím Telex/VNI cuối.
  Escape hủy preedit; reset từ client hủy preedit. Khi focus-out, Fcitx5 hoặc
  client đã phụ trách commit client preedit thì addon chỉ xóa trạng thái để
  tránh lặp từ; với server-side preedit, addon tự commit. Khi đổi input method,
  addon commit phần đang gõ. Hành vi trên ứng dụng thật vẫn cần kiểm thử desktop.
- Shift/Caps Lock giữ chữ hoa. Ctrl/Alt/Super shortcut commit từ rồi chuyển
  shortcut cho ứng dụng. Ô Password/Sensitive bỏ qua engine.
- Mỗi input context có composition riêng. Vì engine legacy dùng trạng thái
  toàn cục, prototype replay tối đa 256 sự kiện của từ trên event thread Fcitx5;
  đến giới hạn thì commit phần cũ trước phím tiếp theo. Đây không phải giải pháp
  đa luồng hoặc engine nhiều instance; cần đo độ trễ trước beta.
- Direct chỉ thay suffix thuộc từ engine đang quản lý, sau khi surrounding-text
  xác nhận edit trước. Các sự kiện đến sớm được xếp hàng; gặp snapshot thay đổi,
  selection, queue overflow hoặc timeout thì ngừng sửa. Chỉ chuyển tiếp phím gốc
  khi vị trí vẫn khớp snapshot đã biết; nếu context đã thay đổi thì giữ chữ chờ
  trong bảng phụ, cần Enter xác nhận chèn hoặc Escape hủy.
  Từ alpha.8, edit quay lại snapshot cũ phải có notification sau edit, không chỉ
  cache khớp. Alpha.9 giữ một từ và tối đa 32 Space để xóa Space rồi sửa tiếp từ;
  đổi context/focus hoặc gõ từ mới sẽ bỏ lịch sử này.
  Đây không phải cam kết hỗ trợ mọi Electron/Wayland frontend hoặc undo grouping.
- Chưa bật macro, cấu hình engine lưu bền, bảng mã legacy, smart-switch theo ứng dụng,
  global hotkey hay UI riêng. Fcitx5 quản lý chuyển input method ở giai đoạn này.

## Kiểm chứng

Đã build với GCC 15 và SDK Fcitx5 5.1.19. Có bảy mục CTest:

1. Engine smoke: chuyển mã Unicode/VNI.
2. Engine regression: Telex, Backspace và macro ở lớp engine.
3. Composition: Telex/VNI, chữ hoa, Backspace chuyển vị trí dấu, xen kẽ context
   và giới hạn sự kiện.
4. Addon: nạp module bằng `dlopen`, tạo addon qua factory thật, dùng frontend
   giả để kiểm tra preedit, commit, Escape, reset, deactivate, shortcut và ô nhạy cảm.
5. Install fixture: cài vào prefix riêng bên trong build, không ghi hệ thống.
6. Integration: Fcitx tự đọc descriptor và nạp module từ install fixture, chuyển
   key event qua pipeline thật, kiểm tra focus-out không lặp từ (cả client và
   server preedit), đổi Telex/VNI và quay lại ô nhập cũ. Không nạp frontend desktop.
7. Direct: commit tức thì/no preedit, Unicode code-point offsets, Telex/VNI,
   delayed/duplicate/out-of-order ACK, timeout/queue overflow, selection/cursor,
   đổi cấu hình và fallback. Integration cũng chạy Direct qua Fcitx pipeline thật.

Test headless không chứng minh tương thích Firefox/Chrome, LibreOffice, VS Code,
GTK/Qt hoặc GNOME/KDE Wayland/X11. Các mục này vẫn chưa nghiệm thu.
Xem [checklist nghiệm thu desktop](../../../docs/UBUNTU_INPUT_QA.md).

Tham khảo [API phát triển Fcitx5](https://fcitx-im.org/wiki/Develop_an_simple_input_method),
[kế hoạch Ubuntu](../../../docs/UBUNTU_DEVELOPMENT_PLAN.md) và
[task Ubuntu](../../../docs/UBUNTU_TASKS.md).

### Alpha.49 / UI preview.9

Simple Telex 1/2, macro CRUD/JSON/file, sửa từng hotkey Fcitx5 và quy tắc
InputStyle theo program ID: [hướng dẫn và giới hạn](../../../docs/UBUNTU_FEATURES_49.md).
Các chức năng mới cần addon/UI tương ứng; cài gói không tự đổi IM hoặc restart.

## Alpha.50 / UI preview.11

Context riêng, năm tùy chọn nâng cao, chuyển file lớn có Hủy/timeout và
sao lưu IM/phím tắt bằng file: [hướng dẫn và giới hạn](../../../docs/UBUNTU_FEATURES_50.md).

UI preview.12 và service mới: [menu mở Gõ tắt/Chuyển mã/Giới thiệu](../../../docs/UBUNTU_TRAY_PAGES.md).
