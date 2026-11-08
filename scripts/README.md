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
./scripts/build-setup.ps1 -Release -Offline # tùy chọn thủ công khi cần cài không có mạng
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
mới nhất của mỗi loại online/offline và ZIP nguồn, báo cáo hồi quy đạt mới nhất theo shell, báo cáo
chẩn đoán/đo hiệu năng, cache compiler và runtime đầu vào. Các output build
trung gian, gói cũ và fixture kiểm thử được sinh lại khi chạy công cụ.

Chỉ dọn đường dẫn đã xác minh bên trong repository và được Git ignore;
không xóa file được Git quản lý, đường dẫn có junction/symlink hoặc output
OpenKey đang chạy. Báo cáo lần dọn nằm tại `dist/cleanup-report.json`.

## Chẩn đoán tương thích Ubuntu

Python 3: `python3 scripts/linux-compat-report.py --help`. Công cụ chỉ đọc
journal/file log, xuất JSON đã lọc; không đổi IM hoặc restart Fcitx5.
Xem [nghiệm thu Chrome/VS Code](../docs/UBUNTU_DESKTOP_QA.md) để chọn ca thử,
tạo báo cáo và hiểu giới hạn. Chưa được đóng gói vào DEB alpha.50.

Báo cáo tương thích hiện dùng schema v2: đủ cause hủy edit, thêm tối đa 32
record cuối đã lọc/reference/boolean theo thứ tự đầu vào. Không phải trace
của từng context, không tự tính độ trễ hoặc kết luận ứng dụng gõ đúng.

## Ubuntu beta DEB

Run `./scripts/build-linux-beta-deb.sh` for one complete native Ubuntu DEB.
It builds/tests addon + service + settings, packages the GNOME extension,
then verifies the extracted payload. Install using `sudo apt install ./file.deb`
so APT resolves runtime libraries; maintainer scripts never download packages.
See [beta setup](../docs/UBUNTU_BETA.md).

`python3 scripts/uninstall-linux-user.py` lists exact user-local paths;
`--apply` backs them up before removing OpenKey files and stopping its UI/tray.
It preserves the shared Fcitx profile and other applications, and never removes
system DEBs. Remove old system DEBs separately with authenticated APT.
