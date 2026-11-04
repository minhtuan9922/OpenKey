# Session service Linux — nền tảng v0.3

`openkey-service` là tiến trình phụ để nối tray/UI sau này, không phải input
framework hoặc một engine thứ hai. Hiện có lifecycle, single-instance,
IPC status/quit, monitor trạng thái Fcitx5 nền và menu chọn Telex/VNI/tiếng Anh (US).
Không can thiệp phím đang gõ (ngoài hotkey X11 mở cấu hình nếu bật rõ ràng),
không tự sửa danh sách input method, không restart Fcitx5, không
tự bật autostart. Có công cụ opt-in bật/tắt autostart riêng bên dưới.
Tray chỉ bật khi yêu cầu rõ qua `--run-tray`, lấy trạng thái từ
monitor. Thoát service không tắt bộ gõ.

## Build và test

Từ root repository:

```bash
cmake -S . -B build/ubuntu-service -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr \
  -DOPENKEY_BUILD_SERVICE=ON -DOPENKEY_BUILD_FCITX5=ON
cmake --build build/ubuntu-service --parallel 2
ctest --test-dir build/ubuntu-service --output-on-failure
```

Tùy chọn service mặc định OFF; không thêm dependency Qt ở bước này. Build Linux
dùng C++17, API POSIX/Linux và SDK `libsystemd` (`pkg-config`, `libsystemd-dev` trên
Ubuntu). Test reader cần `dbus-run-session`; các dependency đã có trên máy phát
triển, không tự cài thêm. Dependency này chỉ áp dụng khi bật service, không thay
addon build mặc định. Test dùng runtime/D-Bus tạm, không truy vấn hay dừng Fcitx
thật của phiên desktop.

## Chạy thủ công (tùy chọn)

Trong terminal thuộc phiên đăng nhập của bạn, `XDG_RUNTIME_DIR` phải tồn tại,
thuộc UID hiện tại và có permission riêng tư 0700. Không tự đặt thành một thư
mục dùng chung. Không dùng sudo.

```bash
build/ubuntu-service/Sources/OpenKey/linux/service/openkey-service --run
```

Tiến trình chạy foreground để quan sát hoặc được supervisor quản lý sau này;
chưa daemonize/autostart. Trong terminal khác cùng phiên:

```bash
build/ubuntu-service/Sources/OpenKey/linux/service/openkey-service --status
build/ubuntu-service/Sources/OpenKey/linux/service/openkey-service --input-status
build/ubuntu-service/Sources/OpenKey/linux/service/openkey-service --quit
```

Exit code: 0 thành công, 1 không có service (các lệnh client), 2 lỗi môi
trường/tham số/IPC, 3 đã có instance (`--run`). `running` chỉ có nghĩa service nền
đang chạy, không phải đang bật tiếng Việt hay Fcitx5 khỏe mạnh.

## Trạng thái input chỉ đọc

`--input-status` hỏi service đang chạy và đọc cache của monitor nền, không gọi
D-Bus trong vòng xử lý IPC. Giá trị ban đầu có thể là `unknown` cho tới khi đủ mẫu.
Kết quả là một trong các giá trị sau; exit 0 là truy vấn IPC thành công, kể cả
giá trị `unavailable`/`unknown` (không đồng nghĩa engine sẵn sàng):

| Giá trị | Ý nghĩa |
|---|---|
| `telex` | State active và IM là `openkey-telex` |
| `vni` | State active và IM là `openkey-vni` |
| `english` | IM chính xác `keyboard-us`, với state hợp lệ |
| `inactive` | State inactive, IM không phải `keyboard-us`; không khẳng định layout English |
| `other` | Active nhưng IM/layout khác OpenKey hoặc US keyboard |
| `unknown` | Chưa có context, lỗi response, state đổi khi đọc hoặc method timeout |
| `unavailable` | Không kết nối bus hoặc không tìm được owner Fcitx5 |

Không đánh dấu mọi `keyboard-*` là English hoặc mọi bộ gõ Việt khác là OpenKey.
Đây là context gần nhất mà Fcitx controller báo, không phải đảm bảo context đang
focus, trạng thái ô password, hay khả năng Direct/Preedit của từng ứng dụng.
Hai lần đọc State giúp phát hiện một số thay đổi giữa truy vấn, không tạo atomic
snapshot; state có thể đổi lại hoặc IM thay đổi ngay sau khi lấy mẫu.

Gọi GetNameOwner trước, không auto-activate service; State/CurrentInputMethod đi
đến unique owner đã tìm được, không trộn reply từ hai daemon sau restart.
Mỗi method call giới hạn 75 ms. Lỗi giữa mẫu trả unknown, không dùng trạng thái
cũ. Client chờ reply input-status tối đa 1 giây; request thường vẫn 250 ms.
Không có thao tác Activate/Deactivate/Toggle.
Không log nội dung gõ, định danh cửa sổ hay tài liệu.

### Monitor, debounce và dữ liệu cũ

