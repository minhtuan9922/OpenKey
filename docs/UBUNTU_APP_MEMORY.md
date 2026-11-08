# Ghi nhớ bộ gõ theo ứng dụng — alpha.51 / UI preview.13

OpenKey có thể tự nhớ English (US), Telex, VNI và Simple Telex 1/2 theo ứng
dụng, rồi khôi phục khi quay lại. Đây là chức năng riêng với quy tắc InputStyle
Preedit/Direct/CommitOnly/NativeDirect đã có trước đó.

## Bật và sử dụng

1. Cài addon alpha.51 và UI preview.13; nạp addon mới sau khi lưu công việc
   và đăng xuất/đăng nhập. Đóng/mở lại cửa sổ UI cũ.
2. Mở **Ứng dụng & tương thích → Ghi nhớ bộ gõ theo ứng dụng**, chọn **Bật**
   rồi **Áp dụng**. Mặc định tắt; cài gói không tự bật chức năng.
3. Đưa focus vào ứng dụng cần nhớ, chọn Telex/VNI/Simple Telex/English bằng
   phím tắt hoặc menu icon. Chuyển sang ứng dụng khác, chọn bộ gõ khác rồi
   quay lại để kiểm tra lựa chọn đầu được khôi phục.

Lựa chọn được gắn với app đang có input context được focus; nếu đang nhập
trong cửa sổ cài đặt thì đó có thể là chính app cài đặt. Không chọn một tên
ứng dụng khác trong UI để gán bộ gõ cho nó. Mọi cửa sổ/context có cùng program
ID dùng chung lựa chọn. Chưa có phân biệt terminal/editor bên trong VS Code.

Từ thư mục dự án trên Ubuntu 26 amd64:

```bash
sudo apt install ./dist/linux/fcitx5-openkey_3.1.0~alpha.51-1_amd64.deb ./dist/linux/openkey-settings_3.1.0~preview.13-1_amd64.deb
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
  chưa có màn hình quản lý/xóa từng mục. Không cam kết GNOME/KDE hoặc mọi
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

Tắt công tắc dừng cả học và khôi phục, giữ bảng nhớ để bật lại. Muốn xóa toàn
bộ, tắt chức năng và xóa file bảng nhớ khi Fcitx5 đã dừng, rồi nạp lại addon.
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
