# Giao diện OpenKey cho Ubuntu — v0.4 preview

## Phạm vi hiện tại

Đã dựng cửa sổ Qt Quick/QML độc lập, lấy thứ tự điều hướng từ
[MainWindow Windows](../Sources/OpenKey/win32/OpenKey/OpenKey.UI/MainWindow.xaml):
Tổng quan, Chính tả & gõ nhanh, Phím tắt, Ứng dụng & tương thích, Cài đặt nâng cao,
Gõ tắt, Chuyển mã và Giới thiệu ở chân sidebar.

Sidebar có thể thu gọn, điều hướng bằng chuột/bàn phím, card bo góc và theme
sáng/tối/theo hệ thống. Tokens dùng chung cho màu, khoảng cách, cỡ chữ và radius.
Giữ khung cửa sổ native Linux; không giả lập toàn bộ title bar của Windows.

Đây **chỉ là UI preview**, chưa phải ứng dụng cấu hình hoàn chỉnh:

- Tổng quan đọc trạng thái từ IPC của service và hai khóa cấu hình người dùng
  đã lưu. Có thêm card runtime đọc/áp dụng Preedit/Direct trực tiếp qua API Fcitx5;
  chỉ gửi thay đổi khi nhấn Áp dụng, không chuyển Telex/VNI/EN–VI.
- Các trang tính năng ghi rõ “Chưa tích hợp”; không có công tắc giả.
- Chưa mở từ menu tray, chưa tự khởi động, chưa có bảo đảm single-instance cho UI.
- Đóng cửa sổ không dừng bộ gõ/tray. Không cần cài lại addon alpha.9 để thử UI.
- Theme chỉ giữ trong phiên chạy, không lưu xuống đĩa. “Theo hệ thống” suy ra
  sáng/tối từ `SystemPalette.window` do Qt/platform cung cấp; chưa nghiệm thu việc
  đồng bộ GNOME dark preference trực tiếp hoặc khi thay theme lúc đang mở.

## SDK và build

Chọn Qt 6/Qt Quick/QML, yêu cầu Qt >= 6.4; đã build/test với Qt 6.10.2 trên máy
Ubuntu hiện tại. Chưa chạy xác minh trên SDK 6.4 hoặc Ubuntu cũ hơn.
UI opt-in bằng `OPENKEY_BUILD_UI=ON`, mặc định OFF; engine/addon/service vẫn build
độc lập, không cần Qt khi UI tắt. UI dùng Qt Network và Qt DBus trong SDK base;
Qt Test và `dbus-run-session` cần khi bật test.

```bash
sudo apt install qt6-base-dev qt6-declarative-dev \
  qml6-module-qtquick qml6-module-qtquick-controls \
  qml6-module-qtquick-layouts qml6-module-qtquick-window \
  qml6-module-qtquick-templates qml6-module-qtqml-workerscript

cmake -S . -B build/ubuntu-ui -DCMAKE_BUILD_TYPE=Release \
  -DOPENKEY_BUILD_UI=ON -DOPENKEY_BUILD_SMOKE_TESTS=ON \
  -DOPENKEY_BUILD_FCITX5=OFF -DOPENKEY_BUILD_SERVICE=OFF
cmake --build build/ubuntu-ui --parallel 4
ctest --test-dir build/ubuntu-ui --output-on-failure
```

Chạy từ root repository. QML được đóng vào resource của executable bằng
`qt_add_qml_module`; vẫn cần runtime Qt và các QML module tương ứng trên máy chạy.
Với shared Qt, plugin được tải động; không yêu cầu static plugin CMake targets
do distro có thể không cung cấp chúng.

## Mở thử trên desktop

Chạy trong terminal của phiên desktop Ubuntu (không phải terminal SSH thiếu
DISPLAY/WAYLAND_DISPLAY):

```bash
./build/ubuntu-ui/Sources/OpenKey/linux/ui/openkey-settings
```

Tùy chọn xem trước:

```bash
./build/ubuntu-ui/Sources/OpenKey/linux/ui/openkey-settings --theme dark --page about
```

