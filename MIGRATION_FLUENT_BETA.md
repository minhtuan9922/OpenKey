# Thử Fluent beta và quay lại OpenKey cũ

Đây là hướng dẫn thử nghiệm thủ công, chưa phải quy trình nâng cấp đã được xác nhận
trên máy sạch. Beta chưa được phát hành stable. Giữ nguyên thư mục/ZIP bản cũ;
không chép đè beta lên bản đang dùng.

## Dữ liệu và giới hạn

Engine và Fluent UI dùng chung `HKEY_CURRENT_USER\SOFTWARE\TuyenMai\OpenKey`.
Cài đặt là các giá trị DWORD; Gõ tắt lưu trong giá trị binary `macroData`.
Thay đổi cài đặt/Gõ tắt trong beta cũng ảnh hưởng bản cũ của cùng tài khoản.
Thư mục cài riêng không tạo hồ sơ riêng. Không đổi tài khoản Windows hoặc chạy
bằng thông tin đăng nhập của tài khoản khác khi kiểm thử giữ nguyên dữ liệu.

Sao lưu registry không sao lưu Scheduled Task, shortcut hoặc tệp executable.
Xuất Gõ tắt ra văn bản cũng không thay thế sao lưu binary: nội dung nhiều dòng
không được hỗ trợ bởi chức năng xuất macro hiện tại.

## Trước khi thử

1. Giữ bản cũ và ghi lại đường dẫn executable/shortcut dùng để khởi động.
2. Trong bản cũ, ghi lại kiểu gõ, bảng mã, phím tắt, trạng thái khởi động cùng Windows
   và tùy chọn kiểm tra cập nhật. Xuất Gõ tắt nếu chức năng đó hỗ trợ dữ liệu của bạn.
3. Thoát engine và mọi cửa sổ điều khiển OpenKey trước khi sao lưu registry.
4. Dùng Registry Editor xuất khóa `HKCU\SOFTWARE\TuyenMai\OpenKey` ra một tệp mới,
   hoặc chạy lệnh sau trong PowerShell với một đường dẫn sao lưu chưa tồn tại:

   ```powershell
   reg.exe export 'HKCU\SOFTWARE\TuyenMai\OpenKey' 'D:\Backup\OpenKey-before-beta.reg'
   ```

   Thay đường dẫn ví dụ bằng thư mục có sẵn của bạn; kiểm tra exit code và tệp kết quả.
   Nếu khóa chưa tồn tại, ghi nhận trạng thái đó, không tạo khóa chỉ để sao lưu.
   Giữ bản sao ở nơi riêng tư vì macro có thể chứa nội dung nhạy cảm.
5. Nếu có startup task/shortcut, ghi lại cấu hình và đường dẫn đích. Không bật/tắt
   startup trong beta cho đến khi kiểm thử luồng đó. Không dùng nút khôi phục mặc định.

## Thử beta

1. Xác minh ZIP bằng `verify-beta.ps1`; nếu có hash từ kênh đáng tin, truyền ExpectedSha256.
2. Giải nén toàn bộ vào thư mục mới. Chạy `check-prerequisites.ps1` từ thư mục đó;
   nếu thiếu runtime, dùng nguồn Microsoft ghi trong BETA_PACKAGING.md.
3. Đảm bảo bản cũ đã thoát. Chạy OpenKey64.exe (x64) hoặc OpenKey32.exe (x86).
   Chỉ một engine được chạy; mở bảng điều khiển từ tray.
4. Không chấp nhận cập nhật tự động/không chạy OpenKeyUpdate.exe: updater còn tải
   gói stable và không nâng cấp đồng bộ Fluent UI/runtime. Updater của nhánh này
   chỉ thay engine tương ứng kiến trúc build; không trộn engine/updater khác kiến trúc.
   Fluent UI đã chuyển nút cập nhật sang xem trang phát hành, không tự thay tệp/thoát.
   Các đường gọi legacy và updater trong bundle có chốt chặn engine-only update;
   việc này không biến updater stable thành updater Fluent bundle.
5. Trước khi chỉnh cài đặt, đối chiếu kiểu gõ, bảng mã, phím tắt và danh sách macro
   với bản cũ. Thử vài macro không nhạy cảm và các tùy chọn dự định dùng.
   Nếu thấy dữ liệu thiếu/sai, dừng thử, không lưu/xóa/nhập đè macro.

## Quay lại bản cũ

1. Thoát beta (cả engine và UI). Chạy executable bản cũ từ thư mục đã giữ lại.
   Chưa xóa thư mục beta hoặc bản sao lưu trong lúc chẩn đoán.
2. Kiểm tra lại cài đặt/Gõ tắt. Nếu beta chưa sửa dữ liệu, trước tiên dùng trực tiếp
   dữ liệu hiện tại; không tự động nhập registry chỉ vì đã đổi executable.
3. Nếu cần khôi phục, thoát mọi tiến trình OpenKey. Kiểm tra nội dung và đúng tài khoản
   của bản sao lưu, rồi chủ động nhập bằng Registry Editor hoặc `reg.exe import`.
   Nhập registry là thao tác ghi đè các giá trị có trong tệp; nó không xóa giá trị
   mới phát sinh sau sao lưu và không bảo đảm rollback nguyên trạng tuyệt đối.
   Không xóa cả khóa registry để làm sạch. Khi chưa chắc, giữ dữ liệu hiện tại và
   nhờ kiểm tra trước khi nhập. Sau đó chạy bản cũ và đối chiếu lại.
4. Nếu đã đổi startup/shortcut trong lúc thử, kiểm tra đích trỏ về bản cũ và khôi phục
   theo cấu hình đã ghi riêng. Nhập settings registry không tự sửa các đích này.

## Checklist nghiệm thu chưa hoàn tất

- [ ] Nâng cấp trên tài khoản thử nghiệm/VM có snapshot, từ dữ liệu bản cũ thực tế.
- [ ] Settings và macro nguyên vẹn trước/sau mở beta; ghi rõ khác biệt nếu có.
- [ ] Macro tiếng Việt, ký tự đặc biệt/nhiều dòng và dữ liệu lớn không mất nội dung.
- [ ] Quay lại engine cũ, gõ và dùng macro đúng; không còn hai engine hoặc hook dư.
- [ ] Startup thường/admin và shortcut chạy đúng sau nâng cấp/rollback.
- [ ] Máy sạch không Visual Studio chạy được với prerequisite đã cài.

Việc có hướng dẫn hoặc ZIP được xác minh không đánh dấu các kiểm thử này là đã đạt.
