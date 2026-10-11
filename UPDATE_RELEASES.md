# Phát hành cập nhật OpenKey Fluent

Có workflow tự build theo tag và upload setup vào Release; xem `CI_RELEASES.md`.

Ứng dụng đọc metadata qua HTTPS tại
`https://raw.githubusercontent.com/minhtuan9922/OpenKey/master/version.json`.
Nhánh mặc định của repo hiện là `master`. Không dùng metadata hay bản tải của
`tuyenvm/OpenKey`; liên kết tác giả gốc trong Giới thiệu chỉ dùng ghi nguồn.

Kiểm tra công khai ngày 2026-10-11: Release mới nhất là `v2.0.5`, chưa có asset
đính kèm. Bản local mới đã nâng lên `3.0.0`; metadata chuẩn bị phát hành dùng
`v3.0.0` / `3`. Chỉ push metadata sau khi upload bộ cài vào tag `v3.0.0`.
Chưa xác nhận tải/nâng cấp thực tế từ GitHub.

## Chuẩn bị một bản mới

1. Tăng phiên bản ứng dụng và bộ cài đồng bộ. Hiện là `3.0.0`.
2. Build và đóng gói bằng `./build-setup.ps1 -Release`; giữ SHA-256 được in ra.
3. Tạo GitHub Release trong `minhtuan9922/OpenKey`, tag ví dụ `2.0.6`.
4. Upload file đúng tên `OpenKey-Setup.exe` cùng checksum và ghi chú phát hành.
   Giữ mã nguồn tương ứng công khai và notices/GPLv3.
5. Sau khi kiểm tra asset tải được, cập nhật `version.json` trên **master**:

```json
"latestWinVersion": {
  "versionName": "2.0.6",
  "versionCode": 393218
}
```

`versionName` phải trùng chính xác tag (có `v` thì metadata cũng phải có `v`).
`versionCode = major + (minor << 8) + (patch << 16)`; mỗi phần từ 0 đến 255.
Không tăng metadata trước khi bộ cài của tag đã sẵn sàng. Đây là feed chính thức
được chọn thủ công, không tự lấy release mới nhất hoặc preview từ GitHub API.

## Hành vi người dùng

- Kiểm tra khi bấm nút hoặc khi engine khởi động nếu bật tùy chọn kiểm tra.
- Phiên bản cao hơn: hỏi xác nhận, rồi mở trình duyệt tải asset của đúng tag:
  `https://github.com/minhtuan9922/OpenKey/releases/download/2.0.6/OpenKey-Setup.exe`.
- Không tự chạy setup, không tắt engine, không thay file đang dùng. Người dùng
  thoát engine/UI rồi chạy bộ cài; bộ cài quản lý runtime và giữ cấu hình.
- Không xác minh checksum file tải qua trình duyệt trong ứng dụng. Người dùng
  cần đối chiếu checksum phát hành; chữ ký số bộ cài vẫn là hạng mục riêng.
- Gói legacy không có Fluent vẫn dùng updater ZIP cũ, nhưng chuyển sang repo
  này và cần asset `OpenKey<TAG>-Windows.zip`. Không dùng updater ZIP cho Fluent.

Thay đổi trong workspace không tự push metadata hoặc tạo GitHub Release.
Nếu chưa upload asset, liên kết tải trả 404; ứng dụng không báo cài thành công.
Kiểm thử với release thực tế (phát hiện bản mới, hủy, tải setup, nâng cấp giữ dữ liệu)
vẫn cần thực hiện sau khi phát hành lên repo.
