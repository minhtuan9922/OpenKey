# Bộ cài OpenKey Fluent offline

## Guard bảo toàn dữ liệu khi đóng gói

`build-setup.ps1` kiểm tra source cài/gỡ trước khi xử lý compiler, runtime, build hoặc staging. Guard giữ AppId/thư mục cài theo tài khoản, chỉ cho payload vào `{app}`, runtime dạng `dontcopy` và chạy engine sau cài theo lựa chọn (bỏ qua khi silent). Chặn section registry/delete, các API ghi/xóa dữ liệu đã liệt kê và include ngoài policy đã kiểm tra.

Chạy riêng `./scripts/test-setup-preservation.ps1`: 17 kiểm tra source/fixture, không truy cập registry hoặc cài app; cũng được gọi trong `test-setup.ps1`. Khi cố ý thay layout/actions, phải rà và cập nhật policy cùng fixture. Đây là guard bảo thủ, không phải parser/sandbox bảo mật, không chứng minh nâng cấp thật giữ mọi byte dữ liệu hoặc mọi side effect của runtime/engine. Bộ cài đã build trước đó không bị sửa; guard áp dụng cho lần đóng gói tiếp theo.

## Bộ cài 3.0.3 vừa build — 2026-10-11

- File: `dist/setup-b95a532783d34c8eb3d7d1dfd53ab70d/Output/OpenKey-Setup.exe`.
- Dung lượng: 62.109.873 byte (khoảng 59,2 MiB); SHA-256:
  `9DF95E847CC844AEE5BF5312F87362153D46527DF14886800E98A25565D296E8`.
- Build mới bằng `./scripts/build-setup.ps1 -Release`; một EXE chung x64/x86,
  không tách kiến trúc. Engine, updater và UI cả hai bản đều có FileVersion
  `3.0.3.0`, setup có ProductVersion `3.0.3`.
- Chứa sửa con trỏ loading của popup tray, khởi tạo owner của panel và
  policy đóng app fail-closed khi kiểm tra tiến trình lỗi/PID thay đổi.
- Verifier đạt trên PS7/5.1: 566 đầu vào compiler, 281 payload mỗi kiến trúc,
  ZIP/PE/hash và runtime Microsoft đã ký số. Bộ cài OpenKey vẫn chưa ký số.
- Các ca cập nhật/cài/gỡ/Sandbox trước đó đã được người dùng xác nhận;
  artifact vừa build này chưa được chạy cài đặt trong lượt đóng gói.
  Không tự tăng version, push/tag, upload hoặc chuyển kênh stable.

Mở nhanh thư mục chứa file:

```powershell
Invoke-Item -LiteralPath 'D:\Project\OpenKey\dist\setup-b95a532783d34c8eb3d7d1dfd53ab70d\Output'
```

## Artifact 3.0.0 lịch sử — 2026-10-11

- File: `dist/setup-cd44680b743447a889ae5935f2a85976/Output/OpenKey-Setup.exe`.
- Dung lượng: 62.108.317 byte; SHA-256:
  `77099935EFF224B894B0285C6490D980E293707F42BE2C60EC3EBFDB7DCA61EA`.
- Đã xác minh cả engine/updater/UI của x64 và x86 có FileVersion `3.0.0.0`,
  bộ cài có ProductVersion `3.0.0`. Metadata Windows dùng `v3.0.0` / `3`.
- Đã sửa packaging lấy đúng output solution; chặn binary cũ hoặc khác phiên bản.
  Không dùng bản thử `setup-8b52c41c20394dd2aa7e250844cfb870` (engine/updater cũ).
- Build Release hai kiến trúc không cảnh báo/lỗi; kiểm thử updater hai kiến trúc,
  UI 22/22, packaging 17/17 + staging 13/13, setup 24 + 9 + 38 đạt.
  Verifier setup đối chiếu 566 input đạt PS7/5.1. Chưa thực thi setup trên máy sạch.
- Chưa push/tag/upload GitHub. Khi phát hành, upload `OpenKey-Setup.exe` vào
  Release `v3.0.0` rồi mới cập nhật `master/version.json`; xem `UPDATE_RELEASES.md`.
- Vẫn là beta nội bộ chưa ký số; các điều kiện nghiệm thu bên dưới còn áp dụng.

## Artifact beta nội bộ — 2026-10-11

- File duy nhất: `dist/setup-9c0099c69ee6441f8edd91f2ace73cbe/Output/OpenKey-Setup.exe`.
- Dung lượng: 62.053.914 byte (khoảng 59,2 MiB).
- SHA-256: `ED88989112773AB4FB2F7DAC65FE91BE5E9394415280E7FFA89D2C4F27B2B567`.
- Tự chọn x64 trên Windows x64, x86 trên Windows x86; chưa hỗ trợ ARM.
- Windows tối thiểu theo cấu hình hiện tại: Windows 10 1809 (17763).
- Chứa engine, updater, Fluent UI self-contained, notices và hai runtime Microsoft.

