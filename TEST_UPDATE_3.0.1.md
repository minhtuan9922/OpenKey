# Kiểm thử cập nhật 3.0.0 → 3.0.1

Phiên bản engine/updater/UI/setup: `3.0.1`.
Metadata Windows: tag `v3.0.1`, `versionCode = 65539` (`0x00010003`).
Nhánh metadata được ứng dụng đọc: `master` của `minhtuan9922/OpenKey`.
Chưa commit/push/tag/upload tự động từ máy phát triển.

## Chuẩn bị phát hành

- Commit các thay đổi version vào mã nguồn; bảo đảm tag trỏ tới commit đó.
- Push tag `v3.0.1` để workflow `Build offline setup on tag` build bộ cài.
- Đợi workflow hoàn tất và Release có `OpenKey-Setup.exe`, `SHA256SUMS.txt`.
- Bảo đảm `master/version.json` trên GitHub là `v3.0.1` / `65539`.
  Không kiểm thử tải trước khi asset đã sẵn sàng.
- Giữ ứng dụng đang cài ở **3.0.0**; chưa cài bộ cài local 3.0.1 trước khi thử phát hiện update.

## Checklist thực tế

- [ ] Trong Giới thiệu bản 3.0.0, bấm Kiểm tra cập nhật → thông báo `v3.0.1`.
- [ ] Chọn Để sau: không tải, không tắt app, không đổi cấu hình.
- [ ] Kiểm tra lại, chọn Tải bản cập nhật → trình duyệt tải từ
  `https://github.com/minhtuan9922/OpenKey/releases/download/v3.0.1/OpenKey-Setup.exe`.
- [ ] Đối chiếu SHA-256 file tải với checksum Release; không bỏ qua cảnh báo bảo vệ hệ thống.
- [ ] Thoát engine ở tray và đóng UI, rồi chạy bộ cài tải về.
- [ ] Cài vào cùng thư mục với 3.0.0; tùy chọn shortcut/runtime hoạt động đúng.
- [ ] Mở lại: Giới thiệu hiển thị 3.0.1, gõ tiếng Việt hoạt động, cấu hình/gõ tắt còn nguyên.
- [ ] Bấm Kiểm tra cập nhật ở 3.0.1 → báo đang dùng phiên bản mới nhất.
- [ ] Nếu bật kiểm tra lúc khởi động, thử thông báo update từ engine 3.0.0.

Luồng hiện tại tải qua trình duyệt và cài thủ công có xác nhận; không tự chạy
setup, không tự kill app hay thay file. Nhãn Latest/Pre-release không quyết định
việc phát hiện update: ứng dụng dùng metadata master và tải asset của đúng tag.
GitHub Raw có thể cache metadata; nếu còn thấy phiên bản cũ, chờ rồi kiểm tra lại.
