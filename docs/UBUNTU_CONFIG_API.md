# API cấu hình bộ gõ — v0.4 preview

## Phạm vi

[SettingsController](../Sources/OpenKey/linux/ui/SettingsController.h) quản lý
năm khóa `InputStyle=Preedit|Direct`, `DirectDiagnostics=True|False`,
`CheckSpelling=True|False`, `UseModernOrthography=True|False` và
`QuickTelex=True|False`. Hai khóa chính tả cần alpha.10, QuickTelex cần alpha.11
hoặc schema tương thích. QuickTelex mặc định False, chỉ có tác dụng trong Telex;
các thiết lập khác vẫn dùng chung Telex/VNI. UI chọn
giá trị rồi nhấn **Áp dụng**; đổi ComboBox không tự gửi thay đổi. Cấu hình dùng
chung cho OpenKey Telex và VNI, không chuyển input method hoặc EN/VI.
Mỗi giao dịch chỉ sửa đúng một khóa, giữ nguyên khóa còn lại và các khóa khác.

Đích cố định: session bus, service `org.fcitx.Fcitx5`, object `/controller`,
interface `org.fcitx.Fcitx.Controller1`, URI `fcitx://config/addon/openkey`.
Đã đối chiếu chữ ký bằng introspection và `GetConfig` chỉ đọc trên máy thử:

- `GetConfig(s)` → `va(sa(sssva{sv}))`: giá trị runtime và schema.
- `SetConfig(sv)` → phản hồi rỗng: addon xử lý yêu cầu, chưa bảo đảm lưu file.