Chỉ cần chuyển file EXE này sang máy kiểm thử; không cần các ZIP, manifest hoặc
thư mục staging đi cùng. Bộ cài chưa ký số, chưa nghiệm thu máy sạch và chưa đủ
điều kiện phát hành stable. SmartScreen có thể cảnh báo vì chưa ký số; đối chiếu
hash và nguồn trước khi chạy, không tắt các cơ chế bảo vệ Windows.

## Hành vi cài đặt

- Cài cho tài khoản đang chạy, mặc định `%LOCALAPPDATA%\Programs\OpenKey-Fluent`.
  Chạy bằng tài khoản thường, không chọn “Run as administrator” cho toàn setup.
- Thêm Start Menu → OpenKey Fluent → OpenKey (engine/bộ gõ) và OpenKey Settings.
  Shortcut Settings chỉ mở UI; shortcut OpenKey khởi chạy engine.
- Có checkbox tạo icon Desktop, mặc định không chọn.
- Có tiếng Việt/Anh; các trang thông dụng và thông báo riêng đã dịch.
  Thông báo lỗi hiếm của Inno có thể dùng tiếng Anh từ Default.isl.
- Kiểm tra VC++ Runtime đúng kiến trúc, tối thiểu `14.51.36231.0`.
  Nếu thiếu/cũ, yêu cầu đồng ý và UAC để cài runtime Microsoft offline
  `14.51.36247.0` trước khi chép file OpenKey. Không tải mạng.
- Hủy UAC, cài runtime lỗi hoặc không đạt kiểm tra phiên bản: không cài OpenKey.
  Runtime trả 3010/1641: yêu cầu khởi động lại rồi chạy lại setup, không tự reboot.
- Cài silent chỉ tiếp tục khi runtime đã đủ; không tự đồng ý cài runtime.
- Yêu cầu thoát engine ở tray và đóng UI trước cài/gỡ; không tự kill process.
- Cuối cài có tùy chọn chạy engine, mặc định không chọn; silent không chạy app.
- Gỡ trong Windows Installed apps/Apps & features, giữ registry settings/macro;
  không gỡ VC++ runtime dùng chung. Setup không ghi lựa chọn tự khởi động.
  Nếu đã bật tự khởi động trong OpenKey, tắt trong ứng dụng trước khi gỡ để
  tránh Run entry/scheduled task cũ trỏ tới executable đã gỡ.

Windows App SDK/WinUI đã nằm trong payload self-contained; người dùng không
cần cài Visual Studio. VC++ Redistributable cần rà soát quyền phân phối theo
[điều khoản Microsoft](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist)
và notices của các dependency trước khi phát hành công khai.

## Build lại

Yêu cầu Inno Setup 6.7.3 hoặc mới hơn. Compiler hiện dùng chế độ portable tại
`dist/inno-6.7.3-portable/compiler/ISCC.exe`, tải từ release chính thức và đã kiểm
tra Authenticode `Valid / Pyrsys B.V.`. Không thêm PATH/association/uninstaller.
Có thể chỉ định `-IsccPath` hoặc biến `OPENKEY_ISCC_PATH`; helper cũng tìm compiler
trong PATH và thư mục Inno Setup tiêu chuẩn. Script không tự tải/cài compiler.

Build mới cả x64/x86 và đóng gói luôn bằng một lệnh:

```powershell
.\scripts\build-setup.ps1 -Release
```

Script dùng runtime offline trong `dist/setup-inputs/vc_redist.x64.exe` và
`vc_redist.x86.exe`, đối chiếu SHA-256 đã ghim trong `Installer/OfflineInputs.json`.
Máy/clone mới cần chuẩn bị hai file này hoặc truyền cặp `-RuntimeX64` /
`-RuntimeHashX64` và `-RuntimeX86` / `-RuntimeHashX86` đã xác minh. Không tự tải mạng.
Sau build, script tự kiểm tra setup và in `SETUP FILE`, `OUTPUT FOLDER`, lệnh
`Invoke-Item` mở thư mục và `explorer.exe /select,...` chọn file. Không tự chạy bộ cài.

Đã chạy thành công lệnh trên: `dist/setup-5e2bd6e034514684806175f2c9ce22fc/Output/OpenKey-Setup.exe`;
SHA-256 `C9CE37B271FAE67AF858608836219D5ED41C35E44E39DD3F78C8BC08C12837CC`.

Nếu muốn dùng các ZIP có sẵn, tạo payload Release riêng:

```powershell
.\scripts\package-beta.ps1 x64
.\scripts\package-beta.ps1 x86
```

Sau đó truyền hai ZIP và runtime đã xác minh (thay đường dẫn/hash bằng dữ liệu
đầu vào thực tế, không tự chọn gói “mới nhất”):

