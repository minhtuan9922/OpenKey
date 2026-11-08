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
- Trang Nâng cao có bật/tắt DirectDiagnostics qua cùng API, chỉ gửi khi Apply,
  không đổi InputStyle. Các tùy chọn nâng cao còn lại chưa tích hợp.
- Trang Chính tả có kiểm tra chính tả và đặt dấu kiểu mới qua API addon alpha.10;
  Gõ nhanh Telex qua schema alpha.11, mặc định tắt và không áp dụng VNI.
  Các tính năng chưa nối backend được ghi rõ, không có công tắc giả.
- Trang Phím tắt đọc cấu hình runtime toàn cục của Fcitx5, có nút Đọc lại;
  không sửa hoặc đăng ký phím tắt toàn hệ thống.
- Trang Ứng dụng & tương thích có môi trường cửa sổ UI, điều khiển chế độ chung
  và hướng dẫn Direct/VS Code. Chưa có danh sách hoặc quy tắc riêng theo app.
- UI preview.3 thêm khu vực **Thử gõ tiếng Việt** tại trang này: mẫu Telex/VNI,
  kết quả tham chiếu, nút Bắt đầu thử và Xóa nội dung thử. Chọn mẫu không đổi IM
  hoặc cấu hình. Ô nhập nhận bộ gõ thật của phiên desktop; không tự chuyển dấu.
  Bản nháp giữ khi chuyển trang, mất khi đóng cửa sổ; không lưu vào file.
  Đây chỉ là thử trên ô Qt của OpenKey, không xác nhận Chrome/VS Code/terminal.
- UI preview.4 đọc lại cấu hình runtime khi quay về cửa sổ OpenKey sau khi
  đổi chế độ trong công cụ Fcitx5. Chờ 150 ms sau activation, bỏ qua khi controller
  đang bận; không tự Apply, ghi file hoặc restart daemon. Phiên offline không
  truy vấn. Card “Chế độ đã lưu” vẫn chỉ phản ánh file, độc lập với runtime.

Nếu đã cài UI vào `~/.local` trước khi dùng DEB, D-Bus activation của người
dùng có thể vẫn mở binary cũ, kể cả khi `/usr/bin/openkey-settings` đã nâng
cấp. Ca thực tế: binary local chưa hỗ trợ CommitOnly/NativeDirect, trong khi
addon runtime và file đều là NativeDirect. Cần cập nhật bản local đó hoặc
chuyển launcher/activation sang bản hệ thống; chỉ cài lại addon không sửa UI.
Đóng cửa sổ cũ rồi mở lại sau khi cập nhật binary; tiến trình đang chạy vẫn
giữ phiên bản đã nạp. Không đóng tự động để tránh mất bản nháp.
- Trang Chuyển mã có công cụ Unicode thủ công: NFC/NFD, hoa/thường, bỏ dấu
  tiếng Việt. Không tự đọc clipboard hoặc thay văn bản trong ứng dụng khác.
- Preview.8 bổ sung card Chuyển mã VNI/TCVN3: chọn nguồn/đích Unicode, VNI
  Windows, TCVN3, CP1258 hoặc Unicode tổ hợp. Đây là văn bản hiển thị ký tự
  bảng mã trong ô nhập, không đọc byte của file DOC/DOCX hoặc đổi font. Chuyển
  thủ công, không đổi bảng mã đang gõ của Fcitx5. Unicode tổ hợp dùng NFC/NFD
  của Qt; TCVN3 mất thông tin hoa/thường ở byte trùng nên ưu tiên chữ thường.
  Dùng worker, Hủy, giới hạn như công cụ Unicode; các worker legacy khóa tùy
  chọn ConvertTool global trong tiến trình UI. Không tự thao tác clipboard.
- Trang Giới thiệu có phiên bản nguồn UI/Qt, ghi nguồn dự án và giấy phép GPL
  v3 đóng kèm; không suy ra phiên bản addon hoặc kiểm tra cập nhật trên mạng.
- Mở từ menu tray “Cài đặt OpenKey…” khi đã cài component UI. Bản normal dùng
  một cửa sổ/session bus; offline/smoke/screenshot độc lập. Không tự mở UI lúc đăng nhập.
- Đóng cửa sổ không dừng bộ gõ/tray. Addon alpha.9 vẫn dùng được mode/diagnostics;
  muốn thử hai thiết lập chính tả cần nâng cấp addon alpha.10.
- Theme/sidebar được lưu riêng khi thao tác trên UI normal. Offline/smoke/screenshot
  chỉ giữ trong phiên chạy. “Theo hệ thống” suy ra
  sáng/tối từ `SystemPalette.window` do Qt/platform cung cấp; chưa nghiệm thu việc
  đồng bộ GNOME dark preference trực tiếp hoặc khi thay theme lúc đang mở.

## SDK và build

### Sao lưu cấu hình bộ gõ — preview.5

Trong **Cài đặt nâng cao**, chọn **Tạo JSON bộ gõ** để lấy các giá trị runtime
đã đọc từ addon: InputStyle, DirectDiagnostics và các tùy chọn chính tả/đặt dấu/
QuickTelex được schema hiện tại hỗ trợ. Chọn JSON rồi Ctrl+C để lưu thủ công.
Không gồm phím tắt toàn cục Fcitx5, macro, IM đang chọn, theme hoặc nội dung ô thử.

Để khôi phục, dán JSON và nhấn **Kiểm tra / xem trước cấu hình**. Bảng hiển thị
giá trị hiện tại và giá trị sẽ khôi phục. Kết thúc từ đang gõ, rồi nhấn
**Khôi phục cấu hình bộ gõ**. Chỉ thao tác xác nhận này gửi SetConfig.
Đọc lại runtime hoặc sửa JSON sẽ khóa nút khôi phục cho đến khi xem trước lại.

Định dạng `openkey-input-settings`, version 1, tối đa 4096 đơn vị UTF-16;
chỉ chấp nhận khóa đã biết, enum đúng và boolean JSON (không phải chuỗi).
Bản sao tối thiểu có chế độ gõ và chẩn đoán; thiết lập không có trong bản sao
được giữ nguyên. Nếu addon thiếu bất kỳ thiết lập trong bản sao, từ chối toàn
bộ thao tác, không bỏ qua âm thầm. Không nhập JSON theme vào phần này.

Trước ghi, đọc lại và kiểm tra các trường định khôi phục chưa đổi từ lúc xem
trước. Gửi cả nhóm bằng một SetConfig và đọc lại từng giá trị để xác nhận.
API Fcitx5 không có compare-and-swap: một thay đổi ngoài UI giữa GetConfig và
SetConfig vẫn có thể tranh chấp. ACK mất/readback sai được báo chưa xác nhận;
không tự retry hoặc rollback. Xác nhận runtime không chứng minh file đã lưu
thành công; nghiệm thu lưu bền cần đóng/mở và kiểm tra riêng.

JSON là bản nháp trong cửa sổ. Tạo JSON dùng snapshot runtime đang có; có thể
nhấn Đọc lại trước khi xuất nếu cần. Clipboard chỉ dùng qua copy/paste thủ công.

Từ **preview.6**, chọn **Lưu thành file…** để lưu JSON trong ô nhập hoặc
**Mở file JSON…** để đưa bản sao vào ô. Hộp chọn file chỉ mở khi click; hủy
hộp chọn giữ bản nháp. Khi lưu đè, hộp chọn file yêu cầu xác nhận theo Qt/platform.
Chỉ chấp nhận file local dạng thường, UTF-8 (có thể có BOM), tối đa 16384 byte
và 4096 đơn vị UTF-16 sau giải mã, đúng schema bộ gõ. Từ chối URL mạng,
symlink cuối đường dẫn, FIFO/device và dữ liệu sai. Lỗi mở file giữ bản nháp,
không đưa nội dung file tùy ý vào ô. Lưu bằng QSaveFile, không truncate trực
tiếp bản cũ khi atomic save không thực hiện được. Không tự tạo thư mục.

