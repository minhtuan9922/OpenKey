# Kế hoạch phát triển OpenKey cho Ubuntu

## 1. Mục tiêu

Xây dựng phiên bản OpenKey cho Ubuntu với trải nghiệm gần giống bản Windows:

- Giao diện điều hướng bên trái kiểu Fluent.
- Theme sáng, tối và theo hệ thống.
- Menu tray với các thao tác tương đương Windows.
- Giữ lại engine, kiểu gõ, bảng mã, macro và chuyển mã.
- Tích hợp phù hợp với X11, Wayland, Fcitx5 và IBus.

Trạng thái hiện tại: đã có engine build/test Linux, addon Fcitx5 với Preedit/Direct
và gói alpha.9. Người dùng đã kiểm thử các ca Direct trên VS Code với profile
accessible; chưa nghiệm thu toàn bộ backend/ứng dụng. Đang triển khai nền tảng
session service v0.3 với tray chỉ đọc tùy chọn (chưa nghiệm thu panel desktop),
chưa có cửa sổ UI điều khiển. Xem [task đang theo dõi](UBUNTU_TASKS.md),
[Linux README](../Sources/OpenKey/linux/README.md) và [service](UBUNTU_SERVICE.md).
Giao diện Windows vẫn là tham chiếu tại
[MainWindow.xaml](../Sources/OpenKey/win32/OpenKey/OpenKey.UI/MainWindow.xaml).

## 2. Kiến trúc đề xuất

Không port trực tiếp WinUI/XAML sang Ubuntu. Nên tách thành ba lớp:

```text
Shared Engine
├── Vietnamese
├── ConvertTool
├── Macro
└── SmartSwitch

Linux Platform
├── Fcitx5 frontend
├── IBus fallback
├── Global hotkey
├── Clipboard
├── Tray
└── Startup/autostart

Linux UI
├── Main window
├── Sidebar navigation
├── Settings pages
├── Macro editor
└── Convert tool
```

Đề xuất dùng Qt 6/QML cho UI Linux vì phù hợp với sidebar, card, toggle, theme và giao diện gần Windows. Backend vẫn dùng C++ để tái sử dụng engine hiện tại.

## 3. Lộ trình phiên bản

### v0.1 — Engine chạy được trên Ubuntu

Mục tiêu: build và kiểm thử engine trên Linux.

Công việc:

- Tạo hệ thống build Linux bằng CMake.
- Tách phần phụ thuộc Windows/macOS khỏi engine.
- Chuẩn hóa interface cho key event, clipboard, cấu hình và logging.
- Build các test engine hiện có.
- Hỗ trợ Telex, VNI, Unicode và TCVN3.

Tiêu chí hoàn thành:

- Build được trên Ubuntu x86_64.
- Test xử lý chuỗi và chuyển đổi chạy đạt.
- Có thể chạy engine test mà chưa cần UI.

### v0.2 — Input method trên Ubuntu

Mục tiêu: gõ tiếng Việt trong ứng dụng Linux.

Ưu tiên Fcitx5 trước, sau đó bổ sung IBus.

Công việc:

- Tạo frontend cho Fcitx5.
- Nhận và xử lý phím từ ứng dụng.
- Gửi chuỗi Unicode đã xử lý trở lại ứng dụng.
- Xử lý Backspace, Enter, Space, Escape và phím mũi tên.
- Kiểm thử X11 và Wayland.
- Kiểm thử Firefox/Chrome, LibreOffice, VS Code, GTK và Qt.
- Xử lý xung đột với bộ gõ khác.

### v0.3 — Tray và phím tắt

Mục tiêu: OpenKey chạy nền giống bản Windows.

Menu tray nên bám theo [TrayContract.json](../Tests/TrayContract.json):

- Bật/tắt tiếng Việt.
- Bật/tắt kiểm tra chính tả.
- Bật/tắt gõ tắt.
- Chọn Telex/VNI/Simple Telex.
- Chọn bảng mã.
- Mở bảng điều khiển.
- Mở công cụ chuyển mã.
- Đổi theme.
- Thoát ứng dụng.

Công việc nền tảng:

