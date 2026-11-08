# OpenKey Ubuntu 26.04 — beta.1

Một gói `openkey_3.1.0~beta.1-1_amd64.deb` gồm addon Fcitx5, Qt Quick UI,
tray, autostart tray, công cụ chẩn đoán và extension nhận diện GNOME Shell 50.
Bản beta thử nghiệm; các giới hạn nhập liệu Chrome/VS Code còn cần nghiệm thu
trên từng backend. Không coi đóng gói beta là đã hoàn tất QA desktop.

## Cài đặt

Chạy trong thư mục có file DEB, cần mạng nếu thiếu thư viện:

```bash
sudo apt update
sudo apt install ./openkey_3.1.0~beta.1-1_amd64.deb
```

APT tải và cài các dependency từ kho Ubuntu, gồm Fcitx5, frontend GTK/Qt,
Qt/QML và thư viện ELF được xác định từ binary. Không chạy apt/curl trong
maintainer script. `dpkg -i` đơn lẻ không tự tải dependency; dùng APT như trên.
Gói thay thế hai gói cũ `fcitx5-openkey`/`openkey-settings`, giữ cấu hình user.
Chỉ hỗ trợ ABI Ubuntu 26.04 amd64 đang dùng khi build, không hứa tương thích
Ubuntu cũ. APT cần kho Ubuntu đầy đủ, gồm universe cho các gói Fcitx/Qt.

Nếu từng cài bản user-local, gỡ các binary/service user-local trước để không
che bản `/usr/bin`. Không chạy đồng thời autostart XDG và user systemd unit
tray cũ. Gói beta dùng XDG autostart khi đăng nhập lần sau, không restart phiên
hoặc daemon trong lúc APT đang cài.

## Thiết lập lần đầu cho tài khoản

1. Chọn Fcitx5 làm input method bằng `im-config -n fcitx5` nếu tài khoản chưa
   dùng Fcitx5. Lưu công việc rồi đăng xuất/đăng nhập.
2. Mở `fcitx5-configtool`, thêm English (US) và OpenKey Telex/VNI/Simple Telex
   1/2 vào nhóm hiện tại nếu chưa có. Không cần thêm tất cả để bắt đầu dùng.
3. Mở **OpenKey Settings** từ menu ứng dụng hoặc chạy `openkey-settings`.
   Tray tự chạy khi đăng nhập; có thể mở ngay bằng `openkey-service --run-tray`.
4. Trên GNOME Shell 50, extension đã nằm trong gói; sau đăng nhập lại chạy:

```bash
gnome-extensions enable openkey-app-identity@openkey.local
```

Bật **Ghi nhớ bộ gõ theo ứng dụng** và **Nhận diện ứng dụng GNOME** trong UI
nếu cần nhớ riêng Chrome/VS Code qua bridge GNOME. Mặc định các cờ vẫn tắt.
Extension chỉ tương thích Shell 50; không bắt buộc dùng GNOME để gõ thông thường.

Để thử không gạch chân/ô preedit nổi, chọn chế độ NativeDirect và nghiệm thu
trên app cụ thể. Nhận diện app không sửa hạn chế surrounding text của frontend.
Xem [giới hạn Direct](UBUNTU_NATIVE_DIRECT.md), [nhớ bộ gõ](UBUNTU_APP_MEMORY.md)
và [GNOME](UBUNTU_GNOME_IDENTITY.md). Sao lưu dữ liệu trước khi thử nghiệm.

## Gỡ

```bash
sudo apt remove openkey
```

Cấu hình trong tài khoản không bị xóa bởi dpkg, kể cả khi purge. Không gỡ
Fcitx5 hoặc thư viện dùng chung nếu các ứng dụng khác còn cần. Lưu công việc
và đăng xuất/đăng nhập để addon đã nạp không còn nằm trong tiến trình cũ.