Mở file thành công vẫn cần **Kiểm tra / xem trước** và **Khôi phục** riêng;
không tự áp dụng hay tự khôi phục file lần trước. Nếu addon không hỗ trợ các
giá trị trong file, bước xem trước từ chối. Nút mở/lưu hoạt động theo thao tác
trong phiên normal; offline/smoke/screenshot không có file-access model.
Hộp chọn native/portal trên Wayland/X11 vẫn cần nghiệm thu desktop.

### Công cụ build

Chọn Qt 6/Qt Quick/QML, yêu cầu Qt >= 6.4; đã build/test với Qt 6.10.2 trên máy
Ubuntu hiện tại. Chưa chạy xác minh trên SDK 6.4 hoặc Ubuntu cũ hơn.
UI opt-in bằng `OPENKEY_BUILD_UI=ON`, mặc định OFF; engine/addon/service vẫn build
độc lập, không cần Qt khi UI tắt. UI dùng Qt Network và Qt DBus trong SDK base;
Qt Test, `dbus-run-session` và `dbus-monitor` cần khi bật test.

```bash
sudo apt install qt6-base-dev qt6-declarative-dev \
  qml6-module-qtquick qml6-module-qtquick-controls \
  qml6-module-qtquick-layouts qml6-module-qtquick-window \
  qml6-module-qtquick-templates qml6-module-qtqml-workerscript qml6-module-qtquick-dialogs

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

## CI giao diện độc lập

[Workflow Linux](../.github/workflows/linux-engine.yml) có job
`ui-build-and-test` trên `ubuntu-24.04`, tách khỏi matrix engine GCC/Clang và job
đóng gói addon. Job bật UI/test/UI_DEB, tắt addon/service/DEB addon/X11 hotkey; không cần SDK
Fcitx5 để build UI. Qt và QML runtime được cài từ APT của runner, cùng dbus-bin,
dbus-daemon và font DejaVu. Chưa chốt tương thích Qt/Ubuntu cũ chỉ từ cấu hình job.

Job dùng `QT_QPA_PLATFORM=offscreen`, `QT_QUICK_BACKEND=software` và
`QT_IM_MODULE=compose`, chạy toàn bộ CTest (gồm private-bus, resource, interaction
và scale 200%). Không khởi động desktop hoặc bộ gõ. JUnit và log CTest được
upload với `if: always()`, giữ 7 ngày; thiếu report do configure/build lỗi chỉ
cảnh báo ở bước upload, không biến job lỗi thành thành công.

Sau test, component `openkey_ui` cài vào staging bằng DESTDIR dưới RUNNER_TEMP,
prefix `/usr`; kiểm tra binary/service file và chạy binary staging với
`--smoke-test`. Không cài vào hệ thống runner hoặc bật autostart/D-Bus service.
Không phát hành binary staging như bộ cài portable hoặc thay `.deb` addon.
Job còn chạy CPack cho gói `openkey-settings` riêng, kiểm tra dependency,
checksum, payload/desktop entry và smoke từ gói giải nén, rồi upload gói và
SHA-256. `dpkg-dev`/`desktop-file-utils` được cài cho bước này. Chưa chạy
job trên remote; upload artifact không đồng nghĩa phát hành lên kho APT.

Quy trình đã được kiểm tra cục bộ trên Ubuntu 26.04/Qt 6.10.2; kết quả đó không
thay cho chạy runner Ubuntu 24.04. Cần commit/push hoặc chạy workflow sau khi
mã có trên remote để xác nhận CI. Lượt này không tự push hoặc dispatch workflow.
Trigger hiện tại: pull request, push vào `master`, hoặc `workflow_dispatch`;
chỉ push nhánh tính năng mà không có PR không tự kích hoạt workflow này.

## Nhớ giao diện khi mở lại

Phiên normal đọc `openkey/ui.ini` dưới Qt GenericConfigLocation (`$XDG_CONFIG_HOME`,
mặc định `~/.config`). Model [UiPreferences](../Sources/OpenKey/linux/ui/UiPreferences.h)
chỉ lưu theme `system|light|dark` và sidebar mở/thu gọn, không lưu cấu hình bộ gõ,
trang hiện tại, kích thước cửa sổ hoặc nội dung nhập liệu.

- Chỉ thao tác chọn theme/thu gọn sidebar mới lưu; mở cửa sổ hoặc đọc file không
  tạo file mới. Không có file thì dùng Theo hệ thống và sidebar mở.
- Normal single-instance đã có nên chỉ UI chính đọc/lưu. Lần chạy thứ hai chỉ
  gửi Show, không thay thiết lập của cửa sổ đang mở.
- `--theme dark` (hoặc light/system) ghi đè theme **cho lần mở mới đó**, không tự
  lưu. Chọn theme trong UI sau đó mới lưu lựa chọn. `--offline`, `--smoke-test`
  và `--screenshot` không tạo model preferences, không đọc/ghi file này.
- Lưu bằng QSettings với atomic sync bắt buộc. Không ghi được thì báo ở chân
  cửa sổ; lựa chọn vẫn dùng trong phiên, không báo đã lưu thành công.
- File quá 16 KiB, symlink cuối đường dẫn, file đặc biệt, phiên bản không hỗ trợ
  hoặc giá trị sai bị từ chối và không ghi đè. Dùng mặc định lúc khởi động, vẫn
  cho xem theme trong phiên. Sao lưu/đổi tên file UI lỗi rồi mở lại để dùng mặc
  định; chương trình không tự xóa cấu hình người dùng.

Định dạng hiện tại là `FormatVersion=1`, `Interface/Theme` và
`Interface/SidebarExpanded`. Ini riêng này không phải cấu hình Fcitx5; không có
thao tác Apply, restart daemon hoặc tự bật diagnostics khi đổi theme/sidebar.

## Bảng kết nối và lỗi

Trang **Tổng quan** có bảng **Kết nối & lỗi**, tổng hợp ba nguồn độc lập:

- Service trả về mẫu input method từ context gần nhất mà nó quan sát.
  Có phản hồi không chứng minh đây là ô đang focus; thiếu service, timeout
  hoặc phản hồi sai giao thức không chứng minh Fcitx5/bộ gõ đã dừng.
- API cấu hình runtime dùng trạng thái của controller và thông báo giống các
  nút Apply. `unconfirmed` nghĩa là thay đổi có thể đã áp dụng; chỉ đọc lại
  để đối chiếu, không tự gửi lại. Conflict cần đọc lại trước khi chọn/Apply
  có chủ đích. Unsupported yêu cầu kiểm tra addon, không tự nâng cấp/restart.
- File đã lưu được đọc riêng; file thiếu không chứng minh runtime dùng mặc
  định, đọc được file không chứng minh thay đổi runtime đã lưu thành công.

Mức **Có dữ liệu từ các nguồn** chỉ xuất hiện khi controller ready với kết
quả loaded/confirmed, service connected, mẫu input method được nhận diện
và config read. Không gọi đó là chứng nhận engine ổn định, Direct đang hoạt
động, ứng dụng đích nhận đúng dấu hoặc dữ liệu là snapshot đồng thời.
Giá trị enum lạ chỉ dùng thông báo dự phòng cố định, không in nội dung thô.

**Đọc lại trạng thái** gọi refresh model/service/file và reload API cấu hình
đúng một lần theo click; khóa khi controller busy hoặc thiếu model/controller.
Không đăng ký timer mới, tự sửa file, bật diagnostics, Apply, retry write,
khởi động/restart Fcitx5/tray, đọc clipboard hoặc log đang gõ. Model hiện có
vẫn poll service như trước; nút mới không thay chính sách polling.
Trang offline không có controller nên nút bị khóa.

UI: [RuntimeStatus](../Sources/OpenKey/linux/ui/qml/RuntimeStatus.qml).
Đây là bảng thông báo trong cửa sổ, chưa phải health monitor của engine hay
native notification/toast; mất rồi có lại service/API cần nghiệm thu desktop.

## Sao lưu / khôi phục giao diện

Trang **Nâng cao** có công cụ JSON cho **theme và trạng thái menu**, không
phải sao lưu toàn bộ OpenKey. Không chứa cấu hình bộ gõ, EN/VI, Telex/VNI,
phím tắt, macro hoặc nội dung Chuyển mã; không đọc/ghi cấu hình Fcitx5, gọi
Apply hoặc restart dịch vụ.

1. Nhấn **Tạo JSON giao diện** để lấy lựa chọn hiện tại của cửa sổ vào ô
   văn bản. Thao tác này không lưu file hoặc truy cập clipboard. Chọn văn bản
   rồi Ctrl+C để lưu thủ công ra nơi bạn muốn.
2. Dán JSON đã lưu vào ô, nhấn **Kiểm tra / xem trước**. Chưa đổi theme/menu
   hoặc ghi preferences. Nếu sửa JSON, phải kiểm tra lại trước khi khôi phục.
3. Nhấn **Khôi phục giao diện** để xác nhận. Model kiểm tra lại dữ liệu và
   lưu hai lựa chọn trong một lần đồng bộ atomic của QSettings. Chỉ khi lưu
   thành công mới đổi giao diện; lỗi ghi giữ lựa chọn trước đó, không báo đã
   khôi phục hoặc ghi đè file cấu hình đang bị chặn do sai định dạng/symlink.

Định dạng bản sao độc lập với INI preferences:

```json
{"format":"openkey-ui","version":1,"theme":"dark","sidebarExpanded":false}
```

Giới hạn 4096 đơn vị UTF-16; từ chối NUL/surrogate lẻ, JSON không phải object,
thiếu/thừa trường, sai kiểu, format/version hoặc theme ngoài whitelist
`system/light/dark`. JSON được phân tích bằng Qt; khóa trùng lấy giá trị cuối,
cùng quy tắc ở xem trước và khôi phục. Không nhận đường dẫn file từ JSON.
Ô nhập là plain text, bản nháp chỉ trong bộ nhớ và mất khi đóng cửa sổ.
Chuyển trang không tự tạo/xem trước/khôi phục. Trong chế độ offline hoặc
smoke/screenshot không có model preferences nên các nút bị khóa.

Model và kiểm thử: [UiPreferences](../Sources/OpenKey/linux/ui/UiPreferences.h),
[UI sao lưu](../Sources/OpenKey/linux/ui/qml/PreferencesBackup.qml).
Từ **preview.7**, phần giao diện có **Mở file giao diện…** và **Lưu thành file…**.
Tạo JSON trước khi lưu, hoặc mở file để đưa vào nháp; luôn xem trước và xác
nhận **Khôi phục giao diện** riêng. Mở file không đổi theme/menu hoặc ghi INI.
Hủy hộp chọn giữ nháp; file sai định dạng, quá lớn hoặc không đọc được giữ
nội dung cũ và hủy xem trước. Chỉ file local thường, UTF-8/BOM và giới hạn
giống file bộ gõ; lưu QSaveFile. Offline/smoke/screenshot không mở/lưu file.

Sao lưu bộ gõ là phần riêng, có mở/lưu file từ preview.6. Hai phần từ chối
JSON của nhau cả khi đọc và ghi; không dùng JSON giao diện để khôi phục bộ gõ.
Chưa có rollback tự động nhiều khóa runtime Fcitx5. Hộp chọn native/portal
và lưu bền sau khôi phục vẫn cần nghiệm thu desktop.

## Điều hướng bàn phím trong UI

Các phím sau dùng phạm vi **cửa sổ OpenKey**, không đăng ký hotkey hệ thống và
không thay phím EN/VI hoặc cấu hình Fcitx5:

| Phím | Thao tác |
| --- | --- |
| Ctrl+Tab | Trang tiếp theo, quay vòng qua đủ 8 trang |
| Ctrl+Shift+Tab | Trang trước, quay vòng |
| F1 | Mở Giới thiệu, không tự nạp giấy phép hoặc mở URL |
| Tab / Shift+Tab | Chuyển focus giữa điều khiển khả dụng |
| Lên / Xuống trong sidebar | Chuyển trang và focus mục điều hướng |

Chuyển bằng shortcut đưa focus tới mục sidebar tương ứng. Chuyển sang trang
khác cuộn nội dung về đầu; chọn lại trang hiện tại giữ vị trí cuộn. Bản nháp
Chuyển mã không bị xóa, không gọi Apply/Đọc lại, clipboard hoặc chuyển đổi.
Không áp dụng shortcut khi một cửa sổ khác đang active; compositor vẫn có thể
giữ một tổ hợp trước khi ứng dụng nhận được nó.

Menu thu gọn vẫn có tên accessible và tooltip khi hover/focus bàn phím. Nút
thu gọn menu có viền focus theo theme, các mục sidebar khai báo trạng thái đang
chọn; tiêu đề trang được khai báo heading. Đây là hỗ trợ metadata/focus, không
phải chứng nhận đã nghiệm thu Orca/AT-SPI trên desktop thật.

Binary build nằm trong thư mục build. Có gói `.deb` UI riêng `openkey-settings`
preview.1, không nằm trong `.deb` addon và không kèm tray. Xem
[cách build/cài/gỡ và giới hạn bản local](UBUNTU_INSTALL.md#gói-giao-diện-riêng--preview1).
Install component riêng tên `openkey_ui`, gồm binary, D-Bus activation và
desktop entry theo prefix. Không bật autostart và không restart service.
Gói dùng Qt/QML hệ thống, không phải portable/AppImage; cấu hình
`OPENKEY_BUILD_UI_DEB=ON` cần UI ON, addon/service/DEB addon OFF và prefix `/usr`.
Gói preview.1 amd64 đã được build trên Ubuntu 26.04/Qt 6.10.2: 15/15 CTest,
payload sáu file, checksum, dependency và smoke binary giải nén đạt. ELF được
quét bằng dpkg-shlibdeps; QML imports khai báo riêng vì không nằm trong ELF.
Chưa cài gói bằng APT hay nghiệm thu menu/singleton trên desktop; build kiểm
tra không thay bản UI local, addon hoặc daemon đang chạy.
Đã bổ sung [test vòng đời payload](UBUNTU_INSTALL.md#kiểm-tra-vòng-đời-payload-trong-root-tạm)
trong root/database/log dpkg riêng: unpack/reinstall/nâng bản QA/remove/
unpack lại/purge đạt, 5/5 fixture cấu hình nguyên vẹn. Không configure,
giải dependency bằng APT hoặc dùng bản QA để nâng cấp máy thật; fixture/log
được giữ dưới build, không phải gói phát hành mới.

## Mở từ tray và một cửa sổ mỗi phiên

Menu tray có hai mục riêng:

- **Cài đặt OpenKey…** (1000): gọi `org.openkey.Settings1.Show` trên session bus.
  Nếu UI chưa chạy, D-Bus activation mở executable đã cài; nếu đang chạy, hiện
  lại cửa sổ đó. Không chuyển kiểu gõ hoặc gửi Apply.
- **Cấu hình bộ gõ Fcitx5…** (1001): vẫn mở công cụ cấu hình Fcitx5 như trước.

UI giữ name `org.openkey.Settings`, object `/org/openkey/Settings`. Chạy lệnh
normal lần hai chỉ gửi Show đến unique owner hiện có rồi thoát; không thay
theme/page/cấu hình của cửa sổ trước. Đóng UI giải phóng name, tray/bộ gõ vẫn chạy.
Không có session bus thì bản normal báo lỗi thay vì mở bản trùng; dùng `--offline`
nếu chỉ muốn xem trước. Không áp dụng singleton với phiên UI cũ chưa có tính năng này:
hãy đóng cửa sổ cũ sau khi cập nhật.

Tray xử lý mở UI trong worker, timeout 3 giây; chỉ kích hoạt UI khi người dùng
click mục 1000, không tự retry. ACK chỉ xác nhận đã nhận yêu cầu, không bảo đảm
cửa sổ đã được đưa lên trước. Wayland có thể hạn chế focus stealing; xem overview
của desktop nếu cửa sổ không được nâng lên.

Cài riêng cho người dùng từ root repository:

```bash
cmake -S . -B build/ubuntu-ui -DOPENKEY_BUILD_UI=ON \
  -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build build/ubuntu-ui --parallel 4