`--theme`: `system`, `light`, `dark`. `--page`: `overview`, `spelling`, `hotkeys`,
`compatibility`, `advanced`, `macro`, `convert`, `about`.

Binary hiện nằm trong thư mục build, chưa đưa vào gói `.deb` hoặc cài lên desktop.
Install component riêng tên `openkey_ui`; không bật autostart và không thay service.

## Model Tổng quan — chỉ đọc

[SettingsOverview](../Sources/OpenKey/linux/ui/SettingsOverview.h) cung cấp các
property chỉ đọc cho QML; `refresh()` chỉ đọc dữ liệu. Không có setter, không gọi
shell hoặc subprocess và không khởi động daemon.

| Dữ liệu | Nguồn và ý nghĩa |
| --- | --- |
| Telex/VNI/tiếng Anh/khác/chưa kích hoạt | Lệnh `input-status` trên socket của service; context gần nhất service quan sát, không phải trạng thái riêng của ô nhập hiện tại |
| `InputStyle` | Khóa gốc trong file người dùng: `Direct` hoặc `Preedit`; **không phải xác nhận chế độ runtime**, Direct có thể fallback Preedit |
| `DirectDiagnostics` | Khóa gốc `True`/`False` trong cùng file; chỉ hiển thị bật/tắt, không đọc log hoặc nội dung gõ |

Đường dẫn socket là `$XDG_RUNTIME_DIR/openkey/session.sock`. Chỉ kết nối nếu
runtime/thư mục con thuộc người dùng và không cấp quyền group/other, socket tương
tự; kiểm tra UID peer bằng `SO_PEERCRED`. Không tạo thư mục/socket, không tự bật
service. Dùng QLocalSocket bất đồng bộ, poll mỗi giây, một request tại một thời
điểm, deadline 600 ms, tối đa 32 byte phản hồi và chỉ chấp nhận whitelist enum.
Mất kết nối, timeout hoặc sai protocol xóa mẫu cũ về “Chưa xác định”. Service có
debounce/TTL riêng; đây là trạng thái lấy mẫu, không phải realtime/per-window.

File đọc là `fcitx5/conf/openkey.conf` dưới Qt GenericConfigLocation
(`$XDG_CONFIG_HOME`, mặc định `~/.config`). Đây chỉ là file người dùng ở vị trí
chuẩn; không xác minh đường dẫn override của tiến trình Fcitx5, cấu hình hệ thống,
giá trị mặc định hoặc nội dung đang nằm trong bộ nhớ của addon.
Không có file/khóa thì hiển thị chưa xác định, không tự suy ra Preedit.
Chỉ đọc regular file tối đa 64 KiB; từ chối symlink cuối đường dẫn, FIFO/device,
NUL, giá trị khóa không hợp lệ hoặc khóa quan tâm bị lặp. Bỏ qua khóa không biết
và khóa nằm trong section, không sửa hoặc ghi đè file. Kiểm tra lại mỗi giây.

`--offline` bỏ đọc file, kết nối IPC và API cấu hình D-Bus. `--smoke-test`/`--screenshot` luôn dùng
offline để test/render không đụng đến phiên bộ gõ của người dùng.

## Đổi chế độ có/không gạch chân

Trong Tổng quan, card “Chế độ gõ — cấu hình runtime” đọc giá trị/schema từ Fcitx5
đang chạy. Chọn Có gạch chân (Preedit) hoặc Không gạch chân (Direct), sau đó nhấn
**Áp dụng** khi đã gõ xong từ. UI đọc lại sau ACK để xác nhận runtime.
Đổi lựa chọn đơn thuần chưa ghi; nút Đọc lại chỉ đọc. Các điều khiển bị khóa
khi offline, đang xử lý hoặc không có schema tương thích.

Xác nhận runtime không bảo đảm lưu file thành công; xem riêng card “Chế độ đã lưu”.
Nếu báo chưa xác nhận/conflict, nhấn Đọc lại thay vì gửi lại liên tục. Không tự
restart Fcitx5 hoặc retry write. Chi tiết giao dịch, validation và giới hạn race:
[API cấu hình](UBUNTU_CONFIG_API.md).

## Kiểm thử

