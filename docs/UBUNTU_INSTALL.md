# Cài OpenKey thử nghiệm trên Ubuntu

Gói `fcitx5-openkey` cung cấp hai input method **OpenKey Telex (Experimental)** và
**OpenKey VNI (Experimental)**. Dùng engine OpenKey của Mai Vũ Tuyên và phần tích
hợp Linux của bản fork. Mã nguồn/giấy phép: GPL-3.0, xem
<https://github.com/minhtuan9922/OpenKey>.

Đây là bản alpha, chưa có giao diện Fluent giống Windows, tray riêng hoặc macro.
Từ alpha.3 có lựa chọn Preedit/Direct được lưu trong cấu hình Fcitx5.
Chưa nghiệm thu Direct và IntelliSense trên VS Code desktop thật.

## Cài gói

Từ root repository, sau khi đã có gói và checksum trong `dist/linux/`:

```bash
cd dist/linux
sha256sum -c 'fcitx5-openkey_3.1.0~alpha.4-1_amd64.deb.sha256'
sudo apt install './fcitx5-openkey_3.1.0~alpha.4-1_amd64.deb'
```

APT sẽ xử lý dependency được khai báo trong gói. Bản build trên Ubuntu 26.04
amd64 không mặc nhiên tương thích Ubuntu 22.04/24.04: gói ghi yêu cầu runtime
theo thư viện máy build. Không bỏ qua dependency; hãy build lại trên bản Ubuntu
mục tiêu nếu APT báo không đáp ứng phiên bản thư viện.

Gói chỉ đặt module và descriptor vào hệ thống; không có maintainer script,
không đổi profile, không bật autostart, không dừng/chạy lại daemon hoặc thay IBus.
Không có mục mở ứng dụng OpenKey riêng trong launcher ở bản alpha này.

## Bật bộ gõ

Trong phiên desktop đã thiết lập Fcitx5, lưu công việc rồi khởi động lại input
framework theo hướng dẫn desktop (hoặc đăng xuất/đăng nhập). Mở cấu hình Fcitx5,
bỏ lọc ngôn ngữ nếu cần và thêm OpenKey Telex hoặc OpenKey VNI. Chuyển bộ gõ
bằng phím tắt hiện có của Fcitx5.

