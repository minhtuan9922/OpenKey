# API cấu hình chế độ gõ — v0.4 preview

## Phạm vi

[SettingsController](../Sources/OpenKey/linux/ui/SettingsController.h) quản lý
duy nhất `InputStyle=Preedit|Direct`. UI chọn chế độ rồi nhấn **Áp dụng**; đổi
ComboBox không tự gửi thay đổi. Thiết lập dùng chung cho OpenKey Telex và VNI,
không chuyển input method hoặc EN/VI. Không sửa diagnostics hoặc các khóa khác.

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
3. Khi nhấn Apply: lấy lại owner, đọc cấu hình mới nhất; owner hoặc InputStyle
   khác baseline thì không ghi, yêu cầu Đọc lại. Nếu đã đúng target thì không gửi SetConfig.
4. Gửi **patch chỉ chứa InputStyle**, không gửi lại toàn bộ snapshot. Addon
   [nạp partial config](../Sources/OpenKey/linux/fcitx5/OpenKeyAddon.cpp) nên giữ
   diagnostics và các khóa khác, kể cả khi chúng thay đổi sau lần đọc trước.
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

Thông báo thành công chỉ xác nhận cấu hình runtime tại thời điểm readback.
Direct vẫn có thể fallback Preedit theo capability ứng dụng. Việc đổi mode có
thể kết thúc context đang gõ trong addon; chỉ Apply khi đã gõ xong từ.

API Fcitx5 không cung cấp compare-and-swap: đọc lại trước ghi giảm lỗi stale UI
nhưng **không loại bỏ race** nếu một công cụ khác đổi InputStyle giữa hai call.
Tránh dùng đồng thời nhiều cửa sổ cấu hình; chưa có transaction/lock liên ứng dụng.

## Kiểm thử và giới hạn

[Controller tests](../Tests/LinuxSettingsControllerTests.cpp) dùng mock typed
schema trên bus riêng tạo bằng `dbus-run-session`. Kiểm tra schema sai/thiếu,
conflict, ACK/readback, partial patch giữ diagnostics, timeout, không retry,
busy, giá trị ngoài whitelist, owner mất/thay thế và callback muộn.
[UI tests](../Tests/LinuxSettingsTests.cpp) dùng double không D-Bus: chọn không
ghi, nút Apply gửi đúng mode, khóa điều khiển khi busy/offline và cập nhật phản hồi.
Không dùng SetConfig trên bộ gõ thật trong quá trình build/test.

Chờ nghiệm thu desktop: Preedit → Direct → Preedit, gõ thử trong editor/VS Code,
đọc riêng file đã lưu và mở lại UI. Chưa bảo đảm lưu bền khi addon báo lỗi, chưa
test đổi mode khi đang gõ, nâng cấp schema hoặc nhiều client cấu hình cùng lúc.