Build UI có 7 CTest: engine smoke/regression, model chỉ đọc, controller D-Bus,
UI interaction, embedded-resource smoke và UI interaction tại scale 200%.
Đã đạt 7/7; riêng controller có 34 ca QtTest trên bus giả lập riêng.
UI dùng offscreen/software renderer và config/socket fixture riêng, không dùng
service hoặc file cấu hình thật của desktop.
UI interaction kiểm tra chuyển đủ 8 trang, từ chối index ngoài phạm vi, sidebar
244/72, theme và thao tác picker, phím lên/xuống, kích thước tối thiểu 720×560,
binding model sang card. Model tests kiểm tra enum/partial reply, mất kết nối,
timeout/reconnect, polling, phản hồi quá dài/sai và config lỗi/thiếu, FIFO/symlink,
không ghi file và runtime không an toàn.
Controller tests chạy bus mock riêng; UI kiểm tra chọn không apply, explicit Apply
và khóa điều khiển busy/offline. Không gửi SetConfig đến Fcitx5 trên desktop thật.
Đã sửa popup ComboBox từng có chữ trắng trên nền gần trắng ở mục được chọn:
dùng delegate màu chữ/nền ghép cặp cho cả chế độ gõ và theme. Test mở popup thật,
duyệt highlight bằng bàn phím và kiểm tra tương phản chữ/nền ít nhất 4.5:1 cho
từng dòng, cả sáng/tối ở scale 100% và 200%; mở/duyệt không tự Apply.
Ảnh render offscreen được kiểm tra bố cục sáng/tối. Đây không thay thế nghiệm thu
GNOME/Wayland/X11 hoặc screen reader trên desktop thật.

Kiểm tra resource không cần desktop:

```bash
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software QT_IM_MODULE=compose \
  ./build/ubuntu-ui/Sources/OpenKey/linux/ui/openkey-settings --smoke-test
```

Có thể thêm `--screenshot /tmp/openkey-preview.png` để lưu ảnh rồi thoát; phải dùng
đường dẫn file mới. Không đặt đường dẫn trùng file cần giữ.

Checklist desktop còn chờ người dùng:

- [ ] Mở/đóng, chuyển đủ trang, sidebar mở/thu gọn; không ảnh hưởng gõ tiếng Việt.
- [ ] Chuyển Sáng/Tối/Theo hệ thống; kiểm tra chữ, focus và contrast.
- [ ] Resize tối thiểu, cuộn nội dung; thử scale 100%, 150%, 200%.
- [ ] Tab/Shift+Tab, Enter/Space, phím lên/xuống trong menu; screen reader.
- [ ] GNOME/Wayland và X11; theme hệ thống khi đang chạy.
- [ ] Khi service đang chạy, Tổng quan hiện mẫu trạng thái; đối chiếu menu tray
  sau khi chuyển Telex/VNI/tiếng Anh ở ứng dụng khác rồi quay lại cửa sổ.
- [ ] Card “Chế độ đã lưu” khớp cấu hình đã lưu, không hiểu nhầm là runtime hiện tại.
- [ ] Thử `--offline`: không lấy trạng thái, không đọc cấu hình.
- [ ] Chọn mode khác nhưng chưa nhấn Apply: runtime không đổi.
- [ ] Apply Preedit → Direct → Preedit; xác nhận runtime, thử gõ sau mỗi lần đổi,
  đọc file đã lưu riêng và mở lại UI. Không thực hiện khi đang gõ dở từ.

## Bước tiếp theo

Đã có model read-only và API áp dụng chế độ gõ với xác nhận runtime. Tiếp theo
nghiệm thu desktop và mở rộng từng tùy chọn theo schema, không ghi thẳng file
hoặc coi ACK là bằng chứng lưu bền.
Chưa đánh dấu các trang đã port xong chỉ vì đã có mục điều hướng.

Mã nguồn: [UI](../Sources/OpenKey/linux/ui/),
[kiểm thử UI](../Tests/LinuxSettingsTests.cpp),
[kiểm thử model](../Tests/LinuxSettingsModelTests.cpp). Theo dõi trong
[Ubuntu tasks](UBUNTU_TASKS.md#v04--ui-giống-windows).
