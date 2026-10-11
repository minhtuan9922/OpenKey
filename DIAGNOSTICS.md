# Báo cáo hỗ trợ beta

Xuất báo cáo từ thư mục đã cài (hoặc thư mục giải nén ZIP beta):

```powershell
.\collect-diagnostics.ps1 -AppDirectory "$env:LOCALAPPDATA\Programs\OpenKey-Fluent"
```

Script in đường dẫn `dist/diagnostics-<id>/support-report.json` mới, không ghi đè báo cáo cũ. Báo cáo gồm phiên bản Windows/PowerShell và tên tương đối, kích thước, file version, SHA-256 của executable OpenKey trong danh sách cho phép. File thiếu/không đọc được được ghi trạng thái; bỏ qua symlink/junction trong thư mục app. Không quét toàn ổ đĩa. Hash chỉ dùng đối chiếu, không chứng minh chữ ký hay nhà phát hành.

Không thu registry, macro, clipboard, nội dung gõ, user name, đường dẫn app tuyệt đối, event log hoặc crash dump; không tải lên mạng, chạy hoặc đóng app. Kiểm tra JSON trước khi tự gửi kèm phản hồi. Với crash, cung cấp thêm thao tác tái hiện và mã lỗi; báo cáo này không thay thế dump/stack trace. Không bật thu crash tự động.

Kiểm thử fixture: `./test-diagnostics.ps1` (không chạy executable fixture), tích hợp vào `./test-all.ps1 All`.

## Gửi phản hồi beta

Repository có hai mẫu trong `.github/ISSUE_TEMPLATE`: [báo lỗi](.github/ISSUE_TEMPLATE/bug_report.md) và [kết quả kiểm thử](.github/ISSUE_TEMPLATE/beta_test.md). Khi các file này được đưa lên nhánh mặc định, dùng GitHub Issues → New issue để chọn mẫu; mẫu tuân theo [định dạng issue template của GitHub](https://docs.github.com/en/communities/using-templates-to-encourage-useful-issues-and-pull-requests/syntax-for-issue-forms#markdown-issue-template).

Ghi phiên bản thực sự đang chạy, Windows/kiến trúc, thao tác tái hiện và kết quả. Với lỗi bộ gõ, ghi phím đã nhấn, kiểu gõ, bảng mã, Caps Lock và ứng dụng đích bằng văn bản mẫu. Với lỗi tray/layout, ghi theme, DPI, màn hình và bảng icon ẩn. Với nghiệm thu, phân biệt Đạt/Lỗi/Chưa chạy/Không áp dụng cho từng ca; không suy ra cả ma trận từ một ca đạt.

Issue công khai không phải kênh gửi dữ liệu bí mật. Báo cáo metadata cũng cần được kiểm tra trước khi chia sẻ. Không tự thu hoặc upload crash dump/log; không có telemetry mới và không tự tạo issue.