Nếu đang dùng IBus, việc cài gói không tự chuyển sang Fcitx5. Hãy theo
[hướng dẫn setup Fcitx5](https://fcitx-im.org/wiki/Setup_Fcitx_5) và
[hướng dẫn Wayland](https://fcitx-im.org/wiki/Using_Fcitx_5_on_Wayland/en)
cho desktop đang dùng. Không mặc định áp biến môi trường hoặc restart daemon
giống nhau cho mọi GNOME/KDE/Wayland session.

Thử Telex `tieengs vieetj` hoặc VNI `tie6ng1 vie6t5`, rồi Space. Kỳ vọng:
`tiếng việt `. Kiểm tra thêm đổi focus không làm lặp từ.

## Chọn có/không gạch chân

Trong cấu hình Fcitx5, mở cấu hình OpenKey (Telex/VNI, hoặc addon OpenKey), chọn
**Chế độ gõ / Input style**:

- **Preedit**: mặc định, giữ cơ chế gõ có composition để tương thích.
- **Direct**: thử nghiệm, commit từng ký tự và thay dấu qua surrounding-text,
  không gửi preedit khi ứng dụng hỗ trợ.

Lựa chọn áp dụng chung cho cả Telex và VNI. Khi lưu cấu hình, từ Preedit đang
gõ được commit một lần; từ Direct đã nằm trong tài liệu không bị commit lại.
Cấu hình lưu tại `$XDG_CONFIG_HOME/fcitx5/conf/openkey.conf` hoặc mặc định
`~/.config/fcitx5/conf/openkey.conf`, với `InputStyle=Preedit` hoặc `InputStyle=Direct`.

Direct yêu cầu ứng dụng hỗ trợ surrounding-text hợp lệ. Từ alpha.4, selection
có sẵn ở đầu từ (như address bar vừa nhận focus) được thay bởi phím mới qua
`commitString` của ứng dụng; không dùng lệnh xóa surrounding để xóa selection.
Sau đó chỉ theo dõi phần ký tự mới đã gõ. Chỉ phục hồi chữ đang chờ vẫn yêu cầu
con trỏ không có selection như mô tả dưới đây.
Nếu thiếu hỗ trợ ngay đầu từ, addon dùng Preedit. Nếu mất đồng bộ giữa từ, addon
ngừng thay dấu và gõ nguyên trạng đến ranh giới tiếp theo hoặc context hợp lệ mới;
từ đó có thể còn phím
Telex/VNI thô. Khi snapshot vẫn khớp vị trí cũ nhưng chờ quá lâu, phím chờ được
chuyển tiếp nguyên trạng. Nếu context/selection đã thay đổi, phần chữ chưa nhập
được giữ trong bảng phụ **Phím chưa nhập / Pending**: bỏ chọn vùng và đặt con trỏ,
rồi Enter xác nhận chèn tại vị trí hiện tại, Escape hủy. Không chèn khi vẫn có
selection hoặc surrounding-text chưa hợp lệ. Các shortcut/
phím điều hướng đang chờ bị hủy trong trường hợp mất ngữ cảnh; chỉ giữ phần chữ.
Timeout xác nhận là 250 ms, hàng đợi
tối đa 64 sự kiện; mục tiêu là không xóa nhầm nội dung khi client cập nhật chậm.
Snapshot có cùng nội dung/vị trí nhưng không có revision không phân biệt được
ACK mới với echo cũ; trường hợp mơ hồ được ngừng theo dõi thay vì tiếp tục sửa.

Space và ký tự phân cách in được trong Direct được commit theo cùng hàng đợi
để giữ đúng thứ tự nhiều từ, kể cả sau dấu câu.
Enter/Tab/shortcut vẫn được chuyển cho ứng dụng; sau một sự kiện làm đổi vị trí
không thể dự đoán, phần còn lại của burst có thể được gõ nguyên trạng.
Escape ở Direct chỉ kết thúc theo dõi từ, không xóa ký tự đã commit.
Ngoại lệ: khi bảng phục hồi đang mở, Enter/Escape là lệnh xác nhận/hủy chữ chờ,
không được chuyển sang ứng dụng; chữ gõ thêm được thêm vào phần chờ (tối đa 256).

Direct không tắt quy tắc Telex/VNI trong identifier. Ví dụ `console` có thể chứa
phím đặt dấu; chuyển English khi cần gõ code nguyên trạng. Chế độ này nhằm bỏ
composition kéo dài, không thay cấu hình IntelliSense hoặc bảo đảm mọi frontend
Electron đều cung cấp API cần thiết. Hãy thử gõ `print` chưa Space để kiểm tra gợi ý,
và kiểm tra Tab/Enter nhận completion, undo/redo trên VS Code thực tế.

### Chẩn đoán khi Direct không hoạt động trong một ứng dụng

Trong cấu hình OpenKey, bật tạm **Direct diagnostics (no text content)**
(`DirectDiagnostics=True` trong cấu hình). Mặc định tùy chọn này tắt.
Chỉ log reason, định danh app/frontend và capability flags; không log ký tự,
URL, nội dung tài liệu, vị trí con trỏ hoặc phần chữ đang chờ.

Các reason thường gặp:

- `direct:ready` / `direct:selection-start`: đã đi vào Direct.
- `fallback:no-surrounding-capability`: client chưa công bố capability cần thiết.
- `fallback:invalid-surrounding-text`: capability có nhưng context chưa hợp lệ.
- `fallback:preedit-word-in-progress`: từ hiện tại đã bắt đầu ở fallback Preedit;
  kết thúc từ trước khi thử Direct ở từ kế tiếp.
- `context-reset` / `context-ready`: bỏ sở hữu từ cũ khi context đổi, không buộc toggle EN–VI.
- `bypass:unconfirmed-edit` / `recovery:context-changed`: không tiếp tục sửa từ cũ
  vì edit chưa xác nhận hoặc context đã đổi; có thể cần phục hồi theo bảng Pending.
- `bypass:sensitive-field`: ứng dụng khai báo ô Password/Sensitive.

Log đi tới stderr của tiến trình Fcitx5. Nếu desktop launcher chuyển stderr vào
user journal, có thể đọc bằng:

```bash
journalctl --user --since '10 minutes ago' --no-pager | grep 'OpenKey Direct:'
```

Nếu không có log, chưa thể kết luận addon không chạy: nơi thu stderr tùy launcher.
Không chạy thêm daemon hoặc `fcitx5 -r` chỉ để lấy log khi chưa biết cách desktop
khởi chạy Fcitx5. Khi báo lỗi, gửi reason tương ứng và thông tin GNOME/KDE,
Wayland/X11, native Wayland/XWayland của ứng dụng nếu biết; tắt diagnostics sau đó.

## Gỡ và nâng cấp

Chuyển sang input method khác, bỏ OpenKey khỏi danh sách trong cấu hình Fcitx5,
rồi chạy:

```bash
sudo apt remove fcitx5-openkey
```

Gói không xóa profile Fcitx5 hoặc dữ liệu người dùng. Đăng xuất/đăng nhập nếu
daemon còn giữ module cũ. Để nâng cấp, dùng `sudo apt install ./<gói-mới>.deb`;
việc cài không tự khởi động lại phiên gõ.

Nếu trước đây cài module bằng `sudo cmake --install`, đọc install manifest của
bản đó trước; APT chỉ quản lý các đường dẫn nằm trong gói `.deb`, không xóa bản
cũ đặt ở prefix khác như `/usr/local`.

## Tự build gói

```bash
sudo apt install build-essential cmake ninja-build dpkg-dev libfcitx5core-dev
bash scripts/build-linux-deb.sh
```

Script build Release, chạy CTest, tạo `.deb` bằng CPack và checksum SHA-256,
giải nén vào build riêng rồi kiểm tra discovery/nạp addon qua Fcitx5 thật với
frontend giả. Không dùng sudo hoặc cài gói khi build/test.

Các file đầu ra nằm trong `dist/linux/`. Muốn debug dependency và payload:

```bash
dpkg-deb --info 'dist/linux/fcitx5-openkey_3.1.0~alpha.4-1_amd64.deb'
dpkg-deb --contents 'dist/linux/fcitx5-openkey_3.1.0~alpha.4-1_amd64.deb'
```

Đã kiểm tra local trên Ubuntu 26.04 amd64: 7/7 CTest, checksum, payload,
dependency tự sinh, nạp addon từ `.deb` đã giải nén và `apt-get --simulate install`.
Chưa chạy cài/gỡ/nâng cấp bằng APT thật hoặc kiểm thử ứng dụng desktop. CI có
job build gói Ubuntu 24.04 riêng; thêm workflow chưa đồng nghĩa job remote đã chạy.

Tham khảo kỹ thuật đóng gói: [CPack DEB](https://cmake.org/cmake/help/latest/cpack_gen/deb.html).
