# Công cụ PowerShell

Chạy lệnh từ thư mục gốc repository. Các script tự xác định root theo vị trí
file, không phụ thuộc thư mục làm việc hiện tại.

```powershell
./build.ps1 -Configuration Debug -Platform x64 -Target All
./test-all.ps1 All
./scripts/run-ui-debug.ps1 x64
./scripts/test-caret-tone-edit.ps1 x64
./scripts/build-text-service.ps1 All
./scripts/test-text-service.ps1 x64
./scripts/test-caret-deployment.ps1
./scripts/build-setup.ps1 -Release
```

- Build/phát hành: `package-beta.ps1`, `build-setup.ps1`, `prepare-ci.ps1`,
  `build-ci-release.ps1`, `verify-beta.ps1`, `verify-setup.ps1`.
- Phát triển/QA: `run-debug.ps1`, `run-ui-debug.ps1`, `test-*.ps1`,
  `benchmark-engine.ps1`, `measure-runtime.ps1`, `collect-diagnostics.ps1`.
- Dependency/Sandbox: `check-prerequisites.ps1`, `collect-third-party.ps1`,
  `verify-third-party.ps1`, `prepare-sandbox.ps1`.

`check-prerequisites.ps1` và `verify-beta.ps1` vẫn được sao chép vào thư mục
gốc của gói beta để sử dụng độc lập với repository.

TSF là component thử nghiệm riêng, chưa nằm trong gói phát hành. Xem
[`docs/TEXT_SERVICE.md`](../docs/TEXT_SERVICE.md) trước khi chạy
`register-text-service.ps1` (cần Administrator) hoặc probe UI Automation.

## Dọn output sinh tự động

```powershell
./scripts/clean-artifacts.ps1         # Xem trước, không xóa
./scripts/clean-artifacts.ps1 -Apply  # Thực hiện dọn
```

Giữ executable/PDB Debug engine mới nhất, bộ runtime UI Debug, bộ cài local
mới nhất và ZIP nguồn của nó, báo cáo hồi quy đạt mới nhất theo shell, báo cáo
chẩn đoán/đo hiệu năng, cache compiler và runtime đầu vào. Các output build
trung gian, gói cũ và fixture kiểm thử được sinh lại khi chạy công cụ.

Chỉ dọn đường dẫn đã xác minh bên trong repository và được Git ignore;
không xóa file được Git quản lý, đường dẫn có junction/symlink hoặc output
OpenKey đang chạy. Báo cáo lần dọn nằm tại `dist/cleanup-report.json`.
