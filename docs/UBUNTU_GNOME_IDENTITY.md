# Nhận diện app GNOME — addon alpha.52 / UI preview.14

Extension **OpenKey Application Identity** bổ sung desktop app ID cho đường
Fcitx5 chỉ báo `program=gnome-shell`, `frontend=ibus`. Nhờ đó chức năng nhớ
bộ gõ có thể phân biệt Chrome, VS Code… thay vì gán chung cho GNOME Shell.
Đây là bản thử nghiệm cho **GNOME Shell 50** (máy build: 50.1). Chưa nghiệm
thu extension trong desktop đang sử dụng; không cam kết mọi thứ tự focus.

## Cài và bật

Từ thư mục gốc dự án, cài hai gói mới:

```bash
sudo apt install ./dist/linux/fcitx5-openkey_3.1.0~alpha.53-1_amd64.deb ./dist/linux/openkey-settings_3.1.0~preview.15-1_amd64.deb
gnome-extensions install ./dist/linux/openkey-app-identity@openkey.local.shell-extension.zip
```

Lưu công việc rồi đăng xuất/đăng nhập để nạp addon và nhận extension mới. Sau
đó chạy trong desktop:

```bash
gnome-extensions enable openkey-app-identity@openkey.local
```

Đóng/mở lại UI, vào **Ứng dụng & tương thích**, bật và Áp dụng từng công tắc:

1. **Ghi nhớ bộ gõ theo ứng dụng**.
2. **Nhận diện ứng dụng GNOME**.

Hai công tắc mặc định tắt; cài gói không tự bật. Extension được đóng gói riêng,
không nằm trong addon DEB. Nếu đã cài extension cùng UUID, không dùng ghi đè
mù quáng: giữ bản cũ, cập nhật đúng source rồi nạp lại trong phiên mới. UI
user-local có thể che binary `/usr` như các bản trước.

Đưa focus vào Chrome, chọn Telex bằng menu icon; sang VS Code chọn English;
quay lại Chrome để thử khôi phục. Thử riêng VNI/Simple Telex, hai cửa sổ cùng
app, Alt+Tab, click và sau đăng nhập lại. Chọn bộ gõ khi ô nhập của app cần
nhớ đang focus. Không dùng tên/tiêu đề cửa sổ làm khóa.

## Extension gửi những gì?

Trong GNOME Shell, đọc cửa sổ đang focus qua Meta.Display và tìm Shell.App
qua WindowTracker. Chỉ lấy desktop app ID dạng `code.desktop`,
`google-chrome.desktop`; ID dạng `window:123`, đường dẫn hoặc dữ liệu lạ bị
bỏ. Không đọc/gửi tiêu đề, PID, URL, phím hoặc nội dung gõ.

Endpoint trên session bus, tên có sẵn **org.gnome.Shell**:

- Object `/org/openkey/FocusedApplication`.
- Interface `org.openkey.FocusedApplication1`.
- `GetFocus() → (appId: s, sessionNonce: s, serial: u)`.
- Signal `Changed()` để bỏ cache/đang chờ khi focus hoặc trạng thái Shell đổi.

Nonce UUID mới cho mỗi lần enable; serial đổi khi cửa sổ/app/trạng thái thay
đổi, kể cả hai cửa sổ cùng app. Khi lock screen, greeter, không có cửa sổ,
overview hoặc Shell modal thì trả app ID rỗng. Hỗ trợ session mode kế thừa
user của Ubuntu, không buộc tên mode phải là `user`.

Addon chỉ gọi unique owner hiện tại của org.gnome.Shell, không tự khởi động
Shell/extension; xử lý Changed và mất/đổi owner để bỏ dữ liệu cũ. Mỗi truy vấn
bất đồng bộ có timeout 150 ms; hai lượt GetFocus phải khớp owner, nonce,
serial và app ID. Không dùng signal từ peer khác để làm nguồn định danh.

## Ánh xạ và thứ tự focus

Chỉ dùng đường bổ sung này khi cả hai công tắc bật, context là gnome-shell/
ibus, vẫn focus và không Password/Sensitive/Disable. Các app có program ID
riêng tiếp tục đi đường alpha.51, không cần extension.

