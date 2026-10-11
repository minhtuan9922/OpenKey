# GitHub tự build bộ cài theo tag

Workflow: `.github/workflows/release-setup.yml`.

## Kích hoạt

Commit mã nguồn, scripts và workflow đã sửa trước khi tạo tag; tag phải trỏ tới
commit chứa đúng phiên bản. Không tạo tag trên commit cũ chưa có workflow.

```powershell
git tag v3.0.0
git push origin v3.0.0
```

Xem GitHub → Actions → **Build offline setup on tag**.
Hoặc chạy **Run workflow** với input `tag` là tag đã tồn tại (`v3.0.0`).
Để chạy thủ công, workflow phải có trên nhánh mặc định. Repo cần bật Actions
và cho phép quyền `contents: write` của job publish. Không cần PAT riêng.

## Quy trình

1. Checkout đúng tag, đối chiếu `version.json`, UI, tài nguyên version và setup.
2. Runner `windows-2025-vs2026`; kiểm tra VS 2026 C++ x86/x64 và ATL.
3. Tải runtime Microsoft và Inno Setup 6.7.3 từ URL đã ghim. Kiểm tra SHA-256
   và Authenticode. Chỉ chuẩn bị compiler portable trên runner; không cài runtime.
4. Build Release engine/updater/UI x64+x86, đóng gói setup offline và xác minh.
5. Chạy toàn bộ 18 suite fixture. Không có bước cài setup OpenKey hoặc chạy UI.
6. Upload artifact `offline-setup`, chuyển cho job publish riêng (không checkout
   hay thực thi source của repo với token ghi).
7. Kiểm tra checksum, đính kèm `OpenKey-Setup.exe` và `SHA256SUMS.txt` vào Release.
   Nếu chưa có Release, tạo draft pre-release, upload rồi mở draft thành pre-release.
   Nếu đã có Release, chỉ thêm asset. Không ghi đè asset cùng tên; rerun có thể
   dừng ở upload nếu file đã tồn tại. Draft do người dùng tạo sẵn vẫn giữ draft.

Nếu một bước lỗi, không publish setup. Log kiểm thử và manifest được giữ trong
artifact `setup-diagnostics` 14 ngày. Phát hành tự động chỉ chạy ở
`minhtuan9922/OpenKey`; fork vẫn có thể build artifact nhưng không publish.
Chưa ký số và chưa nghiệm thu máy sạch nên mặc định tạo **pre-release**, không stable.

## Metadata cập nhật

`versionName` phải đúng tag, `versionCode` là dạng packed (3.0.0 = 3), không phải
một số tăng dần thông thường. Không quảng bá bản mới trước khi upload hoàn tất.
Nên tag commit trên nhánh phát triển, đợi setup upload thành công, sau đó mới
merge metadata phiên bản vào `master`. Workflow không tự push hoặc sửa master.
Các ứng dụng đọc feed master theo `UPDATE_RELEASES.md`, kể cả bản được chọn là
pre-release; đây là feed thủ công, không phải lọc tự động GitHub Release.

## Dependency và kiểm thử local

URL/hash runtime: `Installer/OfflineInputs.json`. Compiler: `Installer/CiCompiler.json`.
Nếu toolset hoặc dependency thay đổi, cần review lại version/hash và quyền phân phối;
script dừng khi hash/chữ ký/version không đúng, không tự chấp nhận bản khác.

```powershell
.\test-ci-release.ps1
.\prepare-ci.ps1 -DownloadOnly
.\build-ci-release.ps1 -Tag v3.0.0
```

Lệnh cuối build và chạy các fixture; không publish, không chạy setup. Thư mục
`dist/ci-release` phải chưa tồn tại để tránh ghi đè kết quả. Dữ liệu đã có được giữ.
Kiểm tra tại máy phát triển không thay thế lần chạy thật trên GitHub runner.
