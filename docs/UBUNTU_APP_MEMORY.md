# Ghi nhớ bộ gõ theo ứng dụng — alpha.53 / UI preview.17

OpenKey có thể tự nhớ English (US), Telex, VNI và Simple Telex 1/2 theo ứng
dụng, rồi khôi phục khi quay lại. Đây là chức năng riêng với quy tắc InputStyle
Preedit/Direct/CommitOnly/NativeDirect đã có trước đó.

## Bật và sử dụng

1. Cài addon alpha.53 và UI preview.17; nạp addon mới sau khi lưu công việc
   và đăng xuất/đăng nhập. Đóng/mở lại cửa sổ UI cũ.
2. Mở **Ứng dụng & tương thích → Ghi nhớ bộ gõ theo ứng dụng**, chọn **Bật**
   rồi **Áp dụng**. Mặc định tắt; cài gói không tự bật chức năng.
3. Đưa focus vào ứng dụng cần nhớ, chọn Telex/VNI/Simple Telex/English bằng
   phím tắt hoặc menu icon. Chuyển sang ứng dụng khác, chọn bộ gõ khác rồi
   quay lại để kiểm tra lựa chọn đầu được khôi phục.

Lựa chọn được gắn với app đang có input context được focus; nếu đang nhập
trong cửa sổ cài đặt thì đó có thể là chính app cài đặt. Trong danh sách đã nhớ, có thể sửa bộ gõ của ứng dụng khác mà không
chuyển bộ gõ của cửa sổ đang dùng. Mọi cửa sổ/context có cùng program
ID dùng chung lựa chọn. Chưa có phân biệt terminal/editor bên trong VS Code.

Từ thư mục dự án trên Ubuntu 26 amd64:

```bash
sudo apt install ./dist/linux/fcitx5-openkey_3.1.0~alpha.53-1_amd64.deb ./dist/linux/openkey-settings_3.1.0~preview.17-1_amd64.deb
```

Bản user-local có thể che bản UI `/usr`; phiên phát triển cập nhật riêng
binary user-local sau kiểm tra gói. Addon đang chạy không được tự thay/restart.
UI cũ hoặc addon cũ thiếu schema sẽ khóa công tắc tương ứng.

## Nhận diện ứng dụng và giới hạn

- Chỉ dùng **program ID chính xác do frontend Fcitx5 cung cấp**. Không dò tên
  cửa sổ, URL, nội dung nhập, clipboard hoặc đoán app qua process foreground.
- ID rỗng/chung như `gnome-shell`, `ibus`, `fcitx5`, `unknown`, `wayland`
  không được ghi nhớ. GNOME/IBus chỉ báo `gnome-shell` thì chưa thể nhớ riêng
  Chrome và VS Code trên đường bridge đó. Cài bản này không giải quyết giới
  hạn nhận diện hoặc lỗi gõ Chrome omnibox.
- Chỉ khôi phục một trong năm bộ gõ đang có trong nhóm Fcitx5 hiện tại.
  Không tự thêm bộ gõ, đổi nhóm, đổi default của nhóm hoặc cấu hình toàn cục.
  Nhóm thiếu bộ gõ đã nhớ thì bỏ qua khôi phục, giữ lựa chọn đã lưu.
- Lần đầu gặp app hợp lệ, nhớ bộ gõ hiện tại; sau đó cập nhật theo sự kiện đổi
  bộ gõ khi app có focus. Không học từ đổi nhóm, thay capability hoặc sự kiện
  do chính thao tác khôi phục tạo ra. Chọn một bộ gõ ngoài năm tên hỗ trợ sẽ
  bỏ lựa chọn cũ của app đó để không tự kéo nó về Telex lần sau.
- Password/Sensitive/Disable không được học hoặc khôi phục; lựa chọn đã nhớ
  của các context bình thường không bị ghi đè vì chuyển sang ô mật khẩu.
- Tối đa 128 program ID. Khi đầy, giữ các mục đã có và không thêm app mới;
  có màn hình quản lý/xóa từng mục từ alpha.53/preview.15. Không cam kết GNOME/KDE hoặc mọi
  frontend đều phát focus/switch event đầy đủ; cần nghiệm thu desktop.

## Lưu bền

Công tắc nằm trong `~/.config/fcitx5/conf/openkey.conf`, khóa
`RememberInputMethodPerApplication`. Bảng nhớ nằm riêng ở
`~/.config/fcitx5/conf/openkey-app-memory.conf`, trường `Entries` dùng định
dạng hex(UTF-8 ID):hex(tên IM). Đây là encoding, không phải mã hóa bí mật.
Chỉ có program ID và tên bộ gõ; không lưu phím, surrounding text hay tiêu đề.
Đường dẫn tuân theo user config path của Fcitx5 nếu đã tùy chỉnh.

Lưu bằng safeSaveAsIni khi lựa chọn thay đổi, đọc lại khi addon khởi tạo hoặc
reload cấu hình. Nếu ghi thất bại, runtime vẫn nhớ trong phiên đó và journal
có cảnh báo cố định; không bảo đảm giữ sau đăng nhập lại. File sai schema bị
bỏ qua, không áp dụng định danh/IM không hợp lệ.