- Tray icon cho GNOME và KDE Plasma.
- Global hotkey cho X11.
- Xác định giới hạn global hotkey trên Wayland.
- Hiển thị trạng thái V/E.
- Đảm bảo chỉ chạy một tiến trình OpenKey.
- Tự khởi động cùng phiên đăng nhập.

### v0.4 — Giao diện Ubuntu giống Windows

Mục tiêu: hoàn thiện giao diện điều khiển chính theo cấu trúc [MainWindow.xaml](../Sources/OpenKey/win32/OpenKey/OpenKey.UI/MainWindow.xaml).

Các trang:

- Tổng quan.
- Chính tả và gõ nhanh.
- Phím tắt.
- Ứng dụng và tương thích.
- Cài đặt nâng cao.
- Gõ tắt.
- Chuyển mã.
- Giới thiệu.

Phong cách UI:

- Sidebar trái có thể thu gọn.
- Card bo góc.
- Toggle và ComboBox theo phong cách Windows.
- Theme sáng/tối/theo hệ thống.
- Hỗ trợ tiếng Việt và tiếng Anh.
- Hỗ trợ DPI scaling.
- Điều hướng bằng bàn phím và accessibility.

Chỉ tái sử dụng cấu trúc và hành vi từ UI Windows; không sao chép nguyên mã XAML.

### v0.5 — Macro, chuyển mã và cấu hình

Mục tiêu: đạt tương đương chức năng với bản Windows.

Công việc:

- Macro editor.
- Import/export macro.
- Chuyển mã clipboard.
- Cấu hình theo ứng dụng.
- Lưu và khôi phục cấu hình.
- Di chuyển cấu hình từ phiên bản cũ.
- Thông báo lỗi clipboard và trạng thái engine.

Baseline test nên tham khảo:

- [MacroServiceTests.cpp](../Tests/MacroServiceTests.cpp)
- [ConvertServiceTests.cpp](../Tests/ConvertServiceTests.cpp)
- [SettingsMappingTests.cpp](../Tests/SettingsMappingTests.cpp)

### v0.6 — Đóng gói Ubuntu

Mục tiêu: người dùng cài được mà không cần build mã nguồn.

Thứ tự ưu tiên:

1. AppImage để thử nghiệm.
2. Gói `.deb` cho Ubuntu/Debian.
3. Flatpak để phân phối dài hạn.

Công việc:

- Desktop entry.
- Autostart.
- Icon và tray assets.
- Gỡ cài đặt sạch.
- Nâng cấp nhưng giữ nguyên cấu hình.
- Checksum và pipeline build Linux.
- Tài liệu yêu cầu Fcitx5/IBus.

## 4. Ma trận kiểm thử

| Nhóm | Môi trường |
|---|---|
| Desktop | GNOME, KDE Plasma |
| Display server | X11, Wayland |
| Input framework | Fcitx5, IBus |
| Ứng dụng | Firefox/Chrome, LibreOffice, VS Code, GTK, Qt |
| Theme | Sáng, tối, theo hệ thống |
| Kiến trúc | x86_64 trước; ARM64 sau |
| Cài đặt | AppImage, `.deb`, autostart, nâng cấp |
| Tính năng | Telex, VNI, macro, clipboard, hotkey, tray |

## 5. Phiên bản phát hành dự kiến

- `3.1.0-linux-alpha`: engine và Fcitx5 cơ bản.
- `3.1.0-linux-beta`: tray, hotkey và UI chính.
- `3.1.0`: UI hoàn chỉnh, macro, chuyển mã và gói `.deb`.
- `3.1.1`: sửa lỗi tương thích GNOME, KDE và Wayland.
- `3.2.0`: IBus, Flatpak, đồng bộ cấu hình và ARM64.

## 6. Thứ tự ưu tiên

1. Build engine trên Linux.
2. Fcitx5 gõ ổn định.
3. Tray và hotkey.
4. UI chính giống Windows.
5. Macro và chuyển mã.
6. Đóng gói AppImage và `.deb`.
7. IBus, Flatpak và ARM64.

## 7. Bước triển khai đầu tiên

Tạo proof-of-concept Linux gồm CMake, executable engine và frontend Fcitx5 tối thiểu. Chỉ khi việc gõ tiếng Việt ổn định trong các ứng dụng phổ biến mới triển khai toàn bộ giao diện, nhằm tránh xây UI trên nền tảng input chưa ổn định.