Định dạng được đối chiếu với
[controller Fcitx5](https://github.com/fcitx/fcitx5/blob/master/src/modules/dbus/dbusmodule.cpp)
và [mã chuyển RawConfig](https://github.com/fcitx/fcitx5/blob/master/src/lib/fcitx-config/dbushelper.cpp).
Schema/runtime trên bus mới là dữ liệu kiểm tra lúc chạy; không chỉ dựa vào mã master.

## Giao dịch

1. Khi mở UI hoặc nhấn **Đọc lại**: lấy unique owner bằng `GetNameOwner`, gọi
   `GetConfig` trên chính owner đó. Không gọi theo well-known name để kích hoạt daemon.
2. Xác thực chữ ký, hai khóa runtime và schema `InputConfig`: InputStyle là Enum
   với `0=Preedit`, `1=Direct`; DirectDiagnostics là Boolean. Sai/thiếu thì khóa Apply.
   Nhóm chính tả chỉ được hỗ trợ khi cả hai khóa có đúng một schema Boolean và
   giá trị runtime chuỗi `True`/`False`. Thiếu, trùng hoặc sai kiểu trong nhóm
   chỉ khóa nhóm mới; addon cũ vẫn cho dùng mode/diagnostics. Không đoán mặc định
   từ khóa thiếu. Kiểm tra lại capability trước mỗi lần ghi.
   QuickTelex kiểm tra độc lập: đúng một schema Boolean và runtime chuỗi
   `True`/`False`; thiếu/sai/trùng chỉ khóa tùy chọn này. Không ép nhóm hai khóa
   chính tả phải có mặt để hỗ trợ QuickTelex, hoặc ngược lại.
3. Khi nhấn Apply: lấy lại owner, đọc cấu hình mới nhất; owner hoặc khóa đang sửa
   khác baseline thì không ghi, yêu cầu Đọc lại. Nếu đã đúng target thì không gửi SetConfig.
4. Gửi **patch chỉ chứa khóa đang sửa**, không gửi lại toàn bộ snapshot. Addon
   [nạp partial config](../Sources/OpenKey/linux/fcitx5/OpenKeyAddon.cpp) nên giữ
   khóa còn lại và các khóa khác, kể cả khi chúng thay đổi sau lần đọc trước.
   Mọi giá trị boolean được mã hóa thành chuỗi `True`/`False` đúng RawConfig,
   không gửi boolean D-Bus trực tiếp. Thay đổi ở khóa khác không gây conflict giả.
5. Sau ACK, gọi GetConfig trên cùng owner và xác thực schema/giá trị target.
   Chỉ khi khớp mới báo đã xác nhận runtime; không suy ra lưu bền thành công.

Toàn bộ lời gọi bất đồng bộ, timeout 1500 ms mỗi call, chỉ một giao dịch tại một
thời điểm. Mỗi message tắt auto-start. Không gọi Restart, ReloadConfig, Save,
Activate, SetCurrentIM, shell hoặc ghi file. `GetConfig` của Fcitx5 có thể nạp
addon OpenKey trong daemon **đang chạy**; không khởi động daemon mới.

Đổi owner sẽ vô hiệu hóa snapshot và các callback cũ. Nếu đã bắt đầu SetConfig,
lỗi/timeout/mất owner hoặc readback không khớp được báo **chưa xác nhận**:
có thể thay đổi đã xảy ra, không tự retry hoặc rollback. Người dùng nhấn Đọc lại
trước khi quyết định thao tác tiếp.

## Runtime không đồng nghĩa với lưu bền

Addon hiện gọi `safeSaveAsIni` nhưng chỉ ghi cảnh báo nếu lưu thất bại; D-Bus ACK
không trả riêng kết quả ghi đĩa. Vì vậy UI có hai phần tách biệt:

- “Chế độ gõ — cấu hình runtime”: đọc qua Fcitx5, có nút Apply và Đọc lại.
- “Chế độ đã lưu”: đọc file người dùng theo vị trí chuẩn, không xác minh file
  override của Fcitx5 và không xem là bằng chứng cấu hình đang được nạp.
- “Chẩn đoán Direct” trong trang Nâng cao: runtime DirectDiagnostics qua API;
  Apply không đổi InputStyle. Không tự bật diagnostics hoặc đọc/tải log.

Thông báo thành công chỉ xác nhận cấu hình runtime tại thời điểm readback.
Direct vẫn có thể fallback Preedit theo capability ứng dụng. Việc đổi mode có
thể kết thúc context đang gõ trong addon; chỉ Apply khi đã gõ xong từ.
Hai tùy chọn chính tả mặc định `True` giữ hành vi cũ. Mỗi context có bộ options
riêng, replay không phụ thuộc options còn lại từ context khác. Đổi mode/chính tả
kết thúc context theo cơ chế hiện có rồi cập nhật options, kể cả context đang
active nhưng idle; không cần toggle EN/VI. Ghi cấu hình giữ các khóa không biết.

API Fcitx5 không cung cấp compare-and-swap: đọc lại trước ghi giảm lỗi stale UI
nhưng **không loại bỏ race** nếu một công cụ khác đổi cùng khóa giữa hai call.
Tránh dùng đồng thời nhiều cửa sổ cấu hình; chưa có transaction/lock liên ứng dụng.

## Kiểm thử và giới hạn

[Controller tests](../Tests/LinuxSettingsControllerTests.cpp) dùng mock typed
schema trên bus riêng tạo bằng `dbus-run-session`. Kiểm tra schema sai/thiếu,
conflict theo từng khóa, ACK/readback, partial patch giữ khóa khác, timeout, không retry,
busy, giá trị ngoài whitelist, owner mất/thay thế và callback muộn. Tổng 227 ca
QtTest, gồm schema addon cũ, nhóm chính tả thiếu/trùng/sai kiểu, một khóa thay
đổi không làm ghi đè khóa khác và bật/tắt cả hai hướng.
[UI tests](../Tests/LinuxSettingsTests.cpp) dùng double không D-Bus: chọn không
ghi, nút Apply gửi đúng mode/diagnostics, khóa điều khiển khi busy/offline và cập nhật phản hồi.
Không dùng SetConfig trên bộ gõ thật trong quá trình build/test.

Chờ nghiệm thu desktop: Preedit → Direct → Preedit, gõ thử trong editor/VS Code,
đọc riêng file đã lưu và mở lại UI. Chưa bảo đảm lưu bền khi addon báo lỗi, chưa
nghiệm thu đổi mode khi đang gõ hoặc nhiều client cấu hình cùng lúc. Nâng cấp
schema mới/cũ được kiểm thử bằng mock, chưa nghiệm thu nâng cấp addon desktop.
Engine/addon có test Telex/VNI, Preedit/Direct, isolation context, reload cấu hình
ở Preedit và đổi options khi Direct còn ACK/phím chờ; chưa test riêng reload
trong Direct trên desktop.

## Phím tắt toàn cục — chỉ đọc

[HotkeyOverview](../Sources/OpenKey/linux/ui/HotkeyOverview.h) độc lập với
controller ghi addon. Chỉ gọi GetNameOwner và GetConfig với URI cố định
`fcitx://config/global` trên cùng controller/session bus. Message tắt auto-start,
pin unique owner, bất đồng bộ timeout 1500 ms; chỉ một lần đọc đang chạy, không
poll/retry. Owner đổi sẽ hủy hiệu lực callback cũ và xóa snapshot, cần Đọc lại.

Xác thực chữ ký `va(sa(sssva{sv}))`, liên kết schema `GlobalConfig.Hotkey` đến
`Hotkey$HotkeyConfig`, từng khóa đúng một option `List|Key`. Tám khóa là
TriggerKeys, AltTriggerKeys, ActivateKeys, DeactivateKeys, EnumerateForwardKeys,
EnumerateBackwardKeys, EnumerateGroupForwardKeys, EnumerateGroupBackwardKeys.
Runtime ở map con `Hotkey`; list là map chỉ số liên tiếp từ 0, tối đa 32 chuỗi,
mỗi chuỗi không rỗng/tối đa 128 ký tự/không chứa ký tự điều khiển. Chuỗi rỗng
đại diện danh sách rỗng cũng hợp lệ theo wire format Fcitx5; không dùng schema
default thay dữ liệu thiếu. Thứ tự số được giữ, không sắp theo chuỗi `0,1,10,2`.

Không cung cấp API ghi hoặc launch tool; không đọc file hay log nội dung phím.
Runtime chỉ là cấu hình framework, không xác nhận tổ hợp phím được hệ điều hành
chuyển tới framework. Thiết lập có thể dùng chung các input method, nên UI hiện
chỉ hướng dẫn mở công cụ Fcitx5 để chỉnh, không áp dụng mặc định riêng OpenKey.