```powershell
.\scripts\build-setup.ps1 `
  -ZipX64 'path\beta-x64.zip' -ZipX86 'path\beta-x86.zip' `
  -RuntimeX64 'path\vc_redist.x64.exe' -RuntimeHashX64 'SHA256_X64' `
  -RuntimeX86 'path\vc_redist.x86.exe' -RuntimeHashX86 'SHA256_X86'
```

Build script kiểm tra ZIP/kiến trúc/hash và chữ ký Microsoft/version/product
của runtime trước và sau copy; GUID staging mới không ghi đè gói cũ. Compiler
manifest ghi tên/size/hash của tất cả input thực sự nhúng. Output chỉ có một EXE;
`setup-build.json` và `compiler-manifest.txt` ở ngoài thư mục Output để kiểm tra.

```powershell
.\scripts\test-setup.ps1
.\scripts\verify-setup.ps1 -BuildReport 'dist\setup-...\setup-build.json' -ExpectedSha256 'SHA256_SETUP'
.\test-all.ps1 All
```

24 source contract + 38 fixture Pascal native đạt PS7/5.1. Probe fixture luôn
trả False ở InitializeSetup, trước cài đặt; không đọc/ghi registry, không chạy
app/runtime. Full hồi quy 17/17 mỗi shell đạt; source contract/policy chạy lại
sau sửa compiler resolver. Verifier đối chiếu 566 input nhúng, hai ZIP Release
281 payload/kiến trúc, PE bootstrap x86, size/hash setup và runtime ký Microsoft.
Build end-to-end PS7/5.1 tạo EXE cùng hash. Không thực thi setup OpenKey trên host.

## Nghiệm thu thực tế và các ca còn lại

- [x] Người dùng xác nhận luồng cập nhật thực tế lên 3.0.3 hoạt động ổn
  (2026-10-11). Ghi nhận luồng bình thường; các ca từ chối/timeout/debugger,
  ma trận tương thích bên dưới vẫn cần xác nhận riêng.

- [x] Người dùng xác nhận cài đặt, gỡ bỏ và chạy trong Windows Sandbox ổn
  (2026-10-11).
- [x] Người dùng xác nhận kiểm thử trên máy thật hoạt động ổn (2026-10-11).
  Các xác nhận này không bao gồm toàn bộ biến thể UAC/runtime, hệ điều hành,
  kiến trúc và DPI chưa được nêu cụ thể.

Khi cập nhật và OpenKey đang chạy trong đúng thư mục đích, Setup hỏi có đóng
engine/giao diện không (mặc định Không). Đồng ý sẽ gửi IPC thoát engine và
WM_CLOSE cho UI, đợi tối đa tổng cộng 5 giây để tiến trình thoát. Không dùng
TerminateProcess/kill. Từ chối, lỗi quyền, debugger giữ tiến trình hoặc timeout
đều dừng trước khi cài runtime/thay file. Cài silent không tự đóng ứng dụng.
Engine ở thư mục khác không bị đóng; nếu còn phiên OpenKey khác, đóng thủ công.
Nội dung UI chưa lưu có thể mất khi xác nhận đóng, dữ liệu đã lưu được giữ lại.

Bộ cài chỉ gửi lệnh đóng khi handle xác nhận tiến trình còn chạy và PID cửa sổ
vẫn đúng. `WAIT_FAILED`, kết quả chờ không xác định hoặc PID thay đổi đều chặn
cập nhật; tiến trình đã thoát không cần gửi message. Policy và deadline 5 giây
được kiểm tra bằng fixture Pascal dùng chung: 67 ca đạt trên PS7/5.1, mã bộ cài
thật compile đạt. Các fixture không thực hiện đóng app/UAC/cài đặt, nên không
thay thế kiểm thử trực tiếp các ca lỗi dưới đây.

- [ ] Cập nhật khi engine/UI đang mở: Đồng ý đóng; từ chối; timeout/debugger;
  engine thư mục khác; UI minimized; kiểm tra giữ cấu hình/gõ tắt và gỡ hook.

- [ ] Runtime thiếu/cũ/đủ; consent, hủy UAC, mã lỗi, restart cần thiết.
- [ ] Cài offline với mạng tắt, chỉ một EXE; thư mục có dấu/khoảng trắng.
- [ ] Shortcut Start Menu khởi chạy engine và gõ tiếng Việt; Settings mở Fluent.
- [ ] Desktop có/không chọn; tiếng Việt/Anh và Sáng/Tối theo Windows.
- [ ] Cài lại/nâng cấp khi app đang mở; không kill, giữ cấu hình/gõ tắt.
- [ ] Gỡ khi đóng app, giữ dữ liệu/runtime, kiểm tra shortcut và startup cũ.
- [ ] Windows x64/x86 và minimum OS; ARM hiện bị chặn có chủ đích.
- [ ] Chữ ký số, review giấy phép dependency/runtime và phát hành beta.
