# Menu mở trang UI — preview.12

Menu icon OpenKey bổ sung ba lệnh theo TrayContract Windows:

| Lệnh | ID | Trang UI |
| --- | ---: | --- |
| Cấu hình gõ tắt… | 990 | Gõ tắt (`macro`) |
| Công cụ chuyển mã… | 980 | Chuyển mã (`convert`) |
| Giới thiệu OpenKey | 1010 | Giới thiệu (`about`) |

Ba lệnh sử dụng cùng cửa sổ/singleton cài đặt OpenKey; chuyển trang bằng
`org.openkey.Settings1.ShowPage(s) → b` tại `/org/openkey/Settings` trên session
bus. Khi UI chưa chạy, D-Bus activation khởi động UI như lệnh Cài đặt chung.
Không gọi shell, clipboard, công cụ chuyển mã tự động hoặc Apply cấu hình.
Mở trang Gõ tắt không tự bật macro. Hover/open/close menu không gửi lệnh mở UI.

`ShowPage` chỉ nhận tám tên trang sẵn có của UI; tên khác trả false và không
đổi trang/hiện cửa sổ. Yêu cầu trước khi QML dựng xong được giữ đến khi có
window. `Show` cũ giữ trang hiện tại và tiếp tục hoạt động với các client cũ.
Không tạo lại cửa sổ khi chuyển trang, nên các bản nháp vẫn theo vòng đời UI
hiện có. Wayland compositor có thể từ chối đưa cửa sổ lên trước/focus; kết quả
true chỉ xác nhận UI nhận yêu cầu, không chứng minh người dùng đã thấy trang.

Worker tray dùng cùng một slot đang bận cho lệnh mở UI, mở Fcitx và đổi IM;
menu tạm khóa trong khi có yêu cầu. Timeout ba giây, không tự retry. UI cũ
thiếu ShowPage hoặc trả false/lỗi không được báo thành công, không đổi IM hay
thoát tray. Cần đóng/mở lại UI cũ sau khi cập nhật binary.

## Cài thử

Cần **UI preview.12 và service mới**. Addon alpha.50 không thay đổi ở đợt này.
Gói UI không chứa service. Từ thư mục dự án:

```bash
sudo apt install ./dist/linux/openkey-settings_3.1.0~preview.12-1_amd64.deb
```

Nếu service được cài từ mã nguồn, build và cài component riêng bằng tài khoản
thường, sau khi các kiểm thử đạt:

```bash
cmake -S . -B build/ubuntu-tray-pages -DOPENKEY_BUILD_SERVICE=ON -DOPENKEY_BUILD_UI=OFF -DOPENKEY_BUILD_FCITX5=OFF -DOPENKEY_BUILD_X11_HOTKEY=OFF
cmake --build build/ubuntu-tray-pages --parallel 2
ctest --test-dir build/ubuntu-tray-pages --output-on-failure
cmake --install build/ubuntu-tray-pages --component openkey_service --prefix "$HOME/.local"
```

Chỉ nếu đang dùng unit user `openkey-tray.service` trỏ đến binary vừa cài:

```bash
systemctl --user restart openkey-tray.service
```

Lệnh này chỉ khởi động lại tray, không restart Fcitx5. Với cách autostart khác,
nạp lại tray bằng cơ chế đã chọn; không chạy thêm instance song song. Bản UI
user-local có thể che `/usr`, cần bảo đảm đã cập nhật đúng binary.

## Kiểm thử và phần còn lại

Kiểm thử endpoint: đủ tám trang, tên sai, yêu cầu sớm, Show giữ trang và export
D-Bus. Kiểm thử worker: ba key đúng, enum sai, UI cũ, từ chối, lỗi, timeout,
không chặn event thread và không retry. Kiểm thử tray: label/enabled, hover
không tác dụng, thiếu UI không đổi IM hoặc thoát tray.

Build có cả service và UI bổ sung kiểm thử tích hợp: service thật nhận Event
của cả ba menu trên private D-Bus rồi gọi SettingsActivation thật, điều hướng
một QQuickWindow hiện có. Đây không phải nghiệm thu panel GNOME/KDE, file
chooser hoặc nhập liệu trên desktop. Các action bật/tắt chính tả/macro, smart
switch, bảng mã IM, theme và chuyển mã nhanh còn riêng trong backlog; không
coi ba lệnh mở trang là đã port toàn bộ TrayContract.

Kết quả đợt này: 35/35 CTest tổng hợp đạt; nhóm activation chạy lại sau sửa
fixture/MOC đạt 20 ca QtTest, gồm tích hợp ba Event. Gói UI preview.12 qua
verifier và lifecycle root tạm; AppImage thật đạt smoke extract-and-run.
User-local UI/service đã được cập nhật nguyên tử, có backup; chỉ unit tray
được restart. Đọc menu SNI đang đăng ký xác nhận ba label/enabled, chưa click
trên desktop và chưa nghiệm thu việc panel đưa cửa sổ lên trước.