cmake --install build/ubuntu-ui --component openkey_ui
```

Component cài binary và `share/dbus-1/services/org.openkey.Settings.service`.
Nếu lần đầu tạo thư mục service mà session bus chưa phát hiện, có thể yêu cầu
bus đọc lại cấu hình (không restart bus hoặc Fcitx5):

```bash
busctl --user call org.freedesktop.DBus /org/freedesktop/DBus \
  org.freedesktop.DBus ReloadConfig
```

Đường dẫn Exec lấy từ **CMAKE_INSTALL_PREFIX lúc configure**; đổi prefix thì cần
configure/build lại, không chỉ dùng `cmake --install --prefix` khác. Activation
cần session bus desktop có DISPLAY/WAYLAND_DISPLAY thích hợp và thư mục data
người dùng được D-Bus tìm thấy. Không dùng `QT_QPA_PLATFORM=offscreen` cho cài đặt
desktop. Service/tray vẫn không phụ thuộc Qt nếu UI build OFF; nếu thiếu UI thì
menu báo chưa mở được, không tự cài phần mềm hoặc thay cấu hình.

Sau khi cập nhật binary tray, chỉ restart `openkey-tray.service`, không restart
Fcitx5. Thông tin build/install tray: [Ubuntu service](UBUNTU_SERVICE.md).

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

## Chẩn đoán Direct — trang Nâng cao

Mở **Cài đặt nâng cao → Chẩn đoán Direct**. Card hiển thị runtime đọc từ addon,
không suy ra từ file người dùng. Chọn Bật/Tắt rồi nhấn **Áp dụng**; chỉ đổi lựa
chọn chưa ghi. Nút Đọc lại tải runtime mới. Offline, busy hoặc schema lỗi sẽ khóa
điều khiển, không tự bật chẩn đoán.

Patch chỉ chứa `DirectDiagnostics` dạng chuỗi `True`/`False`, không đổi mode
Preedit/Direct, Telex/VNI hoặc các tùy chọn khác. UI xác nhận lại runtime sau ACK;
không bảo đảm lưu bền khi addon báo lỗi ghi file. Card “Chế độ đã lưu” ở Tổng quan
vẫn hiển thị diagnostics từ file riêng để đối chiếu.

Log Direct chứa app/frontend, capability và lý do xử lý; không chứa nội dung gõ,
selection, offset hoặc recovery text. Vẫn có metadata về ứng dụng, nên chỉ bật
khi điều tra lỗi và nên tắt khi đã thu thập đủ. UI không đọc/tải log lên và không
thay đổi cấu hình journal. Các tùy chọn nâng cao khác chưa được port.

Kiểm thử desktop còn chờ: đọc trạng thái thật, bật rồi tắt bằng Apply, xác nhận
runtime đổi và InputStyle giữ nguyên, đối chiếu diagnostics đã lưu ở Tổng quan.
Không cần bật diagnostics nếu bạn không muốn thu thập metadata; có thể chỉ kiểm
tra đọc trạng thái và nút Đọc lại.

## Chính tả & gõ nhanh — tích hợp một phần

Hai card **Kiểm tra chính tả khi đặt dấu** (`CheckSpelling`) và **Đặt dấu kiểu
mới** (`UseModernOrthography`) đọc runtime/schema qua cùng controller. Mặc định
cả hai bật, giữ hành vi engine trước alpha.10; UI không tự áp mặc định.

- Chọn Bật/Tắt chưa ghi; **Áp dụng** chỉ gửi khóa tương ứng rồi đọc lại runtime.
  Đọc lại không ghi, không restart bộ gõ và không chuyển EN/VI hoặc Telex/VNI.
- CheckSpelling bật: `fas` (Telex) / `fa1` (VNI) giữ nguyên; tắt: `fá`.
  Đây là kiểm tra âm tiết trong engine, không phải trình soát lỗi tài liệu.
- Đặt dấu kiểu mới bật: `hoas` / `hoa1` → `hoá`; tắt → `hóa`.
- Hai tùy chọn áp dụng chung Telex/VNI và Preedit/Direct. Chỉ Apply khi đã gõ
  xong từ vì thay đổi kết thúc context hiện tại. Không sửa lại chữ đã commit.
- Cần addon alpha.10 hoặc schema tương thích. Thiếu một khóa, schema sai/trùng
  hoặc runtime sai kiểu sẽ khóa cả nhóm mới, không vô hiệu mode/diagnostics
  tương thích từ addon cũ. Offline/busy cũng khóa thao tác ghi.
- Nâng cấp theo [hướng dẫn cài](UBUNTU_INSTALL.md#chính-tả-và-đặt-dấu--alpha10).
  Gói chỉ chứa addon, không kèm UI/tray. Module đã nạp không tự thay bằng file mới:
  lưu việc rồi nạp lại framework theo desktop/đăng nhập lại, sau đó Đọc lại UI.
  Đóng cửa sổ UI cũ rồi mở lại để chạy binary mới (singleton chỉ Show cửa sổ cũ).

### Gõ nhanh Telex — alpha.11

Card thứ ba dùng khóa `QuickTelex=True|False`, mặc định False trong addon mới.
UI chỉ đọc giá trị thật, không suy ra False nếu thiếu schema. Chọn chưa ghi;
Apply patch một khóa và xác nhận runtime, Đọc lại chỉ đọc. Thay đổi kết thúc
context hiện tại nên chỉ Apply khi đã gõ xong từ.

Khi bật trong Telex: `cc→ch`, `gg→gi`, `kk→kh`, `nn→ng`, `qq→qu`, `pp→ph`,
`tt→th`. Áp dụng cả Preedit/Direct; VNI bỏ qua tùy chọn này. Không tự đổi input
method để phù hợp tùy chọn. Đây không phải gõ tắt/macro hay phụ âm đầu/cuối.

Capability độc lập: thiếu/sai/trùng schema QuickTelex chỉ khóa card này, không
khóa hai thiết lập chính tả từ alpha.10 hoặc mode/diagnostics. Ngược lại, nhóm
chính tả không tương thích không tự vô hiệu QuickTelex hợp lệ. UI không dựa
duy nhất vào số phiên bản để cho ghi. Xem [cài alpha.11](UBUNTU_INSTALL.md#gõ-nhanh-telex--alpha11).

Gõ nhanh phụ âm đầu/cuối, khôi phục từ sai, macro và các quy tắc bổ sung chưa được nối backend.
Xác nhận runtime không thay xác minh lưu bền khi addon không ghi được cấu hình.

## Phím tắt — cấu hình Fcitx5 chỉ đọc

Trang **Phím tắt** hiển thị tám danh sách từ runtime của Fcitx5 đang chạy:
bật/tắt, chuyển tạm thời, bật, tắt, bộ gõ tiếp/trước, nhóm bộ gõ tiếp/trước.
Tên phím giữ nguyên theo Fcitx5 (ví dụ `Control+space`, `Shift_L`), không đoán
phím mặc định của desktop. Danh sách rỗng hợp lệ hiện “Chưa gán”; offline,
schema không tương thích hoặc mất kết nối hiện “Chưa xác định”, không giữ mẫu cũ.

Đây là phím **toàn cục của framework**, không phải phím riêng OpenKey; chuyển
nhóm không đồng nghĩa đổi EN/VI hoặc Telex/VNI. Chuyển tạm thời còn phụ thuộc
trạng thái kích hoạt trước đó. Dữ liệu runtime chỉ xác nhận cấu hình đang nạp,
không chứng minh GNOME/compositor/ứng dụng đã chuyển tổ hợp phím đến Fcitx5.

- Mở UI normal hoặc nhấn **Đọc lại** thực hiện truy vấn chỉ đọc; không poll.
- Khi đổi phím từ công cụ khác, nhấn Đọc lại để lấy cấu hình mới.
- Nếu cần sửa: menu tray **Cấu hình bộ gõ Fcitx5… → Global Options**. Thao tác
  ngoài UI này ảnh hưởng chung các bộ gõ; kiểm tra xung đột với phím desktop.
- Không đọc file cấu hình Fcitx5, không gọi SetConfig/Restart, không bắt phím
  toàn cục hoặc đọc phím đang gõ. Offline/smoke/screenshot không tạo model D-Bus.
- Hotkey X11 thử nghiệm của service để mở cấu hình là tính năng riêng, không
  được truy vấn hay bật ở trang này. Không suy ra hotkey đó hoạt động trên Wayland.

Trang đã có backend đọc; phần chỉnh sửa trực tiếp vẫn chưa triển khai. Xem
[API chỉ đọc](UBUNTU_CONFIG_API.md#phím-tắt-toàn-cục--chỉ-đọc).

## Ứng dụng & tương thích — tích hợp một phần

Trang hiển thị hai thông tin độc lập:

- Loại phiên lấy từ `XDG_SESSION_TYPE` lúc UI normal khởi động, chỉ nhận
  `x11`, `wayland`, `tty`; không kiểm chứng compositor hoặc suy đoán từ DISPLAY.
- Backend lấy từ `QGuiApplication::platformName()`, chỉ thuộc **cửa sổ OpenKey**.
  `xcb` trong phiên Wayland có thể là XWayland, không xác định backend VS Code
  hoặc trình duyệt. Offscreen/minimal được ghi rõ, không giả thành desktop.

[UiEnvironment](../Sources/OpenKey/linux/ui/UiEnvironment.h) chỉ lọc hai chuỗi
theo whitelist, không đọc process/window title, nội dung gõ, log hoặc đường dẫn
socket. Dữ liệu lạ hiện Chưa xác định, không phản chiếu chuỗi môi trường tùy ý
vào giao diện. Offline/smoke/screenshot không đọc `XDG_SESSION_TYPE`; vẫn cho
biết backend render Qt cục bộ như `offscreen`. Dữ liệu là snapshot lúc mở UI,
không có polling hoặc ghi file cho phần môi trường.

Card chế độ gõ dùng **cùng một instance** ở Tổng quan và Tương thích: chuyển trang
không tạo bản nháp thứ hai, không gửi Apply hoặc Đọc lại. Chỉ Apply mới gọi API
addon hiện có, chỉ sửa `InputStyle`; không cần addon mới riêng cho trang này.
Runtime Direct chỉ xác nhận cấu hình, không xác nhận ứng dụng khác đang dùng
Direct. Có thể fallback Preedit nếu thiếu surrounding-text, hoặc ngừng thay dấu
khi mất đồng bộ giữa từ. Đọc được API cũng không thay kiểm thử gõ thực tế.

Các card hướng dẫn nói rõ phạm vi chung Telex/VNI, giới hạn frontend, đối chứng
VS Code bằng profile riêng và checklist gợi ý/Backspace/Space/focus/undo. UI không
chạy script hoặc tự sửa VS Code. Kết quả profile accessible là lịch sử trên máy
đã thử, không cam kết mọi backend/phiên bản. Xem [hướng dẫn cài và giới hạn](UBUNTU_INSTALL.md)
và [đối chứng VS Code](UBUNTU_VSCODE_INPUT_INVESTIGATION.md).

Danh sách ứng dụng, nhận diện backend của ứng dụng đang focus và chính sách
riêng từng app **chưa triển khai**; không có switch giả cho các tính năng này.

## Chuyển mã — công cụ Unicode thủ công

Nhập/dán văn bản vào **Chuyển mã**, chọn ba tùy chọn rồi nhấn **Chuyển đổi**:

- Chuẩn hóa: giữ dạng Unicode, NFC (tổ hợp sẵn) hoặc NFD (phân rã).
- Chữ: giữ nguyên, chữ hoa hoặc chữ thường theo quy tắc Unicode của Qt, không
  phụ thuộc locale desktop; chưa có viết hoa đầu câu/từng từ.
- Dấu: giữ hoặc bỏ dấu chữ cái tiếng Việt, kể cả `đ/Đ → d/D`. Bỏ dấu không
  nhận diện ngôn ngữ: `é` trong tiếng Pháp cũng thành `e`, nhưng không xóa tùy
  ý dấu của `ñ`, `ü`, chữ Hy Lạp hay mọi combining mark.

Ví dụ chọn chữ thường + bỏ dấu: `Tiếng Việt — ĐẶNG 😀` → `tieng viet — dang 😀`.
Nguồn không bị thay; kết quả chỉ đọc, có thể chọn và nhấn Ctrl+C bằng thao tác
người dùng. Đổi nguồn hoặc lựa chọn xóa kết quả cũ, không tự chuyển đổi lại.
Chuyển trang giữ bản nháp trong bộ nhớ; **Xóa văn bản** xóa cả nguồn và kết quả.
Đóng UI không lưu bản nháp vào preferences, không khôi phục ở lần chạy mới.

[TextConverter](../Sources/OpenKey/linux/ui/TextConverter.h) là model Qt Core
cục bộ dùng Qt Concurrent, độc lập engine legacy và cấu hình addon. Pipeline: nếu bỏ dấu thì NFC
trước, ánh xạ bảng chữ tiếng Việt; sau đó đổi hoa/thường, cuối cùng chuẩn hóa
đích nếu có. Bật bỏ dấu có thể đổi dạng tổ hợp của các ký tự khác sang NFC ngay
cả khi bộ chọn chuẩn hóa là Giữ dạng. Không bỏ dấu + giữ dạng + giữ chữ phải
giữ nguyên chuỗi, bao gồm emoji, ký tự ngoài BMP, xuống dòng và khoảng trắng.

Giới hạn nguồn 65536 đơn vị UTF-16 (emoji thường chiếm hai đơn vị), kết quả tối
đa 262144; từ chối surrogate lẻ/NUL hoặc tham số không nằm trong whitelist.
Lỗi xóa kết quả cũ và báo trạng thái, không trả nội dung chuyển một phần. Giới
hạn kiểm tra cả trong model, không chỉ dựa nút UI. Đây không phải kiểm tra dữ
liệu UTF-8 thô: ô nhập Qt đã cung cấp QString.

Xử lý bằng Qt Concurrent worker, không chạy chuẩn hóa trên luồng giao diện.
Sau khi nhấn Chuyển đổi, trạng thái là `running`, kết quả cũ bị xóa và nút
Chuyển đổi khóa; nút **Hủy** giữ nguyên nguồn nhưng bỏ kết quả của tác vụ đó.
Sửa nguồn, đổi tùy chọn hoặc Xóa văn bản cũng vô hiệu hóa tác vụ đang chạy,
không tự chạy lại. Kết quả muộn không được đưa trở lại UI hoặc clipboard.

Mỗi model chỉ nhận một tác vụ tại một thời điểm, không xếp hàng khi bấm liên
tục. Hủy phản hồi ngay về mặt giao diện; mỗi lời gọi chuẩn hóa/hoa-thường của
Qt không thể ngắt giữa chừng. Worker kiểm tra hủy giữa các bước và trong vòng
ánh xạ bỏ dấu, nên nút Chuyển đổi có thể còn khóa đến khi bước Qt cũ kết thúc.
Đóng model không chờ worker hoặc để worker truy cập QObject đã hủy; thoát hẳn
ứng dụng vẫn có thể chờ thread pool Qt hoàn tất bước đang chạy. Không hứa hủy
CPU tức thời hoặc giới hạn thời gian cho mọi chuỗi Unicode. Giới hạn cũ 8192
đã được thay sau khi chuyển tính toán sang worker; tài liệu lớn hơn 65536 vẫn
cần chia nhỏ. Qt Concurrent thuộc gói phát triển Qt base dùng trong build/CI.

Hoạt động cả `--offline`, không cần Fcitx5/tray. Không tự đọc/ghi clipboard,
file, tài liệu editor hoặc gửi văn bản lên dịch vụ; không log nội dung. Dán và
Ctrl+C là thao tác chủ động của người dùng. Chưa có chuyển VNI/TCVN3/CP1258,
đọc/ghi file, tự phát hiện bảng mã, chuyển clipboard nhanh hoặc hotkey toàn cục.
NFC/NFD là dạng chuẩn hóa Unicode, **không phải** chuyển bảng mã legacy.

### Dán nguồn và sao chép kết quả

Nút **Dán từ clipboard** đọc văn bản khi người dùng click, không tự chuyển
đổi. Thành công thay nguồn và xóa kết quả cũ, kể cả khi dán lại cùng nội dung.
Clipboard rỗng/không có text, Unicode lỗi hoặc quá 65536 đơn vị UTF-16 báo lỗi
và giữ nguyên bản nháp/kết quả hiện có. Không giữ một cache clipboard trong model.

Nút **Sao chép kết quả** chỉ bật khi có kết quả chuyển đổi hợp lệ, không rỗng.
Click gửi văn bản thuần tới clipboard; không sao chép tự động sau khi chuyển.
Giới hạn copy 262144 đơn vị UTF-16, kiểm tra Unicode/NUL trước khi ghi. Không
ghi giá trị rỗng hoặc xóa clipboard khi thất bại. Thông báo chỉ xác nhận đã gửi
yêu cầu cho Qt, không xác nhận clipboard manager/ứng dụng khác sẽ giữ nguyên nó.

[ClipboardBridge](../Sources/OpenKey/linux/ui/ClipboardBridge.h) chỉ gọi API Qt
trong hai thao tác trên, không đăng ký signal theo dõi clipboard, timer hay
đọc lại sau copy. Các nút dùng chế độ Clipboard, không chủ động đọc/ghi PRIMARY
selection. Qt nhận văn bản từ owner trước khi model kiểm tra giới hạn; transfer
lớn hoặc owner chậm vẫn có thể gây chờ, nhất là Wayland. Chưa có fetch worker
hoặc timeout clipboard riêng; không coi giới hạn ký tự là giới hạn transfer.

Hoạt động cả offline khi người dùng chủ động bấm nút. Đây là ngoại lệ chủ động
với clipboard, không phải tự đọc dữ liệu nền. Xóa ô nguồn/kết quả hoặc đóng UI
**không xóa clipboard/lịch sử clipboard của hệ điều hành** và không thu hồi văn
bản đã sao chép. Không có thao tác ghi vào tài liệu editor đang mở hoặc tự dán
sang ứng dụng khác. Chuyển clipboard nhanh bằng hotkey toàn cục vẫn chưa có.

## Giới thiệu — metadata UI và giấy phép offline

[AboutInfo](../Sources/OpenKey/linux/ui/AboutInfo.h) cung cấp phiên bản nguồn UI
từ `PROJECT_VERSION` khi build, phiên bản Qt lúc biên dịch và Qt đang chạy.
Không lấy phiên bản từ tên gói `.deb`, không chạy git/dpkg/shell và không hỏi
daemon. Cùng số phiên bản nguồn có thể có các thay đổi chưa commit khác nhau;
đây không phải mã định danh commit hoặc xác nhận addon alpha.11 đang được nạp.

Trang ghi nguồn engine gốc Mai Vũ Tuyên, địa chỉ bản fork và OpenKey gốc. Các
địa chỉ hiển thị PlainText có thể chọn/sao chép thủ công, không phải hành động
mở URL tự động. Không có nút kiểm tra/tải cập nhật, không truy cập mạng hay gửi
metadata/diagnostics ra ngoài.

Nút **Xem giấy phép** đọc nguyên văn [LICENSE](../LICENSE) đã đóng vào Qt resource
của binary; không phụ thuộc checkout hoặc file trên máy cài. Nội dung chỉ được
nạp vào model khi nhấn nút, hiển thị PlainText chỉ đọc trong vùng cuộn; **Ẩn giấy
phép** xóa chuỗi khỏi model. Lỗi resource báo rõ, không tải bản thay thế từ mạng.
Giấy phép dự án không thay thế thông báo giấy phép Qt/thư viện hệ thống; trang
này không tuyên bố đã hoàn thành toàn bộ yêu cầu phân phối các thư viện đó.

Hoạt động offline, không cần Fcitx5/tray. Thiếu model thì metadata hiện Chưa xác
định và nút xem bị khóa. Đóng UI không dừng bộ gõ; phần này không thêm autostart
hoặc cập nhật addon, không lưu trạng thái mở giấy phép vào preferences.

## Kiểm thử

Build UI có 15 CTest: engine smoke/regression, launcher VS Code headless, clipboard bridge, metadata/giấy phép,
chuyển đổi Unicode, preferences UI, môi trường UI,
phím tắt chỉ đọc,
activation/single-instance, model
chỉ đọc, controller D-Bus, UI interaction, embedded-resource smoke và UI interaction
tại scale 200%. Controller hiện có 227 ca QtTest và activation có 17 ca trên
bus giả lập riêng. Bộ addon/service đạt 16/16, gồm launcher worker và menu mới.
Sau mở rộng diagnostics đã chạy UI đạt 8/8 và addon/service đạt 16/16;
thêm preferences UI đạt 9/9. Test preferences dùng đường dẫn tạm riêng: đọc không
tạo file, round-trip mở lại, file sai/phiên bản lạ, symlink/FIFO và lỗi ghi;
UI kiểm tra thao tác lưu và thông báo lỗi ở scale 100%/200%.
Sao lưu giao diện có test xuất/kiểm tra JSON không đọc file (kể cả FIFO),
không tạo thư mục hay phát signal; schema/kiểu/giới hạn/Unicode lỗi, xem trước
không ghi, kiểm tra lại khi khôi phục, lưu cặp giá trị một lần và mở lại.
File lỗi/symlink/FIFO/không tạo được thư mục giữ giá trị cũ khi khôi phục lỗi,
không phát lựa chọn nhập tạm thời. Các thao tác đổi theme/menu thông thường
vẫn giữ lựa chọn trong phiên khi không lưu được, như trước.
UI sáng/tối ở 100%/200% kiểm tra offline khóa nút, tạo JSON hiện tại, xem
trước không đổi giao diện, sửa JSON khóa khôi phục, input sai/quá dài, xác
nhận bằng bàn phím, lưu/mở lại, lỗi ghi và cửa sổ nhỏ. Xác nhận không gọi
controller Apply/refresh, chuyển đổi, clipboard hoặc đưa bản nháp vào JSON.
Sau tích hợp JSON giao diện: 15/15 CTest Release đạt; preferences 67 ca,
UI 48 ca ở mỗi scale 100%/200%. Đã cài riêng component UI, binary khớp bản
build và smoke headless đạt; không thay addon hoặc restart Fcitx5/tray.
Đóng cửa sổ UI cũ rồi mở lại để thấy công cụ mới; clipboard native và thao
tác sao lưu trên desktop vẫn chờ nghiệm thu thủ công.
Activation test kiểm tra cold launch từ service file, bấm Show lặp giữ owner/PID,
mở executable lần hai không nhân bản cửa sổ và các mode offline không chiếm name.
UI dùng offscreen/software renderer và config/socket fixture riêng, không dùng
service hoặc file cấu hình thật của desktop.
Điều hướng bàn phím có test sáng/tối ở scale 100%/200%: quay vòng hai chiều,
F1 từ ô nhập, giữ bản nháp, cuộn về đầu trang mới, focus menu thu gọn và
Tab/Shift+Tab. Một cửa sổ Qt khác nhận focus không kích hoạt shortcut OpenKey.
Test xác nhận không Apply/refresh, đọc giấy phép, chuyển đổi hoặc dùng clipboard;
chưa kiểm chứng screen reader hay tổ hợp phím bị compositor chặn.
UI interaction kiểm tra chuyển đủ 8 trang, từ chối index ngoài phạm vi, sidebar
244/72, theme và thao tác picker, phím lên/xuống, kích thước tối thiểu 720×560,
binding model sang card. Model tests kiểm tra enum/partial reply, mất kết nối,
timeout/reconnect, polling, phản hồi quá dài/sai và config lỗi/thiếu, FIFO/symlink,
không ghi file và runtime không an toàn.
Bảng kết nối/lỗi có test nguồn giả độc lập: loaded/confirmed, applying/busy,
unconfirmed/conflict/unsupported, unavailable/error/invalid; service timeout/
protocol-error, thiếu input method và file thiếu/lỗi. Không hiện mẫu cũ như
đã xác nhận khi service mất kết nối; không hiển thị enum lạ hoặc HTML thô.
Đọc lại bằng bàn phím chỉ gọi refresh/reload một lần, thiếu controller/busy
khóa nút; chuyển trang và đổi trạng thái không Apply, retry write, truy cập
clipboard hoặc chuyển đổi. Có bố cục sáng/tối, cửa sổ nhỏ ở scale 100%/200%.
Sau bảng trạng thái: 15/15 CTest Release đạt, UI có 50 ca ở mỗi scale
100%/200%. Đã cài riêng component UI, binary khớp build và smoke headless
đạt; không thay addon hoặc restart Fcitx5/tray. Đóng cửa sổ UI cũ rồi mở lại
để dùng bảng mới; chưa nghiệm thu trạng thái lỗi/kết nối lại trên desktop thật.
Controller tests chạy bus mock riêng; UI kiểm tra chọn không apply, explicit Apply
và khóa điều khiển busy/offline. Không gửi SetConfig đến Fcitx5 trên desktop thật.
Launcher VS Code có 24 ca stub headless, không mở editor thật hoặc dùng bus
desktop; xem [phạm vi và cách chạy](UBUNTU_VSCODE_INPUT_INVESTIGATION.md#hồi-quy-trình-chạy--cập-nhật-2026-11-04).
Các test launcher/menu “thiếu UI” dùng bus config không có activation directories,
không khởi động UI đã cài trong tài khoản. Cold-activation test dùng service file
fixture riêng, không dùng activation file desktop thật.
Trang Nâng cao có test bật/tắt diagnostics giữ nguyên mode; popup sáng/tối và
scale 100%/200% cũng bao phủ bộ chọn diagnostics. API có test conflict theo từng
khóa, kiểu chuỗi boolean và giữ thay đổi đồng thời ở khóa còn lại.
Trang Chính tả kiểm tra bật/tắt hai hướng qua explicit Apply, giữ nguyên khóa
khác, schema addon cũ vẫn dùng mode, nhóm thiếu/sai/trùng không ghi; UI khóa
busy/offline và test tương phản popup sáng/tối ở 100%/200%. Engine/addon được
kiểm tra Telex/VNI, options riêng từng context, reload Preedit và đổi cấu hình
khi Direct còn ACK/phím chờ. Gói alpha.10 qua 7/7 CTest và test addon giải nén;
chưa tự cài addon, chưa nghiệm thu desktop hai tùy chọn mới.
Model Phím tắt có 33 ca QtTest trên bus riêng: dữ liệu hợp lệ/rỗng, thứ tự chỉ
số qua 10, giới hạn số mục/độ dài, schema sai/thiếu/trùng, dữ liệu lỗi, timeout,
busy, mất/đổi owner và callback muộn. Kiểm tra cờ NO_AUTO_START từ wire headers
qua dbus-monitor trên bus fixture; không dùng dữ liệu bàn phím hoặc bus desktop.
UI thử chỉ đọc, refresh bằng bàn phím, ẩn mẫu cũ khi loading/lỗi/mất kết nối,
khóa offline và bố cục ở cửa sổ nhỏ, scale 100%/200%.
Helper môi trường kiểm thử whitelist, chữ hoa/khoảng trắng/HTML/control và tổ
hợp độc lập như Wayland/xcb hoặc TTY/offscreen. UI Tương thích kiểm tra cùng
instance picker với Tổng quan, không ghi khi chuyển trang/chọn, Apply đúng mode
không đổi diagnostics/chính tả, trạng thái chưa xác nhận, khóa offline/busy,
cuộn cửa sổ nhỏ và popup tương phản sáng/tối ở scale 100%/200%.
Model chuyển đổi kiểm thử các chữ tiếng Việt/hoa/thường, NFC/NFD, emoji/Unicode,
giới hạn nguồn, dữ liệu/option lỗi, xóa kết quả cũ và reset. UI kiểm tra chỉ
chuyển khi nhấn nút, giữ nguồn, đổi option/nguồn xóa kết quả, bản nháp qua điều
hướng, nút Xóa và popup sáng/tối ở scale 100%/200%. Không dùng clipboard thật
để kiểm thử và không gửi nội dung tới API Fcitx5.
Worker có test giữ thread pool bằng semaphore: trạng thái running/busy,
timer trên event loop vẫn chạy, signal phát trên luồng chủ, từ chối gửi liên
tục khi busy, cancel/clear không xuất lại kết quả muộn và nhận tác vụ mới sau
khi worker nghỉ. Hủy model khi worker chưa chạy không chờ và không truy cập
QObject đã hủy. Test Unicode ở giới hạn mới 64 Ki/256 Ki đơn vị UTF-16 giữ
đầy đủ kết quả NFC/NFD, không cắt chuỗi. UI sáng/tối ở 100%/200% thử nút Hủy
bằng bàn phím, khóa Chuyển đổi/Sao chép, giữ nguồn, chuyển trang khi running,
sửa nguồn/tùy chọn và Xóa trước khi worker trả về. Không kiểm thử clipboard thật.
Sau chuyển sang worker: 15/15 CTest Release đạt; model chuyển đổi có 104 ca,
UI có 45 ca cho mỗi scale 100%/200%. Build Debug ASan/UBSan chạy riêng 13 ca
lifecycle/cancel/clear/destruction, bật leak detection, không phát hiện lỗi.
Không suy kết quả sanitizer này sang mọi bước Unicode hoặc desktop native.
Clipboard dùng backend giả cho cả unit/UI test: không đọc clipboard thật,
constructor không truy cập, chỉ đọc/ghi khi gọi, copy không readback, Unicode/
giới hạn/MIME lỗi không ghi, Dán giữ nguồn khi lỗi và Copy bị khóa khi chưa có
kết quả. Clipboard native và hành vi ownership/history cần nghiệm thu desktop.
AboutInfo kiểm tra metadata đúng bản build/Qt runtime, nội dung resource khớp
nguyên bản LICENSE, nạp lặp/ẩn/mở lại và isolation model. UI kiểm tra đọc theo
click, không nạp khi chỉ mở trang, nguồn/giấy phép PlainText chỉ đọc, khóa thiếu
model, bố cục cửa sổ nhỏ ở theme sáng/tối và scale 100%/200%.
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

Người dùng xác nhận đã kiểm thử UI trên desktop sau sửa popup và yêu cầu chuyển
task. Đây là nghiệm thu cơ bản trong phiên hiện tại, không thay thế ma trận
X11/Wayland, nhiều mức DPI, screen reader hoặc xác nhận lưu bền khi addon gặp lỗi.

- [ ] Mở/đóng, chuyển đủ trang, sidebar mở/thu gọn; không ảnh hưởng gõ tiếng Việt.
- [ ] Tổng quan → Kết nối & lỗi: đối chiếu service, runtime và file độc lập;
  Đọc lại không tự Apply/restart. Thử mất rồi có lại service/API, file lỗi và
  unconfirmed/conflict; không coi dữ liệu đã lấy mẫu là kiểm chứng gõ trong app.
- [ ] Chuyển Sáng/Tối/Theo hệ thống; kiểm tra chữ, focus và contrast.
- [ ] Thử Ctrl+Tab/Ctrl+Shift+Tab/F1 từ ô nhập, sidebar mở/thu gọn và cửa sổ
  khác active; giữ bản nháp Chuyển mã, không tự Apply. Thử Orca/AT-SPI riêng.
- [ ] Nâng cao → Sao lưu giao diện: tạo/chọn/Ctrl+C và dán JSON thủ công,
  xem trước chưa đổi theme/menu, sửa JSON phải xem lại, khôi phục rồi mở lại.
  Thử lỗi JSON/quyền ghi, sáng/tối/cửa sổ nhỏ; bộ gõ không thay đổi.
- [ ] Resize tối thiểu, cuộn nội dung; thử scale 100%, 150%, 200%.
- [ ] Đổi theme/thu gọn sidebar, đóng UI rồi mở lại: giữ lựa chọn. Thử offline
  không làm đổi các thiết lập đã lưu của cửa sổ normal.
- [ ] Tab/Shift+Tab, Enter/Space, phím lên/xuống trong menu; screen reader.
- [ ] GNOME/Wayland và X11; theme hệ thống khi đang chạy.
- [ ] Khi service đang chạy, Tổng quan hiện mẫu trạng thái; đối chiếu menu tray
  sau khi chuyển Telex/VNI/tiếng Anh ở ứng dụng khác rồi quay lại cửa sổ.
- [ ] Card “Chế độ đã lưu” khớp cấu hình đã lưu, không hiểu nhầm là runtime hiện tại.
- [ ] Thử `--offline`: không lấy trạng thái, không đọc cấu hình.
- [ ] Chọn mode khác nhưng chưa nhấn Apply: runtime không đổi.
- [ ] Giới thiệu: metadata chỉ thuộc UI, mở/ẩn/đọc cuộn giấy phép, chọn/sao chép
  địa chỉ dự án; không mở browser hoặc kiểm tra cập nhật khi vào trang. Thử
  cửa sổ nhỏ và theme sáng/tối, đóng UI vẫn gõ tiếng Việt bình thường.
- [ ] Trang Chuyển mã: thử NFC/NFD/hoa/thường/bỏ dấu, emoji/xuống dòng, chọn
  kết quả và Ctrl+C thủ công; đổi tùy chọn xóa kết quả, Xóa văn bản xóa cả hai
  ô, đóng/mở UI không khôi phục văn bản. Thử cuộn và chọn chữ ở cửa sổ nhỏ.
- [ ] Với nguồn dài gần 65536 đơn vị UTF-16, thử Hủy bằng bàn phím, sửa
  nguồn/tùy chọn hoặc Xóa trong khi chạy; không hiện lại kết quả cũ. Chuyển
  trang vẫn phản hồi/giữ nguồn; đợi worker cũ dừng rồi chạy lại chủ động.
- [ ] Thử Dán từ clipboard (plain text, emoji, HTML-only, rỗng/quá dài), giữ
  bản nháp nếu lỗi; Chuyển đổi rồi Sao chép kết quả và dán thủ công sang editor.
  Xóa nguồn hoặc đóng UI không tự xóa clipboard; kiểm tra Wayland/X11 riêng.
- [ ] Trang Tương thích: loại phiên/backend Qt chỉ mô tả UI, không hiểu thành
  backend ứng dụng khác; chuyển Tổng quan ↔ Tương thích không tự ghi. Thử Apply
  khi đã gõ xong từ, mode đồng bộ giữa hai trang, diagnostics/chính tả giữ nguyên.
- [ ] Trang Phím tắt khớp Global Options của Fcitx5; Đọc lại sau khi thay đổi
  bằng công cụ Fcitx5. UI không đăng ký hoặc đổi phím khi mở trang; phân biệt
  “Chưa gán” với “Chưa xác định”. Kiểm tra cuộn ở cửa sổ nhỏ và scale 200%.
- [ ] Nâng cấp alpha.10, mở lại UI; thử riêng hai tùy chọn Bật/Tắt với ví dụ trên
  trong Telex/VNI và Preedit/Direct. Đổi từng khóa khi đã xong từ, khóa khác giữ
  nguyên; đóng/mở lại để đọc runtime và đối chiếu cấu hình lưu bền riêng.
- [ ] Apply Preedit → Direct → Preedit; xác nhận runtime, thử gõ sau mỗi lần đổi,
  đọc file đã lưu riêng và mở lại UI. Không thực hiện khi đang gõ dở từ.
- [ ] Click “Cài đặt OpenKey…” khi UI đóng: cửa sổ hiện; click lại nhiều lần
  không nhân bản cửa sổ. Thử cả khi cửa sổ đang minimize.
- [ ] Đóng UI vẫn gõ được tiếng Việt; mở lại từ tray. Mục cấu hình Fcitx5 vẫn
  độc lập, không bị thay bằng cửa sổ OpenKey.

## Bước tiếp theo

Đã có model read-only và API áp dụng chế độ gõ với xác nhận runtime. Tiếp theo
nghiệm thu desktop và mở rộng từng tùy chọn theo schema, không ghi thẳng file
hoặc coi ACK là bằng chứng lưu bền.
Chưa đánh dấu các trang đã port xong chỉ vì đã có mục điều hướng.

Đã cài binary UI và activation file vào `~/.local`, cập nhật service và restart
riêng `openkey-tray.service` trên máy thử; không restart Fcitx5. D-Bus đã nhận
activation name sau ReloadConfig. Tray đang `waiting-host` ở lần kiểm tra cuối,
chưa xác nhận panel đã đăng ký lại; không tự đổi GNOME extension. Chưa tự mở UI trong phiên desktop của
người dùng: nghiệm thu thao tác menu/minimize/đóng-mở ở checklist bên trên.

Mã nguồn: [UI](../Sources/OpenKey/linux/ui/),
[kiểm thử UI](../Tests/LinuxSettingsTests.cpp),
[kiểm thử model](../Tests/LinuxSettingsModelTests.cpp). Theo dõi trong
[Ubuntu tasks](UBUNTU_TASKS.md#v04--ui-giống-windows).

## Preview.9

Thêm Macro CRUD/JSON/file (backend addon alpha.49), editor từng hành động
phím tắt Fcitx5 và quy tắc InputStyle theo program ID cụ thể; hỗ trợ trạng
thái Simple Telex 1/2. [Cách dùng và giới hạn](UBUNTU_FEATURES_49.md).
Các ô nhập mới có màu nền/chữ/placeholder/selection theo theme.

## Preview.10 — chọn bộ gõ

Trang **Tổng quan → Chọn bộ gõ** có Telex, VNI, Simple Telex 1/2 và tiếng Anh
US, tùy các mục đã thêm trong nhóm Fcitx5 hiện tại. Chọn trong danh sách rồi
nhấn **Chuyển bộ gõ**; chỉ thay lựa chọn trên combo chưa gửi lệnh. Nút Đọc lại
và lúc cửa sổ trở lại active đọc danh sách/trạng thái từ Fcitx5.

Controller Qt D-Bus hoạt động độc lập service tray: resolve owner, đọc nhóm và
list/current/state, kiểm tra lại nhóm; trước chuyển đối chiếu nhóm/current/state
với mốc đã đọc, kiểm tra target vẫn trong nhóm. SetCurrentIM một lần rồi đọc
lại target/trạng thái; mất ACK/owner/timeout báo chưa xác nhận, không retry.
Không sửa profile/list IM, cấu hình addon, clipboard hay khởi động Fcitx5.
API Fcitx5 không có compare-and-swap; vẫn có race giữa preflight và ghi. Mẫu
thuộc context gần nhất của Fcitx5, không cam kết mọi app giữ cùng một bộ gõ.

Nếu thiếu Simple Telex, thêm vào nhóm trong Fcitx5 Configuration → Input
Method → Add, bỏ Only Show Current Language, tìm OpenKey Simple Telex 1/2;
sau đó quay lại UI và Đọc lại. Cần addon alpha.49 trở lên để có descriptor.

Menu icon **OpenKey** có các mục chuyển Telex/VNI/Simple Telex 1/2/tiếng Anh,
dấu radio theo trạng thái service lấy mẫu, và Cài đặt OpenKey/Cấu hình Fcitx5.
Source menu đã có từ alpha.49; lần này build/cập nhật binary tray local và nạp
lại riêng `openkey-tray.service` để menu mới có hiệu lực. Truy vấn menu live
xác nhận đủ năm action và radio; không tự chọn IM để thử trên tài liệu thật.
Không coi menu trả dữ liệu là đã nghiệm thu việc gõ trong mọi ứng dụng.

Gói UI: `dist/linux/openkey-settings_3.1.0~preview.10-1_amd64.deb`.
UI đang mở bản cũ cần đóng rồi mở lại. Tray local và Fcitx5 addon cài độc lập.

## Alpha.50 / UI preview.11

Context riêng, năm tùy chọn nâng cao, chuyển file lớn có Hủy/timeout và
sao lưu IM/phím tắt bằng file: [hướng dẫn và giới hạn](UBUNTU_FEATURES_50.md).

## Preview.12 — mở trang từ menu icon

Service mới có lệnh Gõ tắt, Chuyển mã và Giới thiệu, mở đúng trang trong
singleton UI qua ShowPage. UI cũ cần đóng/mở sau cập nhật; gói UI không kèm
service. [Cài thử, giao thức và giới hạn](UBUNTU_TRAY_PAGES.md).

## Preview.13 — nhớ bộ gõ theo ứng dụng

Công tắc ở đầu trang Ứng dụng & tương thích; cần addon alpha.51.
[Hướng dẫn sử dụng, lưu bền và giới hạn program ID](UBUNTU_APP_MEMORY.md).
