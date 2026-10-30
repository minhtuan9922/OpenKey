# Kiểm thử bản vá Caps Lock

- Nguyên nhân: hook Windows gửi VK_CAPITAL vào engine như ký tự, làm từ
  không hợp lệ và ngừng đặt dấu cho tới dấu cách.
- Bản vá: sau kiểm tra hotkey, chuyển Caps Lock thẳng cho Windows; không thay
  buffer từ/gõ tắt hoặc bộ đếm backspace bảng mã hai ký tự. Đọc trạng thái
  bật Caps Lock bằng bit thấp của GetKeyState, không so sánh toàn bộ SHORT với 1.
- Hồi quy: 16 ca Telex/VNI tái hiện thất bại trước sửa, pass sau sửa;
  bổ sung 8 ca Simple Telex 1/2. Harness dùng policy routing production và
  engine thật, không cài hook, SendInput hay đổi trạng thái Caps Lock hệ thống.

## Nghiệm thu thực tế

Đã xác minh: 20/20 ca engine cũ và 24/24 ca Caps Lock trên x64/x86;
18/18 bộ hồi quy đạt (`dist/regression-3ca8b0b76d3f462494e4360c27643ae7/summary.json`).
Engine Debug x64, Release x64/x86 build 0 cảnh báo, 0 lỗi. Chưa chạy hook
hoặc kiểm thử ứng dụng thật trong lượt sửa này.

- [ ] Thoát engine cũ ở tray, chạy engine đã build bản vá.
- [ ] Bật/tắt Caps Lock rồi gõ tiếng Việt ngay, không cần nhấn dấu cách.
- [ ] Gõ `tieeng`, đổi Caps Lock, gõ `s`: vẫn đặt dấu đúng, giữ kiểu chữ đã gõ.
- [ ] Đổi Caps Lock giữa một từ, đổi nhiều lần; thử Telex, Simple Telex và VNI.
- [ ] Caps Lock + Shift, Backspace, gõ tắt và bảng mã đang dùng không bị ảnh hưởng.
- [ ] Thử trong Notepad và ứng dụng thường dùng.

Không tự thay engine đang cài hoặc cập nhật bộ cài cũ. Bộ cài tạo trước bản vá
phải build lại mới chứa thay đổi này.
