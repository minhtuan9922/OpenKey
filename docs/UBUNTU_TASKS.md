# Task phát triển OpenKey cho Ubuntu

Nhánh triển khai hiện tại: `feature/ubuntu-deb-packaging`

Mốc nền tảng v0.1: commit `b22c3eb`. Hướng dẫn build/cài thử và giới hạn của
prototype: [Linux README](../Sources/OpenKey/linux/README.md).

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

- [x] Chọn frontend chính: Fcitx5.
- [x] Tạo module nhận key event từ Fcitx5 (Telex và VNI).
- [x] Cập nhật Unicode/preedit và Backspace trong từ đang gõ, commit qua Fcitx5.
- [x] Test headless nạp module thật, preedit/commit, Escape/reset/deactivate,
  shortcut, ô nhạy cảm và isolation giữa context.
- [x] Bổ sung build/install target và CI build addon.
- [x] Kiểm thử discovery descriptor, nạp addon từ install prefix và routing qua
  Fcitx event pipeline thật (headless, không dùng desktop).
- [x] Sửa lỗi commit lặp từ khi mất focus; test focus-out cho client preedit,
  ClientUnfocusCommit và server-side preedit, chuyển Telex/VNI và quay lại ô cũ.
- [ ] Kiểm thử cài vào desktop, phát hiện input method và focus-out thực tế.
- [ ] Đo độ trễ replay, thiết kế engine context riêng trước beta.
- [ ] Kết nối macro/cấu hình lưu bền và các kiểu gõ còn lại vào addon.
- [ ] Kiểm thử Firefox/Chrome, LibreOffice, VS Code, GTK và Qt.
- [ ] Kiểm thử X11 và Wayland.
- [ ] Đánh giá IBus fallback.

Checklist nghiệm thu thủ công: [Ubuntu input QA](UBUNTU_INPUT_QA.md).

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
- [x] Tạo gói `.deb` alpha cho addon Fcitx5 (chưa có UI).
- [x] Kiểm tra dependency, payload và nạp addon từ gói đã giải nén.
- [ ] Tạo Flatpak.
- [ ] Kiểm tra cài mới, nâng cấp, gỡ cài đặt và giữ cấu hình.
- [x] Thêm checksum SHA-256 và job artifact `.deb` vào CI (chưa chạy trên remote).

Build/cài/gỡ bản alpha: [Ubuntu install](UBUNTU_INSTALL.md). Kiểm thử cài/gỡ bằng
APT trên máy desktop vẫn chưa nghiệm thu; không đánh dấu xong mục vòng đời gói ở trên.

## Điều kiện nghiệm thu bản Ubuntu đầu tiên

- Gõ Telex và VNI ổn định trong các ứng dụng phổ biến.
- Không mất phím khi chuyển cửa sổ hoặc dùng Backspace.
- Tray, hotkey và autostart hoạt động đúng trên desktop được hỗ trợ.
- UI có theme sáng/tối, tiếng Việt và điều hướng bằng bàn phím.
- Có gói cài đặt và hướng dẫn gỡ cài đặt.
