# Task phát triển OpenKey cho Ubuntu

Nhánh triển khai hiện tại: `feature/ubuntu-v0.1-foundation`

## v0.1 — Engine và build nền tảng

- [x] Tạo build entry point bằng CMake.
- [x] Biên dịch engine với macro nền tảng `LINUX`.
- [x] Tạo smoke test cho Unicode và VNI.
- [x] Bổ sung test hồi quy cho Telex, xử lý Backspace và macro.
- [x] Chuẩn hóa cảnh báo compiler trên GCC và Clang qua CMake/CI.
- [x] Tạo workflow build Linux trên CI.
- [ ] Thay `std::wstring_convert` đã deprecated bằng bộ chuyển UTF-8 có test Unicode riêng.
- [ ] Tách các API platform khỏi engine nếu phát hiện phụ thuộc ngoài Linux.

## v0.2 — Input method

- [ ] Chọn frontend chính: Fcitx5.
- [ ] Tạo module nhận key event từ Fcitx5.
- [ ] Gửi kết quả Unicode và lệnh backspace về ứng dụng.
- [ ] Kiểm thử Firefox/Chrome, LibreOffice, VS Code, GTK và Qt.
- [ ] Kiểm thử X11 và Wayland.
- [ ] Đánh giá IBus fallback.

## v0.3 — Tray và hotkey

- [ ] Tạo tiến trình chạy nền Linux.
- [ ] Thêm tray icon cho GNOME và KDE Plasma.
- [ ] Hiện trạng thái `V`/`E`.
- [ ] Port các lệnh trong [TrayContract.json](../Tests/TrayContract.json).
- [ ] Thêm global hotkey trên X11.
- [ ] Hiển thị giới hạn hotkey trên Wayland.
- [ ] Thêm autostart desktop entry.

## v0.4 — UI giống Windows

- [ ] Chọn và khóa framework UI (dự kiến Qt 6/QML).
- [ ] Dựng cửa sổ chính và sidebar.
- [ ] Dựng design tokens cho màu, spacing, typography và corner radius.
- [ ] Tạo theme sáng/tối/theo hệ thống.
- [ ] Port trang Tổng quan, Chính tả, Phím tắt và Tương thích.
- [ ] Port trang Nâng cao, Macro, Chuyển mã và Giới thiệu.
- [ ] Kiểm thử DPI, keyboard navigation và accessibility.

## v0.5 — Tính năng nâng cao

- [ ] Macro editor và import/export.
- [ ] Chuyển mã clipboard.
- [ ] Cấu hình theo ứng dụng.
- [ ] Lưu/khôi phục cấu hình.
- [ ] Thông báo lỗi và trạng thái engine.

## v0.6 — Đóng gói

- [ ] Tạo AppImage thử nghiệm.
- [ ] Tạo gói `.deb` cho Ubuntu.
- [ ] Tạo Flatpak.
- [ ] Kiểm tra cài mới, nâng cấp, gỡ cài đặt và giữ cấu hình.
- [ ] Thêm checksum và artifact Linux vào CI.

## Điều kiện nghiệm thu bản Ubuntu đầu tiên

- Gõ Telex và VNI ổn định trong các ứng dụng phổ biến.
- Không mất phím khi chuyển cửa sổ hoặc dùng Backspace.
- Tray, hotkey và autostart hoạt động đúng trên desktop được hỗ trợ.
- UI có theme sáng/tối, tiếng Việt và điều hướng bằng bàn phím.
- Có gói cài đặt và hướng dẫn gỡ cài đặt.
