# Nghiệm thu bộ gõ Ubuntu — Fcitx5

## Trạng thái

Đã kiểm chứng bằng test headless: discovery của descriptor trong install prefix,
nạp module, preedit/commit, xử lý Backspace, focus-out cho client preedit,
ClientUnfocusCommit và server preedit, chuyển Telex/VNI và quay lại ô nhập cũ.
Đây là Fcitx event pipeline thật với frontend giả, không phải gõ trong desktop.

Chưa nghiệm thu trên GNOME/KDE, X11/Wayland và các ứng dụng bên dưới. Môi trường
phát triển hiện tại là tty, không có DISPLAY hoặc WAYLAND_DISPLAY.

## Chuẩn bị

1. Build, test và cài thử theo [Linux README](../Sources/OpenKey/linux/README.md).
2. Lưu công việc đang soạn thảo trước khi đổi cấu hình input method.
3. Trong phiên desktop đang dùng Fcitx5, thêm OpenKey Telex và OpenKey VNI.
4. Ghi commit, Ubuntu release, desktop, session type, phiên bản Fcitx5,
   phiên bản ứng dụng và backend của ứng dụng (native Wayland hay XWayland).
5. Với mỗi ca kiểm thử, ghi PASS/FAIL, chuỗi đã gõ và kết quả thực tế; chỉ dùng
   văn bản thử nghiệm, không thu nội dung cá nhân hoặc mật khẩu.

Không cần và không được coi việc đổi input framework đang dùng là một phần của
test tự động. Hướng dẫn desktop chính thức:
[Fcitx5 setup](https://fcitx-im.org/wiki/Setup_Fcitx_5) và
[Fcitx5 Wayland](https://fcitx-im.org/wiki/Using_Fcitx_5_on_Wayland/en).

## Ca chức năng

| Ca | Thao tác | Kỳ vọng |
|---|---|---|
| Telex | Gõ `tieengs vieetj` rồi Space | `tiếng việt `, không mất/lặp phím |
| VNI | Gõ `tie6ng1 vie6t5` rồi Space | `tiếng việt ` |
| Chữ hoa | Gõ `TIEENGS` bằng Shift và Caps Lock | `TIẾNG` |
| Backspace | Gõ `tieengs`, Backspace, `g` | `tiến` rồi `tiếng` |
| Đặt lại dấu | Gõ `hoasn`, Backspace | `hoán` rồi `hoá` |
| Escape | Gõ một từ chưa commit rồi Escape | Hủy từ đang preedit, không xóa nội dung cũ |
| Space/Enter/dấu câu | Kết thúc từ bằng mỗi phím | Commit một lần, phím kết thúc vẫn đến ứng dụng |
| Điều hướng | Gõ từ rồi Left/Right/Home/End | Commit một lần trước khi di chuyển con trỏ |
| Shortcut | Gõ từ rồi Ctrl+A/C/V hoặc shortcut ứng dụng | Không nuốt shortcut, không lặp từ |
| Focus-out | Gõ `tieengs` chưa Space, bấm sang ô khác | Từ cũ xuất hiện đúng một lần, không sang ô mới |
| Focus quay lại | Quay lại ô cũ và gõ `dduowngf` | Từ mới đúng, không giữ trạng thái dấu của từ cũ |
| Đổi kiểu gõ | Đang gõ VNI, chuyển Telex rồi gõ từ mới | Commit từ cũ một lần; kiểu gõ mới có hiệu lực |
| Password | Gõ trong ô thử nghiệm loại Password/Sensitive | Không có preedit hoặc biến đổi tiếng Việt |
| Dán/chuột | Dán text hoặc đổi vị trí bằng chuột khi đang gõ | Không mất/lặp từ; ghi rõ hành vi nếu client gửi reset |
| Giới hạn | Gõ từ dài hơn 256 sự kiện | Commit theo phần, không treo, không mất nội dung |

## Ma trận ứng dụng — tất cả chưa chạy

| Môi trường | Firefox/Chrome | VS Code | LibreOffice | GTK editor | Qt editor |
|---|---|---|---|---|---|
| GNOME Wayland | Chưa chạy | Chưa chạy | Chưa chạy | Chưa chạy | Chưa chạy |
| GNOME X11 nếu có | Chưa chạy | Chưa chạy | Chưa chạy | Chưa chạy | Chưa chạy |
| KDE Wayland | Chưa chạy | Chưa chạy | Chưa chạy | Chưa chạy | Chưa chạy |
| KDE X11 nếu có | Chưa chạy | Chưa chạy | Chưa chạy | Chưa chạy | Chưa chạy |

Nếu desktop không cung cấp phiên X11 thì ghi N/A kèm lý do, không coi là PASS.
Nếu ứng dụng chạy qua XWayland thì ghi rõ; kết quả đó không chứng minh native Wayland.

## Điều kiện chuyển sang beta

- Các ca chính trên desktop mục tiêu phải có kết quả thực tế.
- Không có lỗi mất/lặp ký tự khi commit, đổi focus và đổi input method.
- Có phép đo độ trễ cho replay và lộ trình engine context riêng.
- Các giới hạn chưa hỗ trợ được giữ rõ trong tài liệu và UI.

Theo dõi triển khai tại [task Ubuntu](UBUNTU_TASKS.md).