Tắt công tắc dừng cả học và khôi phục, giữ bảng nhớ để bật lại. Có thể xóa từng mục trong giao diện. Nếu vẫn bật chức năng, ứng dụng đó
có thể được tự nhớ lại ở lần focus/chọn bộ gõ sau.
Bản sao lưu cấu hình UI/addon JSON có công tắc mới, **chưa chứa bảng nhớ app**.
Không sửa file bảng nhớ khi đang dùng để tránh ghi đè giữa runtime và file.

Addon chuyển sang `OnDemand=False` để quan sát focus và lựa chọn English ngay
khi Fcitx5 khởi động, trước lần chọn Telex đầu tiên. Nếu addon bị disable ở
cấu hình Fcitx5 thì chức năng không chạy. Không thêm service theo dõi desktop.
Khôi phục dùng API local của Fcitx5 sau bước focus activation; các thông báo
switch phát sinh trong bước focus không được học thành lựa chọn mới.

Tham khảo API/thứ tự sự kiện từ [Fcitx5 5.1.19](https://github.com/fcitx/fcitx5/blob/5.1.19/src/lib/fcitx/instance.cpp).

## Kiểm chứng

Test pipeline thật trong Fcitx5 headless: startup tự nạp addon, đủ năm IM,
context mới cùng app, hai app độc lập, giữ group default, lưu/đọc lại file đã
đổi, gõ VNI sau khôi phục, thiếu bộ gõ trong nhóm, bridge/password, tắt không
học thêm, giới hạn và dữ liệu lưu sai. File/config/data của fixture nằm riêng
trong thư mục tạm; English dùng descriptor fixture, không thử frontend bàn
phím desktop thật. UI kiểm tra công tắc, preflight/readback, JSON và scale
100%/200%; sanitizer kiểm tra thêm đường focus/switch.

Còn nghiệm thu desktop: chuyển app bằng chuột/Alt+Tab, English/VI bằng hotkey
và icon, hai cửa sổ cùng app, thoát/đăng nhập lại, panel GNOME/KDE và Wayland/
X11. Không dùng kết quả headless để đánh dấu mọi ứng dụng đã được hỗ trợ.

## Bổ sung alpha.52

Có [extension nhận diện GNOME](UBUNTU_GNOME_IDENTITY.md) cho context gnome-shell/
ibus. Cần cài extension riêng và bật công tắc bổ sung; vẫn chờ QA desktop,
không thay giới hạn của bản alpha.51 khi không dùng extension.

## Quản lý danh sách — alpha.53 / UI preview.15

Mở **Ứng dụng & tương thích → Bộ gõ đã nhớ theo ứng dụng**:

- Danh sách hiển thị định danh chính xác và một trong năm bộ gõ đã nhớ.
  Chọn **Sửa**, chọn bộ gõ rồi **Lưu bộ gõ cho ứng dụng**; hoặc **Xóa** từng mục.
- Thay đổi chỉ ảnh hưởng lần khôi phục tiếp theo, không đổi bộ gõ đang dùng,
  không thêm bộ gõ vào nhóm. Cần bật ghi nhớ để tự khôi phục.
- Có thể quản lý bảng nhớ ngay cả khi hai công tắc đang tắt. Bảng nhớ không
  nằm trong JSON sao lưu cấu hình chung; từ preview.16 có bản sao JSON riêng
  cho bảng nhớ như hướng dẫn bên dưới.
- **Ứng dụng đang nhận diện** là context focus hiện đã được addon xác nhận;
  không hiển thị ID của context Password/Sensitive/Disable. Khi cửa sổ cài đặt
  được focus, ID có thể thuộc chính OpenKey Settings, hoặc chưa có ID.
- **Kết nối GNOME** là lần quan sát gần nhất của bridge trong addon. `connected`
  xác nhận nhận được phản hồi hợp lệ từ extension, kể cả khi không có app ID;
  không bảo đảm mọi focus đã được xác nhận hoặc extension vẫn đang kết nối.
  `not-observed` nghĩa là chưa truy vấn, `waiting` là đang chờ/đã bị mất xác nhận,
  `unavailable` là lỗi/timeout/phản hồi không hợp lệ; `disabled` khi một trong
  hai công tắc đang tắt. Đọc danh sách không tự kích hoạt Shell/extension.

Trang đang hiển thị đọc nền mỗi hai giây (từ preview.17), giữ trạng thái
và các nút trong lúc đọc. Mẫu không đổi không phát thông báo cập nhật; đổi
ID/trạng thái GNOME không dựng lại danh sách ứng dụng. Nút Đọc lại vẫn là
thao tác đọc chủ động, có trạng thái loading. Sau thao tác sửa/xóa hoặc lỗi, giữ
thông báo kết quả và dừng cập nhật định kỳ; chọn **Đọc lại danh sách và trạng
thái** để tiếp tục. Chỉ đọc không ghi cấu hình hoặc đổi bộ gõ.

API riêng `fcitx://config/addon/openkey/application-memory` dùng
`ApplicationMemoryView` với `Entries`, `ExpectedEntries`, `CurrentApplication`,
`IdentityStatus`. Ghi chỉ nhận đúng hai trường Entries/ExpectedEntries. Addon
so sánh baseline trên cùng event thread với học tự động, kiểm tra toàn bảng,
chỉ công bố runtime sau khi safeSaveAsIni thành công. Baseline cũ, dữ liệu sai,
trường chỉ đọc hoặc ghi file lỗi giữ nguyên bảng. UI kiểm tra owner, schema,
preflight và đọc lại kết quả; không tự thử lại khi chưa xác nhận. Sau xung đột,
đọc lại và chọn **Sửa** lại để lấy giá trị mới nhất.

## Sao lưu / khôi phục bảng nhớ — UI preview.16

Cần addon alpha.53 trở lên; extension GNOME và addon không thay đổi ở đợt này.
Mở **Ứng dụng & tương thích → Sao lưu bộ gõ theo ứng dụng**:

1. Dùng **Đọc lại danh sách và trạng thái** ở phần quản lý để lấy dữ liệu mới,
   rồi **Tạo JSON danh sách** và **Lưu thành file…**. JSON xuất lấy từ danh sách
   runtime UI đã đọc; tạo JSON không tự ghi file hay clipboard.
2. **Mở file…** hoặc dán JSON vào ô nhập. Chỉ mở file chưa áp dụng cấu hình.
3. Chọn **Gộp** hoặc **Thay thế**, nhấn **Kiểm tra / xem trước**. Gộp giữ các
   mục khác, mục trùng lấy bộ gõ trong file. Thay thế dùng toàn bộ danh sách
   trong file, xóa các mục không có; danh sách rỗng có thể xóa toàn bộ bảng nhớ.
4. Xem số lượng và chi tiết thêm/sửa/xóa, nhấn **Xác nhận áp dụng danh sách**.
   Thay đổi dùng cho lần khôi phục sau, không chuyển bộ gõ đang dùng.

Đổi JSON/chế độ nhập, đọc lại runtime, sửa/xóa một mục hoặc owner Fcitx5 thay
đổi sẽ làm bản xem trước hết hiệu lực. Trong lúc có bản xem trước hợp lệ,
UI tạm dừng việc đọc danh sách định kỳ để giữ bản xem trước. Addon vẫn học
bình thường; trước khi ghi, UI đọc lại và addon so sánh baseline nguyên tử.
Nếu bảng đã đổi, phải đọc lại và xem trước lại; không tự gộp lại hoặc thử ghi
lại khi xung đột/mất xác nhận. Mỗi lần xác nhận chỉ cho phép một lần áp dụng.
Không thay công tắc ghi nhớ/nhận diện GNOME hoặc nhóm IM; bộ gõ chưa có trong
nhóm vẫn không được tự thêm. Mục đã xóa có thể được học lại nếu còn bật ghi nhớ.

Định dạng riêng, không dùng JSON cấu hình chung/macro/IM/hotkey thay thế:

```json
{
  "format": "openkey-application-memory",
  "version": 1,
  "entries": [
    {"application": "gnome:google-chrome.desktop", "method": "openkey-telex"},
    {"application": "code", "method": "keyboard-us"}
  ]
}
```

Định danh phải khớp chính xác app thực tế; file chỉ chứa định danh và một
trong năm bộ gõ hỗ trợ. Từ chối app trùng, ID chung/sai, bộ gõ khác, trường
thừa hoặc sai kiểu/phiên bản. Tối đa 128 mục cả sau khi gộp; không cắt bớt
âm thầm. JSON tối đa 65.536 đơn vị UTF-16; file UTF-8 tối đa 128 KiB, chấp
nhận một BOM đầu file. Dùng file picker theo thao tác người dùng, ghi thay
thế nguyên tử; giữ giới hạn đường dẫn/file thường và không theo symlink của
các chức năng sao lưu trước đó. Tên ứng dụng không phải tên cửa sổ; file
không chứa phím hoặc nội dung nhập.

## Cập nhật nền — UI preview.17

Đọc định kỳ không đổi trạng thái sang loading/busy, không khóa nút sửa/xóa,
không phát thông báo nếu dữ liệu không đổi. Danh sách có tín hiệu thay đổi
riêng, nên mẫu chỉ đổi ID focus/trạng thái GNOME không dựng lại các hàng.
Đọc nền không ghi file hoặc chuyển bộ gõ. Mỗi lúc chỉ có một lần đọc nền.

Thao tác đọc lại/sửa/xóa/xem trước sẽ hủy hiệu lực phản hồi nền đang chờ,
để phản hồi đến muộn không ghi đè thao tác mới. Bản xem trước hợp lệ vẫn
chặn đọc định kỳ. Lỗi schema/timeout hoặc owner Fcitx5 mất sẽ hiển thị lỗi
và bỏ dữ liệu không còn xác nhận; không giữ ID cũ như thể vẫn hợp lệ.
