---
name: 'Kết quả kiểm thử beta'
about: 'Ghi nhận các ca đã chạy, đạt, lỗi hoặc chưa kiểm thử'
title: '[Kiểm thử] '
---

## Quyền riêng tư

Chỉ dùng dữ liệu mẫu. Không gửi registry, dữ liệu gõ tắt thật, nội dung gõ cá nhân hoặc crash dump vào issue công khai. Kiểm tra ảnh/log/báo cáo trước khi đính kèm.

- [ ] Tôi đã kiểm tra và loại bỏ thông tin nhạy cảm khỏi nội dung/tệp đính kèm.

## Môi trường

- Phiên bản OpenKey / tag / nguồn tải:
- Windows và OS build; Windows x64/x86; engine x64/x86:
- Máy thật / Sandbox / VM / Remote Desktop:
- Bộ cài / ZIP; cài mới / nâng cấp từ phiên bản nào:
- Theme / ngôn ngữ UI / DPI / cỡ chữ / số màn hình:
- Layout bàn phím, kiểu gõ, bảng mã:
- Ứng dụng đích và phiên bản; chạy thường / Administrator:

## Các ca kiểm thử

Thay từng dòng bằng **Đạt**, **Lỗi**, **Chưa chạy** hoặc **Không áp dụng**, kèm thao tác và kết quả. Không ghi cả nhóm là đạt khi mới thử một trường hợp. Với lỗi, tạo báo lỗi riêng và liên kết lại đây.

- Cài mới, shortcut Start Menu/Desktop, mở engine và UI: Chưa chạy
- Nâng cấp; cấu hình và gõ tắt mẫu trước/sau; không tự chạy app khi bỏ chọn: Chưa chạy
- Thiếu runtime / hủy UAC / runtime cài lỗi hoặc yêu cầu reboot: Chưa chạy
- App còn chạy / đóng app theo consent / hủy cập nhật / debugger giữ app: Chưa chạy
- Gỡ cài đặt; dữ liệu cấu hình/gõ tắt mẫu còn được giữ: Chưa chạy
- Gõ tiếng Việt; chuyển Việt/Anh; đổi Caps Lock giữa từ: Chưa chạy
- Gõ tắt; draft; thêm/sửa/xóa; import/export: Chưa chạy
- Chuyển mã văn bản / clipboard Unicode / clipboard HTML trong app đích: Chưa chạy
- Tray trực tiếp / bảng icon ẩn; submenu; click ngoài; Escape: Chưa chạy
- Đổi Sáng/Tối/Theo hệ thống; tiếng Việt/Anh; resize và DPI: Chưa chạy
- Bàn phím điều hướng / Narrator / High Contrast: Chưa chạy
- Sleep/resume / sign out / nhiều user session / Remote Desktop: Chưa chạy

## Kết luận trong phạm vi đã thử

- Các ca lỗi và liên kết issue:
- Các ca chưa thử / cần thử lại:
- Ảnh/log/báo cáo đã kiểm tra quyền riêng tư (không bắt buộc):
