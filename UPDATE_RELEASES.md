# Phát hành cập nhật OpenKey Fluent

Có workflow tự build theo tag và upload setup vào Release; xem `CI_RELEASES.md`.

Ứng dụng đọc metadata qua HTTPS tại
`https://raw.githubusercontent.com/minhtuan9922/OpenKey/master/version.json`.
Nhánh mặc định của repo hiện là `master`. Không dùng metadata hay bản tải của
`tuyenvm/OpenKey`; liên kết tác giả gốc trong Giới thiệu chỉ dùng ghi nguồn.

Mã nguồn và bộ cài local hiện là `3.0.3`; metadata trong workspace dùng
`v3.0.3` / `196611`. Người dùng đã xác nhận cập nhật thực tế lên 3.0.3 hoạt động ổn.
Tài liệu này không kiểm tra trạng thái Release/asset GitHub hiện tại.
Các sửa lỗi local sau đó cần commit và phát hành bằng một phiên bản mới nếu
muốn ứng dụng 3.0.3 nhận thông báo cập nhật; build lại cùng version không kích hoạt update.

## Chuẩn bị một bản mới

1. Tăng phiên bản ứng dụng, metadata và bộ cài đồng bộ. Hiện là `3.0.3`.
2. Build và đóng gói bằng `./build-setup.ps1 -Release`; giữ SHA-256 được in ra.
3. Commit các thay đổi trước khi tạo tag `vX.Y.Z` trong `minhtuan9922/OpenKey`.
   Workflow build/publish theo tag được mô tả ở `CI_RELEASES.md`; dùng tag mới,
   không di chuyển tag đã phát hành để thay binary cùng phiên bản.
4. Upload file đúng tên `OpenKey-Setup.exe` cùng checksum và ghi chú phát hành.
   Giữ mã nguồn tương ứng công khai và notices/GPLv3.
5. Sau khi kiểm tra asset tải được, cập nhật `version.json` trên **master**:

```json
"latestWinVersion": {
  "versionName": "v3.0.3",
  "versionCode": 196611
}
```

`versionName` phải trùng chính xác tag (có `v` thì metadata cũng phải có `v`).
`versionCode = major + (minor << 8) + (patch << 16)`; mỗi phần từ 0 đến 255.
Không tăng metadata trước khi bộ cài của tag đã sẵn sàng. Đây là feed chính thức
được chọn thủ công, không tự lấy release mới nhất hoặc preview từ GitHub API.

## Hành vi người dùng

- Kiểm tra khi bấm nút hoặc khi engine khởi động nếu bật tùy chọn kiểm tra.
- Phiên bản cao hơn: hỏi xác nhận, rồi mở trình duyệt tải asset của đúng tag:
  `https://github.com/minhtuan9922/OpenKey/releases/download/v3.0.3/OpenKey-Setup.exe`.
- Ứng dụng không tự chạy setup, tắt engine hoặc thay file đang dùng. Người dùng
  chạy bộ cài; bộ cài hỏi trước khi đóng engine/UI trong đúng thư mục đích,
  không cưỡng bức thoát, quản lý runtime và giữ cấu hình đã lưu.
- Không xác minh checksum file tải qua trình duyệt trong ứng dụng. Người dùng
  cần đối chiếu checksum phát hành; chữ ký số bộ cài vẫn là hạng mục riêng.
- Gói legacy không có Fluent vẫn dùng updater ZIP cũ, nhưng chuyển sang repo
  này và cần asset `OpenKey<TAG>-Windows.zip`. Không dùng updater ZIP cho Fluent.

Thay đổi trong workspace không tự push metadata hoặc tạo GitHub Release.
Nếu chưa upload asset, liên kết tải trả 404; ứng dụng không báo cài thành công.
Luồng cập nhật thực tế lên 3.0.3 đã được người dùng xác nhận. Khi phát hành sửa lỗi
mới, kiểm thử lại phát hiện bản mới, hủy, tải setup và nâng cấp giữ dữ liệu;
không suy kết quả của bản trước cho toàn bộ ca lỗi hoặc artifact mới.