Service `--run` tạo một worker chỉ đọc, nghỉ 300 ms sau mỗi lần query (không phải
cam kết chu kỳ chính xác 300 ms). Khi trạng thái hợp lệ đổi, công bố `unknown`
trong lúc chờ hai mẫu liên tiếp cùng trạng thái mới; không giữ V/E cũ trong lúc
candidate thay đổi. `unknown`/`unavailable` được công bố ngay, không chờ debounce.
Độ trễ hiện trạng thái mới ít nhất một khoảng polling và còn phụ thuộc thời gian
truy vấn; chưa đo latency/tài nguyên trên desktop.

Cache dùng monotonic clock, hết hạn sau 2 giây không có kết quả; `--input-status`
sẽ trả `unknown` ngay cả khi worker còn bị kẹt. Kết quả query kéo dài từ 2 giây
trở lên cũng bị loại, không đóng timestamp mới cho mẫu quá cũ. Khôi phục sau gap
dài hoặc lỗi cần hai mẫu mới. Worker không giữ cache mutex khi gọi D-Bus, nên
truy vấn chậm không chặn việc đọc cache/tiếp nhận quit. Khi thoát service, worker
được đánh thức khỏi khoảng chờ và join; query đã bắt đầu vẫn phải kết thúc trước
khi tiến trình thoát, không dùng force-kill/cancel thread.

Monitor chưa subscribe Fcitx signals; exporter tray đọc cache theo tick rồi phát
SNI signals tới host. Hiện service luôn poll
khi đang `--run`; addon alpha.9 không tự chạy service này. Debounce/cache chỉ
giảm nhấp nháy và stale label, không chứng minh focus hoặc hỗ trợ Direct của app.

## Tray trạng thái — tùy chọn

Build service như trên, không cần Qt SDK hoặc cài lại addon alpha.9. Trong Terminal
desktop Ubuntu/KDE, từ repository:

```bash
build/ubuntu-service/Sources/OpenKey/linux/service/openkey-service --run-tray
```

Nếu đã chạy service bằng `--run`, dừng riêng service đó với `--quit` rồi mở lại
`--run-tray` (khóa một instance vẫn dùng chung). Không dừng Fcitx5. Trong terminal
khác cùng phiên:

```bash
build/ubuntu-service/Sources/OpenKey/linux/service/openkey-service --tray-status
build/ubuntu-service/Sources/OpenKey/linux/service/openkey-service --input-status
```

`--tray-status`: `disabled` khi dùng `--run`; `unavailable` nếu chưa kết nối bus;
`waiting-host` khi đang chờ watcher chấp nhận đăng ký; `registered` khi watcher đã
chấp nhận item. **Registered không chứng minh icon được panel render** hoặc host
có mặt; phải kiểm tra màn hình. Không có watcher thì service/IPC vẫn chạy, retry
đăng ký sau 5 giây hoặc ngay khi owner watcher đổi; không auto-start watcher.

Giao thức dùng `org.kde.StatusNotifierItem` tại `/StatusNotifierItem` và
`org.kde.StatusNotifierWatcher`, với menu D-Bus tối thiểu tại `/Menu`. Ký tự được
vẽ bằng nét vector bo tròn vào IconPixmap ARGB 24×24 và 48×48, khử răng cưa,
nền trong suốt, chữ gần trắng có viền tối mảnh để đọc trên panel sáng/tối;
không phụ thuộc font/icon theme. `T` là OpenKey Telex active, `N` là OpenKey VNI active,
`E` là `keyboard-us`, `-` là inactive, `O` là IM khác; unknown/unavailable đều hiện
`?` nhưng title/tooltip và dòng menu phân biệt nguyên nhân. Menu có dòng trạng
thái disabled, **Chuyển sang Telex**, **Chuyển sang VNI**, **Chuyển sang tiếng Anh (US)**,
**Cài đặt OpenKey…**, **Cấu hình bộ gõ Fcitx5…**, dòng kết quả thao tác
và hướng dẫn giới hạn phím tắt, cùng lệnh **Thoát tray OpenKey (giữ bộ gõ)**. Click trực tiếp icon/
scroll không đổi input method hoặc cấu hình.

Lệnh thoát dùng ID `2000` (`POPUP_OPENKEY_EXIT`) từ
[TrayContract.json](../Tests/TrayContract.json), với phạm vi Linux rõ ràng:
chỉ dừng `openkey-service`, tháo icon và dọn IPC; Fcitx5 và addon OpenKey vẫn
gõ bình thường. Không tắt autostart: nếu đã bật, tray sẽ chạy lại lần đăng nhập
sau. Để mở lại ngay, chạy `openkey-service --run-tray`.

