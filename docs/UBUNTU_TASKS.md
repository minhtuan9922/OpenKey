# Task phát triển OpenKey cho Ubuntu

Nhánh triển khai hiện tại: `feature/ubuntu-direct-input`

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

## v0.2.1 — Gõ trực tiếp không preedit/gạch chân (ưu tiên)

Phản hồi kiểm thử: người dùng xác nhận bản alpha gõ tiếng Việt được, nhưng
preedit có gạch chân và gợi ý code trong VS Code không xuất hiện khi đang gõ.
Đây là phản hồi trên môi trường người dùng, chưa thay thế toàn bộ ma trận QA.

Mục tiêu: ký tự được đưa trực tiếp vào tài liệu, không giữ cả từ trong composition;
VS Code có thể cập nhật gợi ý trong lúc gõ. Chỉ ẩn nét gạch chân không đạt mục tiêu.
Giữ chế độ preedit hiện tại để tương thích với các ứng dụng không hỗ trợ gõ trực tiếp.

### Khảo sát và thiết kế

- [ ] Xác minh VS Code/Electron cung cấp surrounding text, vị trí con trỏ và
  API xóa/thay thế văn bản qua Fcitx5 trên native Wayland và X11/XWayland.
- [x] Thiết kế chế độ `Preedit` và `Direct` có thể chọn; không mặc định bật Direct
  cho mọi ứng dụng trước khi kiểm chứng. Xác định cách lưu cấu hình và chuyển chế độ
  khi đang gõ mà không commit lặp hoặc làm mất ký tự.
- [x] Xác định điều kiện hỗ trợ Direct và chính sách fallback khi client thiếu
  capability, surrounding text không hợp lệ, có selection hoặc cursor đã di chuyển.

### Triển khai

- [x] Commit ký tự ngay ở chế độ Direct; không gửi client/server preedit cho từ
  đang gõ. Tính phần văn bản thực sự cần thay đổi khi thêm/bỏ dấu Telex/VNI.
- [x] Thay thế đúng đoạn vừa gõ bằng API surrounding-text của Fcitx5 khi được hỗ trợ;
  kiểm tra Unicode code point, vị trí cursor/anchor và ranh giới từ trước khi sửa.
- [x] Quản lý context riêng cho từng ô nhập, trạng thái văn bản dự kiến và các edit
  đang chờ xác nhận; xử lý surrounding-text cập nhật chậm hoặc đến không đúng thứ tự
  khi gõ nhanh. Không coi một snapshot cũ là trạng thái hiện tại của ứng dụng.
- [ ] Reset ngữ cảnh đúng khi click/di chuyển cursor, chọn vùng, nhận gợi ý code,
  dán, undo/redo, đổi focus hoặc đổi kiểu gõ; không sửa nhầm văn bản ngoài vùng
  engine còn quản lý. Không commit lại từ Direct khi mất focus.
- [x] Định nghĩa hành vi Backspace, Escape, Space/Enter và shortcut trong Direct;
  Escape không được xóa lại từ đã commit như khi hủy preedit.
- [x] Giữ nguyên bypass ô Password/Sensitive. Fallback có kiểm soát về preedit
  hoặc chuyển phím nguyên trạng khi không thể sửa an toàn; không giả định mọi
  ứng dụng đều hỗ trợ Direct. Không dùng clipboard hoặc hook phím toàn hệ thống
  làm giải pháp mặc định.

### Kiểm thử và nghiệm thu

- [x] Thêm test engine/adapter và Fcitx pipeline cho Telex/VNI Direct, thay dấu,
  chữ hoa, Backspace và từ dài; bảo đảm chế độ Preedit cũ không hồi quy.
- [x] Test headless ACK chậm/cũ, burst nhiều từ, timeout/overflow, cursor/selection,
  đổi chế độ, không làm mất panel của IM khác và không ghi đè selection từ phím chờ.
  Chữ chờ ở context đã thay đổi được giữ trong bảng phụ, Enter để chèn, Escape hủy.
- [ ] Thêm test context cập nhật chậm, gõ nhanh liên tiếp, selection/cursor thay đổi,
  chấp nhận completion, focus-out và fallback; không mất/lặp ký tự hoặc sửa sai vùng.
- [ ] Kiểm thử VS Code thực tế: gõ identifier ASCII như `console`/`print` và xác
  nhận popup IntelliSense xuất hiện/cập nhật trước Space; nhận gợi ý bằng Tab/Enter
  không làm hỏng nội dung. Kiểm tra inline completion riêng nếu đang dùng.
- [ ] Kiểm thử tiếng Việt trong comment/string, đổi qua lại English/Telex/VNI;
  phân biệt thay đổi do quy tắc Telex/VNI với lỗi tương tác completion.
- [ ] Kiểm thử native Wayland và X11/XWayland, GTK/Qt, trình duyệt và LibreOffice;
  ghi rõ ứng dụng nào dùng Direct được và ứng dụng nào cần fallback.
- [ ] Đo độ trễ và hành vi undo/redo thực tế; ghi giới hạn trong
  [Ubuntu input QA](UBUNTU_INPUT_QA.md), [Linux README](../Sources/OpenKey/linux/README.md)
  và hướng dẫn lựa chọn chế độ trước khi phát hành `.deb` mới.

Điều kiện hoàn thành: gõ trực tiếp không composition/gạch chân trên các ứng dụng
được công bố hỗ trợ, gợi ý code VS Code hoạt động khi đang gõ, không mất/lặp ký tự
và có fallback đã kiểm chứng. Đã triển khai alpha.3 và kiểm thử headless; chưa
nghiệm thu IntelliSense/undo/redo và Direct trên VS Code desktop thực tế.

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