Bảng nhớ dùng khóa riêng `gnome:<desktop-id>` trong file
`~/.config/fcitx5/conf/openkey-app-memory.conf`. Tối đa 128 app chung với các
mục program ID; chỉ năm bộ gõ được hỗ trợ và đã có trong nhóm Fcitx5. Bản sao
lưu JSON chứa hai công tắc, chưa chứa bảng app đã nhớ.

Phản hồi phải còn đúng vòng đời focus/context; focus-out/destroy, thay config,
Changed hoặc owner đổi làm mất hiệu lực dữ liệu đang chờ. Nếu đã bắt đầu gõ
hoặc còn composition/queue/recovery trong lúc đợi thì không tự đổi bộ gõ ở
giữa từ. Một thao tác đổi bộ gõ rõ ràng được kiểm chứng lại bằng GetFocus
trước khi học, để không ghi theo cache app cũ.

GNOME focus và Fcitx input context đi qua **hai luồng sự kiện khác nhau**, chưa
có token giao dịch chung. Hai lần đọc ổn định và kiểm vòng đời giảm rủi ro,
không chứng minh đồng bộ nguyên tử. Nếu Changed đến muộn khiến yêu cầu bị
hủy, addon bỏ qua và cần lần focus hoặc lựa chọn bộ gõ tiếp theo; không tự
retry rồi đổi bộ gõ khi người dùng đã gõ. Cần QA desktop để đánh giá các thứ
tự này; không gọi đây là đã hết mọi race hoặc hỗ trợ mọi ứng dụng.

Extension chỉ nối vào **ghi nhớ bộ gõ**, chưa thay cách khớp quy tắc InputStyle
ở trang Chế độ theo ứng dụng. Không phân biệt terminal/editor/tab trong cùng
app, không sửa surrounding text hoặc lỗi gõ Chrome omnibox.

## Tắt/gỡ

Tắt Nhận diện ứng dụng GNOME để ngừng truy vấn; tắt Ghi nhớ bộ gõ để ngừng cả
học và khôi phục. Các mục đã nhớ được giữ như alpha.51. Tắt extension:

```bash
gnome-extensions disable openkey-app-identity@openkey.local
```

Có thể gỡ bằng `gnome-extensions uninstall openkey-app-identity@openkey.local`.
Không cần restart Fcitx5 chỉ để tắt bridge. Mất endpoint/timeout/dữ liệu lạ thì
không fallback sang đoán ứng dụng hoặc dùng app ID đã cache trước đó.

## Kiểm thử

- Node: metadata state, đổi cửa sổ cùng app, ID sai, wrap serial, Ubuntu mode,
  lock/overview/modal, export/unexport và ngắt signal khi disable (Shell giả).
- GJS/Gio thật trên private D-Bus: wire signature ssu và Changed không tham số.
- Fcitx5 thật/headless trên private D-Bus: cùng context gnome-shell nhận Chrome/
  Code riêng, lưu theo namespace, nhận diện không ổn định/nonce đổi, dữ liệu
  sai/timeout, signal peer khác, gõ trước phản hồi, focus-out và mất owner.
- ASan/UBSan và UI ở scale 100%/200%; không dùng desktop hoặc bus người dùng
  cho các fixture trên. GJS test đặt GIO_USE_VFS=local để không kích hoạt GVFS.

Chưa cài/bật extension trong phiên desktop người dùng hoặc restart phiên để
đánh dấu QA. Một probe GNOME Shell 50.1 headless thật trong D-Bus/config/data/
runtime tạm đã nạp extension và gọi GetFocus thành công (app ID rỗng do chưa
có cửa sổ app). Probe này chưa chứng minh đổi app Chrome/VS Code thật. Thử desktop theo [checklist](UBUNTU_DESKTOP_QA.md), thêm các ca
bật/tắt extension, lock/unlock, overview và chuyển app thật nhanh.

Nguồn API: [GNOME Display focus-window](https://mutter.gnome.org/meta/property.Display.focus-window.html),
[Shell WindowTracker](https://gnome.pages.gitlab.gnome.org/gnome-shell/shell/method.WindowTracker.get_window_app.html),
[hướng dẫn extension](https://gjs.guide/extensions/development/creating.html).

Từ alpha.53 / preview.15 có [giao diện quản lý bộ gõ đã nhớ và trạng thái
nhận diện](UBUNTU_APP_MEMORY.md#quản-lý-danh-sách--alpha53--ui-preview15).
Extension vẫn dùng bản v1; phần bổ sung này không cần đổi ZIP extension.