Chỉ `Event(2000, "clicked", ...)` yêu cầu thoát; hover/open/close hoặc click dòng
trạng thái/root không tác động. Trả lời host trước khi main loop thoát sạch,
không gọi `exit()` từ callback D-Bus. Cùng UID có thể gọi lệnh qua session bus,
tương tự IPC `--quit`. Chưa port các lệnh bật/tắt tiếng Việt, chính tả, macro, chuyển mã,
theme và các trang cấu hình đầy đủ; UI riêng hiện mở được từ mục 1000 sau khi
cài component UI. Không hiện lệnh giả/chưa hỗ trợ. Đây vẫn là subset
dbusmenu, chưa hỗ trợ `EventGroup` hay chứng nhận mọi host. Đối chiếu
[giao thức libdbusmenu](https://sources.debian.org/src/libdbusmenu/18.10.20180917~bzr492%2Brepack1-2/libdbusmenu-glib/dbus-menu.xml).

### Chọn Telex/VNI hoặc tiếng Anh từ tray

Hai lệnh Telex/VNI dùng ID 910/911 theo TrayContract; tiếng Anh dùng ID 920
riêng Linux, chỉ gửi yêu cầu khi click đúng mục. Đây là selector trực tiếp
`keyboard-us`, không giả lập lệnh toggle bật/tắt tiếng Việt ID 900 trên Windows.
Để trở lại tiếng Việt, chọn Telex/VNI đã có trong nhóm. Không tự đoán kiểu gõ
cuối cùng khi trạng thái chưa rõ, không bật/tắt theo cache cũ.
Dấu chọn của ba mục lấy từ cùng cache monitor với icon, không lấy từ click
gần nhất hoặc kiểu gõ người dùng vừa yêu cầu. Tooltip/title ghi rõ
`OpenKey — Telex (sampled)` hoặc `OpenKey — VNI (sampled)`.
Menu dùng `toggle-type=radio` và `toggle-state` 0/1 theo dbusmenu: panel có thể
vẽ chấm tròn thay vì dấu tick, nhưng chỉ một kiểu gõ được chọn. English đánh dấu
mục tiếng Anh, không chọn Telex/VNI; inactive, other, unknown, unavailable không
chọn mục nào. Khi đổi Telex/VNI/English, debounce
cũng bắt đầu lại: mẫu đầu mới làm mất lựa chọn cũ, đủ hai mẫu mới hiển thị T/N/E.
Hết TTL/lỗi bus xóa lựa chọn; không giữ trạng thái cuối như đang còn hợp lệ.
Vì monitor polling Fcitx5, đổi bằng phím tắt/ứng dụng khác cũng được cập nhật.
Dòng kết quả disabled cho biết đang xử lý/thành công/chưa xác nhận/
thiếu cấu hình. Khi đang xử lý, ba mục chọn và mục cấu hình tạm disabled;
không xếp hàng click. Quit vẫn hoạt động.

Worker riêng thực hiện controller D-Bus với timeout 75 ms mỗi call, pin unique
owner và không auto-activate Fcitx5. Trước khi gửi `SetCurrentIM`, đọc nhóm hiện
tại, kiểm tra target có trong `InputMethodGroupInfo`, đọc lại tên nhóm và owner.
Chỉ whitelist `openkey-telex`/`openkey-vni`/`keyboard-us`; không sửa profile hay thêm target.
Sau gửi, đọc `CurrentInputMethod` để xác nhận; timeout/lỗi sau write là “chưa xác
nhận”, **không tự thử lại** vì write có thể đã được áp dụng. Worker không chặn
vòng menu/IPC; khi thoát service chỉ chờ các call có timeout đang thực hiện.
Icon/dấu chọn vẫn do monitor xác nhận, không đổi T/N/E lạc quan theo click.
IPC `--input-status` nay trả `telex`/`vni` thay cho giá trị gộp `vietnamese` cũ;
các giá trị khác giữ nguyên. Khi nâng cấp, cập nhật binary client/service cùng lúc.

Theo [controller Fcitx5](https://github.com/fcitx/fcitx5/blob/5.1.19/src/modules/dbus/dbusmodule.cpp),
đổi IM là thao tác trên context/nhóm Fcitx5 quản lý, không gắn với cửa sổ ứng dụng
ban đầu. Click panel có thể đổi focus; guard nhóm/owner chỉ best-effort, không
atomic với `SetCurrentIM`. Cần nghiệm thu khi quay lại VS Code/editor và khi
ShareInputState khác nhau; không hứa menu điều khiển đúng context mọi desktop.

Máy thử đã quan sát nhóm có `keyboard-us` và Telex: chọn VNI phải báo thêm kiểu
gõ vào nhóm trước. Nếu thiếu `keyboard-us`, chọn tiếng Anh cũng báo thiếu cấu hình,
không tự đổi layout khác hay deactivate Fcitx5. Đây không phải nút tắt daemon.
Để thử VNI, người dùng mở cấu hình Fcitx5, thêm **OpenKey VNI** vào nhóm đang dùng;
không tự thêm chỉ để chạy test. Có thể mở cấu hình từ menu bên dưới; chưa có
cửa sổ chọn kiểu gõ trong UI OpenKey; UI mới chỉ áp dụng chế độ Preedit/Direct.
Test tự động chỉ đổi IM trên mock/private D-Bus; không chuyển bộ gõ người dùng.

### Mở cấu hình Fcitx5 từ tray

Mục mới **Cài đặt OpenKey…** (1000) mở UI Qt riêng, không thay thế mục 1001 bên
dưới. Gọi `org.openkey.Settings1.Show` qua session bus, cho phép D-Bus activation
chỉ với thao tác click; worker không chạy shell hoặc tìm executable qua PATH.
Thiếu UI/activation file thì báo lỗi, không tự cài hoặc khởi động Fcitx5.
ACK không bảo đảm compositor đưa cửa sổ lên trước. UI giữ một cửa sổ cho phiên
normal, đóng UI không thoát tray/bộ gõ. Hướng dẫn cài và giới hạn Wayland:
[Ubuntu UI](UBUNTU_UI.md#mở-từ-tray-và-một-cửa-sổ-mỗi-phiên).

UI request chia sẻ slot với các lệnh chuyển IM/cấu hình Fcitx5, timeout 3 giây,
không tự retry. Hotkey X11 hiện vẫn mở cấu hình Fcitx5, chưa đổi sang UI OpenKey.
Service build hiện đạt 16/16 CTest; launcher có test Show, thiếu UI, lỗi/timeout,
slot busy và không chặn event loop trên private bus; menu test giữ nguyên trạng
thái bộ gõ/tiến trình tray khi UI không có. UI có private-bus cold-activation và
single-instance tests riêng, chưa thay nghiệm thu panel/focus trên desktop.
Launcher/menu tests dùng [bus config không có activation dirs](../Tests/fixtures/dbus/no-activation.conf)
để tình huống “thiếu UI” không vô tình khởi động executable đã cài ở tài khoản.

Mục **Cấu hình bộ gõ Fcitx5…** dùng ID 1001 riêng Linux, khác với control panel
OpenKey Windows ID 1000. Chỉ `clicked` gửi controller `Configure` đến unique
owner Fcitx5 đang chạy, không auto-activate daemon và không chạy shell/đường dẫn
chương trình do bên gọi cung cấp. Hover/open/close không mở cửa sổ.

Worker có timeout và chia sẻ slot với lệnh chuyển IM; request không chặn vòng
tray. Nhận reply rỗng đúng signature chỉ báo **Đã gửi yêu cầu mở cấu hình Fcitx5**,
không xác nhận cửa sổ đã hiện. Lỗi/timeout không tự thử lại vì ứng dụng có thể
đã được mở; Fcitx5 chưa chạy thì báo unavailable, không tự start. GUI được
Fcitx5 mở bằng công cụ cấu hình của nó, không bị kéo theo vòng đời terminal agent.

Cần có công cụ GUI phù hợp (máy thử có `fcitx5-config-qt`). Mở GUI không tự chọn
IM, thêm VNI, ghi profile hay sửa InputStyle; người dùng thay đổi trong cửa sổ
khi muốn. Không phải UI Fluent của OpenKey. Nghiệm thu thực tế: click mục cấu
hình, kiểm tra cửa sổ hiện và có thể thêm OpenKey VNI/chỉnh addon OpenKey; đóng
cửa sổ rồi quay lại editor gõ tiếp. Test private-bus chỉ xác nhận method,
timeout, không auto-retry/auto-start, một request đồng thời và không đổi IM;
không mở GUI trên desktop người dùng trong lúc chạy test.

### Giới hạn phím tắt riêng của OpenKey

Menu có hai dòng disabled (ID 3/4) nêu giới hạn theo phiên và hướng dẫn
**Đổi phím tắt trong cấu hình Fcitx5**. Dùng mục cấu hình ngay phía trên để mở
công cụ Fcitx5; tray không đọc/hiển thị một tổ hợp phím mặc định có thể sai với
cấu hình của người dùng và không tự thay phím tắt.

Service lấy `XDG_SESSION_TYPE` trong môi trường lúc khởi động: `wayland` ghi
“Wayland: hotkey riêng chưa hỗ trợ”; X11 ghi đang tắt/active/xung đột/lỗi theo backend,
thiếu hoặc giá trị khác ghi chưa rõ phiên. Không đoán X11 từ `DISPLAY`, vì biến
này cũng có trên Wayland/XWayland. Nếu môi trường unit thiếu thông tin, dòng
chưa rõ phiên là có chủ ý; không tự ghi biến môi trường của desktop.

Đây là trạng thái **tính năng của OpenKey**, không khẳng định Wayland không thể
có global shortcuts nói chung. Có X11 hotkey mở cấu hình opt-in như bên dưới,
chưa tích hợp portal Wayland hoặc hotkey mở UI OpenKey riêng. Phím tắt chuyển bộ gõ vẫn do
Fcitx5 quản lý. Click/hover các dòng hướng dẫn không mở cửa sổ, chuyển IM hoặc
thoát tray; các dòng hướng dẫn không đăng ký hay bật phím tắt.

Test private-bus kiểm tra nhãn/disabled/no-op cho Wayland, X11, unknown;
không bắt phím trên desktop thật. Máy thử có XDG_SESSION_TYPE=wayland trong
môi trường systemd user; còn cần kiểm tra cách dòng thông tin render trên panel.

### Hotkey mở cấu hình trên X11 — thử nghiệm, opt-in

`Ctrl+Alt+Shift+O` mở cấu hình Fcitx5 qua cùng worker với menu Configure. Đây
**không phải** hotkey đổi Telex/VNI/English hoặc hotkey tắt daemon. Không chiếm
Ctrl+Space hay thay shortcut Fcitx5. Cần bật ở cả build và khi chạy:

```bash
cmake -S . -B build/ubuntu-service -DOPENKEY_BUILD_X11_HOTKEY=ON
cmake --build build/ubuntu-service --parallel 2
# Chỉ trong một phiên desktop X11 thực, sau khi dừng instance tray đang chạy:
build/ubuntu-service/Sources/OpenKey/linux/service/openkey-service --run-tray --x11-settings-hotkey
# Terminal khác, cùng phiên:
build/ubuntu-service/Sources/OpenKey/linux/service/openkey-service --hotkey-status
```

Build option mặc định OFF. Backend ON cần `libxcb1-dev` (runtime libxcb1).
CLI chỉ chấp nhận flag với `--run-tray`; helper desktop/systemd vẫn chạy không
flag, không tự bật hotkey. Khi dùng unit và muốn opt-in trên X11, người dùng
thêm flag vào ExecStart qua drop-in, không bật đồng thời instance thủ công.
Không hướng dẫn giả `XDG_SESSION_TYPE=x11` để vượt chặn Wayland/XWayland.

Backend chỉ kết nối DISPLAY local dạng `:N`, có quyền Xauthority sẵn, khi
XDG_SESSION_TYPE xác định X11. Wayland/unknown trả `unsupported-session` không
kết nối X; build OFF trả `not-built` nếu yêu cầu trên X11. IPC có các trạng thái
`disabled`, `unsupported-session`, `not-built`, `active`, `conflict`, `unavailable`,
`error`; menu phản ánh cùng trạng thái. Không có auto-enable hoặc retry giành phím.

XCB passive grab dùng tổ hợp cố định, kiểm tra từng root và biến thể Caps Lock/
Num Lock; BadAccess báo xung đột và đóng connection để thu hồi mọi grab dở dang.
Không dùng AnyKey/AnyModifier, không đọc/log văn bản. KeyPress chỉ đặt cờ để
main loop gửi yêu cầu mở cấu hình; một slot đang bận thì không xếp hàng lệnh.
Giữ phím không mở lặp; click/hotkey không báo cửa sổ đã hiện chỉ từ D-Bus ack.
Theo [XGrabKey](https://www.x.org/archive/X11R7.5/doc/man/man3/XGrabKey.3.html).

Giới hạn prototype: Ctrl/Alt/Shift dùng mask X11 thông thường, O theo hai cột
đầu keymap; chưa có UI remap/phím tùy chọn/đa layout toàn diện. Keymap thay đổi
hoặc protocol error làm nhả grab và báo error: restart riêng tray sau khi kiểm
tra layout, không giữ keycode cũ hay tự chiếm lại. X disconnect không làm libX11
exit service (backend dùng XCB). I/O X chạy worker, không chặn IPC trong loop;
các truy vấn setup XCB là đồng bộ, nên server bị treo có thể trì hoãn shutdown
worker; unit có TimeoutStopSec=5 nhưng chưa coi đây là bounded I/O X toàn diện.

Test thêm `xvfb`, `libx11-dev`, `libxtst-dev` khi backend ON. CMake tìm Xvfb qua
OPENKEY_XVFB; thiếu Xvfb thì cảnh báo không có integration test, không thay bằng
chạy trên DISPLAY thật. Fixture chọn display bằng `-displayfd`, không TCP;
XTEST chỉ gửi phím vào server ảo đó. Bao phủ plain typing, đúng chord, giữ phím,
Caps/Num Lock, hai client xung đột, thay keymap nhả grab và đăng ký lại sau teardown.
Máy thử dùng gói Xvfb từ kho Ubuntu giải nén tạm (không cài hệ thống); đường dẫn
cache đó không phải yêu cầu cho máy khác. Cần nghiệm thu desktop X11 thật và
xác nhận cửa sổ cấu hình hiện; hiện máy người dùng vẫn Wayland, hotkey để tắt.

Tray xử lý D-Bus và cập nhật từ cache trên vòng sự kiện service (thường tối đa
khoảng một tick 100 ms sau khi monitor đổi). Phát NewIcon/NewTitle/NewToolTip và
PropertiesChanged; watcher mất/xuất hiện lại sẽ đăng ký lại. Không tự thay đổi
GNOME extension, session input framework hoặc autostart.

GNOME cần host hỗ trợ AppIndicator/StatusNotifier; implementation được đối chiếu
với [Ubuntu AppIndicator](https://github.com/ubuntu/gnome-shell-extension-appindicator/blob/master/appIndicator.js).
Giao thức/encoding theo [SNI specification](https://specifications.freedesktop.org/status-notifier-item/latest/status-notifier-item.html)
và [KDE interface](https://github.com/KDE/kstatusnotifieritem/blob/master/src/org.kde.StatusNotifierItem.xml).
Chưa chứng nhận mọi host hoặc toàn bộ dbusmenu contract. Chưa có menu bật/tắt tiếng Việt,
hotkey/theme Windows hoặc các trang điều khiển đầy đủ. UI Qt là preview riêng,
không phải port toàn bộ TrayContract Windows.

### Ẩn tray Fcitx5

Đây là cấu hình **riêng của người dùng**, không phải hành vi tự động của service
hoặc gói cài đặt. Ngày 2026-11-03, theo yêu cầu người dùng trên máy thử:

- Đã sao lưu `~/.config/fcitx5/config` thành
  `~/.config/fcitx5/config.before-openkey-tray-20261103` (giữ quyền 0600).
- Đã gọi controller để tắt riêng module `notificationitem`; chỉ danh sách addon
  bị tắt thay đổi. Không tắt `classicui`, OpenKey hoặc Fcitx5.
- Thiết lập được lưu cho lần khởi động Fcitx5 sau; **chưa ẩn ngay icon đang chạy**.
  Không tự restart daemon để tránh gián đoạn lúc gõ. Lưu công việc và đăng xuất/
  đăng nhập lại khi thuận tiện; nếu chưa bật autostart riêng cho tray thì cần
  chạy lại `--run-tray` sau đăng nhập.

Lệnh tương đương (cần Fcitx5 đang chạy, module là `notificationitem`):

```bash
gdbus call --session --dest org.fcitx.Fcitx5 --object-path /controller \
  --method org.fcitx.Fcitx.Controller1.SetAddonsState "[('notificationitem', false)]"
```

Khôi phục riêng module, không ghi đè các thiết lập khác đã đổi sau đó:

```bash
gdbus call --session --dest org.fcitx.Fcitx5 --object-path /controller \
  --method org.fcitx.Fcitx.Controller1.SetAddonsState "[('notificationitem', true)]"
```

Cũng cần đăng nhập lại để module được nạp lại. Theo
[controller Fcitx5](https://github.com/fcitx/fcitx5/blob/5.1.19/src/modules/dbus/dbusmodule.cpp),
`SetAddonsState` lưu danh sách addon, không unload module đang chạy. Lưu ý
[Classic UI](https://github.com/fcitx/fcitx5/blob/5.1.19/src/ui/classic/classicui.cpp)
có thể fallback sang icon XEmbed nếu không có Status Notifier. Vì vậy phải kiểm
tra panel sau đăng nhập lại; chưa coi là đã loại bỏ mọi icon trùng. Không tự tắt
`legacy-tray-enabled` của GNOME vì đó là thiết lập toàn cục cho ứng dụng khác.

Icon bản mới cần khởi động lại **chỉ service OpenKey** để nạp binary mới:

```bash
build/ubuntu-service/Sources/OpenKey/linux/service/openkey-service --quit
build/ubuntu-service/Sources/OpenKey/linux/service/openkey-service --run-tray
```

Icon bản cũ đã được người dùng xác nhận hiển thị trên GNOME; hình thức icon mới
và việc hết icon trùng vẫn cần nghiệm thu trực tiếp, không suy ra từ D-Bus.

Phản hồi tiếp theo xác nhận Fcitx vẫn có icon dự phòng: kiểm tra controller cho
thấy `notificationitem` đã tắt và Classic UI vẫn bật. Sau đó quan sát thiết lập
`legacy-tray-enabled=false` trên máy thử (không phải service tự đổi). Khi người
dùng báo mất V/E, kiểm tra thấy service đã dừng sau đăng nhập, không phải mất
host tray; chạy lại `--run-tray` thì watcher chấp nhận đăng ký. Đây là lý do bổ
sung autostart opt-in, không coi `registered` là bằng chứng panel đã render.

Ca thử desktop: chuyển OpenKey/English trong một ứng dụng đang focus, chờ monitor
ổn định rồi nhìn icon; không chỉ đổi focus sang terminal để lấy trạng thái. Kiểm
tra tooltip, dòng menu disabled, T/N/E/?, dấu chọn menu, khởi động/dừng riêng service và đảm bảo
bộ gõ vẫn hoạt động. Không tự tắt/khởi động lại watcher để thử nếu chưa lưu công
việc; recovery watcher hiện được test trên private D-Bus.

Interface được đối chiếu với
[Fcitx5 5.1.19 controller](https://github.com/fcitx/fcitx5/blob/5.1.19/src/modules/dbus/dbusmodule.cpp)
và [state/context implementation](https://github.com/fcitx/fcitx5/blob/5.1.19/src/lib/fcitx/instance.cpp).

## IPC và an toàn

Lock và Unix socket nằm trong `$XDG_RUNTIME_DIR/openkey/`, thư mục mode 0700,
socket mode 0600, kiểm tra UID qua SO_PEERCRED ở cả server/client. Lock dùng
flock và không bị xóa khi dừng để tránh race hai instance. Socket được dọn khi
thoát bình thường; instance mới giữ lock mới được thu hồi socket sót sau crash.
Không tự xóa regular file hoặc symlink nằm ở đường dẫn socket.

Protocol nội bộ: một dòng `status`, `input-status`, `tray-status`, `hotkey-status` hoặc `quit`, tối đa 32 byte, thời gian đọc
250 ms; không nhận shell command hoặc văn bản đang gõ. SIGINT/SIGTERM dừng vòng
lặp, không dùng PID file để gửi signal đến tiến trình bên ngoài.
Các tiến trình cùng UID có thể yêu cầu quit; đây không phải ranh giới bảo mật
giữa những ứng dụng không đáng tin chạy dưới cùng tài khoản.

## Tự khởi động tray sau đăng nhập — opt-in

Ưu tiên cách systemd user bên dưới trên máy GNOME đang thử; desktop entry trực
tiếp là phương án cũ. Chỉ bật **một** trong hai, không chạy tray từ terminal agent
để sử dụng lâu dài. Công cụ desktop không tự phát hiện/tắt unit systemd.

[Công cụ autostart](../scripts/openkey-tray-autostart.sh) tạo riêng
`$XDG_CONFIG_HOME/autostart/openkey-tray.desktop`, hoặc
`~/.config/autostart/openkey-tray.desktop` nếu không đặt XDG_CONFIG_HOME.
Theo [XDG Autostart](https://specifications.freedesktop.org/autostart/latest/),
desktop chạy entry sau khi đăng nhập; `Hidden=true` tắt entry.
Build/install service **không tự bật**, công cụ cũng không chạy/dừng service
hoặc Fcitx5. Binary phải tồn tại ở đường dẫn cố định khi đăng nhập sau.

Thử trực tiếp với binary đã build (từ root repository):

```bash
bash scripts/openkey-tray-autostart.sh --enable \
  "$PWD/build/ubuntu-service/Sources/OpenKey/linux/service/openkey-service"
bash scripts/openkey-tray-autostart.sh --status
```

Không xóa/di chuyển build đó khi vẫn dùng entry. Với bản cài, dùng đường dẫn
binary đã cài, ví dụ `/usr/bin/openkey-service`; helper được cài với tên
`openkey-tray-autostart` trong cùng component `openkey_service`.
Service chưa nằm trong gói addon alpha.9: không giả định binary đã có trong
`/usr/bin`, không cần cài lại addon chỉ để thử autostart.

Tắt tự khởi động, không dừng tray đang chạy:

```bash
bash scripts/openkey-tray-autostart.sh --disable
bash scripts/openkey-tray-autostart.sh --status
```

`--status` báo `not-configured`, `enabled` hoặc `disabled` cho **cấu hình**, không
phải tiến trình đang chạy. Dùng `openkey-service --status`/`--tray-status` để kiểm
tra tiến trình. Nếu muốn dừng ngay, dùng riêng `openkey-service --quit`.

Công cụ ghi file mode 0600 qua file tạm cùng thư mục rồi rename; không ghi đè
entry không có marker do công cụ quản lý, không theo symlink ở entry/thư mục
autostart. Chấp nhận binary path ASCII gồm chữ/số, khoảng trắng, `/`, `.`, `_`,
`-`; từ chối ký tự cần escaping phức tạp thay vì chèn vào shell. `Exec` là đường
dẫn tuyệt đối được quote, không qua `sh -c` và không phụ thuộc PATH của GNOME.
Không đặt `OnlyShowIn`, không bật extension GNOME, không tự đổi icon Fcitx5.

Kiểm thử tự động dùng thư mục cấu hình tạm, không sửa cấu hình thật: enable,
disable, re-enable, path có khoảng trắng, input sai, entry lạ/symlink được giữ
nguyên; chạy `desktop-file-validate` nếu có. Test không đăng xuất hay khởi động
phiên desktop. **Cần nghiệm thu thủ công**: bật opt-in, lưu công việc, đăng nhập
lại, kiểm tra đúng một tray, V/E đổi theo IM và gõ vẫn hoạt động. Thử disable
rồi đăng nhập lại để xác nhận không tự chạy; test khóa một instance đã có sẵn.

## Systemd user service — chạy độc lập terminal

[Công cụ tạo unit](../scripts/openkey-tray-systemd.sh) chỉ ghi unit theo người
dùng, không tự kích hoạt hoặc đổi autostart. Cài binary vào vị trí cố định trước:

```bash
cmake --install build/ubuntu-service --prefix "$HOME/.local" --component openkey_service
bash scripts/openkey-tray-systemd.sh --install "$HOME/.local/bin/openkey-service"
```

Binary và hai công cụ được cài trong `~/.local/bin`; không phụ thuộc thư mục
build hay PATH của desktop. Unit ở `$XDG_CONFIG_HOME/systemd/user/openkey-tray.service`
(mặc định `~/.config/systemd/user/openkey-tray.service`), ghi nguyên tử mode 0600.
Từ chối symlink entry/thư mục systemd/user và file unit không có marker quản lý.
Giới hạn ký tự đường dẫn binary giống công cụ desktop; ExecStart quote đường dẫn
có khoảng trắng, không qua shell, không có `%`/`$` để systemd nội suy.

Trước khi kích hoạt: sao lưu desktop entry cũ nếu có; tắt riêng entry tray cũ
bằng helper (không tắt autostart Fcitx5). Nếu tray thủ công đang chạy, thoát nó
bằng `openkey-service --quit` và chờ dừng, rồi:

```bash
bash scripts/openkey-tray-autostart.sh --disable
systemctl --user daemon-reload
systemctl --user enable --now openkey-tray.service
systemctl --user status openkey-tray.service --no-pager
```

Unit gắn với `graphical-session.target` qua WantedBy/PartOf, không bật lingering,
không chạy system service/root. Không dùng X11/Wayland display để vẽ icon trực
tiếp: service dùng session bus của user, chờ watcher nếu panel đến muộn.
`Restart=on-failure`, nghỉ 3 giây; giới hạn 5 lần/60 giây. Exit code 3 (đã có
instance khác) không restart. Quit menu/IPC exit 0 nên **không tự chạy lại**;
`systemctl stop` cũng không restart. Theo
[systemd.service](https://www.freedesktop.org/software/systemd/man/latest/systemd.service.html).
Không có watchdog phát hiện treo hoặc bảo đảm icon render khi host bị tắt.

```bash
# Mở lại sau khi chọn Thoát tray:
systemctl --user start openkey-tray.service
# Xem log khi mất icon:
journalctl --user -u openkey-tray.service -n 50 --no-pager
# Tắt chạy hiện tại và tự chạy ở phiên sau:
systemctl --user disable --now openkey-tray.service
```

Khôi phục kiểu desktop: disable unit trước, rồi dùng helper desktop `--enable`
với đường dẫn binary cố định. Không cần xóa unit hoặc bản sao lưu.
Khi cập nhật binary, `cmake --install ... --component openkey_service` rồi
`systemctl --user restart openkey-tray.service`; build không cập nhật bản cài
hoặc tự restart. Gói addon alpha.9 vẫn chưa bao gồm service này.

Triển khai máy thử 2026-11-03 (đã được người dùng đồng ý): cài binary trong
`~/.local/bin`, sao lưu desktop entry thành `openkey-tray.desktop.before-systemd`,
tắt entry tray cũ và enable unit. Thử SIGKILL chỉ main process của unit xác nhận
PID mới và NRestarts tăng; thử `Event(2000, "clicked", ...)` trên đúng item của
unit xác nhận thoát sạch và không restart. Không tác động Fcitx5.
Phát hiện Ubuntu AppIndicators đang INACTIVE/Enabled: No ở thời điểm triển khai;
service có thể chạy nhưng ở `waiting-host` đến khi panel hỗ trợ tray được bật.
Lần kiểm tra tiếp theo extension đã ACTIVE/Enabled: Yes, không cần chỉnh lại.
Service vẫn cùng PID 68802, NRestarts=0, IPC `running`, tray `registered` và
watcher liệt kê item OpenKey. Legacy tray vẫn false; entry desktop cũ disabled,
unit systemd enabled. Không restart Fcitx5 hoặc tray trong lần kiểm tra này.
Đây là bằng chứng exporter tự phục hồi khi host xuất hiện lại, không phải xác
nhận trực quan icon hoặc nghiệm thu một phiên đăng nhập mới.
Đăng nhập mới và hiển thị icon lâu dài vẫn cần nghiệm thu desktop.
Sau đó người dùng báo dùng thêm một lúc không còn mất icon và yêu cầu chuyển
task; ghi nhận ổn định trong phiên đã thử, chưa suy ra đã qua đăng nhập mới/KDE.

Bước kế tiếp: nghiệm thu tray/polling theo focus thật, rồi chọn menu actions đã hỗ trợ;
chỉ đưa các thao tác đã hỗ trợ vào menu, không giả lập các tính năng chưa port.

Đã kiểm tra: 15/15 CTest Release khi backend X11 và Xvfb được bật, test service,
reader, monitor và tray với ASan/UBSan. Reader dùng mock controller trên private D-Bus: Telex/VNI,
inactive, US keyboard, IM khác, state đổi giữa mẫu, lỗi State/CurrentInputMethod,
timeout và owner mất/xuất hiện lại. Monitor có test V/E, flapping, mất bus,
expiry, gap dài, reader lỗi/treo và dừng worker; có integration transitions qua
private D-Bus. Lifecycle dùng runtime tạm, có ca từ chối
socket path là regular file/symlink và runtime không riêng tư.
Tray có test private-bus host đến muộn, watcher mất/xuất hiện lại, opt-in,
GetAll, pixmap ARGB, icon-change signal, dòng trạng thái readonly và click icon
không đổi trạng thái. Test Quit kiểm tra ID/label/enabled và các event không được
thoát; spawn binary service thật trên private bus/runtime riêng, click Quit,
xác nhận exit code 0 và socket được dọn. Không gửi Quit đến daemon desktop.
Đã introspect interface Fcitx/watcher thật chỉ đọc để đối chiếu; chưa nghiệm thu
reader theo focus/thay IM hay icon/menu trên panel GNOME/KDE thực tế.
